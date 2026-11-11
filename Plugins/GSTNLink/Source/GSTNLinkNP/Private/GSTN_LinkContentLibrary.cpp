// Links, Network Prediction half.

#include "GSTN_LinkContentLibrary.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkInteractProvider.h"
#include "Sim/GSTN_LinkSnapshotExt.h"
#include "Sim/GSTN_LinkTickExtension.h"
#include "GSTN_LinkTags.h"
#include "Harness/GSTN_LinkTestAbilities.h"
#include "Moves/GSTN_LinkThrowMoves.h"
#include "Animation/AnimMontage.h"
#include "Input/GSTN_LinkSlots.h"
#include "Data/GSTN_LinkKitExtension.h"
#include "Hits/GSTN_HitSteps.h"
#include "Kits/GSTN_AbilityBinding.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Pipeline/GSTN_HitStep_GrabImmunity.h"
#include "Pipeline/GSTN_HitStep_LinkBreakOnHit.h"
#include "Pipeline/GSTN_HitStep_LinkRequest.h"
#include "Rules/GSTN_CombatRules.h"
#include "Serialization/MemoryWriter.h"
#include "Sim/GSTN_LinkAttach.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Sim/GSTN_LinkInboxKinds.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_LinkPresentationComponent.h"
#include "GSTN_LinkWatchdog.h"
#include "World/GSTN_CombatWorldSubsystem.h"

namespace
{
	bool IsStepOf(const TSoftClassPtr<UGSTN_HitResolutionStep>& Step, const UClass* Class)
	{
		return Step.ToSoftObjectPath() == FSoftObjectPath(Class);
	}

	/** The timeline data the kit binds to Command (inline on the ability's class default, or its asset), mutable for an in-memory edit */
	FGSTN_CombatTimelineData* FindMoveTimeline(UGSTN_PrimaryKitDefinition* Kit, const FGameplayTag& Command)
	{
		if (!Kit)
		{
			return nullptr;
		}
		// The kit's own bindings, then its primary weapon's (an attack is usually the weapon's)
		TArray<FGSTN_AbilityBinding> Bindings = Kit->KitBindings;
		if (const UGSTN_WeaponDefinition* Weapon = Kit->PrimaryWeapon.LoadSynchronous())
		{
			TArray<FGSTN_AbilityBinding> WeaponBindings;
			Weapon->GetEffectiveBindings(WeaponBindings);
			Bindings.Append(WeaponBindings);
		}
		for (const FGSTN_AbilityBinding& Binding : Bindings)
		{
			if (Binding.Command != Command)
			{
				continue;
			}
			if (UGSTN_CombatTimeline* Override = Binding.TimelineOverride.LoadSynchronous())
			{
				return &Override->Data;
			}
			UClass* AbilityClass = Binding.Ability.LoadSynchronous();
			UGSTN_GA_BaseCombat* Default = AbilityClass ? Cast<UGSTN_GA_BaseCombat>(AbilityClass->GetDefaultObject()) : nullptr;
			if (!Default)
			{
				return nullptr;
			}
			if (UGSTN_CombatTimeline* Asset = Default->TimelineAsset.LoadSynchronous())
			{
				return &Asset->Data;
			}
			return &Default->Timeline;
		}
		return nullptr;
	}

	const FGSTN_SnapshotEntry* FindEntry(const AActor* Actor, const UGSTN_CombatWorldSubsystem*& OutCombat)
	{
		OutCombat = Actor ? UGSTN_CombatWorldSubsystem::Get(Actor->GetWorld()) : nullptr;
		if (!OutCombat)
		{
			return nullptr;
		}
		for (const FGSTN_SnapshotEntry& Entry : OutCombat->GetSnapshot().Entries)
		{
			if (OutCombat->FindOwnerActor(Entry.EntityId) == Actor)
			{
				return &Entry;
			}
		}
		return nullptr;
	}
}

bool UGSTN_LinkContentLibrary::AddLinkStepsToRules(UGSTN_CombatRules* Rules)
{
	if (!Rules)
	{
		return false;
	}
	TArray<TSoftClassPtr<UGSTN_HitResolutionStep>>& Pipeline = Rules->HitPipeline;
	const bool bHasImmunity = Pipeline.ContainsByPredicate([](const TSoftClassPtr<UGSTN_HitResolutionStep>& Step) { return IsStepOf(Step, UGSTN_HitStep_GrabImmunity::StaticClass()); });
	const bool bHasRequest = Pipeline.ContainsByPredicate([](const TSoftClassPtr<UGSTN_HitResolutionStep>& Step) { return IsStepOf(Step, UGSTN_HitStep_LinkRequest::StaticClass()); });
	const bool bHasBreak = Pipeline.ContainsByPredicate([](const TSoftClassPtr<UGSTN_HitResolutionStep>& Step) { return IsStepOf(Step, UGSTN_HitStep_LinkBreakOnHit::StaticClass()); });
	if (bHasImmunity && bHasRequest && bHasBreak)
	{
		return true;
	}
	RemoveLinkStepsFromRules(Rules);
	int32 At = 0;
	for (int32 Index = 0; Index < Pipeline.Num(); ++Index)
	{
		if (IsStepOf(Pipeline[Index], UGSTN_HitStep_HurtState::StaticClass()))
		{
			At = Index + 1;
			break;
		}
	}
	Pipeline.Insert(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_LinkRequest::StaticClass()), At);
	Pipeline.Insert(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_GrabImmunity::StaticClass()), At);
	// LinkBreakOnHit after MeterGain (m6 spec 9 step 8: the knockback is known), else last
	int32 BreakAt = Pipeline.Num();
	for (int32 Index = 0; Index < Pipeline.Num(); ++Index)
	{
		if (IsStepOf(Pipeline[Index], UGSTN_HitStep_MeterGain::StaticClass()))
		{
			BreakAt = Index + 1;
		}
	}
	Pipeline.Insert(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_LinkBreakOnHit::StaticClass()), BreakAt);
	Rules->ResolveHitPipeline();
	return true;
}

bool UGSTN_LinkContentLibrary::RemoveLinkStepsFromRules(UGSTN_CombatRules* Rules)
{
	if (!Rules)
	{
		return false;
	}
	const int32 Removed = Rules->HitPipeline.RemoveAll([](const TSoftClassPtr<UGSTN_HitResolutionStep>& Step)
	{
		return IsStepOf(Step, UGSTN_HitStep_GrabImmunity::StaticClass()) || IsStepOf(Step, UGSTN_HitStep_LinkRequest::StaticClass())
			|| IsStepOf(Step, UGSTN_HitStep_LinkBreakOnHit::StaticClass());
	});
	if (Removed > 0)
	{
		Rules->ResolveHitPipeline();
	}
	return Removed > 0;
}

int32 UGSTN_LinkContentLibrary::SetKitMoveHitType(UGSTN_PrimaryKitDefinition* Kit, FGameplayTag Command, FGameplayTag HitType, FGameplayTag LinkType)
{
	FGSTN_CombatTimelineData* Timeline = FindMoveTimeline(Kit, Command);
	if (!Timeline)
	{
		return 0;
	}
	int32 Changed = 0;
	for (TInstancedStruct<FGSTN_TimelineTrack>& Track : Timeline->Tracks)
	{
		if (FGSTN_TimelineTrack_Hitbox* Hitboxes = Track.GetMutablePtr<FGSTN_TimelineTrack_Hitbox>())
		{
			for (FGSTN_HitboxSpec& Spec : Hitboxes->Hitboxes)
			{
				Spec.Data.HitType = HitType;
				Spec.Data.LinkType = LinkType;
				++Changed;
			}
		}
	}
	return Changed;
}

bool UGSTN_LinkContentLibrary::GetKitMoveHitType(UGSTN_PrimaryKitDefinition* Kit, FGameplayTag Command, FGameplayTag& OutHitType, FGameplayTag& OutLinkType)
{
	FGSTN_CombatTimelineData* Timeline = FindMoveTimeline(Kit, Command);
	if (!Timeline)
	{
		return false;
	}
	for (TInstancedStruct<FGSTN_TimelineTrack>& Track : Timeline->Tracks)
	{
		if (const FGSTN_TimelineTrack_Hitbox* Hitboxes = Track.GetPtr<FGSTN_TimelineTrack_Hitbox>())
		{
			if (!Hitboxes->Hitboxes.IsEmpty())
			{
				OutHitType = Hitboxes->Hitboxes[0].Data.HitType;
				OutLinkType = Hitboxes->Hitboxes[0].Data.LinkType;
				return true;
			}
		}
	}
	return false;
}

bool UGSTN_LinkContentLibrary::FormTestLink(AActor* Initiator, AActor* Target, FGameplayTag LinkType)
{
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const UGSTN_LinkTypeDefinition* Definition = Registry ? Registry->FindDefinition(LinkType) : nullptr;
	UGSTN_CombatWorldSubsystem* Combat = Initiator ? UGSTN_CombatWorldSubsystem::Get(Initiator->GetWorld()) : nullptr;
	const int32 InitiatorId = GetEntityId(Initiator);
	const int32 TargetId = GetEntityId(Target);
	if (!Definition || !Combat || InitiatorId == INDEX_NONE || TargetId == INDEX_NONE || InitiatorId == TargetId)
	{
		return false;
	}
	const FGSTN_LinkFragment* InitiatorShown = UGSTN_LinkPresentationComponent::FindShownFragment(Initiator);
	const FGSTN_LinkFragment* TargetShown = UGSTN_LinkPresentationComponent::FindShownFragment(Target);
	const FGSTN_LinkState None;
	const FGSTN_LinkState& InitiatorState = InitiatorShown ? InitiatorShown->Get(Definition->Channel) : None;
	const FGSTN_LinkState& TargetState = TargetShown ? TargetShown->Get(Definition->Channel) : None;
	if (InitiatorState.Phase != EGSTN_LinkPhase::None || TargetState.Phase != EGSTN_LinkPhase::None)
	{
		return false;
	}

	FGSTN_LinkCommandEntry Command;
	Command.Kind = EGSTN_LinkCommandKind::Form;
	Command.Channel = Definition->Channel;
	Command.Serial = FGSTN_LinkState::NextSerial(InitiatorState.Serial, TargetState.Serial);
	Command.LinkTypeIndex = Registry->GetTypeIndex(LinkType);
	const int32 Frame = Combat->GetSnapshot().Frame;

	FGSTN_InboxEntry ToInitiator = FGSTN_LinkInboxHandlers::MakeCommand([&]
	{
		FGSTN_LinkCommandEntry Own = Command;
		Own.Role = 1;
		Own.PartnerId = TargetId;
		return Own;
	}());
	ToInitiator.Frame = Frame;
	ToInitiator.SenderId = TargetId;
	FGSTN_InboxEntry ToTarget = FGSTN_LinkInboxHandlers::MakeCommand([&]
	{
		FGSTN_LinkCommandEntry Own = Command;
		Own.Role = 2;
		Own.PartnerId = InitiatorId;
		return Own;
	}());
	ToTarget.Frame = Frame;
	ToTarget.SenderId = InitiatorId;
	const bool bInitiator = Combat->PostToInbox(InitiatorId, ToInitiator);
	const bool bTarget = Combat->PostToInbox(TargetId, ToTarget);
	return bInitiator && bTarget;
}

bool UGSTN_LinkContentLibrary::BreakTestLink(AActor* Entity)
{
	UGSTN_CombatWorldSubsystem* Combat = Entity ? UGSTN_CombatWorldSubsystem::Get(Entity->GetWorld()) : nullptr;
	const FGSTN_LinkFragment* Shown = UGSTN_LinkPresentationComponent::FindShownFragment(Entity);
	const int32 EntityId = GetEntityId(Entity);
	if (!Combat || !Shown || EntityId == INDEX_NONE || !Shown->Links.Body.IsLinked())
	{
		return false;
	}
	const FGSTN_LinkState& Body = Shown->Links.Body;
	FGSTN_LinkCommandEntry Command;
	Command.Kind = EGSTN_LinkCommandKind::Break;
	Command.Channel = EGSTN_LinkChannel::Body;
	Command.Serial = Body.Serial;
	Command.Reason = static_cast<uint8>(EGSTN_LinkBreakReason::Forced);
	FGSTN_InboxEntry Entry = FGSTN_LinkInboxHandlers::MakeCommand(Command);
	Entry.Frame = Combat->GetSnapshot().Frame;
	Entry.SenderId = EntityId;
	const bool bOwn = Combat->PostToInbox(EntityId, Entry);
	const bool bPartner = Combat->PostToInbox(Body.PartnerId.Id, Entry);
	return bOwn && bPartner;
}

bool UGSTN_LinkContentLibrary::GetShownBodyLink(const AActor* Actor, int32& OutRole, int32& OutPhase, int32& OutSerial, int32& OutLinkFrame, int32& OutPartnerId)
{
	const FGSTN_LinkFragment* Shown = UGSTN_LinkPresentationComponent::FindShownFragment(Actor);
	if (!Shown)
	{
		OutRole = OutPhase = OutSerial = OutLinkFrame = 0;
		OutPartnerId = INDEX_NONE;
		return false;
	}
	const FGSTN_LinkState& Body = Shown->Links.Body;
	OutRole = Body.Role;
	OutPhase = static_cast<int32>(Body.Phase);
	OutSerial = Body.Serial;
	OutLinkFrame = Body.LinkFrame;
	OutPartnerId = Body.PartnerId.IsNull() ? INDEX_NONE : static_cast<int32>(Body.PartnerId.Id);
	return true;
}

FString UGSTN_LinkContentLibrary::GetShownLinkText(const AActor* Actor)
{
	const FGSTN_LinkFragment* Shown = UGSTN_LinkPresentationComponent::FindShownFragment(Actor);
	if (!Shown)
	{
		return FString();
	}
	TAnsiStringBuilder<512> Builder;
	Shown->ToString(Builder);
	return FString(Builder.ToString());
}

int32 UGSTN_LinkContentLibrary::GetShownLinkPayloadBytes(const AActor* Actor)
{
	const FGSTN_LinkFragment* Shown = UGSTN_LinkPresentationComponent::FindShownFragment(Actor);
	if (!Shown)
	{
		return 0;
	}
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	const_cast<FGSTN_LinkFragment*>(Shown)->NetSerialize(Writer);
	return Bytes.Num();
}

int32 UGSTN_LinkContentLibrary::GetEntityId(const AActor* Actor)
{
	const UGSTN_CombatWorldSubsystem* Combat = nullptr;
	const FGSTN_SnapshotEntry* Entry = FindEntry(Actor, Combat);
	return Entry ? Entry->EntityId : INDEX_NONE;
}

bool UGSTN_LinkContentLibrary::GetSnapshotTransform(const AActor* Actor, FTransform& OutTransform)
{
	const UGSTN_CombatWorldSubsystem* Combat = nullptr;
	const FGSTN_SnapshotEntry* Entry = FindEntry(Actor, Combat);
	if (!Entry)
	{
		return false;
	}
	OutTransform = Entry->Transform;
	return true;
}

int32 UGSTN_LinkContentLibrary::RebuildRegistryAndReattach()
{
	if (UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get())
	{
		Registry->RebuildFromAssetManager();
	}
	int32 Attached = 0;
	if (!GEngine)
	{
		return 0;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!World)
		{
			continue;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->FindComponentByClass<UGSTN_CombatSimComponent>())
			{
				// The kit's link extension stays the one the loadout attached
				const FGSTN_LinkBodyData* Previous = FGSTN_LinkAttach::Find(*It);
				const UGSTN_PrimaryKitDefinition* Kit = Previous ? Previous->Kit.Get() : nullptr;
				FGSTN_LinkAttach::Apply(*It, Kit);
				++Attached;
			}
		}
	}
	return Attached;
}

int32 UGSTN_LinkContentLibrary::GetLinkTypeIndex(FGameplayTag LinkType)
{
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	return Registry ? Registry->GetTypeIndex(LinkType) : 0;
}

bool UGSTN_LinkContentLibrary::GetWatchdogStats(const AActor* WorldActor, int32& OutBreaks, int32& OutOrphans, int32& OutLastReason, int32& OutLastFrame)
{
	const UGSTN_LinkWatchdog* Watchdog = WorldActor ? UGSTN_LinkWatchdog::Get(WorldActor->GetWorld()) : nullptr;
	OutBreaks = Watchdog ? Watchdog->GetNumBreaks() : 0;
	OutOrphans = Watchdog ? Watchdog->GetNumOrphans() : 0;
	OutLastReason = Watchdog && Watchdog->GetLastBreakFrame() != INDEX_NONE ? static_cast<int32>(Watchdog->GetLastBreak().Reason) : 0;
	OutLastFrame = Watchdog ? Watchdog->GetLastBreakFrame() : INDEX_NONE;
	return Watchdog != nullptr;
}

bool UGSTN_LinkContentLibrary::GetShownBodyBreak(const AActor* Actor, int32& OutReason, int32& OutSerial, int32& OutGrabImmunityEndFrame)
{
	const FGSTN_LinkFragment* Shown = UGSTN_LinkPresentationComponent::FindShownFragment(Actor);
	OutReason = Shown ? Shown->Links.Body.BreakReason : 0;
	OutSerial = Shown ? Shown->Links.Body.Serial : 0;
	OutGrabImmunityEndFrame = Shown ? Shown->GrabImmunityEndFrame : 0;
	return Shown != nullptr;
}

FString UGSTN_LinkContentLibrary::GetMirrorText(const AActor* Actor)
{
	int32 Role = 0, Phase = 0, Serial = 0, LinkFrame = 0, Partner = INDEX_NONE;
	if (!GetShownBodyLink(Actor, Role, Phase, Serial, LinkFrame, Partner))
	{
		return FString();
	}
	FString Parked = TEXT("-");
	const UGSTN_LinkPresentationComponent* Presentation = Actor->FindComponentByClass<UGSTN_LinkPresentationComponent>();
	FTransform Shown;
	if (Presentation && Presentation->IsLinkedParked() && Presentation->GetLinkedTransform(Shown))
	{
		const FVector L = Shown.GetLocation();
		Parked = FString::Printf(TEXT("%.2f,%.2f,%.2f"), L.X, L.Y, L.Z);
	}
	return FString::Printf(TEXT("%d,%d,%d,%d,%d;%s"), Role, Phase, Serial, LinkFrame, Partner, *Parked);
}

namespace
{
	/** The harness's separate-process mode: a client process cannot run the editor's Python, so the server sends it this through the test component's
	    remote command. It does what link_scenarios.link_setup does in the editor: rebuild the registry, reattach the pawns, add the link steps to the
	    world's combat rules (in memory, as the editor's copy) */
	FAutoConsoleCommandWithWorldAndArgs GLinkTestPrepareCommand(
		TEXT("gstn.Link.Test.Prepare"),
		TEXT("Harness: rebuild the link registry, reattach the combat pawns and (with 'steps') add the link steps to the combat rules of this world."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const int32 Attached = UGSTN_LinkContentLibrary::RebuildRegistryAndReattach();
			bool bSteps = false;
			if (World && Args.Contains(TEXT("steps")))
			{
				for (TActorIterator<AActor> It(World); It && !bSteps; ++It)
				{
					if (const UGSTN_CombatSimComponent* Combat = It->FindComponentByClass<UGSTN_CombatSimComponent>())
					{
						bSteps = UGSTN_LinkContentLibrary::AddLinkStepsToRules(Combat->GetCombatRulesForTest());
					}
				}
			}
			UE_LOG(LogTemp, Log, TEXT("gstn.Link.Test.Prepare: %d pawns attached, link steps %s"), Attached, bSteps ? TEXT("added") : TEXT("not added"));
		}));
}

namespace
{
	/** What AddLinkTestBindings replaced, per kit, for RemoveLinkTestBindings (harness only; the kits are root content that outlives a PIE) */
	struct FKitBackup
	{
		FGSTN_CommandSet CommandSet;
		TArray<FGSTN_AbilityBinding> KitBindings;
		FGSTN_KitExtensionList Extensions;
	};
	TMap<TWeakObjectPtr<UGSTN_PrimaryKitDefinition>, FKitBackup>& KitBackups()
	{
		static TMap<TWeakObjectPtr<UGSTN_PrimaryKitDefinition>, FKitBackup> Backups;
		return Backups;
	}

	TSoftClassPtr<UGameplayAbility> AbilityFor(const FGSTN_LinkSlotBinding& Binding)
	{
		using namespace GSTNLinkTags;
		const FGameplayTag& Command = Binding.Command;
		UClass* Class = nullptr;
		if (Command == Command_Link_Grab.GetTag())
		{
			Class = UGSTN_GA_LinkTestGrab::StaticClass();
		}
		else if (Command == Command_Link_Pummel.GetTag())
		{
			Class = UGSTN_GA_LinkTestPummel::StaticClass();
		}
		else if (Command == Command_Link_Release.GetTag())
		{
			Class = UGSTN_GA_LinkTestRelease::StaticClass();
		}
		else if (Command == Command_Link_Latch_Attack.GetTag())
		{
			Class = UGSTN_GA_LinkTestLatchAttack::StaticClass();
		}
		else if (Command.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Command.Link.Throw"))))
		{
			Class = UGSTN_GA_LinkTestThrow::StaticClass();
		}
		return TSoftClassPtr<UGameplayAbility>(Class);
	}
}

int32 UGSTN_LinkContentLibrary::AddLinkTestBindings(UGSTN_PrimaryKitDefinition* Kit)
{
	if (!Kit)
	{
		return 0;
	}
	if (!KitBackups().Contains(Kit))
	{
		FKitBackup& Backup = KitBackups().Add(Kit);
		Backup.CommandSet = Kit->CommandSet;
		Backup.KitBindings = Kit->KitBindings;
		Backup.Extensions = Kit->Extensions;
	}

	// The slots: the standard link set replaces the kit's own slots of the same commands
	const TArray<FGSTN_InputSlotSpec> Specs = UGSTN_LinkSlots::Build();
	Kit->CommandSet.AddedSlots.RemoveAll([&Specs](const FGSTN_InputSlotEntry& Entry)
	{
		return Specs.ContainsByPredicate([&Entry](const FGSTN_InputSlotSpec& Spec) { return Spec.Command == Entry.GetSpec().Command; });
	});
	for (const FGSTN_InputSlotSpec& Spec : Specs)
	{
		FGSTN_InputSlotEntry Entry;
		Entry.Inline = Spec;
		Kit->CommandSet.AddedSlots.Add(MoveTemp(Entry));
	}

	// The bindings, with the slots' requirements; the latched role's release is the shake-off (the role's own release reason)
	TArray<FGSTN_LinkSlotBinding> Bindings = UGSTN_LinkSlots::DefaultBindings();
	FGSTN_LinkSlotBinding ShakeOff;
	ShakeOff.Command = GSTNLinkTags::Command_Link_Release;
	ShakeOff.Requirements.RequireTags.AddTag(GSTNLinkTags::Role_Latched);
	ShakeOff.Requirements.IgnoreTags.AddTag(GSTNLinkTags::Role_Grabber);
	ShakeOff.Requirements.IgnoreTags.AddTag(GSTNLinkTags::Role_Latcher);
	Bindings.Add(ShakeOff);
	Kit->KitBindings.RemoveAll([](const FGSTN_AbilityBinding& Binding) { return Binding.Command.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Command.Link"))); });
	int32 Added = 0;
	for (const FGSTN_LinkSlotBinding& Slot : Bindings)
	{
		FGSTN_AbilityBinding& Binding = Kit->KitBindings.AddDefaulted_GetRef();
		Binding.Command = Slot.Command;
		Binding.Requirements = Slot.Requirements;
		Binding.Ability = AbilityFor(Slot);
		++Added;
	}

	if (!Kit->Extensions.Find<FGSTN_LinkKitExtension>())
	{
		Kit->Extensions.ExtensionData.Add(FInstancedStruct::Make(FGSTN_LinkKitExtension()));
	}
	return Added;
}

bool UGSTN_LinkContentLibrary::RemoveLinkTestBindings(UGSTN_PrimaryKitDefinition* Kit)
{
	FKitBackup Backup;
	if (!Kit || !KitBackups().RemoveAndCopyValue(Kit, Backup))
	{
		return false;
	}
	Kit->CommandSet = Backup.CommandSet;
	Kit->KitBindings = Backup.KitBindings;
	Kit->Extensions = Backup.Extensions;
	return true;
}

bool UGSTN_LinkContentLibrary::WriteKitLinkContent(UGSTN_PrimaryKitDefinition* Kit, TSubclassOf<UGameplayAbility> Grab, TSubclassOf<UGameplayAbility> Pummel,
	TSubclassOf<UGameplayAbility> Release, TSubclassOf<UGameplayAbility> ThrowForward, TSubclassOf<UGameplayAbility> ThrowBack,
	TSubclassOf<UGameplayAbility> ThrowUp, TSubclassOf<UGameplayAbility> ThrowDown, TSubclassOf<UGameplayAbility> LatchAttack, int32 MashBaseFrames)
{
	if (!Kit)
	{
		return false;
	}
	using namespace GSTNLinkTags;
	const TMap<FGameplayTag, TSubclassOf<UGameplayAbility>> ByCommand = {
		{ Command_Link_Grab.GetTag(), Grab }, { Command_Link_Pummel.GetTag(), Pummel }, { Command_Link_Release.GetTag(), Release },
		{ Command_Link_Throw_Forward.GetTag(), ThrowForward }, { Command_Link_Throw_Back.GetTag(), ThrowBack }, { Command_Link_Throw_Up.GetTag(), ThrowUp },
		{ Command_Link_Throw_Down.GetTag(), ThrowDown }, { Command_Link_Latch_Attack.GetTag(), LatchAttack } };

	// The same slots and bindings AddLinkTestBindings puts on a kit in memory, built on copies so an unchanged kit is left untouched
	FGSTN_CommandSet CommandSet = Kit->CommandSet;
	const TArray<FGSTN_InputSlotSpec> Specs = UGSTN_LinkSlots::Build();
	CommandSet.AddedSlots.RemoveAll([&Specs](const FGSTN_InputSlotEntry& Entry)
	{
		return Specs.ContainsByPredicate([&Entry](const FGSTN_InputSlotSpec& Spec) { return Spec.Command == Entry.GetSpec().Command; });
	});
	for (const FGSTN_InputSlotSpec& Spec : Specs)
	{
		FGSTN_InputSlotEntry Entry;
		Entry.Inline = Spec;
		CommandSet.AddedSlots.Add(MoveTemp(Entry));
	}

	TArray<FGSTN_LinkSlotBinding> Slots = UGSTN_LinkSlots::DefaultBindings();
	FGSTN_LinkSlotBinding ShakeOff;
	ShakeOff.Command = Command_Link_Release;
	ShakeOff.Requirements.RequireTags.AddTag(Role_Latched);
	ShakeOff.Requirements.IgnoreTags.AddTag(Role_Grabber);
	ShakeOff.Requirements.IgnoreTags.AddTag(Role_Latcher);
	Slots.Add(ShakeOff);
	TArray<FGSTN_AbilityBinding> KitBindings = Kit->KitBindings;
	KitBindings.RemoveAll([](const FGSTN_AbilityBinding& Binding) { return Binding.Command.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Command.Link"))); });
	for (const FGSTN_LinkSlotBinding& Slot : Slots)
	{
		const TSubclassOf<UGameplayAbility>* Class = ByCommand.Find(Slot.Command);
		if (!Class || !*Class)
		{
			continue;
		}
		FGSTN_AbilityBinding& Binding = KitBindings.AddDefaulted_GetRef();
		Binding.Command = Slot.Command;
		Binding.Requirements = Slot.Requirements;
		Binding.Ability = TSoftClassPtr<UGameplayAbility>(Class->Get());
	}

	FGSTN_KitExtensionList Extensions = Kit->Extensions;
	FGSTN_LinkKitExtension LinkExtension;
	LinkExtension.GrabRules.Mash.BaseFrames = MashBaseFrames;
	const int32 Existing = Extensions.ExtensionData.IndexOfByPredicate([](const FInstancedStruct& Entry) { return Entry.GetPtr<FGSTN_LinkKitExtension>() != nullptr; });
	if (Existing != INDEX_NONE)
	{
		Extensions.ExtensionData[Existing] = FInstancedStruct::Make(LinkExtension);
	}
	else
	{
		Extensions.ExtensionData.Add(FInstancedStruct::Make(LinkExtension));
	}

	bool bSame = FGSTN_CommandSet::StaticStruct()->CompareScriptStruct(&CommandSet, &Kit->CommandSet, PPF_None)
		&& FGSTN_KitExtensionList::StaticStruct()->CompareScriptStruct(&Extensions, &Kit->Extensions, PPF_None)
		&& KitBindings.Num() == Kit->KitBindings.Num();
	for (int32 Index = 0; bSame && Index < KitBindings.Num(); ++Index)
	{
		bSame = FGSTN_AbilityBinding::StaticStruct()->CompareScriptStruct(&KitBindings[Index], &Kit->KitBindings[Index], PPF_None);
	}
	if (bSame)
	{
		return false;
	}
	Kit->CommandSet = MoveTemp(CommandSet);
	Kit->KitBindings = MoveTemp(KitBindings);
	Kit->Extensions = MoveTemp(Extensions);
	Kit->MarkPackageDirty();
	return true;
}

bool UGSTN_LinkContentLibrary::SetKitThrows(UGSTN_PrimaryKitDefinition* Kit, TSubclassOf<UGameplayAbility> Forward, TSubclassOf<UGameplayAbility> Back,
	TSubclassOf<UGameplayAbility> Up, TSubclassOf<UGameplayAbility> Down)
{
	if (!Kit)
	{
		return false;
	}
	for (FInstancedStruct& Entry : Kit->Extensions.ExtensionData)
	{
		if (FGSTN_LinkKitExtension* Extension = Entry.GetMutablePtr<FGSTN_LinkKitExtension>())
		{
			FGSTN_LinkThrowSet Throws;
			Throws.Forward = FSoftClassPath(Forward.Get());
			Throws.Back = FSoftClassPath(Back.Get());
			Throws.Up = FSoftClassPath(Up.Get());
			Throws.Down = FSoftClassPath(Down.Get());
			if (FGSTN_LinkThrowSet::StaticStruct()->CompareScriptStruct(&Throws, &Extension->Throws, PPF_None))
			{
				return false;
			}
			Extension->Throws = Throws;
			Kit->MarkPackageDirty();
			return true;
		}
	}
	return false;
}

bool UGSTN_LinkContentLibrary::WriteThrowMove(TSubclassOf<UGameplayAbility> ThrowClass, int32 TotalFrames, int32 FirstActionableFrame, int32 ThrowFrame,
	float Damage, float BaseKnockback, float AngleDeg, UAnimMontage* Montage, float MontagePlayRate)
{
	if (!ThrowClass || !ThrowClass->IsChildOf(UGSTN_GA_LinkThrow::StaticClass()) || ThrowClass->HasAnyClassFlags(CLASS_Native))
	{
		return false;
	}
	UGSTN_GA_LinkThrow* Defaults = GetMutableDefault<UGSTN_GA_LinkThrow>(ThrowClass.Get());
	FGSTN_CombatTimelineData Data = Defaults->Timeline;
	UGSTN_GA_LinkThrow::FillThrowTimeline(Data, TotalFrames, FirstActionableFrame, ThrowFrame, Damage, BaseKnockback, AngleDeg);
	Data.Montage = Montage;
	Data.MontagePlayRate = FMath::Max(MontagePlayRate, 0.01f);
	if (FGSTN_CombatTimelineData::StaticStruct()->CompareScriptStruct(&Data, &Defaults->Timeline, PPF_None))
	{
		return false;
	}
	Defaults->Timeline = MoveTemp(Data);
	Defaults->MarkPackageDirty();
	return true;
}

bool UGSTN_LinkContentLibrary::SetKitLinkWeights(UGSTN_PrimaryKitDefinition* Kit, float MaxGrabWeight, float MinLatchWeight)
{
	if (!Kit)
	{
		return false;
	}
	for (FInstancedStruct& Entry : Kit->Extensions.ExtensionData)
	{
		if (FGSTN_LinkKitExtension* Extension = Entry.GetMutablePtr<FGSTN_LinkKitExtension>())
		{
			if (MaxGrabWeight >= 0.f)
			{
				Extension->GrabRules.MaxGrabWeight = MaxGrabWeight;
			}
			if (MinLatchWeight >= 0.f)
			{
				Extension->LatchRules.MinLatchWeight = MinLatchWeight;
			}
			return true;
		}
	}
	return false;
}

bool UGSTN_LinkContentLibrary::SetTypeAlternatives(FGameplayTag LinkType, const TArray<FGameplayTag>& Alternatives)
{
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	UGSTN_LinkTypeDefinition* Definition = Registry ? const_cast<UGSTN_LinkTypeDefinition*>(Registry->FindDefinition(LinkType)) : nullptr;
	if (!Definition)
	{
		return false;
	}
	Definition->Alternatives = Alternatives;
	return true;
}

bool UGSTN_LinkContentLibrary::GetShownLinkNumbers(const AActor* Actor, int32& OutEndFrame, int32& OutMashProcessedFrame, int32& OutCooldownEndFrame,
	int32& OutGrabImmunityEndFrame, int32& OutBreakReason)
{
	const FGSTN_LinkFragment* Shown = UGSTN_LinkPresentationComponent::FindShownFragment(Actor);
	OutEndFrame = Shown ? Shown->Links.Body.EndFrame : 0;
	OutMashProcessedFrame = Shown ? Shown->MashProcessedFrame : 0;
	OutCooldownEndFrame = Shown ? Shown->Actions.CooldownEndFrame : 0;
	OutGrabImmunityEndFrame = Shown ? Shown->GrabImmunityEndFrame : 0;
	OutBreakReason = Shown ? Shown->Links.Body.BreakReason : 0;
	return Shown != nullptr;
}

FString UGSTN_LinkContentLibrary::DescribeGrabProbe(const AActor* Actor)
{
	const UGSTN_CombatSimComponent* Combat = Actor ? Actor->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	const UGSTN_CombatWorldSubsystem* World = Actor ? UGSTN_CombatWorldSubsystem::Get(Actor->GetWorld()) : nullptr;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const int32 SelfId = GetEntityId(Actor);
	if (!Combat || !World || !Registry || SelfId == INDEX_NONE)
	{
		return FString();
	}
	const FGSTN_StepSnapshot& Snapshot = World->GetSnapshot();
	const FGSTN_SnapshotEntry* Self = Snapshot.Find(SelfId);
	const FGSTN_LinkSnapshotExt* SelfExt = Self ? Self->Extension.Get<FGSTN_LinkSnapshotExt>(GSTNFeatureIds::Link) : nullptr;
	const FGSTN_LinkBodyData* Body = SelfExt && SelfExt->IsValid() ? FGSTN_LinkAttach::At(SelfExt->BodyIndex) : nullptr;
	TArray<FGSTN_LinkInteractOffer, TInlineAllocator<4>> Offers;
	FGSTN_LinkTickExtension::MakeOffersFor(*Registry, Body, Offers);
	if (!Self || Offers.IsEmpty())
	{
		return FString::Printf(TEXT("self %d: no snapshot entry or no interact offer for its kit"), SelfId);
	}
	auto MakeSide = [](const FGSTN_SnapshotEntry& Entry) { return FGSTN_LinkTickExtension::MakeRequestSide(Entry); };
	FGSTN_LinkInteractSource Source;
	Source.Interactor = MakeSide(*Self);
	Source.Offers = Offers;
	Source.Registry = Registry;
	FString Out = FString::Printf(TEXT("self %d tags [%s] frame %d;"), SelfId, *Source.Interactor.Tags.ToStringSimple(), Snapshot.Frame);
	for (const FGSTN_SnapshotEntry& Entry : Snapshot.Entries)
	{
		if (Entry.EntityId == SelfId)
		{
			continue;
		}
		FGSTN_LinkInteractEntity Candidate;
		Candidate.Side = MakeSide(Entry);
		Candidate.Location = Entry.Transform.GetLocation();
		FGSTN_LinkRuleResult Refusal;
		const int32 Offer = FGSTN_LinkInteractProvider::FindAcceptedOffer(Source, Candidate, Snapshot.Frame, &Refusal);
		Out += FString::Printf(TEXT(" %d dist=%.0f hurtboxes=%d tags=[%s] weight=%.0f %s;"), Entry.EntityId, FVector::Dist(Entry.Transform.GetLocation(), Self->Transform.GetLocation()),
			Entry.Hurtboxes ? 1 : 0, *Candidate.Side.Tags.ToStringSimple(), Candidate.Side.EffectiveWeight,
			Offer != INDEX_NONE ? *FString::Printf(TEXT("offer=%d"), Offer) : *FString::Printf(TEXT("refused %d"), static_cast<int32>(Refusal.Refusal)));
	}
	return Out;
}

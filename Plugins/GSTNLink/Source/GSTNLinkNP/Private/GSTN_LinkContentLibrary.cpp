// Links, Network Prediction half.

#include "GSTN_LinkContentLibrary.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GSTN_LinkRegistry.h"
#include "Hits/GSTN_HitSteps.h"
#include "Kits/GSTN_AbilityBinding.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Pipeline/GSTN_HitStep_GrabImmunity.h"
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
	if (bHasImmunity && bHasRequest)
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
		return IsStepOf(Step, UGSTN_HitStep_GrabImmunity::StaticClass()) || IsStepOf(Step, UGSTN_HitStep_LinkRequest::StaticClass());
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
				const FGSTN_LinkKitExtension* Kit = Previous ? Previous->KitExtension : nullptr;
				const uint16 Index = FGSTN_LinkAttach::Apply(*It, nullptr);
				if (Kit)
				{
					const_cast<FGSTN_LinkBodyData*>(FGSTN_LinkAttach::At(Index))->KitExtension = Kit;
				}
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

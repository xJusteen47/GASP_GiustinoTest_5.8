// Tests of the piggyback runtime part (M7 G5): the mount rule on request contexts, the mount type as registered, the loadout bindings and
// disallowed commands, and every break reason through GSTNLink's fake host. No world and no ability system: contexts, compile inputs and the
// fake host are built by hand, as GSTNExtensionTest does for its carry.

#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkRule_Mount.h"
#include "GSTN_LinkTags.h"
#include "GSTN_MountAbilities.h"
#include "GSTN_MountLinkType.h"
#include "GSTN_MountLoadout.h"
#include "GSTN_MountUtils.h"
#include "GSTN_PiggybackTags.h"
#include "GSTN_Placer_Mount.h"
#include "GSTN_CombatCoreTags.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Host/GSTN_FakeLinkHost.h"
#include "Input/GSTN_InputTags.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Misc/AutomationTest.h"
#include "Rules/GSTN_LinkRule_Team.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNPiggybackRuntimeTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr int32 Frame = 500;

	/** A kit extension list holding one mount entry (or none) */
	FGSTN_KitExtensionList MakeKit(const FGSTN_MountKitExtension* Mount)
	{
		FGSTN_KitExtensionList List;
		if (Mount)
		{
			List.ExtensionData.Add(FInstancedStruct::Make(*Mount));
		}
		return List;
	}

	/** A rider (team 1, initiator) asking a carrier (team 1, target), both with default mount data */
	struct FMountRequest
	{
		TStrongObjectPtr<UGSTN_LinkTypeDefinition> Definition;
		FGSTN_MountKitExtension RiderData;
		FGSTN_MountKitExtension CarrierData;
		FGSTN_KitExtensionList RiderKit;
		FGSTN_KitExtensionList CarrierKit;
		FGSTN_LinkRequestContext Context;
		bool bRiderHasKit = true;
		bool bCarrierHasKit = true;

		FMountRequest()
			: Definition(FGSTN_MountLinkType::Build(GetTransientPackage(), FGSTN_MountRules()))
		{
			Context.Frame = Frame;
			Context.LinkType = GSTNPiggybackTags::Type_Mount;
			Context.Initiator.EntityId = 1;
			Context.Initiator.TeamId = 1;
			Context.Target.EntityId = 2;
			Context.Target.TeamId = 1;
		}

		/** Runs the definition's rules in order, as the LinkRequest step does after its own checks: the first refusal wins */
		FGSTN_LinkRuleResult Evaluate()
		{
			RiderKit = MakeKit(bRiderHasKit ? &RiderData : nullptr);
			CarrierKit = MakeKit(bCarrierHasKit ? &CarrierData : nullptr);
			Context.Initiator.KitExtensions = &RiderKit;
			Context.Target.KitExtensions = &CarrierKit;
			Context.Definition = Definition.Get();
			for (const UGSTN_LinkRule* Rule : Definition->Rules)
			{
				const FGSTN_LinkRuleResult Result = Rule->Evaluate(Context);
				if (!Result.bAccepted)
				{
					return Result;
				}
			}
			return FGSTN_LinkRuleResult::Accept();
		}

		uint8 Payload()
		{
			Evaluate();
			for (const UGSTN_LinkRule* Rule : Definition->Rules)
			{
				if (const uint8 Value = Rule->ComputePayload(Context))
				{
					return Value;
				}
			}
			return 0;
		}
	};

	bool HasRequired(const FGSTN_BindingInput& Binding, const FGameplayTag& Tag) { return Binding.Requirements.RequireTags.HasTagExact(Tag); }
}

using namespace GSTNPiggybackRuntimeTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PiggybackRuleAcceptTest, "GSTNPiggyback.Runtime.Rule.AcceptsAllyWithMountData", Flags)
bool FGSTN_PiggybackRuleAcceptTest::RunTest(const FString& Parameters)
{
	FMountRequest Request;
	const FGSTN_LinkRuleResult Result = Request.Evaluate();
	TestTrue(TEXT("an ally with mount data on both kits is accepted"), Result.bAccepted);
	TestTrue(TEXT("the rule's eligibility verdict agrees"),
		UGSTN_MountUtils::EvaluateEligibility(UGSTN_LinkRule_Mount::MakeEligibilityInput(Request.Context)) == EGSTN_MountVerdict::Accept);
	// 100 / 150 = 0.667 -> 170 (round half up of 170.0)
	TestEqual(TEXT("the payload is the carry ratio"), static_cast<int32>(Request.Payload()), static_cast<int32>(UGSTN_MountUtils::ComputeCarryRatio(100.0, 150.0)));

	FMountRequest Equal;
	Equal.RiderData.Rider.Weight = 150.f;
	TestTrue(TEXT("a rider exactly at the capacity is accepted"), Equal.Evaluate().bAccepted);
	TestEqual(TEXT("and is a full load"), static_cast<int32>(Equal.Payload()), 255);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PiggybackRuleRefusalsTest, "GSTNPiggyback.Runtime.Rule.EveryRefusal", Flags)
bool FGSTN_PiggybackRuleRefusalsTest::RunTest(const FString& Parameters)
{
	{
		FMountRequest Request;
		Request.Context.Target.TeamId = 2;
		const FGSTN_LinkRuleResult Result = Request.Evaluate();
		TestTrue(TEXT("an enemy is refused by the team rule"), !Result.bAccepted && Result.Refusal == EGSTN_LinkRefusal::Team);
	}
	{
		FMountRequest Request;
		Request.Context.Initiator.TeamId = 255;
		Request.Context.Target.TeamId = 255;
		TestFalse(TEXT("no team is nobody's ally"), Request.Evaluate().bAccepted);
	}
	{
		FMountRequest Request;
		Request.bCarrierHasKit = false;
		const FGSTN_LinkRuleResult Result = Request.Evaluate();
		TestTrue(TEXT("a carrier kit without mount data refuses"), !Result.bAccepted && Result.Refusal == EGSTN_LinkRefusal::Requirements);
	}
	{
		FMountRequest Request;
		Request.CarrierData.Carrier.bCanBeMounted = false;
		TestFalse(TEXT("bCanBeMounted off refuses"), Request.Evaluate().bAccepted);
	}
	{
		FMountRequest Request;
		Request.bRiderHasKit = false;
		const FGSTN_LinkRuleResult Result = Request.Evaluate();
		TestTrue(TEXT("a rider kit without mount data refuses"), !Result.bAccepted && Result.Refusal == EGSTN_LinkRefusal::Requirements);
	}
	{
		FMountRequest Request;
		Request.RiderData.Rider.Weight = 151.f;
		const FGSTN_LinkRuleResult Result = Request.Evaluate();
		TestTrue(TEXT("a rider over the capacity is too heavy"), !Result.bAccepted && Result.Refusal == EGSTN_LinkRefusal::TooHeavy);
	}
	{
		FMountRequest Request;
		Request.Context.Initiator.CooldownEndFrame = Frame + 1;
		const FGSTN_LinkRuleResult Result = Request.Evaluate();
		TestTrue(TEXT("a rider in its link cooldown is refused"), !Result.bAccepted && Result.Refusal == EGSTN_LinkRefusal::Cooldown);
		Request.Context.Initiator.CooldownEndFrame = Frame;
		TestTrue(TEXT("the cooldown's end frame is accepted"), Request.Evaluate().bAccepted);
	}
	{
		FMountRequest Request;
		Request.Context.Target.Tags.AddTag(GSTNCombatTags::State_Hitstun);
		TestFalse(TEXT("a carrier in hitstun refuses"), Request.Evaluate().bAccepted);
	}
	{
		FMountRequest Request;
		Request.Context.Target.Tags.AddTag(GSTNLinkTags::Lock_Link);
		TestFalse(TEXT("a carrier with Combat.Lock.Link refuses"), Request.Evaluate().bAccepted);
	}
	{
		FMountRequest Request;
		Request.Context.Target.Links.Body.LinkTypeIndex = 1;
		Request.Context.Target.Links.Body.Role = 2;
		Request.Context.Target.Links.Body.Phase = EGSTN_LinkPhase::Active;
		TestFalse(TEXT("a carrier already carrying refuses (one rider, no chains)"), Request.Evaluate().bAccepted);
	}

	// The verdict-to-refusal table covers every verdict
	for (const EGSTN_MountVerdict Verdict : { EGSTN_MountVerdict::NotAllowedKit, EGSTN_MountVerdict::AlreadyLinked, EGSTN_MountVerdict::CarrierHitstun,
		EGSTN_MountVerdict::CarrierBusyLock, EGSTN_MountVerdict::WeightTooHigh, EGSTN_MountVerdict::RiderIneligible, EGSTN_MountVerdict::OutOfRange,
		EGSTN_MountVerdict::Cooldown })
	{
		TestTrue(FString::Printf(TEXT("verdict %d is a refusal"), static_cast<int32>(Verdict)), UGSTN_LinkRule_Mount::ToRefusal(Verdict) != EGSTN_LinkRefusal::None);
	}
	TestTrue(TEXT("Accept is no refusal"), UGSTN_LinkRule_Mount::ToRefusal(EGSTN_MountVerdict::Accept) == EGSTN_LinkRefusal::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PiggybackRuleTypeTest, "GSTNPiggyback.Runtime.Rule.TypeRegisteredAsSpecified", Flags)
bool FGSTN_PiggybackRuleTypeTest::RunTest(const FString& Parameters)
{
	const UGSTN_LinkTypeDefinition* Definition = FGSTN_MountLinkType::Get();
	if (!TestNotNull(TEXT("the module registered the mount type"), Definition))
	{
		return false;
	}
	TArray<FString> Errors;
	Definition->Validate(Errors);
	TestTrue(TEXT("the definition validates"), Errors.IsEmpty());

	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const uint8 Index = Registry->GetTypeIndex(GSTNPiggybackTags::Type_Mount);
	TestTrue(TEXT("the registry indexed it"), Index != 0 && Registry->GetDefinition(Index) == Definition);
	TestEqual(TEXT("the rider is role 1"), Registry->FindRoleIndex(Index, GSTNPiggybackTags::Role_Rider), 1);
	TestEqual(TEXT("the carrier is role 2"), Registry->FindRoleIndex(Index, GSTNPiggybackTags::Role_Carrier), 2);
	TestTrue(TEXT("rider: parked and acting"), Definition->InitiatorRole.bParked && Definition->InitiatorRole.bActsWhileLinked);
	TestTrue(TEXT("carrier: acting, not parked"), !Definition->TargetRole.bParked && Definition->TargetRole.bActsWhileLinked);
	TestTrue(TEXT("the rider predicts its accept two frames on"), Definition->bInitiatorPredicts && Definition->PendingFrames == 2);
	TestTrue(TEXT("Release reasons: Dismount for the rider, Drop for the carrier"),
		Definition->InitiatorRole.ReleaseBreakReason == GSTNPiggybackTags::BreakReason_Dismount && Definition->TargetRole.ReleaseBreakReason == GSTNPiggybackTags::BreakReason_Drop);
	TestTrue(TEXT("the plugin's reasons have indices from FirstPlugin"),
		Registry->GetReasons().IndexOf(GSTNPiggybackTags::BreakReason_Dismount) >= static_cast<uint8>(EGSTN_LinkBreakReason::FirstPlugin)
		&& Registry->GetReasons().IndexOf(GSTNPiggybackTags::BreakReason_Drop) >= static_cast<uint8>(EGSTN_LinkBreakReason::FirstPlugin));
	TestTrue(TEXT("the exit placer is the mount's"), Cast<UGSTN_Placer_Mount>(Definition->GetExitPlacer()) != nullptr);

	// The interact offer: the mount's meaning, below the grab
	int32 MountPriority = INDEX_NONE;
	int32 GrabPriority = INDEX_NONE;
	for (const FGSTN_LinkOfferEntry& Entry : Registry->GetInteractOffers())
	{
		if (Entry.Offer.Meaning == GSTNPiggybackTags::Interact_Target_Mount)
		{
			MountPriority = Entry.Offer.Priority;
		}
		if (Entry.Offer.Meaning == GSTNLinkTags::Interact_Target_Grab)
		{
			GrabPriority = FMath::Max(GrabPriority, Entry.Offer.Priority);
		}
	}
	TestTrue(TEXT("the mount is offered under Interact.Target.Mount"), MountPriority != INDEX_NONE);
	TestTrue(TEXT("below the grab (INT.5: grab wins with both in reach)"), GrabPriority == INDEX_NONE || MountPriority < GrabPriority);

	// The rules switch for rider exits (G6): predicted by default, carried by the type as GSTNLink's initiator-placed exit
	TestTrue(TEXT("rider exits are predicted by default"), FGSTN_MountRules().bPredictRiderExits);
	FGSTN_MountRules ServerGiven;
	ServerGiven.bPredictRiderExits = false;
	const UGSTN_LinkTypeDefinition* Predicted = FGSTN_MountLinkType::Build(nullptr, FGSTN_MountRules());
	const UGSTN_LinkTypeDefinition* Given = FGSTN_MountLinkType::Build(nullptr, ServerGiven);
	TestTrue(TEXT("the mount type places the rider's exit on the rider"), Predicted && Predicted->bInitiatorPlacesExit);
	TestTrue(TEXT("and the switch turns it off"), Given && !Given->bInitiatorPlacesExit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PiggybackMountHitboxTest, "GSTNPiggyback.Runtime.Rule.MountHitboxTargetsAlliesOnly", Flags)
bool FGSTN_PiggybackMountHitboxTest::RunTest(const FString& Parameters)
{
	const UGSTN_GA_Mount* Mount = GetDefault<UGSTN_GA_Mount>();
	const FGSTN_HitboxSpec* Spec = nullptr;
	int32 ActiveFrames = 0;
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Track : Mount->GetTimelineData().Tracks)
	{
		if (const FGSTN_TimelineTrack_Hitbox* Hitboxes = Track.GetPtr<FGSTN_TimelineTrack_Hitbox>())
		{
			Spec = Hitboxes->Hitboxes.Num() == 1 ? &Hitboxes->Hitboxes[0] : nullptr;
			ActiveFrames = Hitboxes->EndFrame - Hitboxes->StartFrame;
		}
	}
	TestEqual(TEXT("one active frame: a refused request is not asked again by the same move"), ActiveFrames, 1);
	if (!TestNotNull(TEXT("the mount move has one hitbox"), Spec))
	{
		return false;
	}
	TestTrue(TEXT("its hit names the mount link type"), Spec->Data.LinkType == GSTNPiggybackTags::Type_Mount.GetTag());
	const FGSTN_HitVariant* Ally = Spec->RelationshipVariants.Find(GSTNCombatHitTags::Relationship_Ally.GetTag());
	const FGSTN_HitVariant* Enemy = Spec->RelationshipVariants.Find(GSTNCombatHitTags::Relationship_Enemy.GetTag());
	TestTrue(TEXT("allies are forced hittable"), Ally && Ally->bForceHittable && !Ally->bIgnore);
	TestTrue(TEXT("enemies are ignored"), Enemy && Enemy->bIgnore);
	TestEqual(TEXT("one carrier per execution"), static_cast<int32>(Spec->MaxTargetsPerGroup), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PiggybackRiderContextTest, "GSTNPiggyback.Runtime.Bindings.RiderHasNoGroundContext", Flags)
bool FGSTN_PiggybackRiderContextTest::RunTest(const FString& Parameters)
{
	// A kit's ordinary jump needs Grounded; the mount data adds the mount meanings of the interact button
	FGSTN_CompileInput Input;
	FGSTN_BindingInput Jump;
	Jump.Command = FGameplayTag::RequestGameplayTag(TEXT("Command.Movement.Jump"), false);
	Jump.Requirements.RequireTags.AddTag(GSTNInputTags::Context_Grounded);
	Input.Kit.Bindings.Add(Jump);
	FGSTN_MountLoadout::ContributeFromKitData(FGSTN_MountKitExtension(), Input);
	TestEqual(TEXT("jump + mount + dismount + drop"), Input.Kit.Bindings.Num(), 4);

	// A parked rider's context: its role and the link tags, no Grounded and no Airborne (the linked mode exposes neither)
	FGameplayTagContainer Riding;
	Riding.AddTag(GSTNPiggybackTags::Role_Rider);
	Riding.AddTag(GSTNLinkTags::Linked);
	TArray<FGameplayTag> Holding;
	for (const FGSTN_BindingInput& Binding : Input.Kit.Bindings)
	{
		if (Binding.Requirements.IsSatisfiedBy(Riding))
		{
			Holding.Add(Binding.Command);
		}
	}
	TestEqual(TEXT("riding, one binding holds"), Holding.Num(), 1);
	TestTrue(TEXT("and it is the dismount"), Holding.Num() == 1 && Holding[0] == FGSTN_MountLoadout::GetDismountCommand());

	// On foot near a mountable ally, the mount holds and the role bindings do not
	FGameplayTagContainer OnFoot;
	OnFoot.AddTag(GSTNInputTags::Context_Grounded);
	OnFoot.AddTag(GSTNPiggybackTags::Interact_Target_Mount);
	int32 OnFootCount = 0;
	for (const FGSTN_BindingInput& Binding : Input.Kit.Bindings)
	{
		if (Binding.Requirements.IsSatisfiedBy(OnFoot))
		{
			++OnFootCount;
			TestTrue(TEXT("on foot: the jump or the mount"), Binding.Command == Jump.Command || Binding.Command == FGSTN_MountLoadout::GetMountCommand());
		}
	}
	TestEqual(TEXT("on foot, two bindings hold"), OnFootCount, 2);

	// Slots: all three fall through when unavailable, Drop ranks above Dismount
	for (const FGSTN_BindingInput& Binding : Input.Kit.Bindings)
	{
		if (Binding.Command != Jump.Command)
		{
			TestTrue(TEXT("a mount slot override that falls through"), Binding.bHasSlotOverride && Binding.SlotOverride.bFallThroughWhenUnavailable);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PiggybackDisallowedTest, "GSTNPiggyback.Runtime.Bindings.DisallowedCommands", Flags)
bool FGSTN_PiggybackDisallowedTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Dash = FGameplayTag::RequestGameplayTag(TEXT("Command.Movement.Dash"), false);
	const FGameplayTag Jump = FGameplayTag::RequestGameplayTag(TEXT("Command.Movement.Jump"), false);
	if (!TestTrue(TEXT("the standard commands exist"), Dash.IsValid() && Jump.IsValid()))
	{
		return false;
	}
	const FGameplayTag Carrier = GSTNPiggybackTags::Role_Carrier.GetTag();

	FGSTN_CompileInput Input;
	FGSTN_BindingInput KitDash;
	KitDash.Command = Dash;
	Input.Kit.Bindings.Add(KitDash);
	FGSTN_BindingInput CarrierDash;
	CarrierDash.Command = Dash;
	CarrierDash.Requirements.RequireTags.AddTag(Carrier);
	Input.Kit.Bindings.Add(CarrierDash);
	FGSTN_BindingInput KitJump;
	KitJump.Command = Jump;
	Input.Kit.Bindings.Add(KitJump);
	FGSTN_WeaponCompileInput& Weapon = Input.Weapons.AddDefaulted_GetRef();
	Weapon.Bindings.Add(KitDash);

	FGSTN_MountKitExtension MountData;
	MountData.bAddDefaultBindings = false;
	MountData.Carrier.DisallowedCommands.AddTag(Dash);
	FGSTN_MountLoadout::ContributeFromKitData(MountData, Input);

	TestTrue(TEXT("the kit's dash now ignores the carrier role"), Input.Kit.Bindings[0].Requirements.IgnoreTags.HasTagExact(Carrier));
	TestFalse(TEXT("a dash that requires the carrier role is left alone"), Input.Kit.Bindings[1].Requirements.IgnoreTags.HasTagExact(Carrier));
	TestFalse(TEXT("a command not listed is left alone"), Input.Kit.Bindings[2].Requirements.IgnoreTags.HasTagExact(Carrier));
	TestTrue(TEXT("weapon bindings are covered too"), Input.Weapons[0].Bindings[0].Requirements.IgnoreTags.HasTagExact(Carrier));
	TestEqual(TEXT("no binding added with bAddDefaultBindings off"), Input.Kit.Bindings.Num(), 3);

	FGameplayTagContainer Carrying;
	Carrying.AddTag(Carrier);
	TestFalse(TEXT("while carrying the disallowed dash does not hold"), Input.Kit.Bindings[0].Requirements.IsSatisfiedBy(Carrying));
	TestTrue(TEXT("on foot it does"), Input.Kit.Bindings[0].Requirements.IsSatisfiedBy(FGameplayTagContainer()));
	TestEqual(TEXT("applying again changes nothing"), FGSTN_MountLoadout::ApplyDisallowedCommands(MountData.Carrier.DisallowedCommands, Input), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PiggybackBreakReasonsTest, "GSTNPiggyback.Runtime.Break.EveryReasonClearsStates", Flags)
bool FGSTN_PiggybackBreakReasonsTest::RunTest(const FString& Parameters)
{
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	TStrongObjectPtr<UGSTN_LinkTypeDefinition> Definition(FGSTN_MountLinkType::Build(GetTransientPackage(), FGSTN_MountRules()));

	TArray<EGSTN_LinkBreakReason> Reasons = { EGSTN_LinkBreakReason::Timeout, EGSTN_LinkBreakReason::Distance, EGSTN_LinkBreakReason::OwnDeath,
		EGSTN_LinkBreakReason::PartnerDeath, EGSTN_LinkBreakReason::Hit, EGSTN_LinkBreakReason::PartnerState, EGSTN_LinkBreakReason::Release,
		EGSTN_LinkBreakReason::Throw, EGSTN_LinkBreakReason::Forced };
	for (const FName Name : FGSTN_MountLinkType::GetBreakReasonNames())
	{
		const uint8 Index = Registry->GetReasons().IndexOf(FGameplayTag::RequestGameplayTag(Name));
		TestTrue(FString::Printf(TEXT("%s has a plugin index"), *Name.ToString()), Index >= static_cast<uint8>(EGSTN_LinkBreakReason::FirstPlugin));
		Reasons.Add(static_cast<EGSTN_LinkBreakReason>(Index));
	}

	for (const EGSTN_LinkBreakReason Reason : Reasons)
	{
		for (const bool bFromRider : { true, false })
		{
			FGSTN_FakeLinkHost Host;
			const uint8 Type = Host.RegisterType(*Definition);
			const FGSTN_EntityRef Rider = Host.AddFighter(FGameplayTagContainer(), 100.f, FTransform::Identity);
			const FGSTN_EntityRef Carrier = Host.AddFighter(FGameplayTagContainer(), 100.f, FTransform(FVector(50.0, 0.0, 0.0)));
			Host.SetFrame(Frame);
			FGSTN_LinkState State;
			State.LinkTypeIndex = Type;
			State.Phase = EGSTN_LinkPhase::Active;
			State.Serial = 3;
			State.LinkFrame = Frame - 10;
			State.Payload = 170;
			State.Role = 1;
			State.PartnerId = Carrier;
			Host.SetLinkState(Rider, EGSTN_LinkChannel::Body, State);
			State.Role = 2;
			State.PartnerId = Rider;
			Host.SetLinkState(Carrier, EGSTN_LinkChannel::Body, State);

			const FString What = FString::Printf(TEXT("reason %d from the %s"), static_cast<int32>(Reason), bFromRider ? TEXT("rider") : TEXT("carrier"));
			TestTrue(What + TEXT(": the break runs"), Host.RequestBreak(bFromRider ? Rider : Carrier, EGSTN_LinkChannel::Body, Reason));
			const FGSTN_FakeLinkHost::FEntity* RiderEntity = Host.FindEntity(Rider);
			const FGSTN_FakeLinkHost::FEntity* CarrierEntity = Host.FindEntity(Carrier);
			TestTrue(What + TEXT(": the rider's state is cleared"), RiderEntity && !RiderEntity->Links.Body.IsLinked() && RiderEntity->Links.Body.Payload == 0);
			TestTrue(What + TEXT(": the carrier's state is cleared"), CarrierEntity && !CarrierEntity->Links.Body.IsLinked() && CarrierEntity->Links.Body.Payload == 0);
			TestTrue(What + TEXT(": the rider was placed off the carrier"), RiderEntity && CarrierEntity
				&& !RiderEntity->Transform.GetLocation().Equals(CarrierEntity->Transform.GetLocation(), 1.0));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PiggybackUnboundTest, "GSTNPiggyback.Runtime.Disabled.UnboundCommandIsLegal", Flags)
bool FGSTN_PiggybackUnboundTest::RunTest(const FString& Parameters)
{
	// A kit without mount data (what every kit is when the plugin is off): the extension adds nothing and changes nothing
	FGSTN_CompileInput Input;
	FGSTN_BindingInput Grab;
	Grab.Command = GSTNLinkTags::Command_Link_Grab;
	Grab.Requirements.RequireTags.AddTag(GSTNLinkTags::Interact_Target_Grab);
	Input.Kit.Bindings.Add(Grab);
	FGSTN_LoadoutExtensionContext Context;
	FGSTN_MountLoadout::Contribute(Context, Input);
	TestEqual(TEXT("no mount data: no binding added"), Input.Kit.Bindings.Num(), 1);
	TestTrue(TEXT("and the grab is unchanged"), Input.Kit.Bindings[0].Requirements.IgnoreTags.IsEmpty());

	// A binding naming a mount command whose slot the kit's set lacks is a binding with its own slot override, never an error: the command stays
	// unbound until a kit binds it, and nothing else of the interact button changes (the grab keeps its priority 0 press)
	const TArray<FGSTN_BindingInput> Defaults = FGSTN_MountLoadout::MakeDefaultBindings(FGSTN_MountKitExtension());
	TestEqual(TEXT("three default bindings"), Defaults.Num(), 3);
	for (const FGSTN_BindingInput& Binding : Defaults)
	{
		TestTrue(TEXT("each carries its own slot"), Binding.bHasSlotOverride && Binding.SlotOverride.Command == Binding.Command);
		TestFalse(TEXT("none requires the grab's meaning"), HasRequired(Binding, GSTNLinkTags::Interact_Target_Grab));
	}
	FGSTN_MountKitExtension CannotRide;
	CannotRide.Rider.bCanRide = false;
	CannotRide.Carrier.bCanBeMounted = false;
	TestEqual(TEXT("a kit that can neither ride nor carry binds nothing"), FGSTN_MountLoadout::MakeDefaultBindings(CannotRide).Num(), 0);
	return true;
}

#endif

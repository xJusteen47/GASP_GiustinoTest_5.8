// Automation tests of the spin and drill ability-system half (plan D4): the spin abilities' defaults, the default bindings and slots, and the
// tuning that reaches the simulation.

#include "Abilities/GSTN_GA_DrillDive.h"
#include "Abilities/GSTN_GA_Spin.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_DrillLinkType.h"
#include "GSTN_LinkRegistry.h"
#include "Rules/GSTN_LinkRule_Drill.h"
#include "State/GSTN_LinkState.h"
#include "Input/GSTN_InputConditions.h"
#include "GSTN_SpinDrillLibrary.h"
#include "GSTN_SpinDrillTags.h"
#include "GSTN_SpinDrillTuning.h"
#include "Input/GSTN_SpinSlots.h"
#include "Misc/AutomationTest.h"
#include "MoverTypes.h"
#include "Sim/GSTN_DrillFragment.h"
#include "Sim/GSTN_DrillTickExtension.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNSpinDrillGASTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SpinDrillGASAbilitiesTest, "GSTNSpinDrill.GAS.Spin.AbilityDefaults", GSTNSpinDrillGASTest::Flags)
bool FGSTN_SpinDrillGASAbilitiesTest::RunTest(const FString& Parameters)
{
	struct FCase
	{
		const UGSTN_GA_Spin* Default;
		EGSTN_SpinSense Sense;
		bool bAir;
	};
	const FCase Cases[] = {
		{ GetDefault<UGSTN_GA_SpinClockwise>(), EGSTN_SpinSense::Clockwise, false },
		{ GetDefault<UGSTN_GA_SpinCounterClockwise>(), EGSTN_SpinSense::CounterClockwise, false },
		{ GetDefault<UGSTN_GA_AirSpinClockwise>(), EGSTN_SpinSense::Clockwise, true },
		{ GetDefault<UGSTN_GA_AirSpinCounterClockwise>(), EGSTN_SpinSense::CounterClockwise, true } };
	for (const FCase& Case : Cases)
	{
		const FString Name = Case.Default->GetClass()->GetName();
		const FGSTN_SpinTrack* Spin = Case.Default->FindSpinTrack();
		if (!TestNotNull(*FString::Printf(TEXT("%s has a spin track"), *Name), Spin))
		{
			continue;
		}
		TestEqual(*FString::Printf(TEXT("%s: sense"), *Name), static_cast<int32>(Spin->Sense), static_cast<int32>(Case.Sense));
		TestEqual(*FString::Printf(TEXT("%s: air flag"), *Name), Spin->bAirSpin, Case.bAir);
		TestEqual(*FString::Printf(TEXT("%s: blocked once the air spin is spent only in the air"), *Name),
			Case.Default->GetBlockedTags().HasTagExact(GSTNSpinDrillTags::State_AirSpinSpent), Case.bAir);
		TestTrue(*FString::Printf(TEXT("%s: identity tag"), *Name), Case.Default->GetAssetTags().HasTagExact(GSTNSpinDrillTags::Ability_Movement_Spin));
		TestNotNull(*FString::Printf(TEXT("%s: has a hop effect"), *Name), Case.Default->HopEffect.Get());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SpinDrillGASBindingsTest, "GSTNSpinDrill.GAS.Spin.DefaultBindingsAndSlots", GSTNSpinDrillGASTest::Flags)
bool FGSTN_SpinDrillGASBindingsTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_InputSlotSpec> Slots = FGSTN_SpinSlots::Build(true);
	TestEqual(TEXT("two spin slots with the keyboard shortcut"), Slots.Num(), 2);
	TestEqual(TEXT("two without it"), FGSTN_SpinSlots::Build(false).Num(), 2);
	for (const FGSTN_InputSlotSpec& Slot : Slots)
	{
		const FGSTN_InputCondition_AnyOf* Either = Slot.Pattern.Steps.Num() == 1 && Slot.Pattern.Steps[0].AllOf.Num() == 1
			? Slot.Pattern.Steps[0].AllOf[0].GetPtr<FGSTN_InputCondition_AnyOf>() : nullptr;
		TestTrue(*FString::Printf(TEXT("%s: the stick rotation or the keys, in one AnyOf"), *Slot.Command.ToString()), Either && Either->Conditions.Num() == 2);
	}
	TSet<FGameplayTag> Commands;
	for (const FGSTN_InputSlotSpec& Slot : Slots)
	{
		bool bSeen = false;
		Commands.Add(Slot.Command, &bSeen);
		TestFalse(*FString::Printf(TEXT("%s is unique in the set"), *Slot.Command.ToString()), bSeen);
		TestEqual(*FString::Printf(TEXT("%s: one step"), *Slot.Command.ToString()), Slot.Pattern.Steps.Num(), 1);
	}

	const TArray<FGSTN_AbilityBinding> Bindings = UGSTN_SpinDrillLibrary::MakeDefaultSpinBindings(true);
	TestEqual(TEXT("a cancel, an air and a ground binding per slot, and the drill"), Bindings.Num(), 7);
	for (int32 Index = 0; Index < Bindings.Num(); ++Index)
	{
		const FGSTN_AbilityBinding& Binding = Bindings[Index];
		const FString Command = Binding.Command.ToString();
		const UClass* Class = Binding.Ability.Get();
		if (Class == UGSTN_GA_DrillDive::StaticClass())
		{
			TestTrue(TEXT("the drill: Command.Movement.Drill while spinning, last"), Binding.Command.MatchesTagExact(GSTNSpinDrillTags::Command_Movement_Drill)
				&& Binding.Requirements.RequireTags.HasTagExact(GSTNSpinDrillTags::State_Spinning) && Index == Bindings.Num() - 1);
			continue;
		}
		if (const UGSTN_GA_SpinCancel* Cancel = Class ? Cast<UGSTN_GA_SpinCancel>(Class->GetDefaultObject()) : nullptr)
		{
			// D5: the cancels come first and require the sense tag of the spin they stop, which is the opposite of the command's
			const FGSTN_SpinCancelTrack* Track = Cancel->FindCancelTrack();
			const bool bClockwiseCommand = Binding.Command.MatchesTag(GSTNSpinDrillTags::Command_Movement_Spin_Clockwise);
			TestTrue(*FString::Printf(TEXT("%s: cancel bindings come first"), *Command), Index < Slots.Num());
			TestTrue(*FString::Printf(TEXT("%s: the cancel's sense is the command's"), *Command), Track && Track->IsClockwise() == bClockwiseCommand);
			TestTrue(*FString::Printf(TEXT("%s: it requires the opposite spin's tag"), *Command), Binding.Requirements.RequireTags.HasTagExact(
				bClockwiseCommand ? GSTNSpinDrillTags::State_Spinning_CounterClockwise : GSTNSpinDrillTags::State_Spinning_Clockwise));
			TestTrue(*FString::Printf(TEXT("%s: identity tag"), *Command), Cancel->GetAssetTags().HasTagExact(GSTNSpinDrillTags::Ability_Movement_SpinCancel));
			TestEqual(*FString::Printf(TEXT("%s: the recovery is the whole cancel timeline"), *Command), Cancel->GetTimelineData().FirstActionableFrame, Cancel->GetTimelineData().TotalFrames);
			continue;
		}
		const bool bAir = Binding.Requirements.RequireTags.HasTagExact(Mover_IsInAir);
		TestTrue(*FString::Printf(TEXT("%s: the slot is inline and names the command"), *Command), Binding.HasSlotOverride() && Binding.GetSlotOverrideCommand() == Binding.Command);
		TestTrue(*FString::Printf(TEXT("%s: air requires Mover.IsInAir, ground ignores it"), *Command), bAir != Binding.Requirements.IgnoreTags.HasTagExact(Mover_IsInAir));
		const UGSTN_GA_Spin* Default = Class ? Cast<UGSTN_GA_Spin>(Class->GetDefaultObject()) : nullptr;
		if (!TestNotNull(*FString::Printf(TEXT("%s: binds a spin class"), *Command), Default))
		{
			continue;
		}
		const FGSTN_SpinTrack* Spin = Default->FindSpinTrack();
		const bool bClockwiseCommand = Binding.Command.MatchesTag(GSTNSpinDrillTags::Command_Movement_Spin_Clockwise);
		TestTrue(*FString::Printf(TEXT("%s: the class's sense and air flag match the binding"), *Command),
			Spin && Spin->IsClockwise() == bClockwiseCommand && Spin->bAirSpin == bAir);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SpinDrillGASTuningTest, "GSTNSpinDrill.GAS.Spin.TuningReachesTheSimulation", GSTNSpinDrillGASTest::Flags)
bool FGSTN_SpinDrillGASTuningTest::RunTest(const FString& Parameters)
{
	UGSTN_SpinDrillTuning* Tuning = NewObject<UGSTN_SpinDrillTuning>();
	Tuning->AirSpin.HopSpeed = 612.f;
	Tuning->AirSpin.bHopCancelsFall = false;
	Tuning->AirSpin.SpinGravityScale = 0.2f;
	Tuning->AirSpin.SpinMaxFallSpeedScale = 0.3f;
	Tuning->AirSpin.UsesPerAirtime = 2;
	const FGSTN_DrillSimTuning Sim = FGSTN_DrillSimTuning::FromTuning(Tuning->AirSpin, Tuning->Cancel, Tuning->Dive, Tuning->Bore, Tuning->Launch);
	TestEqual(TEXT("hop speed"), Sim.HopSpeed, 612.f);
	TestFalse(TEXT("hop mode"), Sim.bHopCancelsFall);
	TestEqual(TEXT("gravity scale"), Sim.SpinGravityScale, 0.2f);
	TestEqual(TEXT("fall speed scale"), Sim.SpinMaxFallSpeedScale, 0.3f);
	TestEqual(TEXT("uses per airtime"), Sim.UsesPerAirtime, 2);
	const FGSTN_DrillSimTuning Defaults = FGSTN_DrillSimTuning::MakeDefault();
	TestEqual(TEXT("the defaults are the struct defaults"), Defaults.HopSpeed, FGSTN_AirSpinTuning().HopSpeed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SpinDrillGASDiveTest, "GSTNSpinDrill.GAS.Dive.AbilityDefaultsAndDecisions", GSTNSpinDrillGASTest::Flags)
bool FGSTN_SpinDrillGASDiveTest::RunTest(const FString& Parameters)
{
	const UGSTN_GA_DrillDive* Default = GetDefault<UGSTN_GA_DrillDive>();
	TestNotNull(TEXT("the dive timeline has the dive track"), FGSTN_DiveTrack::Find(Default->GetTimelineData()));
	TestTrue(TEXT("plunge and bounce effects"), Default->PlungeEffect.Get() != nullptr && Default->BounceEffect.Get() != nullptr);
	TestTrue(TEXT("releasing crouch ends it"), Default->bEndOnTriggerRelease);
	TestTrue(TEXT("identity tag"), Default->GetAssetTags().HasTagExact(GSTNSpinDrillTags::Ability_Combat_Drill_Dive));

	FGSTN_DrillFragment Fragment;
	TestEqual(TEXT("no fragment: nothing"), UGSTN_GA_DrillDive::DecideFromFragment(nullptr, 10, 18), 0);
	Fragment.Phase = EGSTN_DrillPhase::Diving;
	Fragment.PhaseStartFrame = 10;
	TestEqual(TEXT("diving: nothing"), UGSTN_GA_DrillDive::DecideFromFragment(&Fragment, 12, 18), 0);
	Fragment.Phase = EGSTN_DrillPhase::None;
	Fragment.Flags = GSTNDrillFlags::Bounced;
	TestEqual(TEXT("bounced: bounce and end"), UGSTN_GA_DrillDive::DecideFromFragment(&Fragment, 12, 18), 1);
	Fragment.Flags = 0;
	Fragment.Phase = EGSTN_DrillPhase::Lag;
	Fragment.PhaseStartFrame = 20;
	TestEqual(TEXT("lag frame 17 of 18: nothing"), UGSTN_GA_DrillDive::DecideFromFragment(&Fragment, 37, 18), 0);
	TestEqual(TEXT("lag over: end"), UGSTN_GA_DrillDive::DecideFromFragment(&Fragment, 38, 18), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SpinDrillGASLinkTypeTest, "GSTNSpinDrill.GAS.Latch.LinkTypeAndRule", GSTNSpinDrillGASTest::Flags)
bool FGSTN_SpinDrillGASLinkTypeTest::RunTest(const FString& Parameters)
{
	const UGSTN_LinkTypeDefinition* Definition = FGSTN_DrillLinkType::Build(nullptr);
	if (!TestNotNull(TEXT("built"), Definition))
	{
		return false;
	}
	TArray<FString> Errors;
	Definition->Validate(Errors);
	TestEqual(TEXT("the definition validates"), Errors.Num(), 0);
	TestTrue(TEXT("the drill type on the Body channel"), Definition->LinkType == GSTNSpinDrillTags::Link_Type_Drill && Definition->Channel == EGSTN_LinkChannel::Body);
	TestEqual(TEXT("the driller (role 1) is the parked role"), Definition->GetParkedRoleIndex(), 1);
	TestTrue(TEXT("the driller acts and does not turn with the stick"), Definition->InitiatorRole.bActsWhileLinked && Definition->InitiatorRole.YawLimitDeg == 0.f);
	TestTrue(TEXT("the drilled target keeps acting"), Definition->TargetRole.bActsWhileLinked && !Definition->TargetRole.bParked);
	TestTrue(TEXT("the driller's machine predicts the seat"), Definition->bInitiatorPredicts);
	TestFalse(TEXT("hits only: not offered by the interact button"), Definition->InteractOffer.IsOffered());
	TestTrue(TEXT("the drill immunity is in the request requirements (the snapshot's request tag mask)"),
		Definition->RequestRequirements.IgnoreTags.HasTagExact(GSTNSpinDrillTags::Immunity_Drill));

	if (const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get())
	{
		TestTrue(TEXT("registered with GSTNLink at startup"), Registry->GetTypeIndex(GSTNSpinDrillTags::Link_Type_Drill) > 0);
	}

	// The rule on a request context (the shared checks need only the definition and two distinct entities)
	const UGSTN_LinkRule_Drill* Rule = Cast<UGSTN_LinkRule_Drill>(Definition->Rules.Num() > 0 ? Definition->Rules[0].Get() : nullptr);
	if (!TestNotNull(TEXT("the drill rule"), Rule))
	{
		return false;
	}
	FGSTN_LinkRequestContext Context;
	Context.Frame = 200;
	Context.LinkType = GSTNSpinDrillTags::Link_Type_Drill;
	Context.Definition = Definition;
	Context.Initiator.EntityId = 1;
	Context.Target.EntityId = 2;
	TestTrue(TEXT("a neutral target (a prop, no team) is drilled"), Rule->Evaluate_Implementation(Context).bAccepted);
	FGSTN_LinkRequestContext Allies = Context;
	Allies.Initiator.TeamId = 1;
	Allies.Target.TeamId = 1;
	TestTrue(TEXT("an ally refuses (Team)"), Rule->Evaluate_Implementation(Allies).Refusal == EGSTN_LinkRefusal::Team);
	FGSTN_LinkRequestContext Enemies = Allies;
	Enemies.Target.TeamId = 2;
	TestTrue(TEXT("an enemy is drilled"), Rule->Evaluate_Implementation(Enemies).bAccepted);
	FGSTN_LinkRequestContext Immune = Context;
	Immune.Target.Tags.AddTag(GSTNSpinDrillTags::Immunity_Drill);
	TestFalse(TEXT("Combat.Immunity.Drill refuses"), Rule->Evaluate_Implementation(Immune).bAccepted);
	FGSTN_LinkRequestContext Recent = Context;
	Recent.Target.LatchImmunityEndFrame = 210;
	TestTrue(TEXT("the latch immunity after the last latcher refuses"), Rule->Evaluate_Implementation(Recent).Refusal == EGSTN_LinkRefusal::Immunity);
	FGSTN_LinkRequestContext Busy = Context;
	Busy.Target.Links.Body.Role = 2;
	Busy.Target.Links.Body.Phase = EGSTN_LinkPhase::Active;
	Busy.Target.Links.Body.LinkTypeIndex = 1;
	TestFalse(TEXT("one driller per target: a busy Body channel refuses"), Rule->Evaluate_Implementation(Busy).bAccepted);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

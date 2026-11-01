// Automation tests of the spin and drill ability-system half (plan D4): the spin abilities' defaults, the default bindings and slots, and the
// tuning that reaches the simulation.

#include "Abilities/GSTN_GA_Spin.h"
#include "GSTN_SpinDrillLibrary.h"
#include "GSTN_SpinDrillTags.h"
#include "GSTN_SpinDrillTuning.h"
#include "Input/GSTN_SpinSlots.h"
#include "Misc/AutomationTest.h"
#include "MoverTypes.h"
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
	TestEqual(TEXT("four spin slots with the keyboard shortcut"), Slots.Num(), 4);
	TestEqual(TEXT("two without it"), FGSTN_SpinSlots::Build(false).Num(), 2);
	TSet<FGameplayTag> Commands;
	for (const FGSTN_InputSlotSpec& Slot : Slots)
	{
		bool bSeen = false;
		Commands.Add(Slot.Command, &bSeen);
		TestFalse(*FString::Printf(TEXT("%s is unique in the set"), *Slot.Command.ToString()), bSeen);
		TestEqual(*FString::Printf(TEXT("%s: one step"), *Slot.Command.ToString()), Slot.Pattern.Steps.Num(), 1);
	}

	const TArray<FGSTN_AbilityBinding> Bindings = UGSTN_SpinDrillLibrary::MakeDefaultSpinBindings(true);
	TestEqual(TEXT("an air and a ground binding per slot"), Bindings.Num(), 8);
	for (const FGSTN_AbilityBinding& Binding : Bindings)
	{
		const FString Command = Binding.Command.ToString();
		const bool bAir = Binding.Requirements.RequireTags.HasTagExact(Mover_IsInAir);
		TestTrue(*FString::Printf(TEXT("%s: the slot is inline and names the command"), *Command), Binding.HasSlotOverride() && Binding.GetSlotOverrideCommand() == Binding.Command);
		TestTrue(*FString::Printf(TEXT("%s: air requires Mover.IsInAir, ground ignores it"), *Command), bAir != Binding.Requirements.IgnoreTags.HasTagExact(Mover_IsInAir));
		const UClass* Class = Binding.Ability.Get();
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

#endif // WITH_DEV_AUTOMATION_TESTS

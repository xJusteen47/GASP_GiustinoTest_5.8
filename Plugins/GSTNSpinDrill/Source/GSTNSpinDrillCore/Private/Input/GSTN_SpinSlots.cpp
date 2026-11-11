// Spin and drill, pure part (no GAS, no Network Prediction, no Mover).

#include "Input/GSTN_SpinSlots.h"

#include "Layout/GSTN_ButtonLayout.h"
#include "GSTN_InputCoreTypes.h"
#include "GSTN_SpinDrillTags.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_InputConditions.h"

namespace GSTNSpinSlots
{
	using FCondition = TInstancedStruct<FGSTN_InputCondition>;

	FGSTN_InputSlotSpec MakeSlot(const FGameplayTag& Command, FCondition Anchor)
	{
		FGSTN_InputSlotSpec Spec;
		Spec.Command = Command;
		FGSTN_InputStep Step;
		Step.AllOf.Add(MoveTemp(Anchor));
		Spec.Pattern.Steps.Add(MoveTemp(Step));
		return Spec;
	}

	FCondition MakeRotation(EGSTN_SpinRotation Rotation)
	{
		FGSTN_InputCondition_Spin Spin;
		Spin.Space = EGSTN_DirectionSpace::Stick;
		Spin.MinDegrees = 360.f;
		Spin.Rotation = Rotation;
		return FCondition::Make<FGSTN_InputCondition_Spin>(Spin);
	}

	FCondition MakeKeys(EGSTN_StickDirection First, EGSTN_StickDirection Last)
	{
		// D0 (b): the Motion takes the rolled keys with or without the diagonal frames between them
		FGSTN_InputCondition_Motion Motion;
		Motion.Space = EGSTN_DirectionSpace::Stick;
		Motion.Sequence = {
			FGSTN_DirectionSet(GSTN_DirectionMasks::FromDirection(First)),
			FGSTN_DirectionSet(GSTN_DirectionMasks::FromDirection(EGSTN_StickDirection::Down)),
			FGSTN_DirectionSet(GSTN_DirectionMasks::FromDirection(Last)) };
		return FCondition::Make<FGSTN_InputCondition_Motion>(Motion);
	}
}

TArray<FGSTN_InputSlotSpec> FGSTN_SpinSlots::Build(bool bKeyboardShortcut)
{
	using namespace GSTNSpinSlots;
	const auto Spin = [bKeyboardShortcut](EGSTN_SpinRotation Rotation, EGSTN_StickDirection First, EGSTN_StickDirection Last)
	{
		if (!bKeyboardShortcut)
		{
			return MakeRotation(Rotation);
		}
		// One command, two ways in: the stick's 360 or the keys rolled (input.md 9: A-S-D is counter-clockwise, D-S-A clockwise)
		FGSTN_InputCondition_AnyOf Either;
		Either.Conditions = { MakeRotation(Rotation), MakeKeys(First, Last) };
		return FCondition::Make<FGSTN_InputCondition_AnyOf>(Either);
	};
	TArray<FGSTN_InputSlotSpec> Slots;
	Slots.Add(MakeSlot(GSTNSpinDrillTags::Command_Movement_Spin_Clockwise.GetTag(), Spin(EGSTN_SpinRotation::Clockwise, EGSTN_StickDirection::Right, EGSTN_StickDirection::Left)));
	Slots.Add(MakeSlot(GSTNSpinDrillTags::Command_Movement_Spin_CounterClockwise.GetTag(), Spin(EGSTN_SpinRotation::CounterClockwise, EGSTN_StickDirection::Left, EGSTN_StickDirection::Right)));
	return Slots;
}

FGSTN_InputSlotSpec FGSTN_SpinSlots::MakeDrillSlot()
{
	using namespace GSTNSpinSlots;
	// A press, so the drill comes when crouch goes down during the spin; its binding requires Combat.State.Spinning, and the spin's cancel
	// window lets it through from DiveFromFrame. Holding crouch is then read by the dive ability (it ends on the release)
	FGSTN_InputCondition_Press Press;
	Press.Buttons.AddTag(UGSTN_ButtonLayout::GetButton(TEXT("Spin.Drill"), GSTNCombatButtonTags::Button_Crouch.GetTag()));
	return MakeSlot(GSTNSpinDrillTags::Command_Movement_Drill.GetTag(), FCondition::Make<FGSTN_InputCondition_Press>(Press));
}

bool FGSTN_SpinSlots::Find(const FGameplayTag& Command, FGSTN_InputSlotSpec& OutSpec, bool bKeyboardShortcut)
{
	for (FGSTN_InputSlotSpec& Spec : Build(bKeyboardShortcut))
	{
		if (Spec.Command == Command)
		{
			OutSpec = MoveTemp(Spec);
			return true;
		}
	}
	return false;
}

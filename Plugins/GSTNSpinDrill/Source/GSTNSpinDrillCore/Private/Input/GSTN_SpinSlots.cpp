// Spin and drill, pure part (no GAS, no Network Prediction, no Mover).

#include "Input/GSTN_SpinSlots.h"

#include "GSTN_InputCoreTypes.h"
#include "GSTN_SpinDrillTags.h"
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
	TArray<FGSTN_InputSlotSpec> Slots;
	Slots.Add(MakeSlot(GSTNSpinDrillTags::Command_Movement_Spin_Clockwise.GetTag(), MakeRotation(EGSTN_SpinRotation::Clockwise)));
	Slots.Add(MakeSlot(GSTNSpinDrillTags::Command_Movement_Spin_CounterClockwise.GetTag(), MakeRotation(EGSTN_SpinRotation::CounterClockwise)));
	if (bKeyboardShortcut)
	{
		// input.md 9: A-S-D is counter-clockwise, D-S-A clockwise
		Slots.Add(MakeSlot(GSTNSpinDrillTags::Command_Movement_Spin_Clockwise_Keys.GetTag(), MakeKeys(EGSTN_StickDirection::Right, EGSTN_StickDirection::Left)));
		Slots.Add(MakeSlot(GSTNSpinDrillTags::Command_Movement_Spin_CounterClockwise_Keys.GetTag(), MakeKeys(EGSTN_StickDirection::Left, EGSTN_StickDirection::Right)));
	}
	return Slots;
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

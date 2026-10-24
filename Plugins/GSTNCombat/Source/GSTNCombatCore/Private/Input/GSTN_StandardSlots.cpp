// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_StandardSlots.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_InputConditions.h"

namespace
{
	using FCondition = TInstancedStruct<FGSTN_InputCondition>;

	enum class EShape : uint8
	{
		Neutral,
		Directional,
		CrouchNeutral,
		CrouchDirectional,
	};

	template<typename T>
	FCondition MakeInstance(const T& Condition)
	{
		return FCondition::Make<T>(Condition);
	}

	FCondition MakePress(const FGameplayTag& Button)
	{
		FGSTN_InputCondition_Press Condition;
		Condition.Buttons.AddTag(Button);
		return MakeInstance(Condition);
	}

	FCondition MakeCrouchHeld()
	{
		FGSTN_InputCondition_Hold Condition;
		Condition.Button = GSTNCombatButtonTags::Button_Crouch.GetTag();
		Condition.MinFrames = 0;
		return MakeInstance(Condition);
	}

	/** Standing: any stick, as long as it is not pushed all the way. Tilted: any direction, pushed all the way */
	FCondition MakeStick(bool bTilted)
	{
		FGSTN_InputCondition_Direction Condition;
		Condition.Space = EGSTN_DirectionSpace::Facing;
		Condition.Directions = bTilted ? GSTN_DirectionMasks::AnyTilted : GSTN_DirectionMasks::All;
		Condition.MinZone = bTilted ? EGSTN_StickZone::Tilt : EGSTN_StickZone::Neutral;
		Condition.MaxZone = bTilted ? EGSTN_StickZone::Tilt : EGSTN_StickZone::Aim;
		return MakeInstance(Condition);
	}

	FGSTN_InputSlotSpec MakeSlot(const FGameplayTag& Command, FCondition Anchor, EShape Shape)
	{
		FGSTN_InputSlotSpec Spec;
		Spec.Command = Command;

		FGSTN_InputStep Step;
		Step.AllOf.Add(MoveTemp(Anchor));
		if (Shape == EShape::CrouchNeutral || Shape == EShape::CrouchDirectional)
		{
			Step.AllOf.Add(MakeCrouchHeld());
		}
		Step.AllOf.Add(MakeStick(Shape == EShape::Directional || Shape == EShape::CrouchDirectional));
		Spec.Pattern.Steps.Add(MoveTemp(Step));
		return Spec;
	}
}

TArray<FGSTN_InputSlotSpec> UGSTN_StandardSlots::Build()
{
	using namespace GSTNCommandTags;

	struct FFamily
	{
		const FNativeGameplayTag& Button;
		const FNativeGameplayTag* Commands[4];
	};
	const FFamily Families[] = {
		{ GSTNCombatButtonTags::Button_Attack, { &Attack_Neutral, &Attack_Directional, &Attack_CrouchNeutral, &Attack_CrouchDirectional } },
		{ GSTNCombatButtonTags::Button_Strong, { &Strong_Neutral, &Strong_Directional, &Strong_CrouchNeutral, &Strong_CrouchDirectional } },
		{ GSTNCombatButtonTags::Button_Special, { &Special_Neutral, &Special_Directional, &Special_CrouchNeutral, &Special_CrouchDirectional } },
	};
	const FNativeGameplayTag* Combos[] = { &Combo_AttackSpecial_Neutral, &Combo_AttackSpecial_Directional,
		&Combo_AttackSpecial_CrouchNeutral, &Combo_AttackSpecial_CrouchDirectional };

	TArray<FGSTN_InputSlotSpec> Slots;
	for (const FFamily& Family : Families)
	{
		for (int32 Shape = 0; Shape < 4; ++Shape)
		{
			Slots.Add(MakeSlot(Family.Commands[Shape]->GetTag(), MakePress(Family.Button.GetTag()), static_cast<EShape>(Shape)));
		}
	}

	for (int32 Shape = 0; Shape < 4; ++Shape)
	{
		FGSTN_InputCondition_Chord Chord;
		Chord.Buttons.AddTag(GSTNCombatButtonTags::Button_Attack.GetTag());
		Chord.Buttons.AddTag(GSTNCombatButtonTags::Button_Special.GetTag());
		Chord.WithinFrames = 2;
		Slots.Add(MakeSlot(Combos[Shape]->GetTag(), MakeInstance(Chord), static_cast<EShape>(Shape)));
	}
	return Slots;
}

#if WITH_EDITOR
void UGSTN_StandardSlots::FillSlotSet(UGSTN_InputSlotSet* Set)
{
	if (!Set)
	{
		return;
	}

	Set->Modify();
	Set->Slots.Reset();
	for (const FGSTN_InputSlotSpec& Spec : Build())
	{
		FGSTN_InputSlotEntry Entry;
		Entry.Inline = Spec;
		Set->Slots.Add(MoveTemp(Entry));
	}
	Set->MarkPackageDirty();
}
#endif

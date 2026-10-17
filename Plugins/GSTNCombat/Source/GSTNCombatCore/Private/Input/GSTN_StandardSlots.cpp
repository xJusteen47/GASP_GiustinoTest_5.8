// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_StandardSlots.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_CombatInputTags.h"
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

	/** The command tags of one family, per shape */
	struct FFamily
	{
		const FNativeGameplayTag& Button;
		const FNativeGameplayTag* Grounded[4];
		const FNativeGameplayTag* Airborne[4];
	};

	template<typename T>
	FCondition MakeCondition(const T& Condition)
	{
		return FCondition::Make<T>(Condition);
	}

	FCondition MakePress(const FGameplayTag& Button)
	{
		FGSTN_InputCondition_Press Condition;
		Condition.Buttons.AddTag(Button);
		return MakeCondition(Condition);
	}

	FCondition MakeCrouchHeld()
	{
		FGSTN_InputCondition_Hold Condition;
		Condition.Button = GSTNInputTags::Button_Crouch.GetTag();
		Condition.MinFrames = 0;
		return MakeCondition(Condition);
	}

	/** Standing: any stick, as long as it is not pushed all the way. Tilted: any direction, pushed all the way */
	FCondition MakeStick(bool bTilted)
	{
		FGSTN_InputCondition_Direction Condition;
		Condition.Space = EGSTN_DirectionSpace::Facing;
		Condition.Directions = bTilted ? GSTN_DirectionMasks::AnyTilted : GSTN_DirectionMasks::All;
		Condition.MinZone = bTilted ? EGSTN_StickZone::Tilt : EGSTN_StickZone::Neutral;
		Condition.MaxZone = bTilted ? EGSTN_StickZone::Tilt : EGSTN_StickZone::Aim;
		return MakeCondition(Condition);
	}

	FGSTN_InputSlotSpec MakeSlot(const FGameplayTag& Command, FCondition Anchor, EShape Shape, bool bAirborne)
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

		if (bAirborne)
		{
			Spec.Context.RequireTags.AddTag(GSTNCommandTags::Context_Airborne.GetTag());
		}
		else
		{
			Spec.Context.IgnoreTags.AddTag(GSTNCommandTags::Context_Airborne.GetTag());
		}
		return Spec;
	}
}

TArray<FGSTN_InputSlotSpec> UGSTN_StandardSlots::Build()
{
	using namespace GSTNCommandTags;
	const FFamily Families[] = {
		{ GSTNInputTags::Button_Attack,
			{ &Attack_Neutral, &Attack_Directional, &Attack_CrouchNeutral, &Attack_CrouchDirectional },
			{ &Air_Attack_Neutral, &Air_Attack_Directional, &Air_Attack_CrouchNeutral, &Air_Attack_CrouchDirectional } },
		{ GSTNInputTags::Button_Strong,
			{ &Strong_Neutral, &Strong_Directional, &Strong_CrouchNeutral, &Strong_CrouchDirectional },
			{ &Air_Strong_Neutral, &Air_Strong_Directional, &Air_Strong_CrouchNeutral, &Air_Strong_CrouchDirectional } },
		{ GSTNInputTags::Button_Special,
			{ &Special_Neutral, &Special_Directional, &Special_CrouchNeutral, &Special_CrouchDirectional },
			{ &Air_Special_Neutral, &Air_Special_Directional, &Air_Special_CrouchNeutral, &Air_Special_CrouchDirectional } },
	};
	const FNativeGameplayTag* Combos[] = { &Combo_AttackSpecial_Neutral, &Combo_AttackSpecial_Directional,
		&Combo_AttackSpecial_CrouchNeutral, &Combo_AttackSpecial_CrouchDirectional };

	TArray<FGSTN_InputSlotSpec> Slots;
	for (const FFamily& Family : Families)
	{
		for (const bool bAirborne : { false, true })
		{
			for (int32 Shape = 0; Shape < 4; ++Shape)
			{
				const FNativeGameplayTag& Command = bAirborne ? *Family.Airborne[Shape] : *Family.Grounded[Shape];
				Slots.Add(MakeSlot(Command.GetTag(), MakePress(Family.Button.GetTag()), static_cast<EShape>(Shape), bAirborne));
			}
		}
	}

	for (int32 Shape = 0; Shape < 4; ++Shape)
	{
		FGSTN_InputCondition_Chord Chord;
		Chord.Buttons.AddTag(GSTNInputTags::Button_Attack.GetTag());
		Chord.Buttons.AddTag(GSTNInputTags::Button_Special.GetTag());
		Chord.WithinFrames = 2;
		Slots.Add(MakeSlot(Combos[Shape]->GetTag(), MakeCondition(Chord), static_cast<EShape>(Shape), false));
	}
	return Slots;
}

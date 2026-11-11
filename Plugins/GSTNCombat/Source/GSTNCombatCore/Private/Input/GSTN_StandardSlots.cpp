// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_StandardSlots.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_InputConditions.h"
#include "Input/GSTN_MovementSlots.h"

namespace
{
	using FCondition = TInstancedStruct<FGSTN_InputCondition>;

	// IO-2c: the slots are written with the defaults of the layout functions and record them (FGSTN_InputSlotSpec::LayoutButtons); the
	// kit compile swaps in the match's button layout. So the set asset filled from here follows any GameRules, not the editor's layout
	const TPair<const TCHAR*, const FNativeGameplayTag*> LayoutFunctions[] = {
		{ TEXT("Combat.Attack"), &GSTNCombatButtonTags::Button_Attack },
		{ TEXT("Combat.Strong"), &GSTNCombatButtonTags::Button_Strong },
		{ TEXT("Combat.Special"), &GSTNCombatButtonTags::Button_Special },
		{ TEXT("Combat.Crouch"), &GSTNCombatButtonTags::Button_Crouch },
	};

	/** Records the layout function of every default button the slot uses (each default is one function in this set) */
	void RecordLayoutFunctions(FGSTN_InputSlotSpec& Spec)
	{
		FGameplayTagContainer Used;
		for (const FGSTN_InputStep& Step : Spec.Pattern.Steps)
		{
			for (const TInstancedStruct<FGSTN_InputCondition>& Condition : Step.AllOf)
			{
				if (const FGSTN_InputCondition* Pointer = Condition.GetPtr())
				{
					Pointer->GetReferencedButtons(Used);
				}
			}
		}
		for (const TPair<const TCHAR*, const FNativeGameplayTag*>& Function : LayoutFunctions)
		{
			if (Used.HasTagExact(Function.Value->GetTag()))
			{
				Spec.AddLayoutButton(Function.Key, Function.Value->GetTag());
			}
		}
	}

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
		FGameplayTag Button;
		const FNativeGameplayTag* Commands[4];
	};
	const FGameplayTag AttackButton = GSTNCombatButtonTags::Button_Attack.GetTag();
	const FGameplayTag SpecialButton = GSTNCombatButtonTags::Button_Special.GetTag();
	const FFamily Families[] = {
		{ AttackButton, { &Attack_Neutral, &Attack_Directional, &Attack_CrouchNeutral, &Attack_CrouchDirectional } },
		{ GSTNCombatButtonTags::Button_Strong.GetTag(), { &Strong_Neutral, &Strong_Directional, &Strong_CrouchNeutral, &Strong_CrouchDirectional } },
		{ SpecialButton, { &Special_Neutral, &Special_Directional, &Special_CrouchNeutral, &Special_CrouchDirectional } },
	};
	const FNativeGameplayTag* Combos[] = { &Combo_AttackSpecial_Neutral, &Combo_AttackSpecial_Directional,
		&Combo_AttackSpecial_CrouchNeutral, &Combo_AttackSpecial_CrouchDirectional };

	TArray<FGSTN_InputSlotSpec> Slots;
	for (const FFamily& Family : Families)
	{
		for (int32 Shape = 0; Shape < 4; ++Shape)
		{
			Slots.Add(MakeSlot(Family.Commands[Shape]->GetTag(), MakePress(Family.Button), static_cast<EShape>(Shape)));
		}
	}

	for (int32 Shape = 0; Shape < 4; ++Shape)
	{
		FGSTN_InputCondition_Chord Chord;
		Chord.Buttons.AddTag(AttackButton);
		Chord.Buttons.AddTag(SpecialButton);
		Chord.WithinFrames = 2;
		Slots.Add(MakeSlot(Combos[Shape]->GetTag(), MakeInstance(Chord), static_cast<EShape>(Shape)));
	}
	for (FGSTN_InputSlotSpec& Spec : Slots)
	{
		RecordLayoutFunctions(Spec);
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

int32 UGSTN_StandardSlots::AddLayoutFunctions(TArray<FGSTN_InputSlotEntry>& Entries)
{
	// Today's builders, both interact orders, with the hold length of the current settings (the one the old assets were filled with)
	TArray<FGSTN_InputSlotSpec> Templates = Build();
	Templates.Append(UGSTN_MovementSlots::Build(false));
	Templates.Append(UGSTN_MovementSlots::Build(true));

	const auto PatternText = [](const FGSTN_InputPattern& Pattern)
	{
		FString Text;
		FGSTN_InputPattern::StaticStruct()->ExportText(Text, &Pattern, nullptr, nullptr, PPF_None, nullptr);
		return Text;
	};

	int32 Changed = 0;
	for (FGSTN_InputSlotEntry& Entry : Entries)
	{
		// Shared slot assets are migrated on their own, not through every set that lists them
		if (Entry.Asset || !Entry.Inline.LayoutButtons.IsEmpty())
		{
			continue;
		}
		const FString Mine = PatternText(Entry.Inline.Pattern);
		for (const FGSTN_InputSlotSpec& Template : Templates)
		{
			if (Template.Command == Entry.Inline.Command && !Template.LayoutButtons.IsEmpty() && PatternText(Template.Pattern) == Mine)
			{
				Entry.Inline.LayoutButtons = Template.LayoutButtons;
				++Changed;
				break;
			}
		}
	}
	return Changed;
}

int32 UGSTN_StandardSlots::AddLayoutFunctionsToSlotSet(UGSTN_InputSlotSet* Set)
{
	if (!Set)
	{
		return 0;
	}
	TArray<FGSTN_InputSlotEntry> Entries = Set->Slots;
	const int32 Changed = AddLayoutFunctions(Entries);
	if (Changed > 0)
	{
		Set->Modify();
		Set->Slots = MoveTemp(Entries);
		Set->MarkPackageDirty();
	}
	return Changed;
}
#endif

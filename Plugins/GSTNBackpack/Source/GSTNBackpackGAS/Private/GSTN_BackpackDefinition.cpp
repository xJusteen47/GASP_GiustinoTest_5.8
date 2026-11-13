// Backpacks, ability layer: the backpack asset.

#include "GSTN_BackpackDefinition.h"
#include "GameplayEffect.h"
#include "GSTN_BackpackTypes.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_InputConditions.h"
#include "Input/GSTN_InputTags.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

const FPrimaryAssetType UGSTN_BackpackDefinition::AssetType(TEXT("GSTNBackpack"));

UGSTN_BackpackDefinition_JetPack::UGSTN_BackpackDefinition_JetPack()
{
	// The native tag, not a request by name: a class default is built when the module loads
	BackpackTags.AddTag(GSTNBackpackTags::Backpack_JetPack.GetTag());
	Movement.ExtraAirJumps = 1;
	Movement.GlideGravityScale = 0.25f;
	Movement.GlideMaxFallSpeed = 300.f;
}

UGSTN_BackpackDefinition_ShieldDome::UGSTN_BackpackDefinition_ShieldDome()
{
	BackpackTags.AddTag(GSTNBackpackTags::Backpack_ShieldDome.GetTag());
	Abilities.Add(MakeDomeBinding());
}

FGSTN_AbilityBinding UGSTN_BackpackDefinition_ShieldDome::MakeDomeBinding()
{
	using FCondition = TInstancedStruct<FGSTN_InputCondition>;

	FGSTN_AbilityBinding Binding;
	Binding.Command = GSTNBackpackTags::Command_Backpack_Dome.GetTag();
	Binding.Requirements.RequireTags.AddTag(GSTNInputTags::Context_Grounded.GetTag());
	Binding.Requirements.IgnoreTags.AddTag(GSTNBackpackTags::State_Empty_Slot0.GetTag());
	Binding.Ability = TSoftClassPtr<UGameplayAbility>(FSoftObjectPath(TEXT("/Script/GSTNBackpackNP.GSTN_GA_BackpackDome")));

	// Press(dodge) with the stick neutral at the press: one condition more than the dodge's Press(dodge), so it wins exactly when the stick
	// rests (owner: "the dodge button held without the stick, like the PvP shield"). The press raises it, the release (the ability ends on
	// it) lowers it. The button follows the button layout's "Movement.Dodge" function
	const FGameplayTag Dodge = GSTNCombatButtonTags::Button_Dodge.GetTag();
	FGSTN_InputCondition_Press Press;
	Press.Buttons.AddTag(Dodge);
	FGSTN_InputCondition_Direction Neutral;
	Neutral.Space = EGSTN_DirectionSpace::Stick;
	Neutral.MinZone = EGSTN_StickZone::Neutral;
	Neutral.MaxZone = EGSTN_StickZone::Neutral;

	FGSTN_InputStep Step;
	Step.AllOf.Add(FCondition::Make<FGSTN_InputCondition_Press>(Press));
	Step.AllOf.Add(FCondition::Make<FGSTN_InputCondition_Direction>(Neutral));

	FGSTN_InputSlotSpec& Slot = Binding.SlotOverride.Inline;
	Slot.Command = Binding.Command;
	Slot.Pattern.Steps.Add(MoveTemp(Step));
	Slot.AddLayoutButton(TEXT("Movement.Dodge"), Dodge);
	return Binding;
}

FPrimaryAssetId UGSTN_BackpackDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UGSTN_BackpackDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	int32 Identities = 0;
	for (const FGameplayTag& Tag : BackpackTags)
	{
		Identities += Tag.MatchesTag(GSTNBackpackTags::Backpack) ? 1 : 0;
	}
	if (Identities != 1)
	{
		Context.AddError(FText::FromString(TEXT("BackpackTags needs exactly one identity tag under Backpack")));
		Result = EDataValidationResult::Invalid;
	}
	for (const FGSTN_AbilityBinding& Binding : Abilities)
	{
		if (!Binding.Command.IsValid() || Binding.Ability.IsNull())
		{
			Context.AddWarning(FText::FromString(TEXT("A backpack binding without a command or an ability does nothing")));
		}
	}
	TArray<FText> Warnings;
	FGSTN_EquipmentVisualUtils::Validate(Visual, GetName(), Warnings);
	for (const FText& Warning : Warnings)
	{
		Context.AddWarning(Warning);
	}
	return Result;
}
#endif

// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GSTN_GroundContactSettings.h"

#include "Animation/AnimMontage.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_GroundContactSettings)

#define LOCTEXT_NAMESPACE "GSTN_GroundContactSettings"

UGSTN_GroundContactSettings::UGSTN_GroundContactSettings()
{
	// First guesses for the UE5 mannequin skeleton (GASP), to be tuned from in-game captures. Radii are origin-to-skin on the floor side.
	auto Add = [this](const TCHAR* Bone, EGSTN_GroundContactRole Role, float Radius)
	{
		FGSTN_GroundContactBone& Entry = Bones.AddDefaulted_GetRef();
		Entry.Bone = Bone;
		Entry.Role = Role;
		Entry.Radius = Radius;
	};
	Add(TEXT("pelvis"), EGSTN_GroundContactRole::LiftBody, 12.f);
	Add(TEXT("spine_03"), EGSTN_GroundContactRole::LiftBody, 13.f);
	Add(TEXT("head"), EGSTN_GroundContactRole::LiftBody, 11.f);
	Add(TEXT("calf_l"), EGSTN_GroundContactRole::LiftBody, 6.f);       // knees
	Add(TEXT("calf_r"), EGSTN_GroundContactRole::LiftBody, 6.f);
	Add(TEXT("lowerarm_l"), EGSTN_GroundContactRole::LiftBody, 5.f);   // elbows
	Add(TEXT("lowerarm_r"), EGSTN_GroundContactRole::LiftBody, 5.f);
	Add(TEXT("hand_l"), EGSTN_GroundContactRole::LimbEnd, 4.f);
	Add(TEXT("hand_r"), EGSTN_GroundContactRole::LimbEnd, 4.f);
	Add(TEXT("foot_l"), EGSTN_GroundContactRole::LimbEnd, 7.f);        // ankle origin, ~9 cm above the sole when standing
	Add(TEXT("foot_r"), EGSTN_GroundContactRole::LimbEnd, 7.f);

	// No default rules: gameplay tags are not registered yet when the class default object is built. The setup script
	// (Tools/GASNPPTest/ground_contact_setup.py) writes the knockdown / tumble tag rules and the reaction montages into the asset.
}

void UGSTN_GroundContactSettings::SetRule(const FString& Name, const TArray<FName>& TagNames, const TArray<UAnimMontage*>& InMontages, float Weight,
	TArray<FName>& OutUnknownTags)
{
	OutUnknownTags.Reset();
	FGSTN_GroundContactRule* Rule = Rules.FindByPredicate([&Name](const FGSTN_GroundContactRule& R) { return R.Name == Name; });
	if (!Rule)
	{
		Rule = &Rules.AddDefaulted_GetRef();
		Rule->Name = Name;
	}
	Rule->AnyTags.Reset();
	for (const FName TagName : TagNames)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TagName, false);
		if (Tag.IsValid())
		{
			Rule->AnyTags.AddTag(Tag);
		}
		else
		{
			OutUnknownTags.Add(TagName);
		}
	}
	Rule->Montages.Reset();
	for (UAnimMontage* Montage : InMontages)
	{
		if (Montage)
		{
			Rule->Montages.AddUnique(Montage);
		}
	}
	Rule->Weight = FMath::Clamp(Weight, 0.f, 1.f);
	Modify();
}

#if WITH_EDITOR
EDataValidationResult UGSTN_GroundContactSettings::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	bool bHasLift = false;
	for (const FGSTN_GroundContactBone& Entry : Bones)
	{
		if (Entry.Bone.IsNone())
		{
			Context.AddError(LOCTEXT("NoBone", "A contact bone has no name"));
			Result = EDataValidationResult::Invalid;
		}
		bHasLift |= Entry.Role == EGSTN_GroundContactRole::LiftBody;
	}
	if (bHasLift && LiftBone.IsNone())
	{
		Context.AddError(LOCTEXT("NoLiftBone", "LiftBody bones need a LiftBone (usually pelvis)"));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE

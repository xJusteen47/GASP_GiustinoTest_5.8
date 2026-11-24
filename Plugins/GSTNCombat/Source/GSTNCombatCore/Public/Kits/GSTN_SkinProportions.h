// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GSTN_SkinProportions.generated.h"

class USkeletalMesh;
struct FReferenceSkeleton;

/**
 * How far a skin's body may drift from the base mannequin (owner 2026-11-23, skin-proporzioni-analisi.md option A). Every value is a
 * ratio to the base body (1 = same). The simulation never reads a mesh: hitboxes, hurtboxes, grabs and the capsule are baked on the one
 * invisible game skeleton (BirdKnight mannequin), so a skin is only allowed to differ as much as the eye forgives a hand or a foot that
 * is not exactly where the hitbox is. Arms +-5% keep the hand within ~4 cm of the baked hitbox at full reach, legs +-4% keep the feet
 * on the floor (ankles >= 10 cm, rule 15b), shoulders beyond +20% push folded arms into the chest.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_SkinProportionLimits
{
	GENERATED_BODY()

	/** Total height (feet to top of the head, the mesh bounds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin|Proportions", meta = (ClampMin = "0.5", ClampMax = "1.5"))
	float MinHeight = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin|Proportions", meta = (ClampMin = "0.5", ClampMax = "1.5"))
	float MaxHeight = 1.07f;

	/** Leg length, hip to ankle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin|Proportions", meta = (ClampMin = "0.5", ClampMax = "1.5"))
	float MinLegs = 0.96f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin|Proportions", meta = (ClampMin = "0.5", ClampMax = "1.5"))
	float MaxLegs = 1.04f;

	/** Arm length, shoulder to wrist */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin|Proportions", meta = (ClampMin = "0.5", ClampMax = "1.5"))
	float MinArms = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin|Proportions", meta = (ClampMin = "0.5", ClampMax = "1.5"))
	float MaxArms = 1.05f;

	/** Shoulder width, between the two upper arm joints */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin|Proportions", meta = (ClampMin = "0.5", ClampMax = "1.5"))
	float MinShoulders = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin|Proportions", meta = (ClampMin = "0.5", ClampMax = "1.5"))
	float MaxShoulders = 1.20f;

	/** Head size (neck joint to top of the head). Only an upper bound: a big head is the cartoon look the owner wants for the kids */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin|Proportions", meta = (ClampMin = "1", ClampMax = "2"))
	float MaxHead = 1.25f;
};

/**
 * The bones the measure reads. Defaults are the UE5 mannequin (SK_UEFN_Mannequin, the skeleton the whole project and the owner's
 * Cascadeur rig use). Only the left side is measured for legs and arms: a skin is symmetric, and an asymmetric one is an art bug the
 * capture shows anyway.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_SkinMeasureBones
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin|Proportions")
	FName Thigh = TEXT("thigh_l");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin|Proportions")
	FName Foot = TEXT("foot_l");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin|Proportions")
	FName UpperArmLeft = TEXT("upperarm_l");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin|Proportions")
	FName UpperArmRight = TEXT("upperarm_r");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin|Proportions")
	FName Hand = TEXT("hand_l");

	/** The head is measured from this joint to the top of the mesh bounds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin|Proportions")
	FName Neck = TEXT("neck_01");
};

/** Lengths of one body in its reference pose, in cm. A value <= 0 was not measurable (a bone is missing) */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_SkinBodyLengths
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skin|Proportions", meta = (ForceUnits = "cm"))
	float Height = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skin|Proportions", meta = (ForceUnits = "cm"))
	float Legs = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skin|Proportions", meta = (ForceUnits = "cm"))
	float Arms = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skin|Proportions", meta = (ForceUnits = "cm"))
	float Shoulders = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skin|Proportions", meta = (ForceUnits = "cm"))
	float Head = 0.f;
};

/** A skin's body as ratios of the base body (1 = same). A value <= 0 could not be compared */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_SkinProportions
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skin|Proportions")
	float Height = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skin|Proportions")
	float Legs = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skin|Proportions")
	float Arms = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skin|Proportions")
	float Shoulders = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skin|Proportions")
	float Head = 0.f;
};

/**
 * Settings of the skin proportion check (Project Settings > Plugins > GSTN Skins). Editor data validation of every UGSTN_SkinDefinition
 * measures the skin's mesh against BaseMesh with these limits; nothing at runtime reads them.
 */
UCLASS(config = GSTNCombat, defaultconfig, meta = (DisplayName = "GSTN Skins"))
class GSTNCOMBATCORE_API UGSTN_SkinProportionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UGSTN_SkinProportionSettings& Get() { return *GetDefault<UGSTN_SkinProportionSettings>(); }

	/**
	 * The body every skin is measured against: the invisible game skeleton's mesh (BirdKnight mannequin), the one the hitboxes and the
	 * bakes use. Empty = the proportion check is off (a warning on each skin says so)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Skins|Proportions")
	TSoftObjectPtr<USkeletalMesh> BaseMesh;

	UPROPERTY(Config, EditAnywhere, Category = "Skins|Proportions")
	FGSTN_SkinProportionLimits Limits;

	UPROPERTY(Config, EditAnywhere, Category = "Skins|Proportions")
	FGSTN_SkinMeasureBones Bones;
};

/** Stateless measure and check of a skin's proportions. Pure: positions in, numbers out, so it is tested without any mesh */
struct GSTNCOMBATCORE_API FGSTN_SkinProportionUtils
{
	/**
	 * Component space positions of every bone of a reference pose, by name. Local transforms are composed parent first, which is the
	 * order of a reference skeleton (a parent index is always lower than its child's; -1 is the root)
	 */
	static TMap<FName, FVector> ComposeComponentSpace(const TArray<FName>& Names, const TArray<FTransform>& Local, const TArray<int32>& Parents);

	/** The same from a mesh's reference skeleton */
	static TMap<FName, FVector> ComposeComponentSpace(const FReferenceSkeleton& Skeleton);

	/**
	 * Lengths of a body from its bone positions and the Z range of its mesh bounds (feet to top of the head). A length whose bones are
	 * missing stays 0
	 */
	static FGSTN_SkinBodyLengths Measure(const TMap<FName, FVector>& Positions, float BoundsMinZ, float BoundsMaxZ, const FGSTN_SkinMeasureBones& Bones);

	/** Skin over base, per length. A length missing on either side stays 0 */
	static FGSTN_SkinProportions Compare(const FGSTN_SkinBodyLengths& Base, const FGSTN_SkinBodyLengths& Skin);

	/**
	 * One readable line per ratio outside the limits ("legs 1.06, allowed 0.96-1.04"), and one per ratio that could not be measured
	 * in Unmeasured. Empty Errors = the skin fits
	 */
	static void Check(const FGSTN_SkinProportions& Ratios, const FGSTN_SkinProportionLimits& Limits, TArray<FString>& Errors, TArray<FString>& Unmeasured);

	/**
	 * Measures both meshes' reference poses and checks Skin against Base. Also an error when the two meshes are not on the same skeleton
	 * asset: every skin shares the game skeleton, no retarget
	 */
	static void CheckMeshes(const USkeletalMesh& Base, const USkeletalMesh& Skin, const FGSTN_SkinProportionLimits& Limits, const FGSTN_SkinMeasureBones& Bones,
		FGSTN_SkinProportions& OutRatios, TArray<FString>& Errors, TArray<FString>& Unmeasured);
};

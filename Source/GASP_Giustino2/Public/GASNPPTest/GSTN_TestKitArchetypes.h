// GAS + NPP + Mover network test harness: the three test kit archetypes of the hub (M11 K3).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Hits/GSTN_ImpactTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_TestKitArchetypes.generated.h"

class UAnimMontage;

/**
 * One strike of a test kit archetype, as plain numbers (M11 K3, PLACEHOLDER VALUES for the playtest): a sphere that travels forward from
 * ReachFrom to ReachTo over the active frames, swept so it cannot skip a thin hurtbox, and the hit it lands. Written by
 * Tools/GAS/setup_test_kit_archetypes.py through UGSTN_TestKitContentLibrary::WriteStrikeMove; a designer retunes the Blueprint afterwards.
 */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_TestStrikeSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Frames", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 TotalFrames = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Frames", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 FirstActionableFrame = 26;

	/** First and last frame the hitbox is out */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Frames", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 ActiveStart = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Frames", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 ActiveEnd = 9;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Hitbox", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float Radius = 50.f;

	/** Forward distance of the sphere's centre from the attacker's origin at the first and at the last active frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Hitbox", meta = (ForceUnits = "cm"))
	float ReachFrom = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Hitbox", meta = (ForceUnits = "cm"))
	float ReachTo = 160.f;

	/** Height of the sphere's centre over the attacker's origin (the middle of its capsule) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Hitbox", meta = (ForceUnits = "cm"))
	float Height = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Hit", meta = (ClampMin = "0.0"))
	float Damage = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Hit", meta = (ClampMin = "0.0"))
	float BaseKnockback = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Hit", meta = (ClampMin = "0.0"))
	float KnockbackGrowth = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Hit", meta = (ClampMin = "-90.0", ClampMax = "180.0", ForceUnits = "deg"))
	float AngleDeg = 30.f;

	/** Authored hitstop of the victim; the attacker gets AttackerHitstopFrames */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Hit", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 HitstopFrames = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Hit", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 AttackerHitstopFrames = 4;

	/** Placeholder GASP montage, played with the synced montage at the start */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Animation")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.f;

	/** What the hit asks of a surface the victim flies into: Force Bounce = a ground bounce off a floor, Force Splat = a wall splat (2026-11-13) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Hit")
	FGSTN_ImpactProfile Impact;

	/** Tags the user carries from TagsFromFrame to the end of the move (a Tags track; Combat.Move.KnockdownOnLanding for a dropkick); empty = no track */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Tags")
	FGameplayTagContainer Tags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Tags", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 TagsFromFrame = 0;
};

/**
 * Native parent of the test kit archetypes' strikes (M11 K3). Its own timeline is empty: only Blueprint children are bound, and their frame
 * data is written by UGSTN_TestKitContentLibrary::WriteStrikeMove (or edited in their Class Defaults).
 */
UCLASS(Abstract, Blueprintable)
class GASP_GIUSTINO2_API UGSTN_GA_TestKitStrike : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_TestKitStrike(const FObjectInitializer& ObjectInitializer);

	/** The timeline of Spec: frames, one swept root sphere, the hit, the montage. Replaces the tracks of OutTimeline */
	static void FillStrikeTimeline(FGSTN_CombatTimelineData& OutTimeline, const FGSTN_TestStrikeSpec& Spec);
};

/** Editor writers of the test kit archetype content (Tools/GAS/setup_test_kit_archetypes.py) */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_TestKitContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Writes Spec on the class default object of a Blueprint child of UGSTN_GA_TestKitStrike (saved with the Blueprint). Native classes are
	 * refused. Returns whether the defaults changed (dirty only then, so a rerun saves nothing)
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Test|Kits")
	static bool WriteStrikeMove(TSubclassOf<UGameplayAbility> StrikeClass, const FGSTN_TestStrikeSpec& Spec);
};

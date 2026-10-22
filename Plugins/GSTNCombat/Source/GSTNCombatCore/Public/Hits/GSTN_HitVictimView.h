// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_HitVictimView.generated.h"

/** Whether a part of a victim can be hurt at the frame it is read */
UENUM(BlueprintType)
enum class EGSTN_HitVictimPartState : uint8
{
	Vulnerable,
	/** Hits land but do no damage */
	Invincible,
	/** Hits pass through */
	Intangible
};

/** One named value of a victim or of one of its parts (a health, a poise, a shield meter...). The meaning is the tag's, not the struct's */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_HitVictimStat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victim")
	FGameplayTag Stat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victim")
	float Value = 0.f;
};

/** One hittable part of a victim: a hurtbox group, a limb, a weak point */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_HitVictimPartView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victim")
	FName PartName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victim")
	FName GroupName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victim")
	FGameplayTagContainer PartTags;

	/** With the hurtbox modifiers active at this frame applied */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victim")
	EGSTN_HitVictimPartState State = EGSTN_HitVictimPartState::Vulnerable;

	/** Values of this part alone; empty when the victim keeps no per-part values */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victim")
	TArray<FGSTN_HitVictimStat> Stats;
};

/**
 * A read-only description of what can be hit and how, at the end of the previous frame (combat-m4-hits 5: the snapshot).
 * Hit resolution reads victims through it and never reaches into an ability system, so it does not care whether the victim
 * is a fighter, a minion or a held object.
 *
 * It assumes neither one health value nor one hurtbox: values are tagged stats on the victim and on each part (a victim with
 * a single health has one stat and one part, a victim with a health per limb has one per limb), and a victim can have any
 * number of parts, including none that can be hurt. Plain data, no GAS type.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_HitVictimView
{
	GENERATED_BODY()

	/** Network Prediction proxy id of the victim; INDEX_NONE for a victim that is not a proxy (a held object) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victim")
	int32 EntityId = INDEX_NONE;

	/** Team of the victim; 255 = no team (the neutral relationship). A byte so the view needs no engine team type */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victim")
	uint8 TeamId = 255;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victim")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victim")
	FVector Velocity = FVector::ZeroVector;

	/** Degrees, around the victim's up */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victim")
	float FacingYaw = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victim")
	bool bInHitstop = false;

	/** Combat state tags the victim projects (in hitstun, shielding, in a timeline...); the vocabulary is the combat tags', not this struct's */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victim")
	FGameplayTagContainer StateTags;

	/** Values of the victim as a whole (a damage percent, a shared poise...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victim")
	TArray<FGSTN_HitVictimStat> Stats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victim")
	TArray<FGSTN_HitVictimPartView> Parts;

	/** The value of a stat of the victim as a whole, or Default when it has none of that tag */
	float GetStat(const FGameplayTag& Stat, float Default = 0.f) const
	{
		for (const FGSTN_HitVictimStat& Entry : Stats)
		{
			if (Entry.Stat == Stat)
			{
				return Entry.Value;
			}
		}
		return Default;
	}
};

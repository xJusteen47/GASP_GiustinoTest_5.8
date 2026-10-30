// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Hits/GSTN_ReactionTypes.h"
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

	/** 1 = full health, derived by the pipeline from the health stats (0 when the victim keeps none the damage mode can read) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victim", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealthFraction = 1.f;

	/** The victim's base weight, before WeightScale (same unit as FGSTN_KnockbackInput::Weight) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victim", meta = (ClampMin = "0.0"))
	float Weight = 100.f;

	/**
	 * The compact reaction state the attacker's machine shares (damage percent, weight scale, windows). Plain data that is not
	 * reflected: it is copied byte for byte with the snapshot, so a Blueprint cannot read it.
	 */
	FGSTN_VictimReactionState ReactionState;

	/**
	 * The snapshot's extension POD of the optional plugin with the lowest feature id (broken parts...), or null. Not owned and not reflected:
	 * valid for the frame the view was read, the core never dereferences it. A reader validates the type word the POD starts with.
	 */
	const void* SnapshotExtension = nullptr;

	/** One slot per contributing plugin: the feature id (GSTNFeatureIds) and its POD. Not reflected, same lifetime as SnapshotExtension */
	struct FSnapshotExtensionRef
	{
		uint8 TypeId = 0;
		const void* Data = nullptr;
	};
	static constexpr int32 MaxSnapshotExtensions = 2;
	FSnapshotExtensionRef SnapshotExtensions[MaxSnapshotExtensions];

	/** The POD of a plugin by its feature id, or null (M6 F7: two plugins may contribute to one entity) */
	const void* FindSnapshotExtension(uint8 TypeId) const
	{
		for (const FSnapshotExtensionRef& Ref : SnapshotExtensions)
		{
			if (Ref.TypeId == TypeId && Ref.Data)
			{
				return Ref.Data;
			}
		}
		return nullptr;
	}

	/** Writes every extension slot and the single pointer from the slots: the lowest id first */
	void SetSnapshotExtensions(const FSnapshotExtensionRef* Refs, int32 Num)
	{
		SnapshotExtension = nullptr;
		for (int32 Index = 0; Index < MaxSnapshotExtensions; ++Index)
		{
			SnapshotExtensions[Index] = Index < Num ? Refs[Index] : FSnapshotExtensionRef();
		}
		SnapshotExtension = SnapshotExtensions[0].Data;
	}

	void ClearSnapshotExtensions()
	{
		SetSnapshotExtensions(nullptr, 0);
	}

	/**
	 * The extra hurtboxes the victim's snapshot carries (a shield bubble), so a step can read what the hit landed on when HurtboxIndex is
	 * from GSTNHurtboxIndex::ExtraIndexBase. A copy (at most GSTNHurtboxIndex::MaxExtraHurtboxes), empty for almost every hit
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Victim")
	TArray<FGSTN_ExtraHurtbox> ExtraHurtboxes;

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

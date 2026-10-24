// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Hits/GSTN_HitExtensions.h"
#include "Hits/GSTN_HitMathUtils.h"
#include "Hits/GSTN_HitResolution.h"
#include "Hits/GSTN_KnockbackModel.h"
#include "Hits/GSTN_ReactionTypes.h"
#include "Hits/GSTN_RelationshipResolver.h"
#include "Rules/GSTN_CombatStep.h"
#include "GSTN_CombatRules.generated.h"

class UGSTN_SurfaceImpactTable;

/** How the victim's damage is counted */
UENUM(BlueprintType)
enum class EGSTN_DamageMode : uint8
{
	/** A damage percent that only grows (platform fighters) */
	Percent,
	/** A health value that falls to zero (hit point games) */
	Health,
	/** A percent bounded by MaxHealth; reaching the cap is a KO */
	PercentCapped
};

/**
 * Base of the rules data an optional plugin attaches to the rules asset (RulesExtensions, modularity-and-tags 5b), so the asset
 * never grows a property that names a feature. Data only. An entry whose struct is not loaded stays in the asset and is ignored.
 */
USTRUCT(BlueprintType, meta = (Hidden))
struct GSTNCOMBATCORE_API FGSTN_RulesExtension
{
	GENERATED_BODY()

	virtual ~FGSTN_RulesExtension() = default;
};

/**
 * The rules of a match's combat, as data (combat extension surface, modularity-and-tags 5b): the activation pipeline of
 * candidates, and the hit resolution pipeline with the tuning of hits (combat-m4-hits 3).
 *
 * It needs no GAS type, so it lives in the Core module; a step that does needs a GAS-aware plugin and is added to the list
 * by that plugin's own rules asset. The effect classes (damage, meter gain, knockback, freeze) are soft classes that the GAS
 * layer fills and resolves; the core only passes them on.
 *
 * Both pipelines hold classes, not objects: steps are stateless (see UGSTN_CombatStep). Classes are soft references, loaded once
 * by ResolvePipeline / ResolveHitPipeline outside the simulation, and the simulation only runs the resolved lists.
 *
 * Every default is the identity: the hit pipeline is empty and the model is none, so a rules asset nobody tuned resolves
 * nothing. The standard behaviour is CreateStandard, which is also what UGSTN_CombatSettings uses when it names no asset.
 */
UCLASS(BlueprintType)
class GSTNCOMBATCORE_API UGSTN_CombatRules : public UDataAsset
{
	GENERATED_BODY()

public:
	UGSTN_CombatRules();

	/** Steps in the order they run. A class that is missing (its plugin is not loaded), abstract or null is reported by validation and skipped */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Pipeline")
	TArray<TSoftClassPtr<UGSTN_CombatStep>> Pipeline;

	/**
	 * Hit resolution steps in the order they run (UGSTN_HitStep_* and the steps of optional plugins). Empty by default: the
	 * standard rules fill it. A class that is missing, abstract, null or not a hit step is reported and skipped; the names of the
	 * documented placeholders (Shield, Parry, PartResolve) are known to the validator
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Hits")
	TArray<TSoftClassPtr<UGSTN_HitResolutionStep>> HitPipeline;

	/** Turns one hit on one victim into a reaction. None = the ComputeKnockback step writes nothing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Rules|Hits")
	TObjectPtr<UGSTN_KnockbackModel> KnockbackModel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Damage")
	EGSTN_DamageMode DamageMode = EGSTN_DamageMode::Percent;

	/** Percent: the percent at which the health weight curve ends. PercentCapped: the cap, in percent points. Health: the full health */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Damage", meta = (ClampMin = "0.001"))
	float MaxHealth = 100.f;

	/** Hits on allies are allowed (Ally is added to the hittable relationships) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Teams")
	bool bFriendlyFire = false;

	/**
	 * The relationships the TeamFilter step lets through. Self is hittable only if it is listed. Empty means Enemy and Neutral (the
	 * default is not written into the class because a class default object is built before the native tags exist)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Teams", meta = (Categories = "Combat.Relationship"))
	FGameplayTagContainer HittableRelationships;

	/** Decides a victim's relationship. None = the default rule: self, same team Ally, no team Neutral, else Enemy */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Rules|Teams")
	TObjectPtr<UGSTN_RelationshipResolver> RelationshipResolver;

	/** Rewind the victims to what the attacker saw (M10). The simulation does not read it before then */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Network")
	bool bUseLagCompensation = true;

	/** The most the host rewinds a victim for lag compensation; M8's snapshot ring size */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Network", meta = (ClampMin = "0", ForceUnits = "ms"))
	int32 MaxRewindMs = 250;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Hits")
	FGSTN_DIParams DI;

	/** The angles of the Sakurai3D special angle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Hits")
	FGSTN_SakuraiParams Sakurai;

	/** The global default of the hitstun condition: stagger force at or above it staggers. 0 = every hit staggers. A hurtbox or a set can override it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Hits", meta = (ClampMin = "0.0"))
	float StaggerThreshold = 0.f;

	/** Scales the knockback of a hit on a victim already in tumble: the knob that allows or limits long juggles */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Hits", meta = (ClampMin = "0.0"))
	float AirborneJuggleScale = 1.f;

	/** The shortest hitstun lock of a staggered victim (frames); read when the victim side applies the reaction (H9) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Hits", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 MinStaggerLockFrames = 31;

	/** Weight against progress of damage taken, used by the HealthWeight step when the hurtbox set has no curve. Empty = flat (the step writes nothing) */
	UPROPERTY(EditAnywhere, Category = "Rules|Hits")
	FGSTN_BakedCurve DefaultWeightCurve;

	/** Surface impact responses (M4 7b). Soft: loaded outside the simulation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Hits")
	TSoftObjectPtr<UGSTN_SurfaceImpactTable> SurfaceImpactTable;

	/** The gameplay effect the damage request applies (filled by the GAS layer, H5) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Effects")
	TSoftClassPtr<UObject> DamageEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Effects")
	TSoftClassPtr<UObject> MeterGainEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Effects")
	TSoftClassPtr<UObject> KnockbackEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Effects")
	TSoftClassPtr<UObject> FreezeEffect;

	/** The meter the MeterGain step fills */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Effects")
	FGameplayTag MeterGainMeter;

	/** Meter gained per point of damage. 0 = the MeterGain step writes nothing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Effects", meta = (ClampMin = "0.0"))
	float MeterGainPerDamage = 0.f;

	/**
	 * Blueprint outcome appliers: each handles the outcome extensions of one struct type (combat-m4-hits 3b). Pure hooks; native
	 * appliers register in FGSTN_OutcomeApplierRegistry instead
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Rules|Hits")
	TArray<TObjectPtr<UGSTN_OutcomeApplier>> OutcomeAppliers;

	/** Blueprint attacker outcome extensions: the first that answers Ricochet bounces the hit. Pure hooks. None = always Normal */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Rules|Hits")
	TArray<TObjectPtr<UGSTN_AttackerOutcome>> AttackerOutcomes;

	/** Rules data of optional plugins, each entry a struct derived from FGSTN_RulesExtension */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (BaseStruct = "/Script/GSTNCombatCore.GSTN_RulesExtension", ExcludeBaseStruct))
	TArray<FInstancedStruct> RulesExtensions;

	/**
	 * Loads the classes of Pipeline and keeps the usable ones, in order. Call it once when the match's rules are chosen
	 * (BeginPlay), never while simulating. Calling it again rebuilds the list
	 */
	void ResolvePipeline();

	/**
	 * The same for HitPipeline. Each skipped entry is logged; when OutReports is given the reasons are also appended to it (the
	 * automation tests read them)
	 */
	void ResolveHitPipeline(TArray<FString>* OutReports = nullptr);

	/** The classes ResolvePipeline kept */
	const TArray<TSubclassOf<UGSTN_CombatStep>>& GetResolvedPipeline() const { return ResolvedPipeline; }

	/** The classes ResolveHitPipeline kept */
	const TArray<TSubclassOf<UGSTN_HitResolutionStep>>& GetResolvedHitPipeline() const { return ResolvedHitPipeline; }

	/**
	 * Runs the resolved steps on View in order; the first Reject ends it and is the result, otherwise the result is
	 * Continue. Safe to call from the simulation: it reads only the resolved list and the step class defaults.
	 */
	FGSTN_CombatStepResult RunPipeline(const FGSTN_CombatStepView& View) const;

	/** The relationship of a victim for an attacker: the resolver's, else the default rule. Pure */
	FGameplayTag ResolveRelationship(const FGSTN_RelationshipQuery& Query) const;

	/** True when the TeamFilter step lets a hit on this relationship through */
	bool IsRelationshipHittable(const FGameplayTag& Relationship) const;

	/**
	 * How far the victim has come along the health weight curve, 0 to 1, from the damage taken before the current hit: Health
	 * counts 1 - HealthFraction, the percent modes count the damage percent over MaxHealth.
	 */
	float GetDamageProgress(const FGSTN_HitVictimView& Victim) const;

	/**
	 * The standard rules (the built-in equivalent of the GSTN_CombatRules_Standard asset): the whole hit pipeline with the weight
	 * driven model, the health weight in it, a push back threshold of 0.3 and a launch threshold of 0.6 of the tumble threshold,
	 * friendly fire off, Enemy and Neutral hittable, a weight that falls from 1.0 to 0.6 over the damage range. The hit pipeline is
	 * resolved. Outer owns the object (the transient package for a built-in copy)
	 */
	static UGSTN_CombatRules* CreateStandard(UObject* Outer);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(Transient)
	TArray<TSubclassOf<UGSTN_CombatStep>> ResolvedPipeline;

	UPROPERTY(Transient)
	TArray<TSubclassOf<UGSTN_HitResolutionStep>> ResolvedHitPipeline;
};

// GAS + NPP + Mover network test harness.

#pragma once

#include "CoreMinimal.h"
#include "GAS/GSTN_GameplayAbility.h"
#include "DataTypes/SyncedvariableTypes/BasicSyncedVariablesTypes.h"
#include "GSTN_TestProbeAbilities.generated.h"

class UGSTN_GASNPPTestComponent;

/**
 * Abilities of the M3 spike probes (grants and triggers). None of them is in the character's DefaultAbilities: the harness
 * grants and revokes them on the server (UGSTN_GASNPPTestComponent::ServerGrantProbe*), so the test Blueprint is not touched
 * and the default scenarios run unchanged.
 *
 * Every probe reports to the test component of its avatar: each CanActivateAbility call and each ActivateAbility call, with
 * whether the ability system was re-simulating or restoring a frame at that moment. Those counters are observation only (they
 * are not rolled back, like the rollback counters of the component); anything the probes change in the simulation goes
 * through gameplay effects, so the synced state carries it.
 */
UCLASS(Abstract)
class GASP_GIUSTINO2_API UGSTN_ProbeAbilityBase : public UGSTN_GameplayAbility
{
	GENERATED_BODY()

public:
	UGSTN_ProbeAbilityBase(const FObjectInitializer& ObjectInitializer);

	/** Counted before anything can refuse the activation, so an attempt made while a frame is restored shows even if it fails */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** Dates the end of the activation (outside a restore), to measure how long a triggered ability lived */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Name under which the counters are reported in the snapshot */
	FName GetProbeName() const { return ProbeName; }

protected:
	static UGSTN_GASNPPTestComponent* FindTestComponent(const FGameplayAbilityActorInfo* ActorInfo);

	/** Applies an effect to the owner unless a frame is being restored (the restored state already holds its result) */
	void ApplyProbeEffect(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, TSubclassOf<UGameplayEffect> Effect);

	UPROPERTY(EditDefaultsOnly, Category = "Test|Probe")
	FName ProbeName;
};

/**
 * Raises the probe GameplayEvent (Test.Probe.Event) from inside the simulation, then ends. Activated on the release (Completed)
 * of IA_GASNPP_Buff: the test buff ability uses the Started event of the same action, and the ability system activates only
 * the first ability found for an (action, event) pair, so the two never compete. Grant at most one raiser at a time (the tag
 * raiser uses the same input).
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_ProbeEventRaiser : public UGSTN_ProbeAbilityBase
{
	GENERATED_BODY()

public:
	UGSTN_GA_ProbeEventRaiser(const FObjectInitializer& ObjectInitializer);

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** Applies the effect that grants Test.Probe.TagTrigger (1s) from inside the simulation, then ends. Same input as the event raiser. */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_ProbeTagRaiser : public UGSTN_ProbeAbilityBase
{
	GENERATED_BODY()

public:
	UGSTN_GA_ProbeTagRaiser(const FObjectInitializer& ObjectInitializer);

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/**
 * Base of the two triggered probes. On activation it applies an instant effect that bumps a counter attribute (the footprint
 * in the synced state: a double activation doubles it; a phantom one made while a frame is restored leaves none because
 * effects are refused then, which the restore counters of the component catch) and then stays active for ActiveFrames
 * simulation frames, so the active ability list can be compared between machines.
 */
UCLASS(Abstract)
class GASP_GIUSTINO2_API UGSTN_ProbeTargetBase : public UGSTN_ProbeAbilityBase
{
	GENERATED_BODY()

public:
	UGSTN_ProbeTargetBase(const FObjectInitializer& ObjectInitializer);

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Test|Probe", meta = (ClampMin = "1"))
	int32 ActiveFrames = 30;

	/** Instant effect applied once per activation */
	UPROPERTY(EditDefaultsOnly, Category = "Test|Probe")
	TSubclassOf<UGameplayEffect> CountEffect;

private:
	/** Simulation frames since activation. A sync var: rolled back with the ability instance on corrections */
	UPROPERTY()
	FSyncInteger FramesSinceActivation;
};

/** Triggered by the GameplayEvent Test.Probe.Event (AbilityTriggers, GameplayEvent source). Counts in ProbeEventCount. */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_ProbeEventTarget : public UGSTN_ProbeTargetBase
{
	GENERATED_BODY()

public:
	UGSTN_GA_ProbeEventTarget(const FObjectInitializer& ObjectInitializer);
};

/** Triggered by the owned tag Test.Probe.TagTrigger being added (AbilityTriggers, OwnedTagAdded source). Counts in ProbeTagCount. */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_ProbeTagTarget : public UGSTN_ProbeTargetBase
{
	GENERATED_BODY()

public:
	UGSTN_GA_ProbeTagTarget(const FObjectInitializer& ObjectInitializer);
};

/**
 * Base of the 40 distinct ability classes the grant probe gives to a pawn in one batch (a fighter's loadout is about that
 * size). They do nothing: the point is the cost of granting, replicating and looking up many distinct ability specs, each with
 * its own class to export. The first 30 are predicted, the last 10 are ServerOnly.
 */
UCLASS(Abstract)
class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrantBase : public UGSTN_GameplayAbility
{
	GENERATED_BODY()

public:
	UGSTN_GA_ProbeGrantBase(const FObjectInitializer& ObjectInitializer);
};

UCLASS(Abstract)
class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrantServerOnlyBase : public UGSTN_GA_ProbeGrantBase
{
	GENERATED_BODY()

public:
	UGSTN_GA_ProbeGrantServerOnlyBase(const FObjectInitializer& ObjectInitializer);
};

// One class per line on purpose: UHT reads the header text, a macro cannot declare them
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant00 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant01 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant02 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant03 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant04 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant05 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant06 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant07 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant08 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant09 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant10 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant11 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant12 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant13 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant14 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant15 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant16 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant17 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant18 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant19 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant20 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant21 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant22 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant23 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant24 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant25 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant26 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant27 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant28 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant29 : public UGSTN_GA_ProbeGrantBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant30 : public UGSTN_GA_ProbeGrantServerOnlyBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant31 : public UGSTN_GA_ProbeGrantServerOnlyBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant32 : public UGSTN_GA_ProbeGrantServerOnlyBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant33 : public UGSTN_GA_ProbeGrantServerOnlyBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant34 : public UGSTN_GA_ProbeGrantServerOnlyBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant35 : public UGSTN_GA_ProbeGrantServerOnlyBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant36 : public UGSTN_GA_ProbeGrantServerOnlyBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant37 : public UGSTN_GA_ProbeGrantServerOnlyBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant38 : public UGSTN_GA_ProbeGrantServerOnlyBase { GENERATED_BODY() };
UCLASS() class GASP_GIUSTINO2_API UGSTN_GA_ProbeGrant39 : public UGSTN_GA_ProbeGrantServerOnlyBase { GENERATED_BODY() };

namespace GSTNProbe
{
	/** The 40 grant probe classes, in index order (0-29 predicted, 30-39 ServerOnly) */
	GASP_GIUSTINO2_API const TArray<TSubclassOf<UGameplayAbility>>& GetGrantClasses();
}

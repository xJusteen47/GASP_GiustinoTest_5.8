// Minion runtime (M8 G8): the StateTree schema, tasks and conditions of a minion brain.

#pragma once

#include "CoreMinimal.h"
#include "Brain/GSTN_MinionBrain.h"
#include "GameplayTagContainer.h"
#include "StateTreeConditionBase.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "StateTreeSchema.h"
#include "StateTreeTaskBase.h"
#include "GSTN_MinionStateTree.generated.h"

/**
 * The schema of a minion brain tree (M8 G8). A tree of this schema runs inside UGSTN_MinionBrainSubsystem, not on a component: the
 * subsystem owns one FStateTreeInstanceData per brained slot and runs the execution context itself, so a crowd pays no ticking
 * component per minion.
 *
 * The context is one external struct, FGSTN_MinionBrainContext (the minion's slot, position, tier, target and the pending intent):
 * tasks and conditions link it as external data and never touch a world object, a Mover query or the store. Everything a task wants to
 * do goes into the context's pending intent, which the subsystem commits with SetIntent after the tick. Only the structs under
 * the StateTree "common" bases are allowed (ours plus the stock comparison conditions, delays and the like); Blueprint nodes are not.
 * Scheduled (sleeping) ticks are off: the subsystem decides when a brain runs, from its LOD tier.
 */
UCLASS(BlueprintType, hidecategories = (Object), meta = (DisplayName = "GSTN Minion Brain"))
class GSTNMINIONRUNTIME_API UGSTN_MinionStateTreeSchema : public UStateTreeSchema
{
	GENERATED_BODY()

public:
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;
	virtual bool IsScheduledTickAllowed() const override { return false; }
};

// ---------------------------------------------------------------------------------------------------------------------------------
// Tasks. Each acts on EnterState and on every Tick (the same pure action either way), so a state change takes effect on the tick
// of the transition and the native brain, which does the same, writes identical intents.

USTRUCT()
struct GSTNMINIONRUNTIME_API FGSTN_MoveToTargetInstanceData
{
	GENERATED_BODY()

	/** Ground speed, cm/s (clamped by the type's walk and run speed when the batch applies it) */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float Speed = 200.f;

	/** Stop this far from the target (zero speed inside it) */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float StopRadius = 150.f;

	/** Width of the ring outside the stop radius over which the speed ramps down to zero (0 = full speed up to the stop radius) */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float SlowRadius = 0.f;

	/** Neighbours closer than this push the minion away */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float SeparationRadius = 120.f;

	/** How hard the separation push bends the seek direction (0 = none) */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float SeparationWeight = 0.6f;
};

/** Seek and arrive at the brain's target with separation from close neighbours. Never fails or completes: the state ends by a transition */
USTRUCT(meta = (DisplayName = "GSTN Move To Target", Category = "GSTN|Minion"))
struct GSTNMINIONRUNTIME_API FGSTN_MoveToTargetTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FGSTN_MoveToTargetInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<FGSTN_MinionBrainContext> ContextHandle;

private:
	EStateTreeRunStatus Run(FStateTreeExecutionContext& Context) const;
};

/** Empty: the 5.8 StateTree compiler rejects a task without an instance value ("Malformed task, missing instance value") */
USTRUCT()
struct GSTNMINIONRUNTIME_API FGSTN_FaceTargetInstanceData
{
	GENERATED_BODY()
};

/** Asks the minion to face its target */
USTRUCT(meta = (DisplayName = "GSTN Face Target", Category = "GSTN|Minion"))
struct GSTNMINIONRUNTIME_API FGSTN_FaceTargetTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FGSTN_FaceTargetInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<FGSTN_MinionBrainContext> ContextHandle;
};

USTRUCT()
struct GSTNMINIONRUNTIME_API FGSTN_RequestAttackInstanceData
{
	GENERATED_BODY()

	/** The move to ask for (a Command.Minion.* tag). Empty = the type's first move */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "Command.Minion"))
	FGameplayTag Command;
};

/**
 * Asks for an attack every tick. Only a request: the batch still checks range, cooldown and the attack tokens of the target, so a
 * crowd of minions all asking at once is exactly what the tokens are for. The task never starts a move itself
 */
USTRUCT(meta = (DisplayName = "GSTN Request Attack", Category = "GSTN|Minion"))
struct GSTNMINIONRUNTIME_API FGSTN_RequestAttackTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FGSTN_RequestAttackInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<FGSTN_MinionBrainContext> ContextHandle;

private:
	EStateTreeRunStatus Run(FStateTreeExecutionContext& Context) const;
};

// ---------------------------------------------------------------------------------------------------------------------------------
// Conditions

USTRUCT()
struct GSTNMINIONRUNTIME_API FGSTN_TargetInRangeInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MinRange = 0.f;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxRange = 200.f;

	/** True when the target must be OUTSIDE the range (a leave-the-state transition) */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

/** True when the ground distance to the target is within [MinRange, MaxRange] (inverted: outside, or no target at all) */
USTRUCT(meta = (DisplayName = "GSTN Target In Range", Category = "GSTN|Minion"))
struct GSTNMINIONRUNTIME_API FGSTN_TargetInRangeCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FGSTN_TargetInRangeInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	TStateTreeExternalDataHandle<FGSTN_MinionBrainContext> ContextHandle;
};

USTRUCT()
struct GSTNMINIONRUNTIME_API FGSTN_IsLODAtLeastInstanceData
{
	GENERATED_BODY()

	/** The coarsest tier that still passes: Near passes Combat and Near */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	EGSTN_MinionLODTier Tier = EGSTN_MinionLODTier::Near;
};

/** True when the minion simulates at Tier or more finely (a brain can skip fancy behaviour while it is far) */
USTRUCT(meta = (DisplayName = "GSTN Is LOD At Least", Category = "GSTN|Minion"))
struct GSTNMINIONRUNTIME_API FGSTN_IsLODAtLeastCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FGSTN_IsLODAtLeastInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	TStateTreeExternalDataHandle<FGSTN_MinionBrainContext> ContextHandle;
};

namespace GSTNMinion
{
	/**
	 * Builds the "chase and swing" test tree (a UStateTree, returned as UObject so this header needs no StateTree types). Bound by the
	 * editor module (GSTNMinionEditor), which can compile a tree in code; unbound in a build without it, where the bench and the
	 * brain fall back to the native brain. Returns the same tree on every call (the factory caches it)
	 */
	DECLARE_DELEGATE_RetVal(UObject*, FTestBrainTreeFactory);
	GSTNMINIONRUNTIME_API FTestBrainTreeFactory& GetTestBrainTreeFactory();
}

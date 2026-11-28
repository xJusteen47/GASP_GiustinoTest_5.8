// Combat simulation (GAS-NPP fork): the run stages in Mover's sync state (docs/notes/corsa-mario-piano.md, P1).

#pragma once

#include "CoreMinimal.h"
#include "MoverTypes.h"
#include "Movement/GSTN_RunStageTypes.h"
#include "GSTN_RunStageSyncState.generated.h"

class AActor;
class UMoverComponent;
struct FGSTN_LocomotionSimTuning;
struct FMoverSyncState;
struct FMoverTickStartData;
struct FProposedMove;
struct FMoverTimeStep;

/**
 * The run state (FGSTN_RunStageState) as an entry of Mover's sync state collection: a rollback restores it with the rest of the
 * movement, a re-simulation steps it again, and the authority's copy corrects a misprediction (ShouldReconcile). Registered as a
 * persistent type, so the modes that do not write it (all of them) carry it from frame to frame. Nothing lives on a component.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_RunStageSyncState : public FMoverDataStructBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	FGSTN_RunStageState Run;

	virtual FMoverDataStructBase* Clone() const override { return new FGSTN_RunStageSyncState(*this); }
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
};

template<>
struct TStructOpsTypeTraits<FGSTN_RunStageSyncState> : public TStructOpsTypeTraitsBase2<FGSTN_RunStageSyncState>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

/**
 * The run stages inside Mover's tick, called by FGSTN_StatsMovementModifier (the permanent modifier every combat pawn has):
 *   - ApplyStageValues from OnPreMovement, after the stats factor: assigns the stage of the start of the frame (read from the
 *     rollback-following tracker) to the walking mode's properties named by the profile. Assigned, never adjusted, like the stats;
 *   - StepFromMovement from OnPostMovement: steps the state of the output sync state with the frame's end velocity and the frame's
 *     command (stick, boost) from the combat model, which ticked before Mover in the same frame.
 * A pawn whose kit has no locomotion entry is left alone (no state entry is ever written for it).
 */
struct GSTNCOMBATNP_API FGSTN_RunStageMover
{
	/** Makes FGSTN_RunStageSyncState persistent on MoverComp (idempotent; outside the simulation, when the modifier is queued) */
	static void RegisterPersistentState(UMoverComponent& MoverComp);

	/**
	 * gstn.RunStage.Readable (owner 2026-11-20, docs/notes/run-stages-leggibili-piano.md): 1 = the readable stage steps of the kit's data
	 * (entry burst, tell, charge; a stage with a burst has no ramp), 0 = the old continuous run. It changes the simulation: same value on
	 * server and clients. Read by the sim adapter and the presentation
	 */
	static bool IsReadableSteps();

	/** The run state in SyncState, null when it has none */
	static const FGSTN_RunStageState* Find(const FMoverSyncState& SyncState);

	/**
	 * Writes the stage's values (Factor = stats factor) into every mode of MoverComp. Returns the number of values written.
	 * GroundSpeed >= 0 (the speed at the start of the frame, from the rollback-following tracker) adds the analog walk start: more
	 * acceleration and turning at low speed, and the stage speed on the walk gait too (FGSTN_AnalogWalkTuning). < 0 = stage values only.
	 * MinTurning >= 0 raises the turning strength to at least that (the run reversal, FGSTN_RunReversalTuning).
	 * MinTopSpeed > 0 raises the top speed to at least that, stats already in (the dash burst, FGSTN_RunStageLogic::GetDashTopSpeed)
	 */
	static int32 ApplyStageValues(UMoverComponent& MoverComp, const FGSTN_LocomotionSimTuning& Tuning, int32 Stage, float Factor, float GroundSpeed = -1.f,
		float AccelRamp = 1.f, float MinTurning = -1.f, float MinTopSpeed = -1.f);

	/**
	 * The air control and air top speed (P2, decision 1C) into Mover's shared settings and the profile's named air properties: the AirControl
	 * stat (or the archetype's value) times the takeoff stage's scale, and, airborne with the TakeoffCap model, the takeoff speed as MaxSpeed.
	 * State = the run state of the start of the frame (null = grounded defaults). Returns the number of values written
	 */
	static int32 ApplyAirValues(UMoverComponent& MoverComp, const FGSTN_LocomotionSimTuning& Tuning, const FGSTN_RunStageState* State, float Factor);

	/** OnPreMovement half (see the struct comment) */
	static void PreMovement(UMoverComponent& MoverComp, float Factor);

	/** OnPostMovement half; OutputSyncState is the state machine's output of the substep */
	static void PostMovement(UMoverComponent& MoverComp, const FMoverTimeStep& TimeStep, FMoverSyncState& OutputSyncState, float Factor);

	/**
	 * Air momentum (owner 2026-11-18, FGSTN_AirMomentumLogic), from Mover's ProcessGeneratedMovement (after the mode and the layered
	 * moves): replaces the horizontal velocity the falling mode proposed with the two-channel model. Only when the kit's profile has the
	 * model on, the frame's mode is a UFallingMode (GASP's Blueprint one included) and no layered move overrides the velocity. The vertical
	 * velocity and the rotation are left as they are. Reads the air top speed and air control PreMovement wrote for this substep.
	 * Returns true when it ran; OutExternal is the external channel after the step and OutProposedHorizontal the horizontal velocity
	 * proposed (both before collisions), which PostMovement compares with the moved velocity and stores
	 */
	static bool ApplyAirMomentum(const UMoverComponent& MoverComp, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep,
		FProposedMove& InOutMove, FVector& OutExternal, FVector& OutProposedHorizontal);

	/** True when SyncState has an active layered move that counts as an external push: any FLayeredMove_Launch child, a knockback */
	static bool HasExternalPushMove(const FMoverSyncState& SyncState);
};

// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Intent/GSTN_MinionIntent.h"
#include "LOD/GSTN_MinionLOD.h"
#include "Store/GSTN_MinionStore.h"
#include "GSTN_MinionBrain.generated.h"

/**
 * Everything a brain reads about one minion this tick, and the intent it writes (M8 G8). The runtime fills it from the store and the
 * fighters right before the brain runs; the brain only writes PendingIntent, which the subsystem commits with SetIntent (one call per
 * ticked slot, never touching the store directly). The StateTree tasks, the native brain and the tests all work on this one struct,
 * so every brain sees the same world and speaks the same intent API.
 *
 * Reflected only so a StateTree can name it as the schema's external data; none of the fields is a binding source. Positions are
 * world cm, distances are on the ground plane (the batch and the LOD measure the same way).
 */
USTRUCT()
struct GSTNMINIONCORE_API FGSTN_MinionBrainContext
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Minion|Brain")
	FGSTN_MinionHandle Handle;

	UPROPERTY(VisibleAnywhere, Category = "Minion|Brain")
	int32 Slot = INDEX_NONE;

	/** Server frame of the last batch, the frame the intent will be stamped with */
	UPROPERTY(VisibleAnywhere, Category = "Minion|Brain")
	int32 Frame = 0;

	UPROPERTY(VisibleAnywhere, Category = "Minion|Brain")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Minion|Brain")
	float YawDegrees = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Minion|Brain")
	EGSTN_MinionLODTier Tier = EGSTN_MinionLODTier::Combat;

	UPROPERTY(VisibleAnywhere, Category = "Minion|Brain")
	bool bHasTarget = false;

	/** Pipeline id of the current target (GSTNEntityKinds ids), INDEX_NONE for none */
	UPROPERTY(VisibleAnywhere, Category = "Minion|Brain")
	int32 TargetId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category = "Minion|Brain")
	FVector TargetPosition = FVector::ZeroVector;

	/** Ground distance to the target; a huge value when there is none */
	UPROPERTY(VisibleAnywhere, Category = "Minion|Brain")
	float TargetDistance = TNumericLimits<float>::Max();

	/** Where to steer: the target, or the next waypoint of an async navigation path when one is known */
	UPROPERTY(VisibleAnywhere, Category = "Minion|Brain")
	FVector MovePoint = FVector::ZeroVector;

	/** Walk speed of the type, cm/s */
	UPROPERTY(VisibleAnywhere, Category = "Minion|Brain")
	float WalkSpeed = 200.f;

	/** The first move's command of the type: what RequestAttack asks for when its own command is empty */
	UPROPERTY(VisibleAnywhere, Category = "Minion|Brain")
	FGameplayTag DefaultCommand;

	/** The intent this tick builds. Reset before each brain tick, so what a task does not write is "do nothing" */
	UPROPERTY(VisibleAnywhere, Category = "Minion|Brain")
	FGSTN_MinionIntent PendingIntent;

	/** Ground positions of the minions close to this one, for separation. Filled by the subsystem; not reflected */
	TConstArrayView<FVector2D> Neighbours;

	FVector2D Position2D() const { return FVector2D(Position.X, Position.Y); }
};

/** Numbers of the "chase and swing" behaviour, shared by the native brain and the test StateTree so the two stay comparable (spike 6) */
struct GSTNMINIONCORE_API FGSTN_MinionChaseSwingParams
{
	/** Chase ground speed, cm/s */
	float Speed = 200.f;
	/** Chase ends inside this distance of the target (the swing's range is further out, so the swing starts without walking on) */
	float StopRadius = 150.f;
	float SlowRadius = 0.f;
	float SeparationRadius = 120.f;
	float SeparationWeight = 0.6f;
	/** Chase turns into Swing at or inside this distance */
	float EnterRange = 200.f;
	/** Swing turns back into Chase beyond this distance (above EnterRange: the gap is the hysteresis against flicker) */
	float ExitRange = 320.f;
};

/**
 * The brain's actions as pure functions on the context. The StateTree tasks and conditions call exactly these, so "the same tree
 * logic written by hand" (the native brain) cannot drift from the asset.
 */
struct GSTNMINIONCORE_API FGSTN_MinionBrainActions
{
	/** Steers toward MovePoint with arrival at the target and separation. No target = no move. Writes MoveDir, DesiredSpeed, TargetEntityId */
	static void MoveToTarget(FGSTN_MinionBrainContext& Context, float Speed, float StopRadius, float SlowRadius, float SeparationRadius, float SeparationWeight);

	/** Asks to face the target (DesiredYaw, bFaceRequested). No target = no request */
	static void FaceTarget(FGSTN_MinionBrainContext& Context);

	/**
	 * Asks for a move by its command (Command, else the type's DefaultCommand). The batch still checks range, cooldown and attack
	 * tokens: a request is only ever a wish, which is why a brain can repeat it every tick
	 */
	static void RequestAttack(FGSTN_MinionBrainContext& Context, const FGameplayTag& Command);

	/** True when the ground distance to the target is within [MinRange, MaxRange]; false without a target */
	static bool IsTargetInRange(const FGSTN_MinionBrainContext& Context, float MinRange, float MaxRange);

	/** True when the minion simulates at Tier or more finely (the tiers are ordered Combat < Near < Far < Dormant) */
	static bool IsLODAtLeast(const FGSTN_MinionBrainContext& Context, EGSTN_MinionLODTier Tier);
};

/**
 * The hand-written "chase and swing" brain (M8 G8, spike 6 fallback): the same behaviour as the test StateTree, with the same intent
 * API. A tick runs the actions of the current phase and then checks the phase change; the new phase's actions run at once on top, as
 * a StateTree runs the entered state's tasks (EnterState) right after the transition. Selected when a minion has no tree or by the
 * console variable gstn.Minion.Brain.Native.
 */
struct GSTNMINIONCORE_API FGSTN_MinionNativeBrain
{
	enum class EPhase : uint8
	{
		Chase,
		Swing
	};

	EPhase Phase = EPhase::Chase;
	bool bStarted = false;

	/** Resets the intent of Context and runs one tick. The first tick only enters Chase (as a tree's Start does) */
	void Tick(FGSTN_MinionBrainContext& Context, const FGSTN_MinionChaseSwingParams& Params = FGSTN_MinionChaseSwingParams());

	static const TCHAR* PhaseName(EPhase InPhase) { return InPhase == EPhase::Chase ? TEXT("Chase") : TEXT("Swing"); }

private:
	void RunPhase(FGSTN_MinionBrainContext& Context, const FGSTN_MinionChaseSwingParams& Params) const;
};

/**
 * How often a brain runs, per LOD tier (M8 G8). The cost of the AI has to stay flat as the crowd grows, so far minions think rarely
 * and spread over the frames: a slot runs when (TickCounter + Slot) % Interval == 0, so slots of one tier never all land on the same
 * frame. Dormant minions do not think at all; a wake (hit, launch) forces one tick.
 *
 * These rates are not the batch intervals of FGSTN_MinionLODParams (1/2/4/16 frames of the fixed tick): the brain runs on engine
 * frames and is the expensive part, so it is stepped down harder (T1 every 4th, T2 every 16th). The intent stays valid for 30 batch frames.
 */
struct GSTNMINIONCORE_API FGSTN_MinionBrainBudget
{
	/** T1 Near: one brain tick every this many engine frames */
	int32 NearInterval = 4;
	/** T2 Far: one brain tick every this many engine frames */
	int32 FarInterval = 16;

	/** Interval of a tier in frames; 0 for Dormant (never) */
	int32 GetInterval(EGSTN_MinionLODTier Tier) const;

	/** True when the slot's brain runs on this tick of the counter */
	bool ShouldTick(EGSTN_MinionLODTier Tier, int32 Slot, int32 TickCounter, bool bWoken) const;
};

namespace GSTNMinion
{
	/** Target selection refresh: the nearest fighter is re-chosen at most this often per slot (2 Hz); between refreshes only its position is read */
	constexpr double BrainTargetRefreshSeconds = 0.5;
}

// Platform runtime: components, colliders, the pose point (Mover, no GAS, no direct Network Prediction code).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_PlatformPoseContext.h"
#include "GSTN_PlatformSubsystem.generated.h"

class UGSTN_EnemyPlatformComponent;
class UMoverComponent;
struct FMoverInputCmdContext;
struct FMoverTimeStep;

/**
 * Runs the platforms' pose point: one defined point per simulation frame where every platform collider of the world takes its
 * pose (many-enemies-replication.md 4.3, traversal-surfaces.md 4.2).
 *
 * Driver today: the start of every Mover simulation tick of the world (UMoverComponent::OnPreSimulationTick). Mover ticks inside
 * the fixed simulation step, before its movement, with the frame and the time of the step (FMoverTimeStep::ServerFrame,
 * BaseSimTimeMs, in server simulation timespace): the same numbers on the server, on a client predicting that frame and on a
 * client re-simulating it. The first Mover tick of a frame poses every platform at the end of the previous frame; the other pawns
 * of the same frame find it done (once per step, whatever the number of pawns and their order). A re-simulation goes back in
 * frames, which poses again.
 *
 * Why not a post-fixed-tick hook of Network Prediction: the fork has none yet (combat-m8-minions.md spike 2), and the plugin
 * must build with GSTNSurfaceCore and Mover only. When the hook exists it calls ApplyPosePoint and this driver goes away; nothing
 * else changes. Consequence of the Mover driver: simulation code that ticks before Mover in the same frame (combat, the ability
 * system: CLAUDE.md rule 5) sees the platforms at the pose of the frame before (one frame older), and on the first frame of a
 * re-simulation at the pose of the newest frame. Nothing in that code sweeps against platforms today.
 */
UCLASS()
class GSTNPLATFORMRUNTIME_API UGSTN_PlatformSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	void RegisterPlatform(UGSTN_EnemyPlatformComponent* Platform);
	void UnregisterPlatform(UGSTN_EnemyPlatformComponent* Platform);

	/**
	 * The pose point. Poses every registered platform for Context unless this exact frame and time was the last one applied
	 * (another pawn's tick of the same frame). True if it posed
	 */
	bool ApplyPosePoint(const FGSTN_PlatformPoseContext& Context);

	/** The pose context of a Mover simulation tick: the end of the previous frame (Frame = ServerFrame - 1, TimeMs = BaseSimTimeMs) */
	static FGSTN_PlatformPoseContext MakePoseContext(const FMoverTimeStep& TimeStep);

	/** A simulation ran the pose point within the settings' PresentationFallbackSeconds (real time) */
	bool HasRecentSimulationPose() const;

	/** Hooks a Mover component's simulation tick to the pose point (once per component) */
	void BindMover(UMoverComponent* Mover);

	const TArray<TWeakObjectPtr<UGSTN_EnemyPlatformComponent>>& GetPlatforms() const { return Platforms; }

private:
	UFUNCTION()
	void HandleMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd);

	void HandleActorSpawned(AActor* Actor);
	void BindMoversOf(AActor* Actor);
	void BindAllMovers();

	TArray<TWeakObjectPtr<UGSTN_EnemyPlatformComponent>> Platforms;
	TSet<TWeakObjectPtr<UMoverComponent>> BoundMovers;
	FDelegateHandle ActorSpawnedHandle;

	bool bHasLastApplied = false;
	int32 LastAppliedFrame = INDEX_NONE;
	double LastAppliedTimeMs = 0.0;
	double LastSimulationPoseRealSeconds = -1.e9;
};

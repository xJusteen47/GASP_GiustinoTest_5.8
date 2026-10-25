// Platform runtime: components, colliders, the pose point (Mover, no GAS, no direct Network Prediction code).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSTN_PlatformPoseContext.h"
#include "GSTN_PlatformSpec.h"
#include "GSTN_PoseHistory.h"
#include "GSTN_EnemyPlatformComponent.generated.h"

class UGSTN_PlatformDefinition;
class UPrimitiveComponent;

/**
 * Makes its owner (a fighter) a platform (many-enemies-replication.md 4.2-4.5, traversal-surfaces.md 4.1-4.2): one kinematic,
 * Movable, unattached collider per shape of the spec, named Surface<ShapeId>, posed at one defined point per simulation step.
 *
 * Why unattached and posed at a point, not attached to the actor: an attached child moves in the enemy's own tick, so its pose
 * at the moment a player's simulation sweeps would depend on the order of NP instances in the frame, and on a client it would be
 * the interpolated pose of the game frame rather than one of the simulation frame. A player's wall slide rides its base from
 * the base pose of the previous frame (wall-slide-v2.md 6.14), which must be the same on every machine for a given frame.
 *
 * The pose point (UGSTN_PlatformSubsystem::ApplyPosePoint) runs before the movement of each simulation frame F, once per frame
 * whatever the number of pawns, and poses every platform at the end of F-1 through ComputeOwnerPose. The default pose is the
 * owner's current transform (approach B: the authoritative actor on the host, the interpolated proxy on a client); a scripted
 * enemy overrides it with a function of the frame time (approach C, UGSTN_ScriptedPlatformComponent), which is exact on every
 * machine. When no simulation runs the pose point, the component poses itself from the game tick for presentation only.
 *
 * The colliders are net addressable (stable names, SetNetAddressable): Mover's sync state names a movement base by object
 * reference, so a client must resolve the server's Surface0 to its own Surface0. The owner must be replicated or stably named.
 */
UCLASS(ClassGroup = (GSTN), Blueprintable, meta = (BlueprintSpawnableComponent, DisplayName = "GSTN Enemy Platform"))
class GSTNPLATFORMRUNTIME_API UGSTN_EnemyPlatformComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_EnemyPlatformComponent();

	/** Shared spec of this kind of enemy. When set it wins over the inline Spec */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Platform")
	TObjectPtr<const UGSTN_PlatformDefinition> Definition;

	/** Spec used when there is no Definition */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Platform", meta = (EditCondition = "Definition == nullptr"))
	FGSTN_PlatformSpec Spec;

	/** The spec in use: the definition's, else the inline one */
	const FGSTN_PlatformSpec& GetSpec() const;

	/**
	 * The owner's pose at the end of a simulation frame, from which every shape is posed (scale ignored). Called inside the
	 * simulation (once per frame, also on a re-simulated frame): answer from the context and from replicated or confirmed state
	 * only, no clocks, randoms or timers. Default: the owner's current root transform
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Platform")
	FTransform ComputeOwnerPose(const FGSTN_PlatformPoseContext& Context) const;

	/** Poses every collider at the end of Context.Frame and records the pose. Called by the subsystem's pose point */
	void ApplyPosePoint(const FGSTN_PlatformPoseContext& Context);

	/** Poses the colliders from the game tick (no frame, no history): what the component does when no simulation runs */
	void ApplyPresentationPose();

	/** The collider of a shape id, null if the spec has no such shape or the colliders are not created (not a game world) */
	UFUNCTION(BlueprintPure, Category = "Platform")
	UPrimitiveComponent* GetShapeComponent(int32 ShapeId) const;

	/** Poses recorded at the pose point, by simulation frame */
	const FGSTN_PoseHistory& GetPoseHistory() const { return PoseHistory; }

	/** Frame whose end pose the colliders hold now (during a re-simulation: the replayed frame), INDEX_NONE before the first */
	int32 GetCurrentPoseFrame() const { return CurrentPoseFrame; }

	/** Highest frame posed so far */
	int32 GetLatestPoseFrame() const { return LatestPoseFrame; }

	/** How many times the pose point posed this platform (tests and debug) */
	int32 GetPoseApplicationCount() const { return PoseApplicationCount; }

	/** Errors and warnings of the spec in its owner (shared by IsDataValid and the tests). True if there is no error */
	bool CollectFindings(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	/** After the colliders took a pose at the pose point or in the presentation fallback (a scripted owner moves its visual here) */
	virtual void OnPoseApplied(const FGSTN_PlatformPoseContext& Context, const FTransform& OwnerPose) {}

	/** Collision of a new collider (object channel, responses). Called once, before it is registered */
	virtual void ConfigureCollision(UPrimitiveComponent& Collider, const FGSTN_PlatformShape& Shape) const;

	/** Whether a shape's collider has collision at a frame (INDEX_NONE: the presentation pose, which keeps the current state) */
	virtual bool IsShapeSolidAt(int32 ShapeId, int32 Frame) const { return true; }

	void CreateColliders();
	void DestroyColliders();
	void PoseColliders(const FTransform& OwnerPose, int32 Frame);

	/** Colliders by shape id (null where the spec has no shape) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> Colliders;

	FGSTN_PoseHistory PoseHistory;
	int32 CurrentPoseFrame = INDEX_NONE;
	int32 LatestPoseFrame = INDEX_NONE;
	int32 PoseApplicationCount = 0;
};

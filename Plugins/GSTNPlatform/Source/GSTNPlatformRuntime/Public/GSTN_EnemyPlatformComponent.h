// Platform runtime: components, colliders, the pose point (Mover, no GAS, no direct Network Prediction code).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GSTN_PlatformPoseContext.h"
#include "GSTN_PlatformShapeStates.h"
#include "GSTN_PlatformSpec.h"
#include "GSTN_PlatformTypes.h"
#include "GSTN_PoseHistory.h"
#include "GSTN_SurfaceTypes.h"
#include "GSTN_EnemyPlatformComponent.generated.h"

class UGSTN_PlatformDefinition;
class UPrimitiveComponent;

/**
 * Makes its owner (a fighter) a platform and gives its body traversal surfaces (many-enemies-replication.md 4.2-4.5,
 * traversal-surfaces.md 4.1-4.4): one kinematic, Movable, unattached collider per shape of the spec, named Surface<ShapeId>,
 * posed and switched on or off at one defined point per simulation step, and answering for its surface (behaviour, velocity) as
 * an IGSTN_SurfaceProvider.
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
 * Shape activation (traversal-surfaces.md 4.4): two inputs, SetShapeMask (parts, the T6 bridge) and SetActionState (the enemy's
 * state, T7), give the shapes that should be solid. A change takes effect at the next frame the pose point simulates and is kept
 * as a timeline (FGSTN_PlatformShapeStates), so the collision of a frame is a function of the frame: a re-simulated frame finds
 * the collision it had the first time. A shape that goes away with a telegraph stays solid as Breaking for its DropGraceFrames.
 * The simulation never reads these inputs: a player's sweep simply finds or does not find the collider.
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

	/** Poses every collider at the end of Context.Frame, applies the shape states of that frame and records the pose. Called by the pose point */
	void ApplyPosePoint(const FGSTN_PlatformPoseContext& Context);

	/** Poses the colliders from the game tick (no frame, no history): what the component does when no simulation runs */
	void ApplyPresentationPose();

	// ---- Activation inputs (outside the simulation: confirmed state, presentation, AI)

	/**
	 * Shapes (bit per ShapeId) that may be solid; the others go away, with the Breaking telegraph of their DropGraceFrames when
	 * DisableAs is Breaking, at once when it is Inactive. Shapes turned back on are Active at once. Takes effect at the next frame
	 * the pose point simulates (traversal-surfaces.md 4.4: the parts bridge calls it with the confirmed broken mask)
	 */
	UFUNCTION(BlueprintCallable, Category = "Platform")
	void SetShapeMask(uint8 EnabledShapes, EGSTN_PlatformShapeState DisableAs = EGSTN_PlatformShapeState::Breaking);

	/** The enemy's action state: shapes not active in it go away with their telegraph (many-enemies-replication.md 4.6; wired in T7) */
	UFUNCTION(BlueprintCallable, Category = "Platform")
	void SetActionState(EGSTN_PlatformActionState State);

	UFUNCTION(BlueprintPure, Category = "Platform")
	uint8 GetShapeMaskInput() const { return ShapeMaskInput; }

	UFUNCTION(BlueprintPure, Category = "Platform")
	EGSTN_PlatformActionState GetActionState() const { return ActionState; }

	// ---- Queries (the simulation may call them: static data and the pose history only)

	/** State of a shape at the frame the colliders hold (the final state when no frame was simulated yet) */
	UFUNCTION(BlueprintPure, Category = "Platform")
	EGSTN_PlatformShapeState GetShapeState(int32 ShapeId) const;

	/** The Platform.State.* tag of a shape's state, for presentation and cues */
	UFUNCTION(BlueprintPure, Category = "Platform")
	FGameplayTag GetShapeStateTag(int32 ShapeId) const;

	/** The surface a shape offers (static data of the spec); empty for an unknown shape id */
	UFUNCTION(BlueprintPure, Category = "Platform")
	FGSTN_SurfaceBehaviour GetShapeBehaviour(int32 ShapeId) const;

	/**
	 * Velocity of a shape at a world point, between the end of the frame before and the end of the frame the colliders hold, from
	 * the pose history (never from physics: the colliders are teleported). What a launch with bAddSurfaceVelocity adds (T4) and
	 * what a wall or walking mode can add on leaving. Zero for an unknown or inactive shape or without two recorded frames
	 */
	UFUNCTION(BlueprintPure, Category = "Platform")
	FVector GetSurfaceVelocityAt(int32 ShapeId, FVector WorldPoint) const;

	/** How the enemy responds to a character on a shape (data only until T7) */
	UFUNCTION(BlueprintPure, Category = "Platform")
	FGSTN_OccupantResponse GetOccupantResponse(int32 ShapeId) const;

	/**
	 * A surface asked to break (IGSTN_SurfaceProvider::BreakSurface, H11), outside the simulation. Default: nothing; a game
	 * decides what breaking a body part means (the parts plugin, through its own bridge)
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Platform")
	void OnShapeBreakRequested(int32 ShapeId);

	/** The collider of a shape id, null if the spec has no such shape or the colliders are not created (not a game world) */
	UFUNCTION(BlueprintPure, Category = "Platform")
	UPrimitiveComponent* GetShapeComponent(int32 ShapeId) const;

	/** Poses recorded at the pose point, by simulation frame */
	const FGSTN_PoseHistory& GetPoseHistory() const { return PoseHistory; }

	/** The shape timelines */
	const FGSTN_PlatformShapeStates& GetShapeStates() const { return ShapeStates; }

	/** Frame whose end pose the colliders hold now (during a re-simulation: the replayed frame), INDEX_NONE before the first */
	int32 GetCurrentPoseFrame() const { return CurrentPoseFrame; }

	/** Highest frame posed so far */
	int32 GetLatestPoseFrame() const { return LatestPoseFrame; }

	/** How many times the pose point posed this platform (tests and debug) */
	int32 GetPoseApplicationCount() const { return PoseApplicationCount; }

	/**
	 * Errors and warnings of the spec in its owner (shared by IsDataValid and the tests): the spec's own findings (traversal-
	 * surfaces.md 4.1) and a UGSTN_SurfaceBehaviourComponent on the owner, which would shadow every shape's surface. True if no error
	 */
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

	/** Collision of a new collider: the platform object channel and the responses derived from the shape's behaviour */
	virtual void ConfigureCollision(UPrimitiveComponent& Collider, const FGSTN_PlatformShape& Shape) const;

	/** Whether a shape's collider has collision at a frame */
	bool IsShapeSolidAt(int32 ShapeId, int32 Frame) const;

	/** Shapes the two inputs allow */
	uint8 ComputeDesiredMask() const;

	/** Feeds the inputs to the shape timelines, stamped with the next frame the pose point will simulate */
	void RequestShapeStates(EGSTN_PlatformShapeState DisableAs);

	void CreateColliders();
	void DestroyColliders();
	void PoseColliders(const FTransform& OwnerPose, int32 Frame);
	void DrawDebugShapes() const;

	/** Colliders by shape id (null where the spec has no shape) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> Colliders;

	FGSTN_PoseHistory PoseHistory;
	FGSTN_PlatformShapeStates ShapeStates;
	uint8 ShapeMaskInput = 0xFF;
	EGSTN_PlatformActionState ActionState = EGSTN_PlatformActionState::Idle;
	int32 CurrentPoseFrame = INDEX_NONE;
	int32 LatestPoseFrame = INDEX_NONE;
	int32 PoseApplicationCount = 0;
};

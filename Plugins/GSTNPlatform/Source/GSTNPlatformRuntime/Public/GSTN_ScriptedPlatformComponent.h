// Platform runtime: components, colliders, the pose point (Mover, no GAS, no direct Network Prediction code).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_EnemyPlatformComponent.h"
#include "GSTN_PlatformScriptedPath.h"
#include "GSTN_ScriptedPlatformComponent.generated.h"

/**
 * A platform on rails (many-enemies-replication.md 1.5 approach C): the enemy's pose is a function of the simulation time and of
 * an origin, so every machine poses the colliders identically for a given frame, a client predicting ahead included, and a ridden
 * shape never makes a prediction error of its own. Moves its owner's root along (bMoveOwner) so what is drawn follows the colliders.
 *
 * The origin is the owner's transform at BeginPlay on the authority, replicated once (initial only): the owner does not replicate
 * movement, each machine computes it. Until a client has the origin it poses at the owner's own transform.
 */
UCLASS(ClassGroup = (GSTN), Blueprintable, meta = (BlueprintSpawnableComponent, DisplayName = "GSTN Scripted Platform"))
class GSTNPLATFORMRUNTIME_API UGSTN_ScriptedPlatformComponent : public UGSTN_EnemyPlatformComponent
{
	GENERATED_BODY()

public:
	UGSTN_ScriptedPlatformComponent();

	/** The motion, in the origin's frame */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Platform|Path")
	FGSTN_PlatformScriptedPath Path;

	/** Move the owner's root to the pose too (the enemy itself is on rails, not only its colliders) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Platform|Path")
	bool bMoveOwner = true;

	virtual FTransform ComputeOwnerPose_Implementation(const FGSTN_PlatformPoseContext& Context) const override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Sets the origin of the path (the authority does it at BeginPlay; tests do it themselves) */
	void SetPathOrigin(const FTransform& Origin);

	bool HasPathOrigin() const { return bHasPathOrigin; }
	const FTransform& GetPathOrigin() const { return PathOrigin; }

protected:
	virtual void OnPoseApplied(const FGSTN_PlatformPoseContext& Context, const FTransform& OwnerPose) override;

	UPROPERTY(Replicated)
	FTransform PathOrigin;

	UPROPERTY(Replicated)
	bool bHasPathOrigin = false;
};

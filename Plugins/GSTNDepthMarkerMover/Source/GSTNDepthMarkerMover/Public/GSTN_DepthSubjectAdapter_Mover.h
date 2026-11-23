// Mover adapter of the depth marker. Presentation only: reads, never writes, the pawn.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_DepthSubject.h"
#include "GSTN_DepthSubjectAdapter_Mover.generated.h"

class UMoverComponent;

/**
 * A pawn moved by a UMoverComponent (SandboxCharacter_Mover and its children). Wins over the Actor adapter.
 *
 * Mover simulates the root at the fixed tick (62.5 Hz) and, with smoothing mode VisualComponentOffset, writes the PRIMARY VISUAL component
 * every frame at BaseVisualComponentTransform * smoothed root (UMoverComponent::FinalizeSmoothingFrame, Network Prediction's
 * OnWorldPreActorTick). The adapter reads the smoothed root back from that component (visual world * base^-1), so the marker sits under
 * the character the player sees, not under the simulated root that runs up to a tick apart from it.
 * Unlike the generic GetPresentedOffset it uses Mover's own visual component and base (no "first mesh" guess, no archetype rest place:
 * the per-move capsule changes the base at run time).
 */
UCLASS()
class GSTNDEPTHMARKERMOVER_API UGSTN_DepthSubjectAdapter_Mover : public UGSTN_DepthSubjectAdapter
{
	GENERATED_BODY()

public:
	UGSTN_DepthSubjectAdapter_Mover() { Priority = 200; }

	/** Speed along Up above which a pawn whose mover is not a character mover counts as airborne */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DepthMarker", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float AirborneSpeed = 5.f;

	virtual bool CanHandle(const AActor* Actor) const override;
	virtual bool ReadState(const AActor* Actor, FGSTN_DepthSubjectState& OutState) const override;

	/**
	 * The root transform the player sees: the visual's world transform with Mover's base visual transform taken off. Falls back to the
	 * simulated root when there is no separate visual, smoothing is not VisualComponentOffset or bFollowPresentedMesh is off.
	 */
	FTransform GetPresentedRoot(const UMoverComponent& Mover) const;

	/** Pure part, for tests: Visual = Base * Root, so Root = Base^-1 * Visual */
	static FTransform PresentedRootFromVisual(const FTransform& VisualWorld, const FTransform& BaseVisualRelative);
};

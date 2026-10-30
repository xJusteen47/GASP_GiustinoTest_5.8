// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Math/GSTN_DefenceViews.h"
#include "GSTN_SDIDrift.generated.h"

class UAbilitySystemComponent;
struct FGSTN_CombatSyncCore;
struct FGSTN_CombatTickContext;
struct FGSTN_DefenceEntityData;

/**
 * Held-direction SDI wired into the model (m6 spec 5.5, M6 F8). On every frame of a staggered victim's hitstop but the last, the tick extension
 * (AfterClocks) takes the frame's stick on the movement plane, asks UGSTN_SDIMath::NextDisplacement for the drift of that frame against the
 * distance already used (the core's Hit.SDIDistanceUsed, renamed from M4's reserved SDICount and reset by every staggering hit), adds the
 * frame's distance to it and gives Mover a one-frame drift freeze (UGSTN_GE_SDIDrift: a FGSTN_LayeredMove_Freeze with DriftVelocity, above the
 * hitstop freeze). The drift is a velocity Mover sweeps, so a wall or a ledge stops it; the accounted distance is the requested one (what the
 * rules cap), not the swept one.
 *
 * Not on the last frame of the window: the hitstop freeze finishes on the frame after it and hands back its velocity, and a drift move finishing
 * with it would fight it. Not for a hit whose HitFlags say SDIBlocked (a throw, armor, the stagger-in-place tier per the rules).
 *
 * Simulation code: the stick is the frame's command, the distance is sync state, the victim's tags are those of the end of the previous frame.
 */
struct GSTNDEFENCENP_API FGSTN_SDIDrift
{
	/** Fixed tick of the model, seconds (one drift move lasts one frame) */
	static constexpr float FrameSeconds = 0.016f;

	/** Whether Frame is a drift frame of Core's hitstop: a staggered victim (Hitstun or Tumble) inside the window, not on its last frame */
	static bool IsDriftFrame(const FGSTN_CombatSyncCore& Core, int32 Frame);

	/**
	 * The frame's drift for Core: NextDisplacement with the distance used so far (blocked by the fragment's HitFlags); writes the new distance
	 * into Core.Hit.SDIDistanceUsed. Zero velocity when nothing drifts
	 */
	static FGSTN_SDIStep Step(FGSTN_CombatSyncCore& Core, int32 Frame, const struct FGSTN_SDIRules& Rules, uint8 HitFlags, const FVector& StickOnPlane, const FGameplayTagContainer& VictimTags);

	/** The tick extension's AfterClocks part: Step on the entity, then the drift move */
	static void Run(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data);

	/** Queues the one-frame drift freeze on the entity's Mover through its ability system */
	static void ApplyDrift(UAbilitySystemComponent* AbilitySystem, const FVector& Velocity);
};

/**
 * Instant: a one-frame freeze that drifts with the SetByCaller velocity (Combat.SetByCaller.Move.Velocity.X/Y/Z) over the hitstop freeze it
 * rides on (UGSTN_FreezeMoveCreator with bDriftFromSetByCaller). The lifetime is Combat.SetByCaller.FreezeSeconds.
 */
UCLASS()
class GSTNDEFENCENP_API UGSTN_GE_SDIDrift : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_SDIDrift();
};

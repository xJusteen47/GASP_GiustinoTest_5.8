// Combat simulation (GAS-NPP fork): lag compensation of the attacker side (combat-m4-hits 9, M4 H10).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

class UGSTN_CombatRules;
class UGSTN_CombatWorldSubsystem;
class UNetworkPredictionWorldManager;
struct FGSTN_StepSnapshot;

/**
 * Lag compensation of the hits an attacker makes on the authority. A client attacks what it sees: the other pawns are interpolated proxies,
 * shown a little in the past. The authority simulates the attacker's frame against the present, so without this the hit would land on
 * the victim where it is now, not where the attacker aimed. The authority therefore resolves such an attacker against a per-attacker view
 * of the victims from the interpolation time the attacker's input carries, capped at the rules' MaxRewindMs:
 *
 *  - the transforms come from the fork's UNetworkPredictionLagCompensation (RewindActors / UnwindActors, kept for the instant the view is
 *    copied), or from the history ring of the world subsystem for an entity whose actor carries no such component;
 *  - the compact reaction state of the victim (damage percent, weight, hitstop, armor, intangible flags) comes from that ring, at the same time,
 *    so the hitstop the authority computes is the one the attacker's own client computed from what it showed.
 *
 * The view is an overlay: a copy of the shared step snapshot with the victims replaced, handed to this attacker alone. The shared snapshot
 * is never written, so no other attacker of the frame and no victim step sees a rewound value. The host's own pawn and AI never rewind
 * (they already hit what they show: the present).
 */
namespace GSTNLagComp
{
	/** True for the pawn of a remote player on the authority (a listen-server host and an AI pawn are authority with a simulated remote role) and a rules asset that asks for it */
	GSTNCOMBATNP_API bool ShouldRewind(ENetRole LocalRole, ENetRole RemoteRole, const UGSTN_CombatRules* Rules);

	/**
	 * The time to rewind to: RequestedMs (the interpolation time of the attacker's input) limited to the window [CurrentMs - MaxRewindMs, CurrentMs].
	 * A request in the future is the present; a request older than the window is the oldest the rules allow
	 */
	GSTNCOMBATNP_API float ClampRewindTime(float CurrentMs, float RequestedMs, int32 MaxRewindMs);

	/** Writes the rewound pose of an entity when something other than the ring knows it (the actor after RewindActors); false falls back to the ring */
	using FPoseProvider = TFunctionRef<bool(int32 EntityId, FTransform& OutPose)>;

	/**
	 * Fills Out with Source seen at RewindMs: every entry but the attacker's own gets the ring's reaction state, velocity and hitstop flag at that
	 * time, and the provider's pose (else the ring's blended pose). An entity with no history stays as it is in Source. Source is not modified
	 */
	GSTNCOMBATNP_API void BuildOverlay(const FGSTN_StepSnapshot& Source, int32 AttackerId, float RewindMs, const UGSTN_CombatWorldSubsystem& World, FPoseProvider Pose, FGSTN_StepSnapshot& Out);
}

/**
 * Brackets a RewindActors / UnwindActors pair, so an early return cannot leave the pawns of other players in the past. Nothing outside the
 * scope sees a rewound actor: it exists to copy the poses into the overlay.
 */
class GSTNCOMBATNP_API FGSTN_LagCompRewindScope
{
public:
	FGSTN_LagCompRewindScope(UNetworkPredictionWorldManager* InManager, AActor* Requester, float RewindMs);
	~FGSTN_LagCompRewindScope();

	FGSTN_LagCompRewindScope(const FGSTN_LagCompRewindScope&) = delete;
	FGSTN_LagCompRewindScope& operator=(const FGSTN_LagCompRewindScope&) = delete;

	/** At least one actor was moved back */
	bool DidRewind() const { return bRewound; }

private:
	UNetworkPredictionWorldManager* Manager = nullptr;
	bool bRewound = false;
};

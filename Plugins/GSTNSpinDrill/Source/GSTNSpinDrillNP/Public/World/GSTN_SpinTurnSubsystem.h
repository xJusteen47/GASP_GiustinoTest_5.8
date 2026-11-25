// Turns the world's IGSTN_SpinTurnable actors from the authoritative spin and drill events (spin-drill.md 13c).

#pragma once

#include "CoreMinimal.h"
#include "Events/GSTN_SimEventSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "World/GSTN_SpinTurnable.h"

#include "GSTN_SpinTurnSubsystem.generated.h"

struct FGSTN_SimEvent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FGSTN_OnSpinTurnDelegate, AActor*, Turnable, AActor*, Turner, int32, Sense, EGSTN_SpinTurnSource, Source);

/**
 * The bridge between the spin simulation and level objects. It listens to Combat.Event.Spin.Started and Combat.Event.Drill.GroundLag, which only
 * the authority publishes and publishes once (the combat outbox, never on a re-simulated frame), and calls IGSTN_SpinTurnable::OnSpinTurned on:
 *  - spin: every turnable whose collision overlaps a sphere of SpinReachCm around the spinner;
 *  - drill: every turnable overlapping a sphere of DrillReachCm at the driller's feet (the dive's ground lag starts on whatever it stood on).
 * Nothing here is simulation state: a client never runs it, the object replicates what it does with the turn.
 */
UCLASS(meta = (DisplayName = "GSTN Spin Turn Subsystem"))
class GSTNSPINDRILLNP_API UGSTN_SpinTurnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UGSTN_SpinTurnSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Turns every turnable in reach of Turner as a spin or a drill would (authority only; does nothing on a client). Public so a level
	 * script, a test or another ability (a spinning attack of a kit) can turn things through the same rule. Returns how many it turned
	 */
	UFUNCTION(BlueprintCallable, Category = "SpinDrill|Turnable")
	int32 TurnInReach(AActor* Turner, int32 Sense, EGSTN_SpinTurnSource Source);

	/** Reach of a spin, from the spinner's centre. Default a little past the spin ring (FGSTN_SpinTimelineParams::RingRadius 110) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Turnable", meta = (ClampMin = "0", ForceUnits = "cm"))
	float SpinReachCm = 130.f;

	/** Reach of a drill dive, from the driller's feet: what it landed on */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Turnable", meta = (ClampMin = "0", ForceUnits = "cm"))
	float DrillReachCm = 60.f;

	/** Every turn this subsystem hands out (authority), for level scripts and debug */
	UPROPERTY(BlueprintAssignable, Category = "SpinDrill|Turnable")
	FGSTN_OnSpinTurnDelegate OnTurned;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleSimEvent(const FGSTN_SimEvent& Event);

	FGSTN_SimEventListenerHandle EventListener;
};

// The last hits of every pawn, for the combat Gameplay Debugger category (combat-m5-editor.md 10.2, plan E15).
#pragma once

#include "CoreMinimal.h"
#include "Events/GSTN_SimEventSubsystem.h"
#include "GameplayDebugger/GSTN_CombatDebugText.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_CombatDebugHitRecorder.generated.h"

/**
 * Keeps a ring of the last hits dealt and received per pawn, fed by the HitLanded and HitReceived outbox events of
 * UGSTN_SimEventSubsystem. Those are published once on the authority, so the rings exist where the category collects (the host of
 * a listen server, the dedicated server) and are empty on clients, which get the lines through the category's data pack.
 *
 * Never reads or writes the sync state: a hit that a correction removes was still published once by the authority, which is the
 * truth the ring shows. gstn.Combat.Debug.HitRing 0 stops recording (the rings keep what they have).
 */
UCLASS()
class UGSTN_CombatDebugHitRecorder : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UGSTN_CombatDebugHitRecorder* Get(const UObject* WorldContextObject);

	/** Null when the actor has no hit yet */
	const FGSTN_CombatHitRing* FindRing(const AActor* Actor) const { return Rings.Find(Actor); }

	/** Adds one outbox event to the rings of the pawns it concerns (both sides). Public for the tests */
	void Record(const FGSTN_SimEvent& Event);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TMap<TWeakObjectPtr<const AActor>, FGSTN_CombatHitRing> Rings;
	FGSTN_SimEventListenerHandle Listener;
};

// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"
#include "GSTN_LinkedModifier.generated.h"

class AActor;
class UMoverComponent;

/**
 * The Mover half of a parked link role (m6 spec 7.3): an endless modifier that carries Combat.Mover.Linked, which Mover reads from its sync
 * state (UMoverComponent::HasGameplayTagInState), so UGSTN_LinkedModeTransition moves the entity into UGSTN_LinkedMovementMode on the tick it
 * appears and out of it on the tick it is gone. It holds no data: being in the sync state is the whole meaning, so a rollback restores it
 * with the rest of Mover.
 *
 * Queued ONCE, by the link tick extension on the frame the entity's Body role becomes a parked role (the edge of the fragment, the same frame
 * on every machine and in a re-simulation), and cancelled by tag on the frame it stops being one. The combat tick runs before Mover in the
 * fixed tick, so the modifier is in the state Mover simulates that same frame (gas-npp.md rule 5). An edge that a re-simulation does not
 * repeat (the re-simulation starts after it) has nothing to queue: the restored Mover state already holds the modifier.
 */
USTRUCT()
struct GSTNLINKNP_API FGSTN_LinkedModifier : public FMovementModifierBase
{
	GENERATED_BODY()

	FGSTN_LinkedModifier();

	virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const override;
	virtual void GetGameplayTags(FGameplayTagContainer& InOutTags) const override;
	virtual FMovementModifierBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;

	/** Queues the modifier on Owner's Mover. False when the owner has no Mover or Mover's simulation does not exist yet */
	static bool Queue(const AActor* Owner);

	/** Ends the modifier on Owner's Mover (cancel by Combat.Mover.Linked); nothing for an owner without one */
	static void Cancel(const AActor* Owner);
};

template<>
struct TStructOpsTypeTraits<FGSTN_LinkedModifier> : public TStructOpsTypeTraitsBase2<FGSTN_LinkedModifier>
{
	enum
	{
		WithCopy = true
	};
};

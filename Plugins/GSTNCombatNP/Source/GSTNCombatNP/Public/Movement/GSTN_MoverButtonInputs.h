// Combat simulation (GAS-NPP fork): logical buttons Mover modes need, carried in the Mover command (rollback plan F1, single input channel).

#pragma once

#include "CoreMinimal.h"
#include "MoverTypes.h"
#include "GSTN_MoverButtonInputs.generated.h"

/**
 * The few logical buttons that Mover movement modes read (wall jump, wall detach), taken from the combat input command on the machine that
 * produces it (UGSTN_CombatSimComponent::ProduceInput_Implementation) and carried in the Mover command, so a mode reads them from the
 * predicted, replayed input and never from a component or from GASP's own jump and crouch intents (which have no key in the combat layout,
 * and whose crouch intent is not part of the predicted input). Same buttons as the combat command, so the two cannot disagree: one source,
 * two carriers, until the Mover command and the combat command are one (single channel, docs/notes/input-azioni-mappa.md P2).
 *
 * Which logical button each flag is comes from the button layout functions "Wall.Jump" and "Wall.Detach" (defaults Jump and Crouch).
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_MoverButtonInputs : public FMoverDataStructBase
{
	GENERATED_BODY()

	enum EFlags : uint8
	{
		JumpPressed = 1 << 0,
		JumpHeld = 1 << 1,
		DetachHeld = 1 << 2,
	};

	/** EFlags of this frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Buttons")
	uint8 Flags = 0;

	bool Has(EFlags Flag) const { return (Flags & Flag) != 0; }

	virtual FMoverDataStructBase* Clone() const override { return new FGSTN_MoverButtonInputs(*this); }
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;

	// Required on input entries (NP smoothing interpolates the input collection). Buttons are not blended: the nearer frame wins, and a
	// press of a dropped frame is kept by Merge
	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override
	{
		return Flags != static_cast<const FGSTN_MoverButtonInputs&>(AuthorityState).Flags;
	}
	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override
	{
		Flags = static_cast<const FGSTN_MoverButtonInputs&>(Pct < 0.5f ? From : To).Flags;
	}
	virtual void Merge(const FMoverDataStructBase& From) override
	{
		Flags |= static_cast<const FGSTN_MoverButtonInputs&>(From).Flags & JumpPressed;
	}
};

template<>
struct TStructOpsTypeTraits<FGSTN_MoverButtonInputs> : public TStructOpsTypeTraitsBase2<FGSTN_MoverButtonInputs>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

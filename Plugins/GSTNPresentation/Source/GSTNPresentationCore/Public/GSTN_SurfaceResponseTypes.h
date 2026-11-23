// Surface audio and haptics: one table, keyed by event x footwear x floor, drives the footstep sound and the pad rumble
// (docs/notes/surface-feedback.md; research docs/notes/research/surface-audio.md and haptics.md). No GAS, Network Prediction or Mover.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Chaos/ChaosEngineInterface.h"
#include "GameplayTagContainer.h"
#include "GSTN_PresentationTypes.h"
#include "GSTN_SurfaceResponseTypes.generated.h"

class UForceFeedbackEffect;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/** Which pad motors a dynamic rumble drives */
UENUM(BlueprintType)
enum class EGSTN_HapticMotors : uint8
{
	/** Large and small motors */
	Both,
	/** Large (low, heavy) motors only: a thump */
	Large,
	/** Small (high, light) motors only: a tick */
	Small
};

/** The foot of a foley event (the stock GASP notify carries one); None = both or not a foot */
UENUM(BlueprintType)
enum class EGSTN_FoleySide : uint8
{
	None,
	Left,
	Right
};

/**
 * One row of the surface response table: what an event sounds and feels like for one footwear on one floor. Every part is optional.
 *
 * Sound: one MetaSound per event family that mixes two layers (owner decision 2026-11-21): the footwear layer (the contact) picked by the
 * int input Footwear, the floor layer (the texture) picked by the int input Floor. The row's numbers go to the MetaSound inputs listed in
 * docs/notes/surface-feedback.md, so N footwear + M floors need N + M sample sets, not N x M rows.
 *
 * Rumble: a short dynamic pulse (intensity, duration, motors: works with no asset) or a designed Force Feedback Effect asset (its curves;
 * the intensity scales only the dynamic pulse). Only the pad of the local player who controls the character rumbles.
 */
USTRUCT(BlueprintType)
struct GSTNPRESENTATIONCORE_API FGSTN_SurfaceResponseRow
{
	GENERATED_BODY()

	/** Foley.Event.* (stock GASP notifies: Run, Walk, Land, Scuff...) or Presentation.Event.* (Land, Hit, Shield.Hit...). A parent tag
	 *  answers every child without a row of its own */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row")
	FGameplayTag EventTag;

	/** Empty = any footwear */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row", meta = (Categories = "Presentation.Footwear"))
	FGameplayTag Footwear;

	/** On = any floor; off = only Floor (the surface type of the Physical Material under the foot) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row")
	bool bAnyFloor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row", meta = (EditCondition = "!bAnyFloor"))
	TEnumAsByte<EPhysicalSurface> Floor = SurfaceType_Default;

	/** Below this magnitude nothing plays (speed cm/s for foley events; the presentation event's unit otherwise) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row")
	float MinMagnitude = 0.f;

	/** Magnitude -> intensity (default: always 1). Intensity scales the volume, the MetaSound input Intensity and the dynamic rumble */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row")
	FGSTN_PresentationScale Scale;

	// Sound

	/** The layered MetaSound (or any sound). None = this row plays no sound and the stock GASP foley plays as before */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row|Sound")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row|Sound", meta = (ClampMin = "0"))
	float Volume = 1.f;

	/** Random pitch in this range on every play (1,1 = none) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row|Sound")
	FVector2D PitchRange = FVector2D(1.0, 1.0);

	/** Multiply the volume by the intensity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row|Sound")
	bool bScaleVolume = true;

	/** MetaSound input FootwearGain: the contact layer's level */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row|Sound", meta = (ClampMin = "0"))
	float FootwearLayerGain = 1.f;

	/** MetaSound input FloorGain: the texture layer's level */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row|Sound", meta = (ClampMin = "0"))
	float FloorLayerGain = 1.f;

	/** None = the sound's own */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row|Sound")
	TObjectPtr<USoundAttenuation> Attenuation;

	/** Voice cap (a brawler with many characters must not stack steps). None = the sound's own */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row|Sound")
	TObjectPtr<USoundConcurrency> Concurrency;

	// Rumble

	/** Dynamic pulse strength at intensity 1 (0..1 of the motor). 0 = no dynamic pulse */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row|Haptics", meta = (ClampMin = "0", ClampMax = "1"))
	float HapticIntensity = 0.f;

	/** Dynamic pulse length (short: ~0.06 s for a step; follow the hitstop for a hit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row|Haptics", meta = (ClampMin = "0", ForceUnits = "s"))
	float HapticDuration = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row|Haptics")
	EGSTN_HapticMotors HapticMotors = EGSTN_HapticMotors::Small;

	/** Left foot = left motors, right foot = right motors (events with a side) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row|Haptics")
	bool bHapticFollowsFoot = true;

	/** Multiply the dynamic pulse by the intensity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row|Haptics")
	bool bScaleHaptic = true;

	/** A designed rumble (curves per motor, per-device overrides). Played as is, on top of the dynamic pulse */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Row|Haptics")
	TObjectPtr<UForceFeedbackEffect> HapticEffect;

	bool HasSound() const { return Sound != nullptr; }
	bool HasHaptic() const { return HapticIntensity > 0.f || HapticEffect != nullptr; }
};

/**
 * The surface response of a character set (data, never code). Lookup of (event, footwear, floor): the row whose event tag is the event's
 * or its nearest ancestor wins; among those, a row naming the footwear beats "any footwear", then a row naming the floor beats "any floor";
 * a tie goes to the first row. No row here = Parent's answer. No row anywhere = stock GASP sound, no rumble.
 */
UCLASS(BlueprintType)
class GSTNPRESENTATIONCORE_API UGSTN_SurfaceResponseTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface Response", meta = (TitleProperty = "EventTag"))
	TArray<FGSTN_SurfaceResponseRow> Rows;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface Response")
	TObjectPtr<UGSTN_SurfaceResponseTable> Parent;

	/**
	 * The int sent to the MetaSound input Footwear: the footwear's index in this list. Empty = Clay 0, Metal 1, Barefoot 2, Shoe sole 3
	 * (see GetFootwearParam). A footwear missing from the list sends 0 (Clay)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface Response", meta = (Categories = "Presentation.Footwear"))
	TArray<FGameplayTag> FootwearParamOrder;

	/** Pure. Null when neither this table nor its parents answer. A parent chain longer than 16 (a loop) stops there */
	const FGSTN_SurfaceResponseRow* FindRow(const FGameplayTag& EventTag, const FGameplayTag& Footwear, EPhysicalSurface Floor) const;

	/** Pure: does any row of this table or its parents for EventTag name a floor (else the floor trace can be skipped) */
	bool NeedsFloor(const FGameplayTag& EventTag) const;

	/** Pure: the MetaSound Footwear int of a footwear tag */
	UFUNCTION(BlueprintPure, Category = "Surface Response")
	int32 GetFootwearParam(FGameplayTag Footwear) const;

	/** Blueprint face of FindRow */
	UFUNCTION(BlueprintPure, Category = "Surface Response", meta = (DisplayName = "Find Surface Response Row"))
	bool FindRowBP(FGameplayTag EventTag, FGameplayTag Footwear, TEnumAsByte<EPhysicalSurface> Floor, FGSTN_SurfaceResponseRow& OutRow) const;

	/** The default footwear order (Clay, Metal, Barefoot, ShoeSole): the index is the MetaSound int and the debug override's value */
	static const TArray<FGameplayTag>& DefaultFootwearOrder();

	/** Pure score of one row for a query; < 0 = the row does not answer. Exposed for the tests */
	static int32 ScoreRow(const FGSTN_SurfaceResponseRow& Row, const FGameplayTag& EventTag, const FGameplayTag& Footwear, EPhysicalSurface Floor);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

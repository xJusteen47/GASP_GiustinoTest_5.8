// Presentation events and their effects (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GSTN_PresentationTypes.generated.h"

class UAnimSequenceBase;
class UCameraShakeBase;
class UNiagaraSystem;
class USoundBase;

/**
 * One presentation event: something the player should see or hear (a jump, a landing, a hit), derived from the synchronized state on
 * the machine that shows it. Frame is the simulation frame the state carried (INDEX_NONE when the source has none): it is the key of the
 * rollback dedupe, so the same event at the same frame never plays twice after a resimulation.
 */
USTRUCT(BlueprintType)
struct GSTNPRESENTATIONCORE_API FGSTN_PresentationEvent
{
	GENERATED_BODY()

	/** Presentation.Event.* (the most specific leaf the source knows: Jump.Air, Skid.Enter...) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation", meta = (Categories = "Presentation.Event"))
	FGameplayTag EventTag;

	/** Simulation frame of the state the event was read from; INDEX_NONE = unknown (no dedupe) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	int32 Frame = INDEX_NONE;

	/** What the table's scale reads: the unit depends on the event (Presentation.Event.* tag comments: cm/s, frames, stage) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	float Magnitude = 0.f;

	/** World location; zero = the owner's location when it plays */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	FVector Location = FVector::ZeroVector;

	/** World direction (unit or zero): the motion, the hit's push, the bounce's surface normal */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	FVector Direction = FVector::ZeroVector;

	/** Frame was stamped by the simulation (a confirmed hit, a move's start): the dedupe matches that frame only, never its neighbours */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	bool bExactFrame = false;
};

/**
 * Turns an event's magnitude into an intensity: the input range maps linearly (then by Exponent) onto the output range. With the default
 * output 1..1 every event plays at intensity 1. Intensity scales the Niagara system (size and its float parameter), the sound's volume, the
 * shake and the additive's weight, each where the row says so.
 */
USTRUCT(BlueprintType)
struct GSTNPRESENTATIONCORE_API FGSTN_PresentationScale
{
	GENERATED_BODY()

	/** Magnitude at the bottom and at the top of the range (unit of the event) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale")
	FVector2D InputRange = FVector2D(0.0, 1.0);

	/** Intensity at the bottom and at the top */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale")
	FVector2D OutputRange = FVector2D(1.0, 1.0);

	/** 1 = linear; above 1 the strong end grows faster (a heavy landing stands out), below 1 the weak end does */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale", meta = (ClampMin = "0.05"))
	float Exponent = 1.f;

	/** Off = magnitudes outside the input range extrapolate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scale")
	bool bClampInput = true;

	/** Pure */
	float Evaluate(float Magnitude) const;
};

/** Who feels a camera shake */
UENUM(BlueprintType)
enum class EGSTN_PresentationShakeScope : uint8
{
	/** Only the local players whose camera looks through this character */
	ViewTarget,
	/** Every local camera near the event (inner/outer radius) */
	World
};

/** What one event plays, as a row of a presentation table. Every effect is optional; an empty row only fires the Blueprint events */
USTRUCT(BlueprintType)
struct GSTNPRESENTATIONCORE_API FGSTN_PresentationCue
{
	GENERATED_BODY()

	/** The event it answers; a parent tag answers every child that has no row of its own */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue", meta = (Categories = "Presentation.Event"))
	FGameplayTag EventTag;

	/** Below this magnitude nothing plays (a soft landing makes no dust) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue")
	float MinMagnitude = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue")
	FGSTN_PresentationScale Scale;

	// Niagara

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue|Niagara")
	TObjectPtr<UNiagaraSystem> Niagara;

	/** None = spawned in the world at the event; else attached to this socket or bone of the owner's mesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue|Niagara")
	FName AttachSocket;

	/** Offset from the spawn point (relative to the socket when attached, world otherwise) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue|Niagara")
	FVector Offset = FVector::ZeroVector;

	/** World spawn only: X of the system along the event's direction (a skid trail, a bounce along the surface normal) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue|Niagara")
	bool bOrientToDirection = false;

	/** Multiply the system's scale by the intensity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue|Niagara")
	bool bScaleSize = true;

	/** Float user parameter set to the intensity (None = not set) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue|Niagara")
	FName IntensityParameter = TEXT("User.Intensity");

	// Sound

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue|Sound")
	TObjectPtr<USoundBase> Sound;

	/** Multiply the volume by the intensity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue|Sound")
	bool bScaleVolume = true;

	// Camera shake

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue|Camera Shake")
	TSubclassOf<UCameraShakeBase> CameraShake;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue|Camera Shake")
	EGSTN_PresentationShakeScope ShakeScope = EGSTN_PresentationShakeScope::ViewTarget;

	/** World scope: full shake inside, none beyond the outer radius (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue|Camera Shake", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ShakeInnerRadius = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue|Camera Shake", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ShakeOuterRadius = 1500.f;

	// Short additive (through the directional additive node of the main anim graph, GSTNAnimation)

	/** Additive clip played once on the torso; null = none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue|Additive")
	TObjectPtr<UAnimSequenceBase> Additive;

	/** Weight at intensity 1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue|Additive", meta = (ClampMin = "0", ClampMax = "1"))
	float AdditiveWeight = 1.f;

	/** Multiply the weight by the intensity (clamped to 1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue|Additive")
	bool bScaleAdditive = true;

	bool HasEffect() const { return Niagara != nullptr || Sound != nullptr || CameraShake != nullptr || Additive != nullptr; }
};

/**
 * The effects of the presentation events for one kit or one skin (data, never code: an effect is swapped by editing a row). A table
 * may fall back to a Parent (a skin's table lists only what it changes and names the kit's as its parent; the kit's names a project default).
 * Lookup: the row whose tag is the event's or its nearest ancestor, in this table; none = the parent's answer.
 */
UCLASS(BlueprintType)
class GSTNPRESENTATIONCORE_API UGSTN_PresentationTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (TitleProperty = "EventTag"))
	TArray<FGSTN_PresentationCue> Cues;

	/** Asked when no row here answers the event */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<UGSTN_PresentationTable> Parent;

	/** Pure. Null when neither this table nor its parents answer EventTag. A parent chain longer than 16 (a loop) stops there */
	const FGSTN_PresentationCue* FindCue(const FGameplayTag& EventTag) const;

	/** Blueprint face of FindCue */
	UFUNCTION(BlueprintPure, Category = "Presentation", meta = (DisplayName = "Find Presentation Cue"))
	bool FindCueBP(FGameplayTag EventTag, FGSTN_PresentationCue& OutCue) const;

	/** The row of this table alone (no parent) that answers EventTag: exact tag first, then the nearest ancestor. Pure */
	const FGSTN_PresentationCue* FindOwnCue(const FGameplayTag& EventTag) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

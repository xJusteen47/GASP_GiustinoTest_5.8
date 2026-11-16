// Combat presentation (GAS-NPP fork): what the run skid and the attack slides show (docs/notes/corsa-mario-piano.md, P2). Never read by the simulation.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSTN_AdditiveReactionSource.h"
#include "Movement/GSTN_RunSkidTypes.h"
#include "GSTN_RunPresentationComponent.generated.h"

class UMoverComponent;

/** Which sliding move the feet slide in */
UENUM(BlueprintType)
enum class EGSTN_FootSlideSource : uint8
{
	None,
	RunSkid,
	/** A ground attack's momentum slide (FGSTN_LayeredMove_MomentumSlide) */
	AttackSlide
};

/**
 * The foot slide hook (plan: "solo l'hook, niente VFX"): the feet slide on the ground in a skid or an attack slide. Decals, dust and sound
 * hang on it from Blueprint (OnFootSlideChanged + GetFootSlide each frame); the anim skid layer (L3) will read the phase.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_FootSlideState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Foot Slide")
	EGSTN_FootSlideSource Source = EGSTN_FootSlideSource::None;

	/** The skid's phase (None for an attack slide) */
	UPROPERTY(BlueprintReadOnly, Category = "Foot Slide")
	EGSTN_RunSkidPhase SkidPhase = EGSTN_RunSkidPhase::None;

	/** Sliding speed on the ground, cm/s (0 in the skid's exit) */
	UPROPERTY(BlueprintReadOnly, Category = "Foot Slide")
	float Speed = 0.f;

	/** Unit sliding direction, world */
	UPROPERTY(BlueprintReadOnly, Category = "Foot Slide")
	FVector Direction = FVector::ZeroVector;

	/** Live frames since the slide started */
	UPROPERTY(BlueprintReadOnly, Category = "Foot Slide")
	int32 Frame = 0;

	/** Skid only: frames from the start to rest, and the distance left to the stop (the anim layer's distance matching) */
	UPROPERTY(BlueprintReadOnly, Category = "Foot Slide")
	int32 RestFrame = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Foot Slide")
	float DistanceToStop = 0.f;

	bool IsSliding() const { return Source != EGSTN_FootSlideSource::None; }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSTN_OnFootSlideChanged, bool, bSliding, const FGSTN_FootSlideState&, State);

/**
 * Reads the presented Mover state (game thread, after the movement of the frame) and turns the sliding moves into presentation:
 *   - the foot slide hook above;
 *   - the PLACEHOLDER skid pose until the real clips come with the anim layer plan (anim-layer-skid-wallslide-piano L0/L3): the torso leans
 *     back against the slide through the directional additive node already in the main graph (IGSTN_AdditiveReactionSource), by the
 *     kit's FGSTN_RunSkidTuning::PlaceholderLeanDeg. No Blueprint wiring: UGSTN_CombatSimComponent creates this component at BeginPlay.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class GSTNCOMBATNP_API UGSTN_RunPresentationComponent : public UActorComponent, public IGSTN_AdditiveReactionSource
{
	GENERATED_BODY()

public:
	UGSTN_RunPresentationComponent();

	/** Adds the component to Owner once (game thread, outside the simulation) */
	static UGSTN_RunPresentationComponent* FindOrAdd(AActor* Owner);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The foot slide of the presented frame */
	UFUNCTION(BlueprintPure, Category = "Combat|Run")
	const FGSTN_FootSlideState& GetFootSlide() const { return FootSlide; }

	/** A slide started (true) or ended (false) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Run")
	FGSTN_OnFootSlideChanged OnFootSlideChanged;

	/** Off = no placeholder lean (the real skid layer replaces it) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Run")
	bool bPlaceholderSkidLean = true;

	virtual void GetAdditiveReaction(FGSTN_AdditiveReactionRequest& Out) const override;

	/** The foot slide of a Mover sync state (pure but for the kit lookup; the tests call it) */
	static FGSTN_FootSlideState ReadFootSlide(const struct FMoverSyncState& SyncState, float StepSeconds);

private:
	UPROPERTY(Transient)
	TObjectPtr<UMoverComponent> Mover;

	FGSTN_FootSlideState FootSlide;
	float LeanDeg = 0.f;
	int32 EnterFrames = 0;
	int32 ExitFrames = 0;
};

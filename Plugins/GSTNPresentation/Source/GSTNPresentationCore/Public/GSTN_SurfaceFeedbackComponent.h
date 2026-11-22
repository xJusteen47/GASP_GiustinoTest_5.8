// Surface audio and haptics (docs/notes/surface-feedback.md). No GAS, Network Prediction or Mover.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "GSTN_SurfaceResponseTypes.h"
#include "GSTN_SurfaceFeedbackComponent.generated.h"

class APlayerController;
class UAudioComponent;
struct FGSTN_PresentationEvent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FGSTN_OnSurfaceResponse, FGameplayTag, EventTag, FGameplayTag, Footwear,
	TEnumAsByte<EPhysicalSurface>, Floor, float, Intensity, bool, bPlayedSound);

/**
 * What a character's steps (and its presentation events) sound and feel like on the floor under them: it resolves the floor (one line
 * trace, Physical Material -> Surface Type), takes the footwear of the skin, asks the surface response table and plays the row's layered
 * sound and the local player's rumble.
 *
 * Two inputs:
 * - Foley events: the stock GASP foley notify (BP_AnimNotify_FoleyEvent, Foley.Event.*) calls HandleFoleyEvent (or the static
 *   HandleFoleyEventOn) before it plays its own sound, and skips its own sound when this returns true. No row, or a row with no sound,
 *   returns false: the stock sound plays exactly as before (Clay = stock, project rule 14; divergence row in docs/notes/gasp-divergenze.md).
 * - Presentation events: UGSTN_PresentationComponent forwards every accepted event (Land, Hit, Shield.Hit...), so a hit rumbles from the
 *   same table, on the 62.5 Hz simulation event and after its rollback dedupe.
 *
 * Presentation only, per machine, nothing replicated: no sound and no rumble on a dedicated server; the rumble only on the pad of the local
 * player that controls this character. Switches: gstn.Audio.* and gstn.Haptics.* (GSTN Debug Options, one control each).
 */
UCLASS(ClassGroup = (GSTN), Config = Game, meta = (BlueprintSpawnableComponent))
class GSTNPRESENTATIONCORE_API UGSTN_SurfaceFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_SurfaceFeedbackComponent();

	/** Adds the component to Owner once (game thread) */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Surface Feedback", meta = (DefaultToSelf = "Owner"))
	static UGSTN_SurfaceFeedbackComponent* FindOrAdd(AActor* Owner);

	/**
	 * The hook for the stock foley notify, callable on any actor (adds the component on first use). True = a row played its sound: skip the
	 * stock sound. False = play the stock sound as before
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Surface Feedback", meta = (DefaultToSelf = "Owner", Categories = "Foley.Event"))
	static bool HandleFoleyEventOn(AActor* Owner, FGameplayTag FoleyEvent, FVector Location, EGSTN_FoleySide Side = EGSTN_FoleySide::None);

	/** See HandleFoleyEventOn. Magnitude = the owner's horizontal speed (cm/s). Location zero = the owner's feet */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Surface Feedback", meta = (Categories = "Foley.Event"))
	bool HandleFoleyEvent(FGameplayTag FoleyEvent, FVector Location, EGSTN_FoleySide Side = EGSTN_FoleySide::None);

	/** Any event with its own magnitude (the row's unit). True = a sound played */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Surface Feedback")
	bool PlaySurfaceResponse(FGameplayTag EventTag, FVector Location, float Magnitude, EGSTN_FoleySide Side = EGSTN_FoleySide::None);

	/** Called by UGSTN_PresentationComponent for every accepted event (location, magnitude and frame already resolved) */
	void HandlePresentationEvent(const FGSTN_PresentationEvent& Event);

	/** The skin's footwear (UGSTN_SkinComponent sets it on apply; empty = Clay, the stock sound) */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Surface Feedback")
	void SetSkinFootwear(UPARAM(meta = (Categories = "Presentation.Footwear")) FGameplayTag Footwear);

	/** The footwear in use: gstn.Audio.FootwearOverride, else FootwearOverride, else the skin's, else Clay */
	UFUNCTION(BlueprintPure, Category = "GSTN|Surface Feedback")
	FGameplayTag GetFootwear() const;

	/** The surface type under Location (one trace TraceUp above to TraceDown below). Default when nothing is hit */
	UFUNCTION(BlueprintPure, Category = "GSTN|Surface Feedback")
	TEnumAsByte<EPhysicalSurface> GetFloorAt(FVector Location) const;

	/** The table asked first; null = ProjectDefaultTable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Feedback")
	TObjectPtr<UGSTN_SurfaceResponseTable> Table;

	/**
	 * The project's table, from DefaultGame.ini ([/Script/GSTNPresentationCore.GSTN_SurfaceFeedbackComponent] ProjectDefaultTable=...):
	 * a component added at run time has no Table of its own
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Surface Feedback")
	TSoftObjectPtr<UGSTN_SurfaceResponseTable> ProjectDefaultTable;

	/** Per character (a Blueprint or a kit): beats the skin's footwear. Empty = the skin's */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Feedback", meta = (Categories = "Presentation.Footwear"))
	FGameplayTag FootwearOverride;

	/** MetaSound input Weight (0 light .. 1 heavy): the Overlord is heavier than the agile base */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Feedback", meta = (ClampMin = "0", ClampMax = "1"))
	float Weight = 0.5f;

	/** Floor trace: from this far above the event point... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Feedback|Floor", meta = (ClampMin = "0", ForceUnits = "cm"))
	float TraceUp = 30.f;

	/** ...to this far below it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Feedback|Floor", meta = (ClampMin = "1", ForceUnits = "cm"))
	float TraceDown = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Feedback|Floor")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/**
	 * The same foot within this time plays once (motion matching blends fire the same notify on adjacent poses: Unreal Assistant, and the
	 * double foley at the run start hand-off, runstart-sprinter-s2). Per side; 0 = off
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Feedback", meta = (ClampMin = "0", ForceUnits = "s"))
	float MinFootInterval = 0.12f;

	/** Fires for every event that found a row (also on a dedicated server, where nothing plays) */
	UPROPERTY(BlueprintAssignable, Category = "Surface Feedback")
	FGSTN_OnSurfaceResponse OnSurfaceResponse;

	virtual void BeginPlay() override;

	/** Pure: gstn.Haptics.Intensity level (0 off, 1 low, 2 normal, 3 high) -> scale */
	static float HapticLevelScale(int32 Level);

private:
	const UGSTN_SurfaceResponseTable* GetTable() const;
	bool PlayRow(const FGSTN_SurfaceResponseRow& Row, const UGSTN_SurfaceResponseTable& Asked, const FGameplayTag& Footwear,
		EPhysicalSurface Floor, const FVector& Location, float Intensity, float Magnitude, EGSTN_FoleySide Side);
	void PlayHaptic(const FGSTN_SurfaceResponseRow& Row, float Intensity, EGSTN_FoleySide Side) const;
	APlayerController* GetLocalController() const;

	UPROPERTY(Transient)
	FGameplayTag SkinFootwear;

	/** Last play time per EGSTN_FoleySide (MinFootInterval) */
	double LastFootTime[3] = {-1.0, -1.0, -1.0};
};

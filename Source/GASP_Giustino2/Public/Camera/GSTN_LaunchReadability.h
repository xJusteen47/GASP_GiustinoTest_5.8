// Launch readability (owner 2026-11-24, decision 3A): the local camera backs off for a while to keep a launch it caused in frame, and an arrow on
// the screen edge points at players out of frame. Presentation only: read from the presented state of this machine, never by the simulation.
// The third part, the launch puff and tumble trail, lives with the other cues in GSTNPresentation (UGSTN_PresentationComponent::LaunchEffectClass).
// Each part has its own switch (owner: "make them switchable"): gstn.Launch.Debug.Zoom, gstn.Launch.Debug.Arrow, gstn.Launch.Debug.Effects.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/DeveloperSettings.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_LaunchReadability.generated.h"

struct FMinimalViewInfo;

/**
 * The launch zoom. Which launches move the camera, in order of the owner's rule ("the effect grows with how much the launch matters, not with
 * how many launches there are"): only launches this machine's viewed character caused; a player or an important enemy only when it tumbles;
 * a small enemy only when it flies fast enough to be leaving the level; and only when the flight is about to leave the frame. One launch at a
 * time (the fastest), held for HoldSeconds, then the camera eases back
 */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_LaunchZoomTuning
{
	GENERATED_BODY()

	/** How far ahead along its velocity the flight is looked at, so the camera starts backing off before the victim leaves the frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch|Zoom", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float LookaheadSeconds = 0.35f;

	/** The part of the half screen the flight must stay inside (0.85 = a 15 % border kept clear) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch|Zoom", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float ScreenMargin = 0.85f;

	/** How long the camera keeps the launch after the last frame it needed to back off */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch|Zoom", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float HoldSeconds = 0.8f;

	/** The furthest the camera backs off, cm: a KO launch is not followed to the blast zone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch|Zoom", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxExtraDistance = 1500.f;

	/** Interp speeds (FMath::FInterpTo): backing off fast, coming back slowly so the return is not a second jolt */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch|Zoom", meta = (ClampMin = "0.1"))
	float ZoomOutSpeed = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch|Zoom", meta = (ClampMin = "0.1"))
	float ZoomInSpeed = 2.f;

	/** A small enemy moves the camera only when it flies at least this fast (on its way out of the level) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch|Zoom", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float MinorEnemySpeed = 3000.f;
};

/** The off-screen arrow: players only, never enemies */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_OffscreenArrowTuning
{
	GENERATED_BODY()

	/** Distance of the arrow from the screen edge, in slate units */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch|Arrow", meta = (ClampMin = "0.0"))
	float EdgeInset = 48.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch|Arrow", meta = (ClampMin = "4.0"))
	float Size = 26.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch|Arrow", meta = (ClampMin = "1.0"))
	float Thickness = 4.f;

	/** Height above the actor's location the arrow points at (the chest, not the feet) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch|Arrow", meta = (ForceUnits = "cm"))
	float AimHeight = 60.f;
};

/** Project Settings > Game > GSTN Launch Readability */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "GSTN Launch Readability"))
class GASP_GIUSTINO2_API UGSTN_LaunchReadabilitySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UGSTN_LaunchReadabilitySettings& Get() { return *GetDefault<UGSTN_LaunchReadabilitySettings>(); }

	UPROPERTY(Config, EditAnywhere, Category = "Launch")
	FGSTN_LaunchZoomTuning Zoom;

	UPROPERTY(Config, EditAnywhere, Category = "Launch")
	FGSTN_OffscreenArrowTuning Arrow;
};

/** The view math of both parts. Pure, for the tests */
struct GASP_GIUSTINO2_API FGSTN_LaunchViewMath
{
	/**
	 * How far the view must back off along its forward so Point sits inside Margin of the half screen (0 when it already does). Horizontal FOV
	 * from the view, vertical from its aspect ratio (16:9 when the view has none)
	 */
	static float PushBackToFit(const FMinimalViewInfo& View, const FVector& Point, float Margin);

	/**
	 * Where an off-screen arrow goes: false when Point is on screen. Else OutPosition (slate units from the top left of a ScreenSize screen,
	 * EdgeInset in from the edge, on the line from the centre towards the point) and OutDirection (unit, screen space, y down). A point behind
	 * the view points the way it lies around the camera
	 */
	static bool OffscreenArrow(const FMinimalViewInfo& View, const FVector& Point, const FVector2D& ScreenSize, float EdgeInset, FVector2D& OutPosition, FVector2D& OutDirection);

	/** The colour of a player number (1-based): P1 red, P2 blue, P3 yellow, P4 green (game-design-direction, crystal colours), then grey */
	static FLinearColor PlayerColor(int32 PlayerNumber);
};

/**
 * Draws the off-screen arrows of the other players for its owning player. Added to the viewport by UGSTN_LaunchReadabilitySubsystem; no
 * widget tree, the arrows are drawn in NativePaint. Hidden to hit tests
 */
UCLASS(NotBlueprintable)
class GASP_GIUSTINO2_API UGSTN_OffscreenArrowWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};

/**
 * One per game world (not on a dedicated server): picks the launch the local camera follows (UpdateZoom, called by the playtest camera
 * modifier with the view before the zoom) and keeps the arrow widget on the first local player's screen
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_LaunchReadabilitySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** The extra distance the view backs off this frame, cm (0 with the zoom off or nothing to follow). View: before the zoom */
	float UpdateZoom(float DeltaTime, const FMinimalViewInfo& View, const AActor* ViewTarget);

private:
	TWeakObjectPtr<AActor> Focus;
	double HoldUntil = 0.0;
	float CurrentExtra = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_OffscreenArrowWidget> ArrowWidget;
};

// Depth marker runtime: small shared types (depth-marker.md §3, §5).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_DepthMarkerTypes.h"
#include "GSTN_DepthMarkerRuntimeTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGSTNDepthMarker, Log, All);

/** Whose markers a machine draws (§3). Enemies show only when their component says All: opt-in by data */
UENUM(BlueprintType)
enum class EGSTN_DepthAudience : uint8
{
	/** Only the locally controlled pawn */
	LocalOnly,
	/** The local pawn and pawns of the same team */
	LocalAndAllies,
	All
};

/** What a linked or parked entity does with its marker (§2.6) */
UENUM(BlueprintType)
enum class EGSTN_DepthLinkPolicy : uint8
{
	/** The parked entity has no marker; the holder or carrier keeps its own */
	Hide,
	ShowOnGround,
	/** Same as Hide in v1 (the holder's marker is the one that stays) */
	Inherit
};

/** Which layer of a result a presenter draws */
UENUM(BlueprintType)
enum class EGSTN_DepthLayer : uint8
{
	/** The marker under the entity: on a surface, or the "no ground" ring on the plane */
	Below,
	/** The ring on the reference plane above the entity */
	Above,
	/** Marker B: the column projected onto the plane of a nearby higher platform (own style slot ApproachStyle) */
	Approach
};

/**
 * Tuning of the toy look, on the style (UGSTN_DepthMarkerStyle::Toy). One size drives the landing spot: the decal radius is the
 * beam base radius, and the fog puff of an air marker is sized from it too. Presentation only.
 */
USTRUCT(BlueprintType)
struct GSTNDEPTHMARKERRUNTIME_API FGSTN_DepthToyParams
{
	GENERATED_BODY()

	/** The style is a toy style: presenters use the shared size and the visibility rules below. Set by PopulateToyPresenters */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy")
	bool bEnabled = false;

	/** Landing radius (decal radius = beam base radius) as a multiple of the subject's foot radius, before the height curve */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Landing", meta = (ClampMin = "0.1"))
	float LandingRadiusFootMultiplier = 1.9f;

	/** Below this height the landing spot has its tight size and crisp edge */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Landing", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float NearHeightCm = 60.f;

	/** From this height up it has its large, soft size */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Landing", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float FarHeightCm = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Landing", meta = (ClampMin = "0.1"))
	float NearScale = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Landing", meta = (ClampMin = "0.1"))
	float FarScale = 1.45f;

	/** 0 = crisp, 1 = very soft (shadow and ring), at the near and far heights */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Landing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SoftnessNear = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Landing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SoftnessFar = 0.8f;

	/** The decal fades out as the surface normal leaves Up: full above StartDot (dot of normal and Up), nothing at EndDot (no smear on walls) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Landing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlopeFadeStartDot = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Landing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlopeFadeEndDot = 0.6f;

	/** Half depth of the decal box along -Up on a flat floor; it grows with the slope up to the maximum */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Landing", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float DecalDepthCm = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Landing", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float DecalMaxDepthCm = 45.f;

	/** Beam radius at the character as a multiple of the foot radius (never above the landing radius) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Beam", meta = (ClampMin = "0.05"))
	float BeamTopFootMultiplier = 0.6f;

	/**
	 * Where the beam of the below layer starts, as a fraction of the subject height above the feet (0 feet, 0.5 the core of the body): the light
	 * passes through the character, so it seems to leave its core (the cracks glow there). The minimum height still counts from the feet.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Beam", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BeamStartFraction = 0.5f;

	/** Time constant of the LightThrough value written to the character materials */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Character", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float LightThroughSeconds = 0.08f;

	/** Peak opacity of the beam: it is a hint, the decal is the marker */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Beam", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BeamOpacity = 0.24f;

	/** Nothing is drawn while the column from the character to the landing spot is shorter than this; fades in over FadeBandCm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Visibility", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MinHeightCm = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Visibility", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float FadeBandCm = 40.f;

	/** A capsule (sphere) of the beam radius is swept along the column; the beam fades out while it is blocked (a wall beside the character) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Visibility")
	bool bBeamClearance = true;

	/** Radius of the clearance sweep between the top radius (0) and the landing radius (1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Visibility", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ClearanceRadiusMix = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Visibility", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ClearanceFadeOutSeconds = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Visibility", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ClearanceFadeInSeconds = 0.35f;

	/** Time constant of the exponential smoothing of the size and of the beam end (along Up only, so the spot stays on the column) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Toy|Visibility", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float SmoothingSeconds = 0.06f;
};

/** What the subsystem computed for the toy presenters this frame (the below layer; the other layers derive their look from the params) */
USTRUCT(BlueprintType)
struct GSTNDEPTHMARKERRUNTIME_API FGSTN_DepthToyView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Toy")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Toy")
	FGSTN_DepthToyParams Params;

	/** Smoothed landing radius of the below layer: decal radius and beam base radius */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Toy")
	float LandingRadius = 0.f;

	/** 1 clear, 0 blocked (a wall hugs the column) or wall sliding; smoothed */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Toy")
	float BeamClearance = 1.f;

	/** The beam end and puff centre of the below layer: the landing point, smoothed along Up only */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Toy")
	FVector BeamEnd = FVector::ZeroVector;
};

/** Everything a presenter needs for one frame; presented values only (§3) */
USTRUCT(BlueprintType)
struct GSTNDEPTHMARKERRUNTIME_API FGSTN_DepthMarkerView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|View")
	TWeakObjectPtr<AActor> Owner;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|View")
	FGSTN_DepthSubjectState Subject;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|View")
	FGSTN_DepthTarget Below;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|View")
	FGSTN_DepthTarget Above;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|View")
	FGSTN_DepthTarget Approach;

	/** The below layer should be drawn (visibility hysteresis, audience, link policy and tags already applied) */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|View")
	bool bBelowVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|View")
	bool bAboveVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|View")
	bool bApproachVisible = false;

	/** Fade by height, edge hold and gravity cross-fade, 0..1 */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|View")
	float BelowAlpha = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|View")
	float AboveAlpha = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|View")
	float ApproachAlpha = 1.f;

	/** Tint of the approach style (it brings its own colour); the presenters of that layer draw with it */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|View")
	FLinearColor ApproachTint = FLinearColor(0.4f, 0.85f, 1.f, 1.f);

	/** Radius factor of the approach layer */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|View")
	float ApproachRadiusScale = 1.f;

	/** Radius factor over height (1 near, shrinking with height) */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|View")
	float RadiusScale = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|View")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|View")
	float TimeSeconds = 0.f;

	/** Toy look data (inactive for the classic styles) */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|View")
	FGSTN_DepthToyView Toy;

	/** Hold the current target of the layer */
	const FGSTN_DepthTarget& GetTarget(EGSTN_DepthLayer Layer) const
	{
		return Layer == EGSTN_DepthLayer::Below ? Below : (Layer == EGSTN_DepthLayer::Above ? Above : Approach);
	}
	bool IsVisible(EGSTN_DepthLayer Layer) const
	{
		return Layer == EGSTN_DepthLayer::Below ? bBelowVisible : (Layer == EGSTN_DepthLayer::Above ? bAboveVisible : bApproachVisible);
	}
	float GetAlpha(EGSTN_DepthLayer Layer) const
	{
		return Layer == EGSTN_DepthLayer::Below ? BelowAlpha : (Layer == EGSTN_DepthLayer::Above ? AboveAlpha : ApproachAlpha);
	}
};

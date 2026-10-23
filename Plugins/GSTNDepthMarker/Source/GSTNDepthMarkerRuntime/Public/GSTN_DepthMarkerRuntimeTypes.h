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

// Toy look of the depth marker: a fake light along gravity (beam), a coloured fog puff where it lands in the air, a stained-glass decal
// on the ground, tinted per player. Presentation only. The pure helpers here are what the tests cover.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_DepthMarkerPresenter.h"
#include "GSTN_DepthMarkerRuntimeTypes.h"
#include "GSTN_DepthMarkerToy.generated.h"

class UMaterialInterface;
class UPrimitiveComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Which look the console variable gstn.DepthMarker.Style asks for */
enum class EGSTN_DepthLookRequest : uint8
{
	/** Empty or unknown: the settings decide */
	None,
	Toy,
	Classic
};

/** A participating player as the colour ordering sees it */
struct FGSTN_DepthPlayerKey
{
	FString Name;
	/** Tie-break when two players share a name (the net id string or the player id); must be unique and stable */
	FString UniqueId;
};

/** Pure helpers of the toy look (no world, no rendering) */
struct GSTNDEPTHMARKERRUNTIME_API FGSTN_DepthToy
{
	/** Layout of the custom primitive data the toy materials read, relative to the settings' base index */
	static constexpr int32 ColorOffset = 0;      // float4 colour (a unused)
	static constexpr int32 OpacityOffset = 4;    // float
	static constexpr int32 BeamStartOffset = 5;  // float4 world position (w unused)
	static constexpr int32 BeamEndOffset = 9;    // float4 world position (w unused)
	static constexpr int32 NumFloats = 13;

	static EGSTN_DepthLookRequest ParseLookRequest(const FString& Value);

	/**
	 * Colour order of the players: case-insensitive alphabetical by display name, then by unique id, so the same group of friends
	 * always gets the same colours whatever the join order. Returns the rank of Self (0 = first), INDEX_NONE if Self is not listed.
	 */
	static int32 ColorRankOf(const TArray<FGSTN_DepthPlayerKey>& Players, const FGSTN_DepthPlayerKey& Self);

	/** Per component override, then the player's palette slot (rank wraps), then the team palette, then the fallback */
	static FLinearColor ResolveColor(const TArray<FLinearColor>& PlayerPalette, int32 PlayerRank,
		const TMap<int32, FLinearColor>& TeamPalette, int32 TeamId, const FLinearColor* Override, const FLinearColor& Fallback);

	/** Air states: where the fog puff exists (a beam that ends in the air). The ground and edge-hold states land on a surface instead */
	static bool IsAirState(EGSTN_DepthMarkerState State);

	/** Start and end of the beam of a layer: from the feet (from the head for the above layer) to the marker point */
	static void BeamEnds(const FGSTN_DepthSubjectState& Subject, EGSTN_DepthLayer Layer, const FVector& Point, FVector& OutStart, FVector& OutEnd);

	/**
	 * Transform of a tapered mesh (tip at local +Z unless bFlipTaper, wide base at the other end) laid along Start..End with the tip
	 * at Start and the base at End. Mesh bounds are local: Z range [ZMin, ZMax], XY extents ExtentXY (half size). WidthCm = base diameter.
	 */
	static FTransform BeamTransform(const FVector& Start, const FVector& End, float ZMin, float ZMax, float ExtentXY, float WidthCm, bool bFlipTaper);

	/** Rotation of a quad whose normal (local Z) is Normal and whose local Y leans towards UpHint */
	static FQuat QuadRotation(const FVector& Normal, const FVector& UpHint);

	/** Writes colour and opacity (and nothing else) into the custom primitive data of Component at Base */
	static void WriteColor(UPrimitiveComponent* Component, int32 Base, const FLinearColor& Color, float Opacity);
	static void WriteBeamEnds(UPrimitiveComponent* Component, int32 Base, const FVector& Start, const FVector& End);
};

/** Shared plumbing of the toy presenters: soft content, no dynamic material (custom primitive data carries the colour) */
UCLASS(Abstract)
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthPresenter_Toy : public UGSTN_DepthMarkerPresenter
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Content")
	TSoftObjectPtr<UMaterialInterface> MaterialOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Content")
	TSoftObjectPtr<UStaticMesh> MeshOverride;

	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }

protected:
	/** Creates the component on first use; null while the material or mesh is missing (the presenter then draws nothing) */
	UStaticMeshComponent* EnsureMesh(AActor* Holder, UStaticMesh* DefaultMesh);
	virtual void OnMarkerReleased_Implementation() override;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> MeshComponent;
};

/** The beam: a tapered translucent cone from the feet (head for the above layer) to the marker point, tip at the character */
UCLASS(meta = (DisplayName = "Beam (cone)"))
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthPresenter_Cone : public UGSTN_DepthPresenter_Toy
{
	GENERATED_BODY()

public:
	UGSTN_DepthPresenter_Cone();

	/** Diameter of the base (at the marker point) as a multiple of the foot radius */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Beam", meta = (ClampMin = "0.1"))
	float BaseWidthMultiplier = 2.4f;

	/** For a cone mesh whose tip is at local -Z */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Beam")
	bool bFlipTaper = false;

protected:
	virtual void OnMarkerUpdated_Implementation(const FGSTN_DepthMarkerView& View, AActor* Holder) override;
};

/**
 * A small semi-fake fog puff where the beam lands in the AIR (no-ground ring, above reference, approach marker), so the beam does not
 * end abruptly. A flat quad that faces the camera or lies on the layer's plane; the material brings the noise and the depth fade.
 */
UCLASS(meta = (DisplayName = "Fog puff (air landing)"))
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthPresenter_FogPuff : public UGSTN_DepthPresenter_Toy
{
	GENERATED_BODY()

public:
	UGSTN_DepthPresenter_FogPuff();

	/** Side of the quad as a multiple of the foot radius */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Puff", meta = (ClampMin = "0.1"))
	float SizeMultiplier = 3.f;

	/** True: faces the player camera. False: lies on the layer's present normal (plane aligned) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Puff")
	bool bFaceCamera = true;

protected:
	virtual void OnMarkerUpdated_Implementation(const FGSTN_DepthMarkerView& View, AActor* Holder) override;
};

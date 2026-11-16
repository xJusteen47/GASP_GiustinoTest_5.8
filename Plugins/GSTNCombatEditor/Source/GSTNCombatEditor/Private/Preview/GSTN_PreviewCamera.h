// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"

/** The camera presets of the preview toolbar. Directions are in root space (+X forward, Z up) */
enum class EGSTN_PreviewCameraPreset : uint8
{
	/** From the character's right, looking at its side: the view for judging reach */
	Side,

	/** From in front of the character, looking back at it */
	Front,

	/** From front, right and above */
	ThreeQuarter
};

/** Camera maths of the preview viewport. Pure, so the framing is tested without a viewport (GSTNCombat.Editor.Preview.FollowRootFramesMesh) */
namespace GSTNPreviewCamera
{
	/** The unit direction the camera looks along for a preset */
	inline FVector LookDirection(EGSTN_PreviewCameraPreset Preset)
	{
		switch (Preset)
		{
		case EGSTN_PreviewCameraPreset::Front:
			return FVector(-1.0, 0.0, 0.0);
		case EGSTN_PreviewCameraPreset::ThreeQuarter:
			return FVector(-1.0, -1.0, -0.35).GetSafeNormal();
		case EGSTN_PreviewCameraPreset::Side:
		default:
			return FVector(0.0, -1.0, 0.0);
		}
	}

	/** How far from the centre a camera with a horizontal field of view must stand for a sphere of the radius to fit, with a margin */
	inline double FramingDistance(double Radius, double FieldOfViewDegrees, double Margin = 1.15)
	{
		const double HalfFov = FMath::DegreesToRadians(FMath::Clamp(FieldOfViewDegrees, 5.0, 170.0) * 0.5);
		return FMath::Max(Radius, 1.0) * Margin / FMath::Sin(HalfFov);
	}

	/** Camera location for looking at the bounds along the preset's direction so the whole bounding sphere is in view */
	inline FVector FrameBounds(EGSTN_PreviewCameraPreset Preset, const FBoxSphereBounds& Bounds, double FieldOfViewDegrees)
	{
		return Bounds.Origin - LookDirection(Preset) * FramingDistance(Bounds.SphereRadius, FieldOfViewDegrees);
	}

	/**
	 * Follow root: the camera keeps its offset from the root. A root that moves by a delta moves the camera by the same delta, so what was in
	 * frame stays in frame whatever the Movement track or the root motion does to the character.
	 */
	inline FVector FollowRoot(const FVector& CameraLocation, const FVector& PreviousRoot, const FVector& NewRoot)
	{
		return CameraLocation + (NewRoot - PreviousRoot);
	}

	/** True when a camera at the location, looking along the direction, has the whole sphere inside its field of view (half-angle of the sphere under half the FOV) */
	inline bool SphereInView(const FVector& CameraLocation, const FVector& LookAt, const FVector& SphereCenter, double Radius, double FieldOfViewDegrees)
	{
		const FVector ToSphere = SphereCenter - CameraLocation;
		const double Distance = ToSphere.Size();
		if (Distance <= Radius)
		{
			return false;
		}
		const double HalfFov = FMath::DegreesToRadians(FieldOfViewDegrees * 0.5);
		const double AngleOfCentre = FMath::Acos(FMath::Clamp(FVector::DotProduct(ToSphere.GetSafeNormal(), (LookAt - CameraLocation).GetSafeNormal()), -1.0, 1.0));
		const double AngularRadius = FMath::Asin(Radius / Distance);
		return AngleOfCentre + AngularRadius <= HalfFov;
	}
}

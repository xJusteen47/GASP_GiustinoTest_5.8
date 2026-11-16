// Blueprint wrapper of the pure camera maths (camera.md §10). The maths itself lives in GSTNCameraCore (FGSTN_CameraMath).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_CameraMath.h"
#include "GSTN_CameraTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_CameraMathLibrary.generated.h"

UCLASS()
class GSTNCAMERARUNTIME_API UGSTN_CameraMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The deterministic reference direction of a frame: world X projected on the plane normal to Up, world Y when degenerate */
	UFUNCTION(BlueprintPure, Category = "Camera|Math")
	static FVector ReferenceForward(const FVector& Up) { return FGSTN_CameraMath::ReferenceForward(Up); }

	/** Heading about Up of a direction, measured from the reference forward of Up (the one function the input encoder shares with the camera) */
	UFUNCTION(BlueprintPure, Category = "Camera|Math")
	static float HeadingAboutUp(const FVector& Up, const FVector& Direction)
	{
		return FGSTN_CameraMath::DirectionToHeading(FGSTN_CameraMath::MakeFrame(Up, FVector::ZeroVector), Direction);
	}

	UFUNCTION(BlueprintPure, Category = "Camera|Math")
	static FVector DirectionFromHeading(const FVector& Up, float HeadingDeg)
	{
		return FGSTN_CameraMath::HeadingToDirection(FGSTN_CameraMath::MakeFrame(Up, FVector::ZeroVector), HeadingDeg);
	}

	UFUNCTION(BlueprintPure, Category = "Camera|Math")
	static float ShortestAngleDelta(float FromDeg, float ToDeg) { return FGSTN_CameraMath::ShortestAngleDelta(FromDeg, ToDeg); }

	/** One rate-limited step of Up towards a target */
	UFUNCTION(BlueprintPure, Category = "Camera|Math")
	static FVector StepUp(const FVector& CurrentUp, const FVector& TargetUp, float MaxDegPerSecond, float DeltaSeconds)
	{
		return FGSTN_CameraMath::StepUp(CurrentUp, TargetUp, MaxDegPerSecond, DeltaSeconds);
	}

	UFUNCTION(BlueprintPure, Category = "Camera|Math")
	static float BlendAlpha(EGSTN_CameraBlendFunction Function, float Progress, float Exponent = 2.f)
	{
		return FGSTN_CameraMath::BlendAlpha(Function, Progress, Exponent);
	}

	UFUNCTION(BlueprintPure, Category = "Camera|Math")
	static FGSTN_CameraPose ComputePose(const FGSTN_CameraFrame& Frame, const FGSTN_CameraView& View) { return FGSTN_CameraMath::ComputePose(Frame, View); }
};

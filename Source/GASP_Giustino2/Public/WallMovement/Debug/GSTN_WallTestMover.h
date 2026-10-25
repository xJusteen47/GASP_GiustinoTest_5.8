// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSTN_WallTestMover.generated.h"

class UStaticMeshComponent;

/**
 * A kinematic box that ping-pongs along an axis and turns back and forth about the up axis: the moving wall of the wall slide A/B map
 * (lane h, wall-slide-v2.md 4). Test content only: it lives in the debug area of the wall movement code, the map uses it as a plain actor.
 *
 * The pose is a pure function of the world time (UWorld::GetTimeSeconds) and of the pose the actor had at BeginPlay, never integrated from
 * the previous frame, so the host and every client place it the same way up to their clock difference and a rolled-back Mover simulation
 * that rides it meets the same wall. The box is Movable with simulation off: Chaos derives its velocity from the pose change of the
 * physics step, which is what Mover's movement base reads (GetPhysicsLinearVelocityAtPoint).
 *
 * Triangle waves instead of a constant yaw rate: the wall faces the character for the whole lane. TranslationSpeed and YawRate are the
 * slopes of the waves; TravelDistance and YawRange their half amplitudes.
 */
UCLASS(Blueprintable)
class GASP_GIUSTINO2_API AGSTN_WallTestMover : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_WallTestMover();

	/** Pose at time T (seconds) from the base pose: pure, used by Tick and by tests */
	UFUNCTION(BlueprintPure, Category = "WallTestMover")
	static void ComputePose(double TimeSeconds, const FVector& BaseLocation, const FRotator& BaseRotation, const FVector& TravelAxisWorld, float TranslationSpeed,
		float TravelDistance, float YawRateDeg, float YawRangeDeg, FVector& OutLocation, FRotator& OutRotation);

	/** Value of a triangle wave of the given slope (units/s) between -Amplitude and +Amplitude, starting at 0 and rising. Zero when slope or amplitude is zero */
	static double TriangleWave(double TimeSeconds, double Slope, double Amplitude);

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	/** Direction of the translation in the actor's BeginPlay frame (local space, normalised on use) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallTestMover|Translation")
	FVector TravelAxis = FVector(0.0, 1.0, 0.0);

	/** Speed of the ping-pong translation (cm/s); 0 = no translation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallTestMover|Translation", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float TranslationSpeed = 150.f;

	/** Half the stroke: the box moves between -TravelDistance and +TravelDistance around the BeginPlay position (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallTestMover|Translation", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float TravelDistance = 300.f;

	/** Yaw speed of the back-and-forth turn about the up axis (deg/s); 0 = no turn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallTestMover|Rotation", meta = (ClampMin = "0.0", ForceUnits = "deg/s"))
	float YawRate = 20.f;

	/** Half the turn: the yaw moves between -YawRange and +YawRange around the BeginPlay yaw (deg) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallTestMover|Rotation", meta = (ClampMin = "0.0", ForceUnits = "deg"))
	float YawRange = 45.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "WallTestMover")
	TObjectPtr<UStaticMeshComponent> Mesh;

private:
	FVector BaseLocation = FVector::ZeroVector;
	FRotator BaseRotation = FRotator::ZeroRotator;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MoverTypes.h"
#include "GSTN_WallTestMover.generated.h"

class UMoverComponent;
class UStaticMeshComponent;

/**
 * A kinematic box that ping-pongs along an axis and turns back and forth about the up axis: the moving wall of the wall slide A/B map
 * (lane h, wall-slide-v2.md 4). Test content only: it lives in the debug area of the wall movement code, the map uses it as a plain actor.
 *
 * The pose is a pure function of a time and of the pose the actor had at BeginPlay, never integrated from the previous frame, and the time is not the
 * machine's own clock (UWorld::GetTimeSeconds differs between the host and each client by the time they have been running and by the connection
 * latency, which made a ridden wall a prediction error source of its own in the A/B harness, lanes h):
 *  - While a Mover simulation ticks (a pawn riding or sliding on the box), the pose is set at the start of every simulation tick to the pose of that
 *    tick's SIMULATION time (FMoverTimeStep::BaseSimTimeMs is in server simulation timespace: the same number on the server and on a client that predicts
 *    or re-simulates that frame). A client predicts about half a second ahead of the server, so the server's clock is not enough: a client frame must meet
 *    the wall the server will have when it simulates that frame, and a re-simulation after a correction must meet the wall of each replayed frame, not the
 *    one of the current game frame. The pose is teleported (physics) so that the sweeps of the tick see it.
 *  - Otherwise (no Mover ticking, the editor, a preview) the pose follows the server's world time (AGameStateBase::GetServerWorldTimeSeconds, the client's
 *    estimate of it), the offset to the local clock slewed and not applied at once: the estimate is refreshed every few seconds and jitters by the network
 *    jitter, and a jump in the pose would be a spike in the velocity Chaos derives from it.
 * The box is Movable with simulation off. Moved by the game tick, Chaos derives its velocity from the pose change of the physics step (what Mover's movement
 * base reads, GetPhysicsLinearVelocityAtPoint); the teleported pose of a simulation tick has none, so a character leaving the wall does not take the wall's
 * speed along in this test setup.
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

	/**
	 * One step of the clock offset between this machine and the server: Current moves toward Target by at most MaxSlewPerSecond * DeltaSeconds,
	 * or jumps there when it is further than SnapThreshold (first sample, a map change, a bad clock). Pure, used by Tick and by tests
	 */
	static double StepTimeOffset(double Current, double Target, double DeltaSeconds, double MaxSlewPerSecond, double SnapThreshold);

	/** Time to evaluate the pose at for a Mover simulation tick: the end of its step, in server simulation timespace (seconds). Pure */
	static double SimPoseTimeSeconds(double BaseSimTimeMs, double StepMs);

	/** Value of a triangle wave of the given slope (units/s) between -Amplitude and +Amplitude, starting at 0 and rising. Zero when slope or amplitude is zero */
	static double TriangleWave(double TimeSeconds, double Slope, double Amplitude);

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	/** Largest rate at which the clock offset to the server is corrected (seconds of offset per second): 2% means the wall is at most 2% slower or faster while it catches up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallTestMover|Clock", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float MaxClockSlew = 0.02f;

	/** An offset error beyond this (seconds) is corrected at once instead of slewed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallTestMover|Clock", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ClockSnapThreshold = 0.25f;

private:
	/** Before every simulation tick of a Mover component in this world (UMoverComponent::OnPreSimulationTick) */
	UFUNCTION()
	void OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd);

	/** Hooks the Mover components of the pawns in the world that are not hooked yet (pawns come and go) */
	void BindToMovers();

	void ApplyPoseAt(double TimeSeconds, ETeleportType Teleport);

	/** The time the pose is computed at when no simulation drives it: the server's world time as far as this machine knows it, the local one without a game state */
	double GetPoseTimeSeconds(float DeltaSeconds);

	TSet<TWeakObjectPtr<UMoverComponent>> BoundMovers;
	double LastSimDrivenTimeSeconds = -1.e9;
	double NextBindScanSeconds = 0.0;
	double TimeOffset = 0.0;
	bool bHasTimeOffset = false;
	FVector BaseLocation = FVector::ZeroVector;
	FRotator BaseRotation = FRotator::ZeroRotator;
};

// Spin and drill, presentation.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/GSTN_SpinDrillTypes.h"
#include "GSTN_DrillPresentationComponent.generated.h"

class USceneComponent;
struct FGSTN_DrillFragment;

/** What the component shows on a frame, from the drill fragment (pure, so tests check it without a mesh) */
struct FGSTN_DrillPresentationPose
{
	/** False = nothing to show: the mesh keeps its own rotation */
	bool bActive = false;

	/** Turn around the character's up axis, degrees (positive = counter-clockwise seen from above, the engine's yaw) */
	float SpinYawDegrees = 0.f;

	/** Tilt of the drill away from its axis and the direction of the tilt around it (the seat's wobble), degrees */
	float WobbleDegrees = 0.f;
	float WobblePhaseDegrees = 0.f;
};

/**
 * The procedural layer of the spin and drill (plan D11, spin-drill.md 9): turns the owner's mesh around its up axis while it spins or dives and
 * wobbles it while the drill seats, from the drill fragment this machine shows (simulated, predicted or interpolated). Presentation only: it
 * never writes simulation state and nothing in the simulation reads it, so a dedicated server has none (the module's loadout extension adds it
 * on every machine that renders, the listen host included).
 *
 * Everything is a function of the fragment and the frame: the spin angle is SpinRps over the frames since the phase started, the wobble is
 * UGSTN_DrillMath::ComputeWobble over the frames since the latch, so every machine shows the same angle for the same frame. The rotation is
 * asserted on every tick after Mover's presentation (TG_PostPhysics) on top of Mover's base transform of its visual component, because Mover
 * rewrites that relative transform every frame (M7 S2 finding A); while nothing shows, the component touches the mesh once to put it back.
 * Placeholder art: the final animations replace the turn (each ability's montage), the wobble stays procedural.
 */
UCLASS(ClassGroup = (GSTN), meta = (BlueprintSpawnableComponent))
class GSTNSPINDRILLPRESENTATION_API UGSTN_DrillPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_DrillPresentationComponent();

	/** Turns per second of the mesh during a spin (placeholder for the spin montage) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Presentation", meta = (ClampMin = "0", ForceUnits = "Hz"))
	float SpinRps = 2.5f;

	/** The drill's look: visual spin while diving and boring, wobble while seating */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Presentation")
	FGSTN_DrillWobbleTuning Drill;

	/** The fixed tick the frames count in (the project runs Network Prediction at 60 Hz) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Presentation", meta = (ClampMin = "1", ForceUnits = "Hz"))
	float FixedTickHz = 60.f;

	/** The pose of a fragment on a frame. Null fragment: inactive */
	static FGSTN_DrillPresentationPose ComputePose(const FGSTN_DrillFragment* Fragment, int32 Frame, float SpinRps, const FGSTN_DrillWobbleTuning& Drill,
		float FixedTickHz, const TArray<float>& WobbleSamples);

	/** The rotation the pose adds on top of the mesh's base rotation, in the mesh's parent (the capsule) space */
	static FQuat PoseRotation(const FGSTN_DrillPresentationPose& Pose);

	/** The component of an actor, created and registered when missing */
	static UGSTN_DrillPresentationComponent* FindOrAdd(AActor* Owner);

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** The mesh the pose rotates: Mover's primary visual component, else the first skeletal mesh */
	USceneComponent* FindMesh() const;

	/** The mesh's rotation without the pose: Mover's base transform of its visual component when it is that, else the one seen before the first pose */
	FQuat BaseRotation(const USceneComponent& Mesh) const;

	/** The wobble curve sampled once (presentation data, never read by the simulation) */
	UPROPERTY(Transient)
	TArray<float> WobbleSamples;

	/** A pose was applied on the previous tick: the next inactive tick puts the base rotation back once */
	bool bPoseApplied = false;

	/** The mesh's relative rotation before the first pose, for a mesh Mover does not drive */
	FQuat CapturedBase = FQuat::Identity;
	bool bCapturedBase = false;
};

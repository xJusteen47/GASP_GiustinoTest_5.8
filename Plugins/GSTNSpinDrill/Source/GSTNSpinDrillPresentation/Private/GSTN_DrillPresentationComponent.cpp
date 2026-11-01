// Spin and drill, presentation.

#include "GSTN_DrillPresentationComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Actor.h"
#include "Math/GSTN_DrillMath.h"
#include "MoverComponent.h"
#include "Sim/GSTN_DrillFragment.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatSimTypes.h"

UGSTN_DrillPresentationComponent::UGSTN_DrillPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the simulation and Mover's presentation moved the actors of this frame
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

UGSTN_DrillPresentationComponent* UGSTN_DrillPresentationComponent::FindOrAdd(AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}
	if (UGSTN_DrillPresentationComponent* Existing = Owner->FindComponentByClass<UGSTN_DrillPresentationComponent>())
	{
		return Existing;
	}
	UGSTN_DrillPresentationComponent* Component = NewObject<UGSTN_DrillPresentationComponent>(Owner, TEXT("GSTN_DrillPresentation"));
	Component->RegisterComponent();
	return Component;
}

void UGSTN_DrillPresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	// Outside the simulation, once: the curve is content
	UGSTN_DrillMath::SampleWobbleCurve(Drill.WobbleCurve.LoadSynchronous(), 16, WobbleSamples);
}

FGSTN_DrillPresentationPose UGSTN_DrillPresentationComponent::ComputePose(const FGSTN_DrillFragment* Fragment, int32 Frame, float SpinRps,
	const FGSTN_DrillWobbleTuning& Drill, float FixedTickHz, const TArray<float>& WobbleSamples)
{
	FGSTN_DrillPresentationPose Pose;
	if (!Fragment || Frame == INDEX_NONE)
	{
		return Pose;
	}
	const float Hz = FMath::Max(FixedTickHz, 1.f);
	// Clockwise seen from above is a negative yaw in the engine's frame
	const float Sense = Fragment->IsClockwise() ? -1.f : 1.f;
	switch (Fragment->Phase)
	{
	case EGSTN_DrillPhase::Spinning:
		Pose.bActive = true;
		Pose.SpinYawDegrees = FMath::Fmod(Sense * 360.f * SpinRps * Fragment->FramesInPhase(Frame) / Hz, 360.f);
		break;
	case EGSTN_DrillPhase::Diving:
	case EGSTN_DrillPhase::Boring:
		Pose.bActive = true;
		Pose.SpinYawDegrees = FMath::Fmod(Sense * 360.f * Drill.VisualSpinRps * Fragment->FramesInPhase(Frame) / Hz, 360.f);
		break;
	case EGSTN_DrillPhase::Seating:
	{
		Pose.bActive = true;
		Pose.SpinYawDegrees = FMath::Fmod(Sense * 360.f * Drill.VisualSpinRps * Fragment->FramesSinceLatch(Frame) / Hz, 360.f);
		const FGSTN_DrillWobble Wobble = UGSTN_DrillMath::ComputeWobble(Fragment->FramesSinceLatch(Frame), Fragment->SeatFrames, Drill.MaxWobbleDegrees,
			Drill.PrecessionHz, Hz, WobbleSamples);
		Pose.WobbleDegrees = Wobble.AmplitudeDegrees;
		Pose.WobblePhaseDegrees = Wobble.PhaseDegrees;
		break;
	}
	default:
		break;
	}
	return Pose;
}

FQuat UGSTN_DrillPresentationComponent::PoseRotation(const FGSTN_DrillPresentationPose& Pose)
{
	if (!Pose.bActive)
	{
		return FQuat::Identity;
	}
	const FQuat Spin(FVector::UpVector, FMath::DegreesToRadians(Pose.SpinYawDegrees));
	if (FMath::IsNearlyZero(Pose.WobbleDegrees))
	{
		return Spin;
	}
	// The tilt axis lies in the horizontal plane, turned by the wobble's phase: the drill's axis precesses around up
	const float Phase = FMath::DegreesToRadians(Pose.WobblePhaseDegrees);
	const FVector TiltAxis(FMath::Cos(Phase), FMath::Sin(Phase), 0.f);
	return FQuat(TiltAxis, FMath::DegreesToRadians(Pose.WobbleDegrees)) * Spin;
}

USceneComponent* UGSTN_DrillPresentationComponent::FindMesh() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}
	if (const UMoverComponent* Mover = Owner->FindComponentByClass<UMoverComponent>())
	{
		if (USceneComponent* Visual = Mover->GetPrimaryVisualComponent())
		{
			return Visual;
		}
	}
	return Owner->FindComponentByClass<USkeletalMeshComponent>();
}

FQuat UGSTN_DrillPresentationComponent::BaseRotation(const USceneComponent& Mesh) const
{
	if (const UMoverComponent* Mover = GetOwner()->FindComponentByClass<UMoverComponent>(); Mover && Mover->GetPrimaryVisualComponent() == &Mesh)
	{
		return Mover->GetBaseVisualComponentTransform().GetRotation();
	}
	return bCapturedBase ? CapturedBase : Mesh.GetRelativeRotation().Quaternion();
}

void UGSTN_DrillPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* Owner = GetOwner();
	const UGSTN_CombatSimComponent* Combat = Owner ? Owner->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	const FGSTN_CombatSyncState* Sync = Combat ? Combat->GetSyncState() : nullptr;
	const FGSTN_DrillFragment* Fragment = Sync ? FGSTN_SyncFragmentList::Find<FGSTN_DrillFragment>(Sync->Fragments) : nullptr;
	const FGSTN_DrillPresentationPose Pose = ComputePose(Fragment, Combat ? Combat->GetSimulationFrame() : INDEX_NONE, SpinRps, Drill, FixedTickHz, WobbleSamples);
	if (!Pose.bActive && !bPoseApplied)
	{
		// The idle cost: one fragment lookup
		return;
	}
	USceneComponent* Mesh = FindMesh();
	if (!Mesh)
	{
		return;
	}
	if (!bCapturedBase)
	{
		CapturedBase = Mesh->GetRelativeRotation().Quaternion();
		bCapturedBase = true;
	}
	const FQuat Base = BaseRotation(*Mesh);
	// The pose turns the mesh around the capsule's up, on top of the mesh's own offset rotation (the skeletal mesh usually faces -90 yaw)
	Mesh->SetRelativeRotation(Pose.bActive ? PoseRotation(Pose) * Base : Base);
	bPoseApplied = Pose.bActive;
}

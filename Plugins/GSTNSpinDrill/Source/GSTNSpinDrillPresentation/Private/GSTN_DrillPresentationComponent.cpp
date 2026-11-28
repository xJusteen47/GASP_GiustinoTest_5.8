// Spin and drill, presentation.

#include "GSTN_DrillPresentationComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/GSTN_DrillMath.h"
#include "MoverComponent.h"
#include "Sim/GSTN_DrillFragment.h"
#include "Sim/GSTN_DrillTickExtension.h"
#include "Sim/GSTN_SpinDrillAttach.h"
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

FGSTN_SpinCuePose UGSTN_DrillPresentationComponent::ComputeCue(const FGSTN_DrillFragment* Fragment, int32 Frame, bool bFloaty, int32 PopFrames, float PopScale)
{
	FGSTN_SpinCuePose Cue;
	if (!Fragment || Frame == INDEX_NONE || !Fragment->IsSpinning())
	{
		return Cue;
	}
	Cue.bVisible = true;
	Cue.bFloaty = bFloaty;
	const int32 Since = Fragment->FramesInPhase(Frame);
	if (PopFrames > 0 && Since >= 0 && Since < PopFrames)
	{
		// Ease out: fast at the start, settled on the ring by the end of the pop
		const float Alpha = static_cast<float>(Since) / static_cast<float>(PopFrames);
		Cue.Scale = FMath::Lerp(PopScale, 1.f, 1.f - FMath::Square(1.f - Alpha));
	}
	return Cue;
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

void UGSTN_DrillPresentationComponent::UpdateCue(const FGSTN_SpinCuePose& Cue, float SpinYawDegrees)
{
	const bool bShow = bShowSpinCue && Cue.bVisible;
	if (!bShow && !bCueShown)
	{
		return;
	}
	AActor* Owner = GetOwner();
	if (bShow && CueMeshes.Num() != FMath::Clamp(CueOrbs, 2, 16) && Owner && Owner->GetRootComponent())
	{
		for (UStaticMeshComponent* Old : CueMeshes)
		{
			if (Old)
			{
				Old->DestroyComponent();
			}
		}
		CueMeshes.Reset();
		// Engine content, always cooked: the cue needs no asset of the project
		UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		CueMaterial = Base ? UMaterialInstanceDynamic::Create(Base, this) : nullptr;
		for (int32 Index = 0; Index < FMath::Clamp(CueOrbs, 2, 16); ++Index)
		{
			UStaticMeshComponent* Orb = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
			Orb->SetStaticMesh(Sphere);
			Orb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Orb->SetCastShadow(false);
			Orb->SetGenerateOverlapEvents(false);
			Orb->SetCanEverAffectNavigation(false);
			Orb->SetUsingAbsoluteRotation(true);
			Orb->SetupAttachment(Owner->GetRootComponent());
			if (CueMaterial)
			{
				Orb->SetMaterial(0, CueMaterial);
			}
			Orb->RegisterComponent();
			CueMeshes.Add(Orb);
		}
		bCueFloatyShown = !Cue.bFloaty;
	}
	if (bShow && CueMaterial && bCueFloatyShown != Cue.bFloaty)
	{
		CueMaterial->SetVectorParameterValue(TEXT("Color"), Cue.bFloaty ? CueFloatColor : CueSpinColor);
		bCueFloatyShown = Cue.bFloaty;
	}
	const FRotator Facing = Owner ? Owner->GetActorRotation() : FRotator::ZeroRotator;
	const float OrbScale = CueOrbSizeCm / 100.f;
	for (int32 Index = 0; Index < CueMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Orb = CueMeshes[Index];
		if (!Orb)
		{
			continue;
		}
		Orb->SetVisibility(bShow);
		if (!bShow)
		{
			continue;
		}
		// The ring turns with the spin (the same angle as the mesh), so its sense reads too
		const float Angle = FMath::DegreesToRadians(Facing.Yaw + SpinYawDegrees + 360.f * Index / CueMeshes.Num());
		const float Radius = CueRadiusCm * Cue.Scale;
		const FVector Offset(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), CueHeightCm);
		// The root may be rotated (the capsule turns with the actor): the offset is taken in world axes, then brought back to the root
		const USceneComponent* Root = Owner->GetRootComponent();
		Orb->SetRelativeLocation(Root ? Root->GetComponentTransform().InverseTransformVectorNoScale(Offset) : Offset);
		Orb->SetWorldScale3D(FVector(OrbScale));
	}
	bCueShown = bShow;
}

void UGSTN_DrillPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* Owner = GetOwner();
	const UGSTN_CombatSimComponent* Combat = Owner ? Owner->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	const FGSTN_CombatSyncState* Sync = Combat ? Combat->GetSyncState() : nullptr;
	const FGSTN_DrillFragment* Fragment = Sync ? FGSTN_SyncFragmentList::Find<FGSTN_DrillFragment>(Sync->Fragments) : nullptr;
	const int32 Frame = Combat ? Combat->GetSimulationFrame() : INDEX_NONE;
	const FGSTN_DrillPresentationPose Pose = ComputePose(Fragment, Frame, SpinRps, Drill, FixedTickHz, WobbleSamples);
	if (Fragment && Fragment->IsSpinning())
	{
		const FGSTN_DrillSimTuning Tuning = FGSTN_SpinDrillAttach::FindOrDefault(Owner);
		const bool bFloaty = FGSTN_DrillTickExtension::WantsLowGravity(Fragment, Frame, Sync->Core.Jump.Phase == EGSTN_JumpPhase::Rising, &Tuning);
		UpdateCue(ComputeCue(Fragment, Frame, bFloaty, CuePopFrames, CuePopScale), Pose.SpinYawDegrees);
	}
	else
	{
		UpdateCue(FGSTN_SpinCuePose(), 0.f);
	}
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

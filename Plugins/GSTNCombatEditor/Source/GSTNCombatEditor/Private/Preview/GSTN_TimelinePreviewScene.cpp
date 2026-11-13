// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Preview/GSTN_TimelinePreviewScene.h"

#include "Animation/DebugSkelMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

FGSTN_TimelinePreviewScene::FGSTN_TimelinePreviewScene()
	: FAdvancedPreviewScene(ConstructionValues())
{
}

FGSTN_TimelinePreviewScene::~FGSTN_TimelinePreviewScene()
{
	ClearSubject();
}

void FGSTN_TimelinePreviewScene::ClearSubject()
{
	for (const TWeakObjectPtr<UMeshComponent>& Attachment : AttachmentComponents)
	{
		if (UMeshComponent* Component = Attachment.Get())
		{
			RemoveComponent(Component);
		}
	}
	AttachmentComponents.Reset();
	if (UDebugSkelMeshComponent* Component = MeshComponent.Get())
	{
		RemoveComponent(Component);
	}
	MeshComponent.Reset();
	if (USceneComponent* Component = PlacementComponent.Get())
	{
		RemoveComponent(Component);
	}
	PlacementComponent.Reset();
	Pose.Bind(nullptr);
	MeshToRoot = FTransform::Identity;
	Capsule = FGSTN_PreviewCapsule();
}

FGSTN_PreviewCapsule FGSTN_TimelinePreviewScene::ComputePlacement(const FBoxSphereBounds& MeshBounds, EAxis::Type MeshForwardAxis, FTransform& OutMeshToRoot)
{
	const FQuat Turn = GSTNPreviewSubject::MeshToRootRotation(MeshForwardAxis);

	// The bounds' box as the turn leaves it (a quarter turn keeps it a box); its middle is where the capsule goes, at the origin's height
	const FBoxSphereBounds Turned = MeshBounds.TransformBy(FTransform(Turn));
	OutMeshToRoot = FTransform(Turn, FVector(0.0, 0.0, -Turned.Origin.Z));

	FGSTN_PreviewCapsule Result;
	Result.HalfHeight = static_cast<float>(FMath::Max(Turned.BoxExtent.Z, 1.0));
	// The narrower of the two horizontal half-sizes, so a character in an A pose (arms out) gets a body-sized capsule, not an arm-span one
	Result.Radius = static_cast<float>(FMath::Clamp(FMath::Min(Turned.BoxExtent.X, Turned.BoxExtent.Y), 1.0, static_cast<double>(Result.HalfHeight)));
	Result.Center = FVector(Turned.Origin.X, Turned.Origin.Y, 0.0);
	return Result;
}

TArray<FText> FGSTN_TimelinePreviewScene::SetSubject(const FGSTN_ResolvedSubject& Subject)
{
	TArray<FText> Problems;
	ClearSubject();
	if (Subject.Mesh == nullptr)
	{
		return Problems;
	}

	UDebugSkelMeshComponent* Main = NewObject<UDebugSkelMeshComponent>(GetTransientPackage());
	Main->SetSkeletalMesh(Subject.Mesh);
	Capsule = ComputePlacement(Subject.Mesh->GetBounds(), Subject.Mesh->GetForwardAxis(), MeshToRoot);
	USceneComponent* Placement = NewObject<USceneComponent>(GetTransientPackage());
	AddComponent(Placement, MeshToRoot);
	AddComponent(Main, FTransform::Identity);
	Main->AttachToComponent(Placement, FAttachmentTransformRules::KeepRelativeTransform);
	Main->SetRelativeTransform(FTransform::Identity);
	PlacementComponent = Placement;
	MeshComponent = Main;
	Pose.Bind(Main, MeshToRoot);

	for (const FGSTN_ResolvedAttachment& Attachment : Subject.Attachments)
	{
		UMeshComponent* Component = nullptr;
		if (USkeletalMesh* Skeletal = Cast<USkeletalMesh>(Attachment.Mesh))
		{
			USkeletalMeshComponent* SkeletalComponent = NewObject<USkeletalMeshComponent>(GetTransientPackage());
			SkeletalComponent->SetSkeletalMesh(Skeletal);
			Component = SkeletalComponent;
		}
		else if (UStaticMesh* Static = Cast<UStaticMesh>(Attachment.Mesh))
		{
			UStaticMeshComponent* StaticComponent = NewObject<UStaticMeshComponent>(GetTransientPackage());
			StaticComponent->SetStaticMesh(Static);
			Component = StaticComponent;
		}
		if (Component == nullptr)
		{
			continue;
		}

		AddComponent(Component, FTransform::Identity);
		FName Socket = Attachment.ParentSocket;
		if (!Socket.IsNone() && !Main->DoesSocketExist(Socket))
		{
			Problems.Add(FText::Format(LOCTEXT("ProblemNoSocket", "The mesh has no bone or socket {0}: {1} hangs from the mesh root instead."),
				FText::FromName(Socket), FText::FromString(Attachment.Mesh->GetName())));
			Socket = NAME_None;
		}
		Component->AttachToComponent(Main, FAttachmentTransformRules::KeepRelativeTransform, Socket);
		// The whole transform, scale included: a longer blade is a scale along one axis
		Component->SetRelativeTransform(Attachment.Relative);
		AttachmentComponents.Add(Component);
	}

	// The floor under the lowest point of what is shown
	const FBoxSphereBounds All = GetSubjectBounds();
	SetFloorOffset(static_cast<float>(-(All.Origin.Z - All.BoxExtent.Z)));
	return Problems;
}

FBoxSphereBounds FGSTN_TimelinePreviewScene::GetSubjectBounds() const
{
	const UDebugSkelMeshComponent* Main = MeshComponent.Get();
	if (Main == nullptr)
	{
		return FBoxSphereBounds(FBox(FVector(-Capsule.Radius, -Capsule.Radius, -Capsule.HalfHeight), FVector(Capsule.Radius, Capsule.Radius, Capsule.HalfHeight)));
	}
	// The scene root is the world origin, so a component's world bounds are root-space bounds
	FBox Box = Main->Bounds.GetBox();
	for (const TWeakObjectPtr<UMeshComponent>& Attachment : AttachmentComponents)
	{
		if (const UMeshComponent* Component = Attachment.Get())
		{
			Box += Component->Bounds.GetBox();
		}
	}
	return FBoxSphereBounds(Box);
}

#undef LOCTEXT_NAMESPACE

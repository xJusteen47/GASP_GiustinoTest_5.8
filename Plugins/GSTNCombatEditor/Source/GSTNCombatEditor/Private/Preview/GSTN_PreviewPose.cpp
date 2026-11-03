// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Preview/GSTN_PreviewPose.h"

#include "Animation/AnimMontage.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "AnimPreviewInstance.h"

void FGSTN_PreviewPose::Bind(UDebugSkelMeshComponent* InComponent, const FTransform& InMeshToRoot)
{
	Component = InComponent;
	MeshToRoot = InMeshToRoot;
	bPlayerConfigured = false;
}

double FGSTN_PreviewPose::ComputePosition(const UAnimMontage* Montage, FName Section, float PlayRate, float TickRate, int32 Frame)
{
	if (Montage == nullptr || TickRate <= 0.f)
	{
		return 0.0;
	}
	double SectionStart = 0.0;
	if (!Section.IsNone())
	{
		const int32 SectionIndex = Montage->GetSectionIndex(Section);
		if (SectionIndex != INDEX_NONE)
		{
			float Start = 0.f;
			float End = 0.f;
			Montage->GetSectionStartAndEndTime(SectionIndex, Start, End);
			SectionStart = Start;
		}
	}
	const double Position = SectionStart + (static_cast<double>(FMath::Max(Frame, 0)) / TickRate) * PlayRate;
	return FMath::Clamp(Position, 0.0, static_cast<double>(Montage->GetPlayLength()));
}

void FGSTN_PreviewPose::SetMontage(UAnimMontage* InMontage, FName InSection, float InPlayRate, float InTickRate)
{
	if (Montage.Get() != InMontage)
	{
		bPlayerConfigured = false;
	}
	Montage = InMontage;
	Section = InSection;
	PlayRate = InPlayRate > 0.f ? InPlayRate : 1.f;
	TickRate = InTickRate > 0.f ? InTickRate : 62.5f;
}

void FGSTN_PreviewPose::ConfigurePlayer(UDebugSkelMeshComponent& Mesh)
{
	// The preview instance plays the montage paused, with no blend-in or blend-out, so the weight is full at every position. With no montage
	// it has no asset and shows the reference pose
	UAnimMontage* Played = Montage.Get();
	Mesh.EnablePreview(true, Played);
	if (Played != nullptr && Mesh.PreviewInstance != nullptr)
	{
		Mesh.PreviewInstance->MontagePreview_PreviewNormal(INDEX_NONE, false);
		Mesh.PreviewInstance->MontagePreview_RemoveBlendOut();
	}
	bPlayerConfigured = true;
}

bool FGSTN_PreviewPose::Evaluate(int32 Frame)
{
	UDebugSkelMeshComponent* Mesh = Component.Get();
	if (Mesh == nullptr || Mesh->PreviewInstance == nullptr)
	{
		return false;
	}

	if (!bPlayerConfigured)
	{
		ConfigurePlayer(*Mesh);
	}

	if (const UAnimMontage* Played = Montage.Get())
	{
		LastPosition = ComputePosition(Played, Section, PlayRate, TickRate, Frame);
		Mesh->PreviewInstance->MontagePreview_JumpToPosition(static_cast<float>(LastPosition));
	}
	else
	{
		LastPosition = 0.0;
	}
	// A zero delta: the update runs once without moving any clock. The refresh evaluates the pose on this thread and publishes it
	Mesh->TickAnimation(0.f, false);
	Mesh->RefreshBoneTransforms();
	return true;
}

bool FGSTN_PreviewPose::GetSocketComponentSpace(FName SocketOrBone, FTransform& OutTransform) const
{
	const UDebugSkelMeshComponent* Mesh = Component.Get();
	if (Mesh == nullptr || SocketOrBone.IsNone() || !Mesh->DoesSocketExist(SocketOrBone))
	{
		return false;
	}
	OutTransform = Mesh->GetSocketTransform(SocketOrBone, RTS_Component);
	return true;
}

bool FGSTN_PreviewPose::GetSocketRootSpace(FName SocketOrBone, FTransform& OutTransform) const
{
	FTransform InComponent;
	if (!GetSocketComponentSpace(SocketOrBone, InComponent))
	{
		return false;
	}
	OutTransform = InComponent * MeshToRoot;
	return true;
}

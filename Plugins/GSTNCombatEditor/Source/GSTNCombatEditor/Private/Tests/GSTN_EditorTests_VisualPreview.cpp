#include "Animation/AnimMontage.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"
#include "Preview/GSTN_PreviewController.h"
#include "Preview/GSTN_TimelineViewportClient.h"
#include "Preview/SGSTN_TimelineViewport.h"
#include "Styling/AppStyle.h"
#include "Tests/GSTN_PreviewTestHelpers.h"
#include "Tests/GSTN_VisualSnapshot.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/Widgets/SGSTN_TimelineToolbarExtra.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"

#if WITH_DEV_AUTOMATION_TESTS

// Offscreen snapshots of the preview viewport and the subject picker (GSTN.Visual.Timeline.Preview*), see Tests/GSTN_VisualSnapshot.h. They need a
// renderer. The viewport is drawn by hand between the Slate passes (an editor viewport is normally drawn by the editor's tick, which a test does
// not run). Content: any montage of the project whose skeleton has a preview mesh, else the engine's own cube with a montage built in memory.

namespace
{
	struct FPreviewSnapshotContent
	{
		TStrongObjectPtr<UAnimMontage> Montage;
		TStrongObjectPtr<USkeletalMesh> Mesh;
		TStrongObjectPtr<UAnimSequence> Sequence;
		TStrongObjectPtr<UAnimMontage> Holder;
		FString Description;
	};

	void FindContent(FPreviewSnapshotContent& Out)
	{
		TArray<FAssetData> Montages;
		IAssetRegistry::GetChecked().GetAssetsByClass(UAnimMontage::StaticClass()->GetClassPathName(), Montages, true);
		Montages.Sort([](const FAssetData& A, const FAssetData& B) { return A.GetSoftObjectPath().LexicalLess(B.GetSoftObjectPath()); });
		for (const FAssetData& Data : Montages)
		{
			UAnimMontage* Candidate = Cast<UAnimMontage>(Data.GetAsset());
			USkeletalMesh* Preview = Candidate && Candidate->GetSkeleton() ? Candidate->GetSkeleton()->GetPreviewMesh(true) : nullptr;
			if (Candidate && Preview && Candidate->GetPlayLength() >= 2.f && Preview->GetRefSkeleton().GetNum() > 8)
			{
				Out.Montage.Reset(Candidate);
				Out.Mesh.Reset(Preview);
				Out.Description = FString::Printf(TEXT("project montage %s on %s"), *Candidate->GetName(), *Preview->GetName());
				return;
			}
		}
		Out.Mesh.Reset(GSTNPreviewTest::LoadEngineCube());
		if (Out.Mesh.IsValid())
		{
			Out.Montage.Reset(GSTNPreviewTest::MakeLinearMontage(*Out.Mesh, Out.Sequence, Out.Holder));
			Out.Description = TEXT("engine cube with a synthetic montage");
		}
	}

	/** A hand to hang the sword from if the skeleton has one (this test may know names; the editor does not), else the bone furthest from the root in the reference pose */
	FName PickHangBone(const USkeletalMesh& Mesh)
	{
		const FReferenceSkeleton& Ref = Mesh.GetRefSkeleton();
		if (Ref.FindBoneIndex(TEXT("hand_r")) != INDEX_NONE)
		{
			return TEXT("hand_r");
		}
		TArray<FTransform> Component;
		Component.SetNum(Ref.GetNum());
		int32 Best = 0;
		double BestDistance = -1.0;
		for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
		{
			const int32 Parent = Ref.GetParentIndex(Bone);
			Component[Bone] = Parent == INDEX_NONE ? Ref.GetRefBonePose()[Bone] : Ref.GetRefBonePose()[Bone] * Component[Parent];
			const double Distance = Component[Bone].GetLocation().Size();
			if (Distance > BestDistance)
			{
				BestDistance = Distance;
				Best = Bone;
			}
		}
		return Ref.GetBoneName(Best);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VisualTimelinePreviewTest, "GSTN.Visual.Timeline.Preview", GSTN_VISUAL_TEST_FLAGS)
bool FGSTN_VisualTimelinePreviewTest::RunTest(const FString& Parameters)
{
	if (!GSTNVisualSnapshot::CanRender(*this))
	{
		return true;
	}
	FPreviewSnapshotContent Content;
	FindContent(Content);
	if (!TestTrue(TEXT("a mesh and a montage to show were found"), Content.Mesh.IsValid() && Content.Montage.IsValid()))
	{
		return true;
	}
	AddInfo(FString::Printf(TEXT("Preview snapshot content: %s"), *Content.Description));

	TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
	Timeline->Data.TotalFrames = 120;
	Timeline->Data.Montage = Content.Montage.Get();
	const TSharedRef<FGSTN_TimelineEditorModel> Model = MakeShared<FGSTN_TimelineEditorModel>(FGSTN_TimelineEditContext::ForAsset(Timeline.Get()));
	Model->SetTickRate(62.5f);
	Model->SetPlayhead(75);

	// An enemy-style subject: any skeletal mesh, with a long thin "sword" (a stretched cylinder) hung from a bone
	const TSharedRef<FGSTN_PreviewController> Controller = FGSTN_PreviewController::CreateUnstored(Model);
	FGSTN_PreviewSubject Subject;
	Subject.Kind = EGSTN_PreviewSubjectKind::SkeletalMesh;
	Subject.Mesh = Content.Mesh.Get();
	FGSTN_PreviewAttachment& Sword = Subject.Attachments.AddDefaulted_GetRef();
	Sword.Mesh = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	Sword.ParentSocket = PickHangBone(*Content.Mesh);
	// The cylinder is 100 cm tall and 100 cm wide: 4 cm thick and 90 cm long along the bone's Z, centred on the bone, so it pierces the hand it hangs from
	Sword.Relative = FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.04, 0.04, 0.9));
	Controller->SetSubject(Subject);
	for (const FText& Problem : Controller->GetProblems())
	{
		AddInfo(FString::Printf(TEXT("Preview problem: %s"), *Problem.ToString()));
	}

	const TSharedRef<SGSTN_TimelineViewport> Viewport = SNew(SGSTN_TimelineViewport).Controller(Controller);
	const TSharedRef<SWidget> Root = SNew(SBorder).BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel")).Padding(0.f)[Viewport];
	const FString Path = GSTNVisualSnapshot::Render(*this, Root, FIntPoint(900, 640), TEXT("timeline-preview-viewport"), [&](int32 Pass)
	{
		if (const TSharedPtr<FGSTN_TimelineViewportClient> Client = Viewport->GetTimelineViewportClient())
		{
			if (Pass == 2)
			{
				Client->FrameSubject();
			}
			Client->Invalidate();
			if (Client->Viewport != nullptr)
			{
				Client->Viewport->Draw(false);
			}
		}
	});
	TestFalse(TEXT("a snapshot was written"), Path.IsEmpty());

	// Without a montage the mesh is in its reference pose, seen from the side: the facing arrow must point where the mesh looks
	Model->ApplyDataEdit([](FGSTN_CombatTimelineData& Data) { Data.Montage.Reset(); }, FText::FromString(TEXT("Clear montage")));
	if (const TSharedPtr<FGSTN_TimelineViewportClient> Client = Viewport->GetTimelineViewportClient())
	{
		Client->SetCameraPreset(EGSTN_PreviewCameraPreset::Side);
	}
	const FString ReferencePath = GSTNVisualSnapshot::Render(*this, Root, FIntPoint(900, 640), TEXT("timeline-preview-viewport-reference-pose"), [&](int32 Pass)
	{
		if (const TSharedPtr<FGSTN_TimelineViewportClient> Client = Viewport->GetTimelineViewportClient())
		{
			Client->Invalidate();
			if (Client->Viewport != nullptr)
			{
				Client->Viewport->Draw(false);
			}
		}
	});
	TestFalse(TEXT("the reference pose snapshot was written"), ReferencePath.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VisualTimelinePreviewToolbarTest, "GSTN.Visual.Timeline.PreviewToolbar", GSTN_VISUAL_TEST_FLAGS)
bool FGSTN_VisualTimelinePreviewToolbarTest::RunTest(const FString& Parameters)
{
	if (!GSTNVisualSnapshot::CanRender(*this))
	{
		return true;
	}
	TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
	const TSharedRef<FGSTN_TimelineEditorModel> Model = MakeShared<FGSTN_TimelineEditorModel>(FGSTN_TimelineEditContext::ForAsset(Timeline.Get()));
	const TSharedRef<FGSTN_PreviewController> Controller = FGSTN_PreviewController::CreateUnstored(Model);
	FGSTN_PreviewSubject Subject;
	Subject.Kind = EGSTN_PreviewSubjectKind::SkeletalMesh;
	Subject.Mesh = GSTNPreviewTest::LoadEngineCube();
	Controller->SetSubject(Subject);

	const TSharedRef<SWidget> Toolbar = SNew(SBorder).BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel")).Padding(FMargin(8.f))
		[
			SNew(SGSTN_TimelineToolbarExtra).Model(Model).Preview(Controller)
		];
	TestFalse(TEXT("a snapshot was written"), GSTNVisualSnapshot::Render(*this, Toolbar, FIntPoint(720, 44), TEXT("timeline-preview-toolbar")).IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

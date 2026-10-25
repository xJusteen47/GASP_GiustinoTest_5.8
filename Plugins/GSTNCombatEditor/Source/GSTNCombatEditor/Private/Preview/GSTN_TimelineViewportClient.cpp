// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Preview/GSTN_TimelineViewportClient.h"

#include "Animation/DebugSkelMeshComponent.h"
#include "Preview/GSTN_PreviewDrawing.h"
#include "Preview/GSTN_TimelinePreviewScene.h"
#include "SEditorViewport.h"
#include "Timeline/GSTN_TimelineEditorModel.h"

FGSTN_TimelineViewportClient::FGSTN_TimelineViewportClient(const TSharedRef<FGSTN_TimelinePreviewScene>& InScene, const TSharedRef<SEditorViewport>& InViewportWidget,
	const TSharedPtr<FGSTN_TimelineEditorModel>& InModel)
	: FEditorViewportClient(nullptr, &InScene.Get(), InViewportWidget)
	, Scene(InScene)
	, Model(InModel)
{
	SetViewportType(LVT_Perspective);
	SetRealtime(false);
	SetViewMode(VMI_Lit);
	ViewFOV = 60.f;
	OverrideNearClipPlane(1.f);
	EngineShowFlags.SetSnap(0);
	EngineShowFlags.SetSeparateTranslucency(true);
	bSetListenerPosition = false;
	SetCameraPreset(Preset);
}

void FGSTN_TimelineViewportClient::SetCameraPreset(EGSTN_PreviewCameraPreset InPreset)
{
	Preset = InPreset;
	FrameSubject();
}

void FGSTN_TimelineViewportClient::FrameSubject()
{
	const TSharedPtr<FGSTN_TimelinePreviewScene> Pinned = Scene.Pin();
	if (!Pinned.IsValid())
	{
		return;
	}
	const FBoxSphereBounds Bounds = Pinned->GetSubjectBounds();
	// A plain camera, not the orbit camera: in orbit mode the engine reads the stored rotation through its own orbit convention (a yaw offset
	// and the pitch as a roll), so a direction set here would not be the direction seen. Alt+drag still orbits around the look-at point
	ToggleOrbitCamera(false);
	const FVector Direction = GSTNPreviewCamera::LookDirection(Preset);
	SetViewRotation(Direction.Rotation());
	SetViewLocation(GSTNPreviewCamera::FrameBounds(Preset, Bounds, ViewFOV));
	SetLookAtLocation(Bounds.Origin);
	FollowedRoot = FVector::ZeroVector;
	Invalidate();
}

void FGSTN_TimelineViewportClient::SetFollowRoot(bool bInFollow)
{
	bFollowRoot = bInFollow;
	Invalidate();
}

void FGSTN_TimelineViewportClient::SetGridShown(bool bShown)
{
	bShowGrid = bShown;
	Invalidate();
}

void FGSTN_TimelineViewportClient::SetRootShown(bool bShown)
{
	bShowRoot = bShown;
	Invalidate();
}

void FGSTN_TimelineViewportClient::SetLit(bool bInLit)
{
	bLit = bInLit;
	SetViewMode(bLit ? VMI_Lit : VMI_Unlit);
	Invalidate();
}

void FGSTN_TimelineViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// Follow root: the camera keeps its offset from the root. The preview mesh does not leave the origin today (root motion is not in the
	// pose, the Movement track is not integrated yet), so this is a no-op until a subject moves; the maths is GSTNPreviewCamera::FollowRoot
	if (bFollowRoot)
	{
		if (const TSharedPtr<FGSTN_TimelinePreviewScene> Pinned = Scene.Pin())
		{
			if (const UDebugSkelMeshComponent* Mesh = Pinned->GetMeshComponent())
			{
				const FVector Root = Mesh->GetComponentLocation() - Pinned->GetMeshToRoot().GetLocation();
				if (!Root.Equals(FollowedRoot, 0.01))
				{
					SetViewLocation(GSTNPreviewCamera::FollowRoot(GetViewLocation(), FollowedRoot, Root));
					SetLookAtLocation(GetLookAtLocation() + (Root - FollowedRoot));
					FollowedRoot = Root;
					Invalidate();
				}
			}
		}
	}
}

void FGSTN_TimelineViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);

	const TSharedPtr<FGSTN_TimelinePreviewScene> Pinned = Scene.Pin();
	if (!Pinned.IsValid())
	{
		return;
	}
	if (bShowGrid)
	{
		const FBoxSphereBounds Bounds = Pinned->GetSubjectBounds();
		GSTNPreviewDrawing::DrawGrid(PDI, static_cast<float>(Bounds.Origin.Z - Bounds.BoxExtent.Z), 50.f, 20);
	}
	if (bShowRoot)
	{
		GSTNPreviewDrawing::DrawRootCapsule(PDI, Pinned->GetRootCapsule());
		GSTNPreviewDrawing::DrawFacingArrow(PDI, Pinned->GetRootCapsule());
	}

	// Hurtboxes, hitboxes and baked markers register here when M4 lands; with none registered this costs a loop over nothing
	FGSTN_PreviewDrawContext Context;
	Context.Model = Model.Pin().Get();
	Context.Scene = Pinned.Get();
	Context.Frame = Context.Model ? Context.Model->GetPlayhead() : 0;
	FGSTN_PreviewOverlayRegistry::DrawAll(Context, PDI);
	ExtraDraw.ExecuteIfBound(PDI);
}

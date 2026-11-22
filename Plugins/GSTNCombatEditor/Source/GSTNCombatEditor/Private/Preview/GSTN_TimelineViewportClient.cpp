// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Preview/GSTN_TimelineViewportClient.h"

#include "Animation/DebugSkelMeshComponent.h"
#include "EditorModeManager.h"
#include "Preview/GSTN_GizmoEdMode.h"
#include "Preview/GSTN_HitboxHitProxy.h"
#include "Preview/GSTN_PreviewDrawing.h"
#include "Preview/GSTN_TimelinePreviewScene.h"
#include "SEditorViewport.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "UnrealWidget.h"

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

	// FWidget draws nothing unless an active mode of this viewport's mode tools uses the transform widget (spike S4): this one does, and nothing else.
	// The widget stays hidden until a gizmo target says there is something to move
	ModeTools->SetDefaultMode(FGSTN_GizmoEdMode::ModeId);
	ModeTools->ActivateDefaultMode();
	ModeTools->SetWidgetMode(UE::Widget::WM_Translate);
	ShowWidget(false);
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

void FGSTN_TimelineViewportClient::SetHitboxesShown(bool bShown)
{
	bShowHitboxes = bShown;
	Invalidate();
}

void FGSTN_TimelineViewportClient::SetGizmoTarget(const TSharedPtr<IGSTN_GizmoTarget>& InTarget)
{
	// A drag in flight belongs to the old target
	if (bGizmoDragging && GizmoTarget.IsValid())
	{
		GizmoTarget->CancelDrag();
	}
	bGizmoDragging = false;
	GizmoTarget = InTarget;
	Invalidate();
}

void FGSTN_TimelineViewportClient::SetGizmoMode(UE::Widget::EWidgetMode Mode)
{
	SetWidgetMode(Mode);
	Invalidate();
}

bool FGSTN_TimelineViewportClient::IsGizmoLocal() const
{
	return FEditorViewportClient::GetWidgetCoordSystemSpace() == COORD_Local;
}

void FGSTN_TimelineViewportClient::ToggleGizmoLocal()
{
	SetWidgetCoordSystemSpace(IsGizmoLocal() ? COORD_World : COORD_Local);
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
	// The widget is drawn by the base Draw, so whether there is one is settled first: a target with nothing selected (or nothing active at the playhead) shows none
	FVector WidgetLocation;
	FQuat WidgetRotation;
	ShowWidget(GizmoTarget.IsValid() && GizmoTarget->GetWidgetFrame(WidgetLocation, WidgetRotation));

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
	Context.bShowHitboxes = bShowHitboxes;
	FGSTN_PreviewOverlayRegistry::DrawAll(Context, PDI);
	ExtraDraw.ExecuteIfBound(PDI);
}

bool FGSTN_TimelineViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
	if (GizmoTarget.IsValid() && EventArgs.Event == IE_Pressed && EventArgs.Viewport != nullptr)
	{
		// RMB held is the fly camera: W is forward there, not Translate
		const bool bFlying = EventArgs.Viewport->KeyState(EKeys::RightMouseButton);
		const bool bCtrl = IsCtrlPressed();
		const bool bAlt = IsAltPressed();
		const bool bShift = IsShiftPressed();
		const FKey Key = EventArgs.Key;
		if (!bFlying && !bAlt)
		{
			if (!bCtrl && !bShift)
			{
				if (Key == EKeys::W)
				{
					SetGizmoMode(UE::Widget::WM_Translate);
					return true;
				}
				if (Key == EKeys::E)
				{
					SetGizmoMode(UE::Widget::WM_Rotate);
					return true;
				}
				if (Key == EKeys::R)
				{
					SetGizmoMode(UE::Widget::WM_Scale);
					return true;
				}
				if (Key == EKeys::K && GizmoTarget->HasKeys())
				{
					GizmoTarget->AddKeyAtPlayhead();
					Invalidate();
					return true;
				}
			}
			if (Key == EKeys::K && bShift && !bCtrl && GizmoTarget->HasKeys())
			{
				GizmoTarget->SetAutoKeyEnabled(!GizmoTarget->IsAutoKeyEnabled());
				Invalidate();
				return true;
			}
			if (Key == EKeys::Tilde && bCtrl)
			{
				ToggleGizmoLocal();
				return true;
			}
		}
	}
	return FEditorViewportClient::InputKey(EventArgs);
}

void FGSTN_TimelineViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	FEditorViewportClient::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);
	if (!GizmoTarget.IsValid() || Key != EKeys::LeftMouseButton || IsAltPressed())
	{
		return;
	}
	// A click on a handle of the gizmo itself selects nothing
	if (HitProxy != nullptr && HitProxy->IsA(HWidgetAxis::StaticGetType()))
	{
		return;
	}
	// The line that was drawn under the cursor is the precise answer; a click inside the volume (no line there) is found by the ray
	if (HitProxy != nullptr && HitProxy->IsA(HGSTN_HitboxProxy::StaticGetType()))
	{
		const HGSTN_HitboxProxy* Proxy = static_cast<const HGSTN_HitboxProxy*>(HitProxy);
		if (GizmoTarget->Pick(Proxy->Track, Proxy->Lane))
		{
			Invalidate();
			return;
		}
	}
	const FViewportCursorLocation Cursor(&View, this, HitX, HitY);
	TArray<GSTNGizmo::FPickCandidate> Candidates;
	GizmoTarget->CollectPickables(Candidates);
	const int32 Picked = GSTNGizmo::PickNearest(Cursor.GetOrigin(), Cursor.GetDirection(), Candidates);
	if (Picked != INDEX_NONE && GizmoTarget->Pick(Candidates[Picked].Track, Candidates[Picked].Lane))
	{
		Invalidate();
	}
}

bool FGSTN_TimelineViewportClient::InputWidgetDelta(FViewport* InViewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale)
{
	// No handle under the mouse: this is the camera's drag, not ours
	if (!GizmoTarget.IsValid() || CurrentAxis == EAxisList::None)
	{
		return false;
	}
	if (bGizmoDragCancelled)
	{
		return true;
	}
	if (!bGizmoDragging)
	{
		// The first movement opens the drag, so a click on a handle that does not move leaves nothing in the undo history. Shift is read once, here
		const bool bOffsetAll = IsShiftPressed();
		if (!GizmoTarget->BeginDrag(bOffsetAll))
		{
			bGizmoDragCancelled = true;
			return true;
		}
		bGizmoDragging = true;
		GizmoDragDelta = GSTNGizmo::FDragDelta();
		GizmoDragDelta.bOffsetAllKeys = bOffsetAll;
	}
	// World-space deltas (the widget converts through GetWidgetCoordSystem): translation and rotation as they are; scale along the widget's own axes
	GizmoDragDelta.Accumulate(Drag, Rot.Quaternion(), Scale);
	GizmoTarget->ApplyDelta(GizmoDragDelta);
	Invalidate();
	return true;
}

void FGSTN_TimelineViewportClient::TrackingStopped()
{
	FEditorViewportClient::TrackingStopped();
	if (bGizmoDragging && GizmoTarget.IsValid())
	{
		GizmoTarget->EndDrag();
	}
	bGizmoDragging = false;
	bGizmoDragCancelled = false;
	Invalidate();
}

void FGSTN_TimelineViewportClient::AbortTracking()
{
	// Escape: the data and the undo history go back to before the mouse went down
	if (bGizmoDragging && GizmoTarget.IsValid())
	{
		GizmoTarget->CancelDrag();
		bGizmoDragging = false;
		bGizmoDragCancelled = true;
	}
	FEditorViewportClient::AbortTracking();
}

FVector FGSTN_TimelineViewportClient::GetWidgetLocation() const
{
	FVector Location;
	FQuat Rotation;
	if (GizmoTarget.IsValid() && GizmoTarget->GetWidgetFrame(Location, Rotation))
	{
		return Location;
	}
	return FEditorViewportClient::GetWidgetLocation();
}

FMatrix FGSTN_TimelineViewportClient::GetWidgetCoordSystem() const
{
	FVector Location;
	FQuat Rotation;
	if (GizmoTarget.IsValid() && GizmoTarget->GetWidgetFrame(Location, Rotation) && GetWidgetCoordSystemSpace() == COORD_Local)
	{
		return FQuatRotationMatrix(Rotation);
	}
	return FMatrix::Identity;
}

ECoordSystem FGSTN_TimelineViewportClient::GetWidgetCoordSystemSpace() const
{
	// Scaling is along the shape's own axes (its extent is per local axis), so that handle is local whatever the toggle says
	return GetWidgetMode() == UE::Widget::WM_Scale ? COORD_Local : FEditorViewportClient::GetWidgetCoordSystemSpace();
}

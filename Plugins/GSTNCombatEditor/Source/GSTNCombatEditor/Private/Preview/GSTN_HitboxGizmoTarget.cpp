// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Preview/GSTN_HitboxGizmoTarget.h"

#include "Animation/DebugSkelMeshComponent.h"
#include "Hits/GSTN_HitShapeUtils.h"
#include "Preview/GSTN_TimelinePreviewScene.h"
#include "Timeline/GSTN_HitboxKeyedAccess.h"
#include "Timeline/GSTN_TimelineEditorModel.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace
{
	FTransform KeyToTransform(const FGSTN_HitboxKey& Key)
	{
		return FTransform(Key.Rotation.Quaternion(), Key.Offset, Key.Scale);
	}
}

FGSTN_HitboxGizmoTarget::FGSTN_HitboxGizmoTarget(const TSharedRef<FGSTN_TimelineEditorModel>& InModel, const TWeakPtr<FGSTN_TimelinePreviewScene>& InScene)
	: Model(InModel)
	, Scene(InScene)
{
}

bool FGSTN_HitboxGizmoTarget::FindSelectedLane(const FGSTN_TimelineEditorModel& Model, FLane& OutLane)
{
	const FGSTN_CombatTimelineData* Data = Model.GetData();
	if (Data == nullptr)
	{
		return false;
	}
	for (const FGSTN_Selection& Item : Model.GetSelection())
	{
		if (!Item.IsKey() || !Data->Tracks.IsValidIndex(Item.TrackIndex))
		{
			continue;
		}
		const FGSTN_TimelineTrack_Hitbox* Track = Data->Tracks[Item.TrackIndex].GetPtr<FGSTN_TimelineTrack_Hitbox>();
		if (Track == nullptr)
		{
			continue;
		}
		FLane Lane;
		Lane.Track = Item.TrackIndex;
		if (FGSTN_HitboxKeyedAccess::Locate(*Track, Item.KeyIndex, Lane.Spec, Lane.KeyInSpec))
		{
			OutLane = Lane;
			return true;
		}
	}
	return false;
}

bool FGSTN_HitboxGizmoTarget::ResolveLiveSocket(const FGSTN_TimelinePreviewScene* Scene, FName Socket, FTransform& OutRootRelative)
{
	const UDebugSkelMeshComponent* Mesh = Scene != nullptr ? Scene->GetMeshComponent() : nullptr;
	if (Mesh == nullptr || Socket.IsNone() || !Mesh->DoesSocketExist(Socket))
	{
		return false;
	}
	OutRootRelative = Mesh->GetSocketTransform(Socket, RTS_World) * GetRootTransform().Inverse();
	return true;
}

const FGSTN_TimelineTrack_Hitbox* FGSTN_HitboxGizmoTarget::GetHitboxTrack(int32 TrackIndex) const
{
	const FGSTN_CombatTimelineData* Data = Model->GetData();
	return (Data != nullptr && Data->Tracks.IsValidIndex(TrackIndex)) ? Data->Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack_Hitbox>() : nullptr;
}

FTransform FGSTN_HitboxGizmoTarget::ComputeSpace(const FGSTN_HitboxSpec& Spec, int32 Frame) const
{
	TOptional<FTransform> Socket;
	if (Spec.Space == EGSTN_HitboxSpace::BakedSocket)
	{
		FTransform Found;
		const FGSTN_CombatTimelineData* Data = Model->GetData();
		if ((Data != nullptr && Data->FindBakedSocket(Spec.Socket, Frame, Found)) || ResolveLiveSocket(Scene.Pin().Get(), Spec.Socket, Found))
		{
			Socket = Found;
		}
	}
	return GSTNGizmo::SpaceTransform(Spec.Space, GetRootTransform(), Socket);
}

bool FGSTN_HitboxGizmoTarget::GetWidgetFrame(FVector& OutLocation, FQuat& OutLocalRotation) const
{
	FLane Lane;
	if (!FindSelectedLane(*Model, Lane))
	{
		return false;
	}
	const FGSTN_TimelineTrack_Hitbox* Track = GetHitboxTrack(Lane.Track);
	const int32 Frame = Model->GetPlayhead();
	// A hitbox outside its window is not drawn and not active: there is nothing to put a gizmo on
	if (Track == nullptr || !Track->Contains(Frame) || !Track->Hitboxes.IsValidIndex(Lane.Spec))
	{
		return false;
	}
	const FGSTN_HitboxSpec& Spec = Track->Hitboxes[Lane.Spec];
	const FTransform World = UGSTN_HitShapeUtils::EvaluateKeys(Spec.Keys, Frame - Track->StartFrame) * ComputeSpace(Spec, Frame);
	OutLocation = World.GetLocation();
	OutLocalRotation = World.GetRotation();
	return true;
}

void FGSTN_HitboxGizmoTarget::CollectPickables(TArray<GSTNGizmo::FPickCandidate>& Out) const
{
	const FGSTN_CombatTimelineData* Data = Model->GetData();
	if (Data == nullptr)
	{
		return;
	}
	const TSharedPtr<FGSTN_TimelinePreviewScene> Pinned = Scene.Pin();
	const FGSTN_TimelinePreviewScene* SceneRaw = Pinned.Get();
	TArray<GSTNGizmo::FActiveHitbox> Active;
	GSTNGizmo::CollectActiveHitboxes(*Data, Model->GetPlayhead(), GetRootTransform(),
		[SceneRaw](FName Socket, FTransform& OutTransform) { return ResolveLiveSocket(SceneRaw, Socket, OutTransform); }, Active);
	for (const GSTNGizmo::FActiveHitbox& Hitbox : Active)
	{
		GSTNGizmo::FPickCandidate& Candidate = Out.AddDefaulted_GetRef();
		Candidate.Track = Hitbox.Track;
		Candidate.Lane = Hitbox.Lane;
		Candidate.Shape = Hitbox.Shape;
		Candidate.Carrier = Hitbox.Space;
	}
}

bool FGSTN_HitboxGizmoTarget::Pick(int32 Track, int32 Lane)
{
	const FGSTN_TimelineTrack_Hitbox* Hitbox = GetHitboxTrack(Track);
	if (Hitbox == nullptr || !Hitbox->Hitboxes.IsValidIndex(Lane))
	{
		return false;
	}
	const TArray<FGSTN_HitboxKey>& Keys = Hitbox->Hitboxes[Lane].Keys;
	const int32 KeyInSpec = GSTNGizmo::FindKeyForPick(Keys, Model->GetPlayhead() - Hitbox->StartFrame);
	if (KeyInSpec != INDEX_NONE)
	{
		// Announces OnKeySelected: the row of the key highlights, and the details panel and any parts hook follow
		Model->SelectKey(Track, FGSTN_HitboxKeyedAccess::ToFlat(*Hitbox, Lane, KeyInSpec));
	}
	else
	{
		// A hitbox with no key has no pose to put a gizmo on; the window is selected so the row shows and K can give it its first key
		const FGSTN_Selection One[] = { FGSTN_Selection::Window(Track) };
		Model->SetSelection(One);
	}
	return true;
}

bool FGSTN_HitboxGizmoTarget::OpenDrag(bool bInOffsetAll, const FText& Description)
{
	if (bDragOpen)
	{
		return false;
	}
	FLane Lane;
	if (!FindSelectedLane(*Model, Lane))
	{
		return false;
	}
	const FGSTN_TimelineTrack_Hitbox* Track = GetHitboxTrack(Lane.Track);
	const int32 Frame = Model->GetPlayhead();
	if (Track == nullptr || !Track->Contains(Frame) || !Track->Hitboxes.IsValidIndex(Lane.Spec))
	{
		return false;
	}
	const FGSTN_HitboxSpec& Spec = Track->Hitboxes[Lane.Spec];
	const int32 RelativeFrame = Frame - Track->StartFrame;
	const GSTNGizmo::FDragKeyResolution Resolution = GSTNGizmo::ResolveDragKey(Spec.Keys, RelativeFrame, Model->IsAutoKeyEnabled());
	if (!bInOffsetAll && Resolution.Action == GSTNGizmo::EDragKeyAction::None)
	{
		return false;
	}
	if (!Model->BeginDrag(Description))
	{
		return false;
	}

	// Taken before anything is made: the path as it is now
	StartShape = Spec.Shape;
	StartKeys = Spec.Keys;
	DragSpace = ComputeSpace(Spec, Frame);
	DragLane = Lane;
	bOffsetAll = bInOffsetAll;
	DragKeyFlat = INDEX_NONE;
	StartKey = UGSTN_HitShapeUtils::EvaluateKeys(Spec.Keys, RelativeFrame);

	if (!bInOffsetAll)
	{
		if (Resolution.Action == GSTNGizmo::EDragKeyAction::CreateAtFrame)
		{
			// Joins the drag's transaction: the new key and the move are one undo step
			DragKeyFlat = Model->AddKey(Lane.Track, Frame, Lane.Spec);
			if (DragKeyFlat == INDEX_NONE)
			{
				Model->CancelDrag();
				return false;
			}
			if (StartKeys.IsEmpty())
			{
				// A hitbox with no keys sits where its space puts it (identity); the adapter's first key is a default sphere place, so it is put back
				StartKey = FTransform::Identity;
				Model->SetKeyTransform(Lane.Track, DragKeyFlat, StartKey);
			}
		}
		else
		{
			DragKeyFlat = FGSTN_HitboxKeyedAccess::ToFlat(*Track, Lane.Spec, Resolution.KeyInLane);
			StartKey = KeyToTransform(StartKeys[Resolution.KeyInLane]);
		}
	}
	bDragOpen = true;
	return true;
}

bool FGSTN_HitboxGizmoTarget::BeginDrag(bool bOffsetAllKeys)
{
	return OpenDrag(bOffsetAllKeys, bOffsetAllKeys ? LOCTEXT("OffsetHitboxKeys", "Offset hitbox keys") : LOCTEXT("MoveHitboxGizmo", "Edit hitbox with gizmo"));
}

bool FGSTN_HitboxGizmoTarget::ApplyDelta(const GSTNGizmo::FDragDelta& Delta)
{
	if (!bDragOpen)
	{
		return false;
	}
	bool bChanged = false;
	if (bOffsetAll)
	{
		if (!Delta.Translation.IsNearlyZero(1e-9))
		{
			const FVector KeyDelta = GSTNGizmo::WorldTranslationToKey(Delta.Translation, DragSpace);
			const int32 Spec = DragLane.Spec;
			bChanged |= Model->ApplyTrackEdit(DragLane.Track, [this, Spec, KeyDelta](FGSTN_TimelineTrack& Edited)
			{
				FGSTN_TimelineTrack_Hitbox& Hitbox = static_cast<FGSTN_TimelineTrack_Hitbox&>(Edited);
				if (Hitbox.Hitboxes.IsValidIndex(Spec))
				{
					TArray<FGSTN_HitboxKey>& Keys = Hitbox.Hitboxes[Spec].Keys;
					for (int32 Index = 0; Index < Keys.Num() && Index < StartKeys.Num(); ++Index)
					{
						Keys[Index].Offset = StartKeys[Index].Offset + KeyDelta;
					}
				}
			}, LOCTEXT("OffsetHitboxKeysEdit", "Offset hitbox keys"));
		}
	}
	else
	{
		bChanged |= Model->SetKeyTransform(DragLane.Track, DragKeyFlat, GSTNGizmo::ApplyDragToKey(StartKey, DragSpace, Delta));
	}

	if (!Delta.Scale.IsNearlyZero(1e-9))
	{
		const FVector Extent = GSTNGizmo::ScaleExtent(StartShape, Delta.Scale);
		const int32 Spec = DragLane.Spec;
		bChanged |= Model->ApplyTrackEdit(DragLane.Track, [Spec, Extent](FGSTN_TimelineTrack& Edited)
		{
			FGSTN_TimelineTrack_Hitbox& Hitbox = static_cast<FGSTN_TimelineTrack_Hitbox&>(Edited);
			if (Hitbox.Hitboxes.IsValidIndex(Spec))
			{
				Hitbox.Hitboxes[Spec].Shape.Extent = Extent;
			}
		}, LOCTEXT("ScaleHitboxExtent", "Scale hitbox"));
	}
	return bChanged;
}

void FGSTN_HitboxGizmoTarget::EndDrag()
{
	if (bDragOpen)
	{
		bDragOpen = false;
		Model->EndDrag();
	}
}

void FGSTN_HitboxGizmoTarget::CancelDrag()
{
	if (bDragOpen)
	{
		bDragOpen = false;
		Model->CancelDrag();
	}
}

bool FGSTN_HitboxGizmoTarget::IsAutoKeyEnabled() const
{
	return Model->IsAutoKeyEnabled();
}

void FGSTN_HitboxGizmoTarget::SetAutoKeyEnabled(bool bEnabled)
{
	Model->SetAutoKey(bEnabled);
}

bool FGSTN_HitboxGizmoTarget::AddKeyAtPlayhead()
{
	// The hitbox of the selected key; with only a hitbox track selected, the model's own choice (its first hitbox)
	FLane Lane;
	int32 TrackIndex = INDEX_NONE;
	int32 LaneIndex = INDEX_NONE;
	if (FindSelectedLane(*Model, Lane))
	{
		TrackIndex = Lane.Track;
		LaneIndex = Lane.Spec;
	}
	else
	{
		for (const FGSTN_Selection& Item : Model->GetSelection())
		{
			if (GetHitboxTrack(Item.TrackIndex) != nullptr)
			{
				TrackIndex = Item.TrackIndex;
				break;
			}
		}
	}
	const FGSTN_TimelineTrack_Hitbox* Track = GetHitboxTrack(TrackIndex);
	if (Track == nullptr)
	{
		return false;
	}
	const int32 Frame = Model->GetPlayhead();
	if (LaneIndex != INDEX_NONE && Track->Hitboxes.IsValidIndex(LaneIndex))
	{
		// A key already on the frame is selected rather than doubled
		const TArray<FGSTN_HitboxKey>& Keys = Track->Hitboxes[LaneIndex].Keys;
		const GSTNGizmo::FDragKeyResolution Existing = GSTNGizmo::ResolveDragKey(Keys, Frame - Track->StartFrame, false);
		if (Existing.Action == GSTNGizmo::EDragKeyAction::UseKeyAtFrame)
		{
			Model->SelectKey(TrackIndex, FGSTN_HitboxKeyedAccess::ToFlat(*Track, LaneIndex, Existing.KeyInLane));
			return false;
		}
	}
	return Model->AddKey(TrackIndex, Frame, LaneIndex) != INDEX_NONE;
}

bool FGSTN_HitboxGizmoTarget::MirrorSelectedKey()
{
	// Mirroring is a one-step edit of the key on the playhead, made like a drag so that a key auto-keyed for it is in the same undo step
	if (!OpenDrag(false, LOCTEXT("MirrorHitboxKey", "Mirror hitbox key")))
	{
		return false;
	}
	const bool bChanged = Model->SetKeyTransform(DragLane.Track, DragKeyFlat, GSTNGizmo::MirrorKeyAcrossRootYZ(StartKey, DragSpace, GetRootTransform()));
	EndDrag();
	return bChanged;
}

FText FGSTN_HitboxGizmoTarget::GetStatusText() const
{
	FLane Lane;
	if (!FindSelectedLane(*Model, Lane))
	{
		return LOCTEXT("GizmoNoKey", "Select a hitbox key (click a hitbox, or a key in the timeline) to move it with the gizmo.");
	}
	const FGSTN_TimelineTrack_Hitbox* Track = GetHitboxTrack(Lane.Track);
	const int32 Frame = Model->GetPlayhead();
	if (Track == nullptr || !Track->Hitboxes.IsValidIndex(Lane.Spec))
	{
		return FText::GetEmpty();
	}
	if (!Track->Contains(Frame))
	{
		return LOCTEXT("GizmoOutsideWindow", "The playhead is outside the window of the selected hitbox: move it inside to use the gizmo.");
	}
	const GSTNGizmo::FDragKeyResolution Resolution = GSTNGizmo::ResolveDragKey(Track->Hitboxes[Lane.Spec].Keys, Frame - Track->StartFrame, Model->IsAutoKeyEnabled());
	switch (Resolution.Action)
	{
	case GSTNGizmo::EDragKeyAction::UseKeyAtFrame:
		return FText::Format(LOCTEXT("GizmoOnKey", "Gizmo: editing the key on frame {0}."), FText::AsNumber(Frame));
	case GSTNGizmo::EDragKeyAction::CreateAtFrame:
		return FText::Format(LOCTEXT("GizmoAutoKey", "Gizmo: no key on frame {0}. Auto-key is on, so a drag adds one."), FText::AsNumber(Frame));
	case GSTNGizmo::EDragKeyAction::EditNearest:
		return FText::Format(LOCTEXT("GizmoNearest", "Gizmo: no key on frame {0}. Auto-key is off, so a drag edits the nearest key (frame {1})."), FText::AsNumber(Frame),
			FText::AsNumber(Track->StartFrame + Track->Hitboxes[Lane.Spec].Keys[Resolution.KeyInLane].Frame));
	default:
		return LOCTEXT("GizmoNoKeyAtAll", "This hitbox has no key and auto-key is off. Turn auto-key on or press K to add one.");
	}
}

#undef LOCTEXT_NAMESPACE

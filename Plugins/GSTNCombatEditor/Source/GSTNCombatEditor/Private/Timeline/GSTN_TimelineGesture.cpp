// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_TimelineGesture.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace
{
	/** Is any part of the track's window selected (the body, an edge)? Key entries do not count */
	bool IsWindowSelected(const FGSTN_TimelineEditorModel& Model, int32 TrackIndex)
	{
		return Model.GetSelection().ContainsByPredicate([TrackIndex](const FGSTN_Selection& Item) { return Item.TrackIndex == TrackIndex && !Item.IsKey(); });
	}

	bool HasKeyAtFrame(const FGSTN_CombatTimelineData& Data, int32 TrackIndex, int32 Frame)
	{
		const int32 KeyCount = FGSTN_TimelineLayout::GetKeyCount(Data, TrackIndex);
		for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
		{
			if (FGSTN_TimelineLayout::GetKeyFrame(Data, TrackIndex, KeyIndex) == Frame)
			{
				return true;
			}
		}
		return false;
	}
}

void FGSTN_TimelineGesture::Reset()
{
	Kind = EGSTN_GestureKind::None;
	AnchorFrame = 0;
	AppliedDelta = 0;
	ResizedTrack = INDEX_NONE;
	MovedTracks.Reset();
	bDuplicated = false;
	InitialTailStart = INDEX_NONE;
	BoxPriorSelection.Reset();
	bBoxAdditive = false;
	bMoved = false;
	bCollapseOnClick = false;
	ClickItem = FGSTN_Selection();
}

bool FGSTN_TimelineGesture::BeginOnHit(FGSTN_TimelineEditorModel& Model, const FGSTN_LaneHit& Hit, double X, bool bExtend, bool bDuplicate)
{
	if (IsActive() || !Hit.IsHit() || Model.GetData() == nullptr)
	{
		return false;
	}

	const FGSTN_Selection Item = Hit.ToSelection();
	const bool bKey = Hit.Kind == EGSTN_LaneHitKind::Key;
	const bool bSelected = bKey ? Model.IsSelected(Item) : IsWindowSelected(Model, Hit.TrackIndex);

	if (bExtend)
	{
		// Shift adds the item to the selection, or takes it out if it is already in: no drag when it went out
		if (bKey)
		{
			Model.ToggleSelection(Item);
		}
		else if (bSelected)
		{
			TArray<FGSTN_Selection> Remaining = Model.GetSelection();
			Remaining.RemoveAll([&Hit](const FGSTN_Selection& Other) { return Other.TrackIndex == Hit.TrackIndex && !Other.IsKey(); });
			Model.SetSelection(Remaining);
		}
		else
		{
			Model.AddToSelection(Item);
		}
		const bool bNowSelected = bKey ? Model.IsSelected(Item) : IsWindowSelected(Model, Hit.TrackIndex);
		if (!bNowSelected)
		{
			return false;
		}
	}
	else if (!bSelected)
	{
		Model.SetSelection({ Item });
	}
	else
	{
		// Pressing on a member of a group keeps the group so it can be dragged; a click that moves nothing narrows it to that member
		bCollapseOnClick = Model.GetSelection().Num() > 1;
		ClickItem = Item;
	}

	switch (Hit.Kind)
	{
	case EGSTN_LaneHitKind::Body:
		Kind = EGSTN_GestureKind::MoveWindows;
		for (const FGSTN_Selection& Other : Model.GetSelection())
		{
			if (!Other.IsKey())
			{
				MovedTracks.AddUnique(Other.TrackIndex);
			}
		}
		break;
	case EGSTN_LaneHitKind::StartEdge:
		Kind = EGSTN_GestureKind::ResizeStart;
		ResizedTrack = Hit.TrackIndex;
		break;
	case EGSTN_LaneHitKind::EndEdge:
		Kind = EGSTN_GestureKind::ResizeEnd;
		ResizedTrack = Hit.TrackIndex;
		break;
	case EGSTN_LaneHitKind::Key:
		Kind = bDuplicate ? EGSTN_GestureKind::DuplicateKeys : EGSTN_GestureKind::MoveKeys;
		break;
	default:
		return false;
	}

	FText Description;
	switch (Kind)
	{
	case EGSTN_GestureKind::MoveWindows:
		Description = LOCTEXT("DragMoveWindows", "Move timeline windows");
		break;
	case EGSTN_GestureKind::ResizeStart:
	case EGSTN_GestureKind::ResizeEnd:
		Description = LOCTEXT("DragResizeWindow", "Resize timeline window");
		break;
	case EGSTN_GestureKind::DuplicateKeys:
		Description = LOCTEXT("DragDuplicateKeys", "Duplicate keys");
		break;
	default:
		Description = LOCTEXT("DragMoveKeys", "Move keys");
		break;
	}

	if (!Model.BeginDrag(Description))
	{
		Reset();
		return false;
	}
	AnchorFrame = FMath::FloorToInt32(Model.ViewTransform.PixelToFrame(X));
	AppliedDelta = 0;
	return true;
}

void FGSTN_TimelineGesture::BeginBoxSelect(FGSTN_TimelineEditorModel& Model, double X, double Y, bool bAdditive)
{
	if (IsActive())
	{
		return;
	}
	Kind = EGSTN_GestureKind::BoxSelect;
	BoxPriorSelection = Model.GetSelection();
	bBoxAdditive = bAdditive;
	BoxStart = FVector2D(X, Y);
	BoxEnd = BoxStart;
	if (!bAdditive)
	{
		Model.ClearSelection();
	}
}

bool FGSTN_TimelineGesture::BeginFirstActionable(FGSTN_TimelineEditorModel& Model)
{
	if (IsActive() || Model.GetData() == nullptr || !Model.BeginDrag(LOCTEXT("DragFirstActionable", "Move first actionable frame")))
	{
		return false;
	}
	Kind = EGSTN_GestureKind::MoveFirstActionable;
	return true;
}

bool FGSTN_TimelineGesture::BeginTailStart(FGSTN_TimelineEditorModel& Model)
{
	const FGSTN_CombatTimelineData* Data = Model.GetData();
	if (IsActive() || Data == nullptr || !Data->Tail.bHasTail || !Model.BeginDrag(LOCTEXT("DragTailStart", "Move cancellable tail start")))
	{
		return false;
	}
	Kind = EGSTN_GestureKind::MoveTailStart;
	InitialTailStart = Data->GetTailStartFrame();
	return true;
}

void FGSTN_TimelineGesture::Update(FGSTN_TimelineEditorModel& Model, double X, double Y)
{
	const FGSTN_CombatTimelineData* Data = Model.GetData();
	if (!IsActive() || Data == nullptr)
	{
		return;
	}

	const double FramePosition = Model.ViewTransform.PixelToFrame(X);
	const int32 CursorFrame = FMath::FloorToInt32(FramePosition);
	const int32 NearestBoundary = FMath::RoundToInt32(FramePosition);

	// Moves the selected keys by the difference between where the mouse is and how far they have gone, and records what the model allowed
	auto MoveSelectedKeys = [&]()
	{
		const int32 Step = (CursorFrame - AnchorFrame) - AppliedDelta;
		if (Step == 0)
		{
			return;
		}
		const TArray<FGSTN_Selection> Keys = Model.GetSelection();
		const int32 Before = FGSTN_TimelineLayout::GetEarliestSelectedKeyFrame(*Data, Keys);
		if (Model.MoveKeys(Keys, Step) > 0)
		{
			const int32 After = FGSTN_TimelineLayout::GetEarliestSelectedKeyFrame(*Data, Model.GetSelection());
			if (Before != INDEX_NONE && After != INDEX_NONE && After != Before)
			{
				AppliedDelta += After - Before;
				bMoved = true;
			}
		}
	};

	switch (Kind)
	{
	case EGSTN_GestureKind::MoveWindows:
	{
		int32 Step = (CursorFrame - AnchorFrame) - AppliedDelta;
		if (Step == 0)
		{
			break;
		}
		// One shared clamp, so the windows keep their distances to each other when the first one reaches the end of the timeline
		const int32 Limit = FGSTN_TimelineEditorModel::GetFrameLimit(*Data);
		int32 Lowest = MIN_int32;
		int32 Highest = MAX_int32;
		for (const int32 TrackIndex : MovedTracks)
		{
			if (const FGSTN_TimelineTrack* Track = Model.GetTrack(TrackIndex))
			{
				int32 Start = 0;
				int32 End = 0;
				FGSTN_TimelineEditorModel::GetTrackSpan(*Track, Start, End);
				Lowest = FMath::Max(Lowest, -Start);
				Highest = FMath::Min(Highest, Limit - End);
			}
		}
		Step = Highest < Lowest ? 0 : FMath::Clamp(Step, Lowest, Highest);
		if (Step != 0)
		{
			for (const int32 TrackIndex : MovedTracks)
			{
				Model.MoveWindow(TrackIndex, Step);
			}
			AppliedDelta += Step;
			bMoved = true;
		}
		break;
	}
	case EGSTN_GestureKind::ResizeStart:
		bMoved |= Model.ResizeWindow(ResizedTrack, EGSTN_WindowEdge::Start, NearestBoundary);
		break;
	case EGSTN_GestureKind::ResizeEnd:
		bMoved |= Model.ResizeWindow(ResizedTrack, EGSTN_WindowEdge::End, NearestBoundary);
		break;
	case EGSTN_GestureKind::MoveKeys:
		MoveSelectedKeys();
		break;
	case EGSTN_GestureKind::DuplicateKeys:
		if (bDuplicated)
		{
			MoveSelectedKeys();
		}
		else if (CursorFrame != AnchorFrame)
		{
			// The copies appear with the first move; from then on they are what the mouse drags
			const TArray<FGSTN_Selection> Keys = Model.GetSelection();
			const int32 Before = FGSTN_TimelineLayout::GetEarliestSelectedKeyFrame(*Data, Keys);
			if (Model.DuplicateKeys(Keys, CursorFrame - AnchorFrame) > 0)
			{
				const int32 After = FGSTN_TimelineLayout::GetEarliestSelectedKeyFrame(*Data, Model.GetSelection());
				bDuplicated = true;
				bMoved = true;
				AppliedDelta = (Before != INDEX_NONE && After != INDEX_NONE) ? After - Before : 0;
			}
		}
		break;
	case EGSTN_GestureKind::BoxSelect:
	{
		BoxEnd = FVector2D(X, Y);
		TArray<FGSTN_Selection> Picked = FGSTN_TimelineLayout::BoxSelect(*Data, Model.ViewTransform, BoxStart.X, BoxStart.Y, BoxEnd.X, BoxEnd.Y);
		if (bBoxAdditive)
		{
			for (const FGSTN_Selection& Item : BoxPriorSelection)
			{
				Picked.AddUnique(Item);
			}
		}
		Model.SetSelection(Picked);
		break;
	}
	case EGSTN_GestureKind::MoveFirstActionable:
		bMoved |= Model.SetFirstActionableFrame(NearestBoundary);
		break;
	case EGSTN_GestureKind::MoveTailStart:
		// Until the handle leaves its frame nothing is written: a tail that follows the first actionable frame keeps following it
		if (bMoved || NearestBoundary != InitialTailStart)
		{
			bMoved |= Model.SetTailStartFrame(NearestBoundary);
		}
		break;
	default:
		break;
	}
}

void FGSTN_TimelineGesture::End(FGSTN_TimelineEditorModel& Model)
{
	if (!IsActive())
	{
		return;
	}
	if (Kind != EGSTN_GestureKind::BoxSelect)
	{
		Model.EndDrag();
	}
	if (bCollapseOnClick && !bMoved)
	{
		Model.SetSelection({ ClickItem });
	}
	Reset();
}

void FGSTN_TimelineGesture::Cancel(FGSTN_TimelineEditorModel& Model)
{
	if (!IsActive())
	{
		return;
	}
	if (Kind == EGSTN_GestureKind::BoxSelect)
	{
		Model.SetSelection(BoxPriorSelection);
	}
	else
	{
		Model.CancelDrag();
	}
	Reset();
}

bool FGSTN_TimelineGesture::GetBox(FVector2D& OutMin, FVector2D& OutMax) const
{
	if (Kind != EGSTN_GestureKind::BoxSelect)
	{
		return false;
	}
	OutMin = FVector2D(FMath::Min(BoxStart.X, BoxEnd.X), FMath::Min(BoxStart.Y, BoxEnd.Y));
	OutMax = FVector2D(FMath::Max(BoxStart.X, BoxEnd.X), FMath::Max(BoxStart.Y, BoxEnd.Y));
	return true;
}

// Actions

int32 FGSTN_TimelineActions::FindKeyedSelectionTrack(const FGSTN_TimelineEditorModel& Model)
{
	const FGSTN_CombatTimelineData* Data = Model.GetData();
	if (Data != nullptr)
	{
		for (const FGSTN_Selection& Item : Model.GetSelection())
		{
			if (FGSTN_TimelineLayout::IsKeyedTrack(*Data, Item.TrackIndex))
			{
				return Item.TrackIndex;
			}
		}
	}
	return INDEX_NONE;
}

bool FGSTN_TimelineActions::AddAtFrame(FGSTN_TimelineEditorModel& Model, int32 TrackIndex, int32 Frame)
{
	const FGSTN_CombatTimelineData* Data = Model.GetData();
	const FGSTN_TimelineTrack* Track = Model.GetTrack(TrackIndex);
	if (Data == nullptr || Track == nullptr || Frame < 0)
	{
		return false;
	}
	if (FGSTN_TimelineLayout::IsKeyedTrack(*Data, TrackIndex))
	{
		return !HasKeyAtFrame(*Data, TrackIndex, Frame) && Model.AddKey(TrackIndex, Frame) != INDEX_NONE;
	}
	const int32 Length = FGSTN_TimelineEditorModel::IsPointTrack(*Track) ? 1 : DefaultWindowFrames;
	return Model.AddTrack(Data->Tracks[TrackIndex].GetScriptStruct(), Frame, Length) != INDEX_NONE;
}

int32 FGSTN_TimelineActions::DeleteSelection(FGSTN_TimelineEditorModel& Model)
{
	const TArray<FGSTN_Selection> Selection = Model.GetSelection();
	if (const int32 Removed = Model.RemoveKeys(Selection))
	{
		return Removed;
	}
	TArray<int32> Tracks;
	for (const FGSTN_Selection& Item : Selection)
	{
		if (!Item.IsKey())
		{
			Tracks.AddUnique(Item.TrackIndex);
		}
	}
	return Model.RemoveTracks(Tracks);
}

bool FGSTN_TimelineActions::AddKeyAtPlayhead(FGSTN_TimelineEditorModel& Model)
{
	const int32 TrackIndex = FindKeyedSelectionTrack(Model);
	const FGSTN_CombatTimelineData* Data = Model.GetData();
	return TrackIndex != INDEX_NONE && Data != nullptr && AddAtFrame(Model, TrackIndex, Model.GetPlayhead());
}

bool FGSTN_TimelineActions::CopySelection(FGSTN_TimelineEditorModel& Model)
{
	return Model.CopyKeys(Model.GetSelection());
}

int32 FGSTN_TimelineActions::PasteAtPlayhead(FGSTN_TimelineEditorModel& Model)
{
	const int32 TrackIndex = FindKeyedSelectionTrack(Model);
	return TrackIndex == INDEX_NONE ? 0 : Model.PasteKeys(TrackIndex, Model.GetPlayhead());
}

int32 FGSTN_TimelineActions::DuplicateSelectionAtPlayhead(FGSTN_TimelineEditorModel& Model)
{
	const FGSTN_CombatTimelineData* Data = Model.GetData();
	if (Data == nullptr)
	{
		return 0;
	}
	const TArray<FGSTN_Selection> Keys = Model.GetSelection();
	const int32 Earliest = FGSTN_TimelineLayout::GetEarliestSelectedKeyFrame(*Data, Keys);
	if (Earliest == INDEX_NONE)
	{
		return 0;
	}
	const int32 Delta = Model.GetPlayhead() - Earliest;
	return Model.DuplicateKeys(Keys, Delta != 0 ? Delta : 1);
}

bool FGSTN_TimelineActions::JumpToAdjacentEdge(FGSTN_TimelineEditorModel& Model, int32 Direction)
{
	const FGSTN_CombatTimelineData* Data = Model.GetData();
	const int32 Edge = Data ? FGSTN_TimelineLayout::FindAdjacentEdge(*Data, Model.GetPlayhead(), Direction) : INDEX_NONE;
	return Edge != INDEX_NONE && Model.SetPlayhead(Edge);
}

#undef LOCTEXT_NAMESPACE

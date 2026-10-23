// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_TimelineEditorModel.h"

#include "Animation/AnimMontage.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ScopedTransaction.h"
#include "Settings/GSTN_CombatEditorSettings.h"
#include "Timeline/GSTN_KeyedTrackAccess.h"
#include "Timeline/GSTN_TimelineTrackEditor.h"
#include "UObject/StructOnScope.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace GSTNTimelineModel
{
	/** The limit of a timeline that has no length yet: nothing to clamp to, so effectively none */
	constexpr int32 UnboundedFrames = MAX_int32 / 4;

	/** Header of the key clipboard text. Bump the number when the format changes so old text is refused, not misread */
	const TCHAR* const ClipboardHeader = TEXT("GSTNKeys1");

	/** One past the last frame a window may end on */
	int32 FrameLimit(const FGSTN_CombatTimelineData& Data)
	{
		return Data.TotalFrames > 0 ? Data.TotalFrames : UnboundedFrames;
	}

	/** Clamp that tolerates an empty range (an already invalid window) by keeping the lower bound */
	int32 ClampRange(int32 Value, int32 Min, int32 Max)
	{
		return Max < Min ? Min : FMath::Clamp(Value, Min, Max);
	}

	/**
	 * Moves a whole track. The weapon switch fires on SwapFrame, not on StartFrame (the player and the validation read SwapFrame),
	 * so SwapFrame is what moves; the window follows it and takes the same frame, which also repairs an asset whose two values
	 * had drifted apart instead of carrying the gap along
	 */
	void ShiftTrack(FGSTN_TimelineTrack& Track, int32 Delta)
	{
		if (Track.GetKind() == EGSTN_TimelineTrackKind::WeaponSwitch)
		{
			FGSTN_TimelineTrack_WeaponSwitch& Switch = static_cast<FGSTN_TimelineTrack_WeaponSwitch&>(Track);
			const int32 Width = Track.EndFrame - Track.StartFrame;
			Switch.SwapFrame += Delta;
			Track.StartFrame = Switch.SwapFrame;
			Track.EndFrame = Track.StartFrame + Width;
			return;
		}
		Track.StartFrame += Delta;
		Track.EndFrame += Delta;
	}

	FGSTN_TimelineTrack* MutableTrack(FGSTN_CombatTimelineData* Data, int32 TrackIndex)
	{
		return (Data != nullptr && Data->Tracks.IsValidIndex(TrackIndex)) ? Data->Tracks[TrackIndex].GetMutablePtr<FGSTN_TimelineTrack>() : nullptr;
	}

	const IGSTN_KeyedTrackAccess* FindAccess(const FGSTN_CombatTimelineData& Data, int32 TrackIndex)
	{
		return Data.Tracks.IsValidIndex(TrackIndex) ? FGSTN_KeyedTrackAccessRegistry::Find(Data.Tracks[TrackIndex].GetScriptStruct()) : nullptr;
	}

	/** A selected key resolved against the data */
	struct FKeyEntry
	{
		int32 Track = INDEX_NONE;
		int32 Key = INDEX_NONE;
		int32 Frame = 0;
	};

	/** The keys of a selection that exist, once each, grouped by track in track order and by key index within a track */
	TArray<FKeyEntry> GatherKeys(const FGSTN_CombatTimelineData& Data, TConstArrayView<FGSTN_Selection> Selection)
	{
		TArray<FKeyEntry> Entries;
		for (const FGSTN_Selection& Item : Selection)
		{
			if (!Item.IsKey())
			{
				continue;
			}
			const IGSTN_KeyedTrackAccess* Access = FindAccess(Data, Item.TrackIndex);
			const FGSTN_TimelineTrack* Track = Access ? Data.Tracks[Item.TrackIndex].GetPtr<FGSTN_TimelineTrack>() : nullptr;
			if (Track == nullptr || Item.KeyIndex < 0 || Item.KeyIndex >= Access->GetKeyCount(*Track))
			{
				continue;
			}
			const bool bKnown = Entries.ContainsByPredicate([&Item](const FKeyEntry& Entry) { return Entry.Track == Item.TrackIndex && Entry.Key == Item.KeyIndex; });
			if (!bKnown)
			{
				Entries.Add({ Item.TrackIndex, Item.KeyIndex, Access->GetKeyFrame(*Track, Item.KeyIndex) });
			}
		}
		Entries.StableSort([](const FKeyEntry& A, const FKeyEntry& B) { return A.Track != B.Track ? A.Track < B.Track : A.Key < B.Key; });
		return Entries;
	}

	/** Shift that keeps every entry on [0, Last], starting from the wanted one */
	int32 ClampKeyDelta(const TArray<FKeyEntry>& Entries, int32 Wanted, int32 LastFrame)
	{
		int32 MinFrame = MAX_int32;
		int32 MaxFrame = MIN_int32;
		for (const FKeyEntry& Entry : Entries)
		{
			MinFrame = FMath::Min(MinFrame, Entry.Frame);
			MaxFrame = FMath::Max(MaxFrame, Entry.Frame);
		}
		const int32 Lowest = -MinFrame;
		const int32 Highest = LastFrame - MaxFrame;
		return Highest < Lowest ? 0 : FMath::Clamp(Wanted, Lowest, Highest);
	}
}

using namespace GSTNTimelineModel;

// Event fires on StartFrame and weapon switch on SwapFrame; both ignore EndFrame (GSTN_TimelineTypes.h), so they have no edges to move
bool FGSTN_TimelineEditorModel::IsPointTrack(const FGSTN_TimelineTrack& Track)
{
	const EGSTN_TimelineTrackKind Kind = Track.GetKind();
	return Kind == EGSTN_TimelineTrackKind::Event || Kind == EGSTN_TimelineTrackKind::WeaponSwitch;
}

void FGSTN_TimelineEditorModel::GetTrackSpan(const FGSTN_TimelineTrack& Track, int32& OutStart, int32& OutEnd)
{
	// The frame a point track fires on: SwapFrame for the weapon switch, the same word the runtime and the validation use
	const int32 FireFrame = (Track.GetKind() == EGSTN_TimelineTrackKind::WeaponSwitch)
		? static_cast<const FGSTN_TimelineTrack_WeaponSwitch&>(Track).SwapFrame
		: Track.StartFrame;
	OutStart = FireFrame;
	OutEnd = IsPointTrack(Track) ? FireFrame + 1 : Track.EndFrame;
}

int32 FGSTN_TimelineEditorModel::GetFrameLimit(const FGSTN_CombatTimelineData& Data)
{
	return FrameLimit(Data);
}

int32 FGSTN_TimelineEditorModel::GetContentEnd(const FGSTN_CombatTimelineData& Data)
{
	int32 End = FMath::Max(Data.FirstActionableFrame, 0);
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Instance : Data.Tracks)
	{
		if (const FGSTN_TimelineTrack* Track = Instance.GetPtr<FGSTN_TimelineTrack>())
		{
			int32 Start = 0;
			int32 TrackEnd = 0;
			GetTrackSpan(*Track, Start, TrackEnd);
			End = FMath::Max(End, TrackEnd);
		}
	}
	return End;
}

int32 FGSTN_TimelineEditorModel::GetPlayableFrames(const FGSTN_CombatTimelineData& Data)
{
	return Data.TotalFrames > 0 ? Data.TotalFrames : FMath::Max(GetContentEnd(Data), MinimumRulerFrames);
}

int32 FGSTN_TimelineEditorModel::ClampPlayheadFrame(const FGSTN_CombatTimelineData& Data, int32 Frame)
{
	return FMath::Clamp(Frame, 0, GetPlayableFrames(Data) - 1);
}

/**
 * The transaction of one mutation. Outside a drag it opens its own scoped transaction; inside a drag it joins the drag's
 * (opened at the first change), so the drag stays one undo step. Records the owner before the caller edits. When it goes out of
 * scope, after the edit, it notifies the owner and the listeners, then the transaction closes.
 */
class FGSTN_TimelineEditorModel::FChangeScope
{
public:
	FChangeScope(FGSTN_TimelineEditorModel& InModel, const FText& Description)
		: Model(InModel)
	{
		if (Model.Drag.IsActive())
		{
			Model.Drag.EnsureTransaction();
		}
		else
		{
			Transaction.Emplace(Description);
		}
		Model.Context.Modify();
	}

	~FChangeScope()
	{
		Model.NotifyDataChanged();
	}

private:
	FGSTN_TimelineEditorModel& Model;
	TOptional<FScopedTransaction> Transaction;
};

FGSTN_TimelineEditorModel::FGSTN_TimelineEditorModel(const FGSTN_TimelineEditContext& InContext)
	: Context(InContext)
{
	TickRate = UGSTN_CombatEditorSettings::GetSimulationTickRate();
	bAutoKey = UGSTN_CombatEditorSettings::Get().bAutoKeyDefault;
}

int32 FGSTN_TimelineEditorModel::GetTotalFrames() const
{
	const FGSTN_CombatTimelineData* Data = Context.Get();
	return Data ? Data->TotalFrames : 0;
}

int32 FGSTN_TimelineEditorModel::GetPlayableFrames() const
{
	const FGSTN_CombatTimelineData* Data = Context.Get();
	return Data ? GetPlayableFrames(*Data) : MinimumRulerFrames;
}

int32 FGSTN_TimelineEditorModel::GetTrackCount() const
{
	const FGSTN_CombatTimelineData* Data = Context.Get();
	return Data ? Data->Tracks.Num() : 0;
}

const FGSTN_TimelineTrack* FGSTN_TimelineEditorModel::GetTrack(int32 TrackIndex) const
{
	const FGSTN_CombatTimelineData* Data = Context.Get();
	return (Data != nullptr && Data->Tracks.IsValidIndex(TrackIndex)) ? Data->Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack>() : nullptr;
}

void FGSTN_TimelineEditorModel::NotifyDataChanged()
{
	// Interactive while a drag runs: listeners that rebuild expensive things wait for the final event of EndDrag
	Context.NotifyChanged(Drag.IsActive() ? EPropertyChangeType::Interactive : EPropertyChangeType::ValueSet);
	OnDataChanged.Broadcast();
}

// Playhead

void FGSTN_TimelineEditorModel::GetEffectivePlayRange(int32& OutFirst, int32& OutEnd) const
{
	const int32 Total = GetPlayableFrames();
	OutFirst = 0;
	OutEnd = Total;
	if (HasPlayRange())
	{
		const int32 First = FMath::Clamp(PlayRangeStart, 0, Total);
		const int32 End = FMath::Clamp(PlayRangeEnd, 0, Total);
		if (End > First)
		{
			OutFirst = First;
			OutEnd = End;
		}
	}
}

void FGSTN_TimelineEditorModel::SetPlayRange(int32 Start, int32 End)
{
	const int32 Total = GetPlayableFrames();
	int32 NewStart = FMath::Clamp(Start, 0, Total);
	int32 NewEnd = FMath::Clamp(End, 0, Total);
	if (NewEnd <= NewStart)
	{
		NewStart = 0;
		NewEnd = 0;
	}
	if (NewStart == PlayRangeStart && NewEnd == PlayRangeEnd)
	{
		return;
	}
	PlayRangeStart = NewStart;
	PlayRangeEnd = NewEnd;
	NotifyViewChanged();
}

bool FGSTN_TimelineEditorModel::SetPlayhead(int32 Frame)
{
	// The length of the timeline, or its content while it has none: a new inline timeline has TotalFrames 0 and must still be scrubbable
	const FGSTN_CombatTimelineData* Data = Context.Get();
	const int32 Clamped = Data ? ClampPlayheadFrame(*Data, Frame) : 0;
	if (Clamped == PlayheadFrame)
	{
		return false;
	}
	PlayheadFrame = Clamped;
	OnPlayheadChanged.Broadcast(PlayheadFrame);
	return true;
}

// Selection

bool FGSTN_TimelineEditorModel::IsSelectionValid(const FGSTN_Selection& Item) const
{
	const FGSTN_CombatTimelineData* Data = Context.Get();
	if (Data == nullptr || !Data->Tracks.IsValidIndex(Item.TrackIndex))
	{
		return false;
	}
	if (!Item.IsKey())
	{
		return true;
	}
	const IGSTN_KeyedTrackAccess* Access = FindAccess(*Data, Item.TrackIndex);
	const FGSTN_TimelineTrack* Track = Data->Tracks[Item.TrackIndex].GetPtr<FGSTN_TimelineTrack>();
	return Access != nullptr && Track != nullptr && Item.KeyIndex >= 0 && Item.KeyIndex < Access->GetKeyCount(*Track);
}

void FGSTN_TimelineEditorModel::CommitSelection(TArray<FGSTN_Selection>&& NewSelection)
{
	if (Selection == NewSelection)
	{
		return;
	}
	Selection = MoveTemp(NewSelection);
	OnSelectionChanged.Broadcast();
}

void FGSTN_TimelineEditorModel::SetSelection(TConstArrayView<FGSTN_Selection> NewSelection)
{
	TArray<FGSTN_Selection> Valid;
	for (const FGSTN_Selection& Item : NewSelection)
	{
		if (IsSelectionValid(Item))
		{
			Valid.AddUnique(Item);
		}
	}
	CommitSelection(MoveTemp(Valid));
}

void FGSTN_TimelineEditorModel::AddToSelection(const FGSTN_Selection& Item)
{
	if (IsSelectionValid(Item) && !Selection.Contains(Item))
	{
		TArray<FGSTN_Selection> New = Selection;
		New.Add(Item);
		CommitSelection(MoveTemp(New));
	}
}

void FGSTN_TimelineEditorModel::ToggleSelection(const FGSTN_Selection& Item)
{
	TArray<FGSTN_Selection> New = Selection;
	if (New.Remove(Item) == 0)
	{
		if (!IsSelectionValid(Item))
		{
			return;
		}
		New.Add(Item);
	}
	CommitSelection(MoveTemp(New));
}

void FGSTN_TimelineEditorModel::SelectKey(int32 TrackIndex, int32 KeyIndex)
{
	const FGSTN_Selection Item = FGSTN_Selection::Key(TrackIndex, KeyIndex);
	if (!IsSelectionValid(Item))
	{
		return;
	}
	CommitSelection({ Item });
	// Announced even when it was already the selection: a click on the selected key in the viewport still has to reach the row and the details panel
	OnKeySelected.Broadcast(Item);
}

void FGSTN_TimelineEditorModel::Revalidate()
{
	TArray<FGSTN_Selection> Valid;
	for (const FGSTN_Selection& Item : Selection)
	{
		if (IsSelectionValid(Item))
		{
			Valid.Add(Item);
		}
	}
	CommitSelection(MoveTemp(Valid));

	const FGSTN_CombatTimelineData* Data = Context.Get();
	const int32 Clamped = Data ? ClampPlayheadFrame(*Data, PlayheadFrame) : 0;
	if (Clamped != PlayheadFrame)
	{
		PlayheadFrame = Clamped;
		OnPlayheadChanged.Broadcast(PlayheadFrame);
	}
}

void FGSTN_TimelineEditorModel::HandleUndoRedo()
{
	Revalidate();
	OnDataChanged.Broadcast();
}

void FGSTN_TimelineEditorModel::RemapSelectionKeys(int32 TrackIndex, const TArray<int32>& OldToNew)
{
	TArray<FGSTN_Selection> Remapped;
	for (FGSTN_Selection Item : Selection)
	{
		if (Item.TrackIndex == TrackIndex && Item.IsKey())
		{
			const int32 NewIndex = OldToNew.IsValidIndex(Item.KeyIndex) ? OldToNew[Item.KeyIndex] : INDEX_NONE;
			if (NewIndex == INDEX_NONE)
			{
				continue;
			}
			Item.KeyIndex = NewIndex;
		}
		Remapped.Add(Item);
	}
	CommitSelection(MoveTemp(Remapped));
}

// Window mutations

int32 FGSTN_TimelineEditorModel::AddTrack(const UScriptStruct* TrackStruct, int32 StartFrame, int32 LengthFrames)
{
	FGSTN_CombatTimelineData* Data = Context.Get();
	const UScriptStruct* Base = FGSTN_TimelineTrack::StaticStruct();
	if (Data == nullptr || TrackStruct == nullptr || TrackStruct == Base || !TrackStruct->IsChildOf(Base))
	{
		return INDEX_NONE;
	}

	const int32 Limit = FrameLimit(*Data);
	const int32 Start = ClampRange(StartFrame, 0, Limit - 1);
	const int32 End = FMath::Min(Start + FMath::Max(LengthFrames, 1), Limit);

	FChangeScope Scope(*this, LOCTEXT("AddTrack", "Add timeline track"));
	TInstancedStruct<FGSTN_TimelineTrack> NewTrack;
	NewTrack.InitializeAsScriptStruct(TrackStruct);
	FGSTN_TimelineTrack& Track = NewTrack.GetMutable();
	Track.StartFrame = Start;
	Track.EndFrame = End;
	// The defaults a kind needs beyond the window (the weapon switch's own copy of its frame) belong to its track editor
	if (const TSharedPtr<const IGSTN_TrackEditor> Editor = FGSTN_TrackEditorRegistry::Get().Find(TrackStruct))
	{
		Editor->InitNewTrack(Track, Start, *Data);
	}

	const int32 NewIndex = Data->Tracks.Add(MoveTemp(NewTrack));
	CommitSelection({ FGSTN_Selection::Window(NewIndex) });
	return NewIndex;
}

int32 FGSTN_TimelineEditorModel::RemoveTracks(TConstArrayView<int32> TrackIndices)
{
	FGSTN_CombatTimelineData* Data = Context.Get();
	if (Data == nullptr)
	{
		return 0;
	}

	TArray<int32> Doomed;
	for (const int32 Index : TrackIndices)
	{
		if (Data->Tracks.IsValidIndex(Index))
		{
			Doomed.AddUnique(Index);
		}
	}
	if (Doomed.IsEmpty())
	{
		return 0;
	}
	Doomed.Sort();

	FChangeScope Scope(*this, LOCTEXT("RemoveTracks", "Remove timeline tracks"));
	for (int32 i = Doomed.Num() - 1; i >= 0; --i)
	{
		Data->Tracks.RemoveAt(Doomed[i]);
	}

	// The tracks after a removed one move up: the selection follows them and forgets the removed ones
	TArray<FGSTN_Selection> Remapped;
	for (FGSTN_Selection Item : Selection)
	{
		if (Doomed.Contains(Item.TrackIndex))
		{
			continue;
		}
		int32 Shift = 0;
		for (const int32 Removed : Doomed)
		{
			Shift += Removed < Item.TrackIndex ? 1 : 0;
		}
		Item.TrackIndex -= Shift;
		Remapped.Add(Item);
	}
	CommitSelection(MoveTemp(Remapped));
	return Doomed.Num();
}

bool FGSTN_TimelineEditorModel::MoveWindow(int32 TrackIndex, int32 DeltaFrames)
{
	FGSTN_CombatTimelineData* Data = Context.Get();
	FGSTN_TimelineTrack* Track = MutableTrack(Data, TrackIndex);
	if (Track == nullptr)
	{
		return false;
	}

	int32 Start = 0;
	int32 End = 0;
	GetTrackSpan(*Track, Start, End);
	const int32 Lowest = -Start;
	const int32 Highest = FrameLimit(*Data) - End;
	const int32 Delta = Highest < Lowest ? 0 : FMath::Clamp(DeltaFrames, Lowest, Highest);
	if (Delta == 0)
	{
		return false;
	}

	FChangeScope Scope(*this, LOCTEXT("MoveWindow", "Move timeline window"));
	ShiftTrack(*Track, Delta);
	return true;
}

bool FGSTN_TimelineEditorModel::ResizeWindow(int32 TrackIndex, EGSTN_WindowEdge Edge, int32 Frame)
{
	FGSTN_CombatTimelineData* Data = Context.Get();
	FGSTN_TimelineTrack* Track = MutableTrack(Data, TrackIndex);
	if (Track == nullptr || IsPointTrack(*Track))
	{
		return false;
	}

	// The window keeps at least one frame and stays inside the timeline
	int32& Moving = (Edge == EGSTN_WindowEdge::Start) ? Track->StartFrame : Track->EndFrame;
	const int32 NewValue = (Edge == EGSTN_WindowEdge::Start)
		? ClampRange(Frame, 0, Track->EndFrame - 1)
		: ClampRange(Frame, Track->StartFrame + 1, FrameLimit(*Data));
	if (NewValue == Moving)
	{
		return false;
	}

	FChangeScope Scope(*this, LOCTEXT("ResizeWindow", "Resize timeline window"));
	Moving = NewValue;
	return true;
}

// Whole-timeline mutations

bool FGSTN_TimelineEditorModel::SetTotalFramesInternal(int32 Frames, const FText& Description)
{
	FGSTN_CombatTimelineData* Data = Context.Get();
	const int32 NewTotal = FMath::Max(Frames, 0);
	if (Data == nullptr || Data->TotalFrames == NewTotal)
	{
		return false;
	}

	FChangeScope Scope(*this, Description);
	Data->TotalFrames = NewTotal;
	// Only the total changes: a window or FirstActionableFrame past the new end stays as written and the validator reports it.
	// The view state, which is not data, is kept inside the new range
	Revalidate();
	return true;
}

bool FGSTN_TimelineEditorModel::SetTotalFrames(int32 Frames)
{
	return SetTotalFramesInternal(Frames, LOCTEXT("SetTotalFrames", "Set timeline length"));
}

bool FGSTN_TimelineEditorModel::SetFirstActionableFrame(int32 Frame)
{
	FGSTN_CombatTimelineData* Data = Context.Get();
	if (Data == nullptr)
	{
		return false;
	}
	const int32 NewFrame = ClampRange(Frame, 0, FrameLimit(*Data));
	if (Data->FirstActionableFrame == NewFrame)
	{
		return false;
	}

	FChangeScope Scope(*this, LOCTEXT("SetFirstActionableFrame", "Move first actionable frame"));
	Data->FirstActionableFrame = NewFrame;
	return true;
}

bool FGSTN_TimelineEditorModel::SetTailStartFrame(int32 Frame)
{
	FGSTN_CombatTimelineData* Data = Context.Get();
	if (Data == nullptr || !Data->Tail.bHasTail)
	{
		return false;
	}
	const int32 NewFrame = ClampRange(Frame, 0, FrameLimit(*Data));
	if (Data->Tail.StartFrame == NewFrame)
	{
		return false;
	}

	FChangeScope Scope(*this, LOCTEXT("SetTailStartFrame", "Move cancellable tail start"));
	Data->Tail.StartFrame = NewFrame;
	return true;
}

int32 FGSTN_TimelineEditorModel::ComputeMontageFrames(const FGSTN_CombatTimelineData& Data, const UAnimMontage* Montage, float InTickRate)
{
	if (Montage == nullptr || Data.MontagePlayRate <= 0.f || InTickRate <= 0.f)
	{
		return INDEX_NONE;
	}

	// The same length the validator compares against TotalFrames (GSTN_TimelineValidation.cpp): the named section if there
	// is one, else the whole montage, divided by the play rate and turned into frames of the tick
	float Seconds = Montage->GetPlayLength();
	if (!Data.MontageSection.IsNone())
	{
		const int32 SectionIndex = Montage->GetSectionIndex(Data.MontageSection);
		if (SectionIndex != INDEX_NONE)
		{
			Seconds = Montage->GetSectionLength(SectionIndex);
		}
	}
	return FMath::Max(FMath::RoundToInt(Seconds / Data.MontagePlayRate * InTickRate), 0);
}

bool FGSTN_TimelineEditorModel::FitTotalFramesToMontage()
{
	const FGSTN_CombatTimelineData* Data = Context.Get();
	if (Data == nullptr)
	{
		return false;
	}
	// The one place the editor loads the montage on its own: the command asked for its length
	const int32 Frames = ComputeMontageFrames(*Data, Data->Montage.LoadSynchronous(), TickRate);
	return Frames != INDEX_NONE && SetTotalFramesInternal(Frames, LOCTEXT("FitTotalFrames", "Fit timeline length to montage"));
}

// Edits that write what the caller says

bool FGSTN_TimelineEditorModel::ApplyTrackEdit(int32 TrackIndex, TFunctionRef<void(FGSTN_TimelineTrack&)> Edit, const FText& Description)
{
	FGSTN_TimelineTrack* Track = MutableTrack(Context.Get(), TrackIndex);
	if (Track == nullptr)
	{
		return false;
	}
	FChangeScope Scope(*this, Description);
	Edit(*Track);
	return true;
}

bool FGSTN_TimelineEditorModel::ApplyDataEdit(TFunctionRef<void(FGSTN_CombatTimelineData&)> Edit, const FText& Description)
{
	FGSTN_CombatTimelineData* Data = Context.Get();
	if (Data == nullptr)
	{
		return false;
	}
	FChangeScope Scope(*this, Description);
	Edit(*Data);
	// The edit may have removed tracks or keys the selection points at
	Revalidate();
	return true;
}

// Keys

int32 FGSTN_TimelineEditorModel::AddKey(int32 TrackIndex, int32 Frame)
{
	FGSTN_CombatTimelineData* Data = Context.Get();
	const IGSTN_KeyedTrackAccess* Access = Data ? FindAccess(*Data, TrackIndex) : nullptr;
	FGSTN_TimelineTrack* Track = MutableTrack(Data, TrackIndex);
	if (Access == nullptr || Track == nullptr || Frame < 0 || Frame >= FrameLimit(*Data))
	{
		return INDEX_NONE;
	}

	int32 NewIndex = INDEX_NONE;
	{
		FChangeScope Scope(*this, LOCTEXT("AddKey", "Add key"));
		NewIndex = Access->AppendKey(*Track, Frame, nullptr);
		TArray<int32> OldToNew;
		Access->SortKeys(*Track, OldToNew);
		RemapSelectionKeys(TrackIndex, OldToNew);
		NewIndex = OldToNew.IsValidIndex(NewIndex) ? OldToNew[NewIndex] : INDEX_NONE;
	}
	if (NewIndex != INDEX_NONE)
	{
		SelectKey(TrackIndex, NewIndex);
	}
	return NewIndex;
}

int32 FGSTN_TimelineEditorModel::MoveKeys(TConstArrayView<FGSTN_Selection> Keys, int32 DeltaFrames)
{
	FGSTN_CombatTimelineData* Data = Context.Get();
	if (Data == nullptr)
	{
		return 0;
	}
	const TArray<FKeyEntry> Entries = GatherKeys(*Data, Keys);
	const int32 LastFrame = Data->TotalFrames > 0 ? Data->TotalFrames - 1 : UnboundedFrames;
	const int32 Delta = Entries.IsEmpty() ? 0 : ClampKeyDelta(Entries, DeltaFrames, LastFrame);
	if (Delta == 0)
	{
		return 0;
	}

	FChangeScope Scope(*this, LOCTEXT("MoveKeys", "Move keys"));
	for (int32 First = 0; First < Entries.Num();)
	{
		const int32 TrackIndex = Entries[First].Track;
		const IGSTN_KeyedTrackAccess* Access = FindAccess(*Data, TrackIndex);
		FGSTN_TimelineTrack* Track = MutableTrack(Data, TrackIndex);
		int32 Next = First;
		for (; Next < Entries.Num() && Entries[Next].Track == TrackIndex; ++Next)
		{
			Access->SetKeyFrame(*Track, Entries[Next].Key, Entries[Next].Frame + Delta);
		}
		TArray<int32> OldToNew;
		Access->SortKeys(*Track, OldToNew);
		RemapSelectionKeys(TrackIndex, OldToNew);
		First = Next;
	}
	return Entries.Num();
}

int32 FGSTN_TimelineEditorModel::DuplicateKeys(TConstArrayView<FGSTN_Selection> Keys, int32 DeltaFrames)
{
	FGSTN_CombatTimelineData* Data = Context.Get();
	if (Data == nullptr)
	{
		return 0;
	}
	const TArray<FKeyEntry> Entries = GatherKeys(*Data, Keys);
	if (Entries.IsEmpty())
	{
		return 0;
	}
	const int32 LastFrame = Data->TotalFrames > 0 ? Data->TotalFrames - 1 : UnboundedFrames;
	const int32 Delta = ClampKeyDelta(Entries, DeltaFrames, LastFrame);

	// Copy every key before the first insertion: appending to a track can move the memory the originals live in
	TArray<TSharedPtr<FStructOnScope>> Copies;
	for (const FKeyEntry& Entry : Entries)
	{
		const IGSTN_KeyedTrackAccess* Access = FindAccess(*Data, Entry.Track);
		const FGSTN_TimelineTrack* Track = Data->Tracks[Entry.Track].GetPtr<FGSTN_TimelineTrack>();
		TSharedPtr<FStructOnScope> Copy = MakeShared<FStructOnScope>(Access->GetKeyStruct());
		Access->GetKeyStruct()->CopyScriptStruct(Copy->GetStructMemory(), Access->GetKey(*Track, Entry.Key));
		Copies.Add(Copy);
	}

	TArray<FGSTN_Selection> Duplicates;
	{
		FChangeScope Scope(*this, LOCTEXT("DuplicateKeys", "Duplicate keys"));
		for (int32 First = 0; First < Entries.Num();)
		{
			const int32 TrackIndex = Entries[First].Track;
			const IGSTN_KeyedTrackAccess* Access = FindAccess(*Data, TrackIndex);
			FGSTN_TimelineTrack* Track = MutableTrack(Data, TrackIndex);
			TArray<int32> NewIndices;
			int32 Next = First;
			for (; Next < Entries.Num() && Entries[Next].Track == TrackIndex; ++Next)
			{
				NewIndices.Add(Access->AppendKey(*Track, Entries[Next].Frame + Delta, Copies[Next]->GetStructMemory()));
			}
			TArray<int32> OldToNew;
			Access->SortKeys(*Track, OldToNew);
			RemapSelectionKeys(TrackIndex, OldToNew);
			for (const int32 NewIndex : NewIndices)
			{
				if (OldToNew.IsValidIndex(NewIndex))
				{
					Duplicates.Add(FGSTN_Selection::Key(TrackIndex, OldToNew[NewIndex]));
				}
			}
			First = Next;
		}
		CommitSelection(MoveTemp(Duplicates));
	}
	return Entries.Num();
}

int32 FGSTN_TimelineEditorModel::RemoveKeys(TConstArrayView<FGSTN_Selection> Keys)
{
	FGSTN_CombatTimelineData* Data = Context.Get();
	if (Data == nullptr)
	{
		return 0;
	}
	const TArray<FKeyEntry> Entries = GatherKeys(*Data, Keys);
	if (Entries.IsEmpty())
	{
		return 0;
	}

	FChangeScope Scope(*this, LOCTEXT("RemoveKeys", "Delete keys"));
	for (int32 First = 0; First < Entries.Num();)
	{
		const int32 TrackIndex = Entries[First].Track;
		const IGSTN_KeyedTrackAccess* Access = FindAccess(*Data, TrackIndex);
		FGSTN_TimelineTrack* Track = MutableTrack(Data, TrackIndex);

		TArray<int32> OldToNew;
		const int32 OldCount = Access->GetKeyCount(*Track);
		for (int32 i = 0; i < OldCount; ++i)
		{
			OldToNew.Add(i);
		}
		int32 Next = First;
		for (; Next < Entries.Num() && Entries[Next].Track == TrackIndex; ++Next)
		{
			OldToNew[Entries[Next].Key] = INDEX_NONE;
		}
		// Highest index first so the lower ones are still where the entries say
		for (int32 i = Next - 1; i >= First; --i)
		{
			Access->RemoveKey(*Track, Entries[i].Key);
		}
		int32 Kept = 0;
		for (int32& Mapped : OldToNew)
		{
			Mapped = (Mapped == INDEX_NONE) ? INDEX_NONE : Kept++;
		}
		RemapSelectionKeys(TrackIndex, OldToNew);
		First = Next;
	}
	return Entries.Num();
}

bool FGSTN_TimelineEditorModel::SetKeyTransform(int32 TrackIndex, int32 KeyIndex, const FTransform& NewTransform)
{
	FGSTN_CombatTimelineData* Data = Context.Get();
	const IGSTN_KeyedTrackAccess* Access = Data ? FindAccess(*Data, TrackIndex) : nullptr;
	FGSTN_TimelineTrack* Track = MutableTrack(Data, TrackIndex);
	FTransform Current;
	if (Access == nullptr || Track == nullptr || KeyIndex < 0 || KeyIndex >= Access->GetKeyCount(*Track) || !Access->GetKeyTransform(*Track, KeyIndex, Current))
	{
		return false;
	}
	if (Current.Equals(NewTransform, 0.f))
	{
		return false;
	}

	FChangeScope Scope(*this, LOCTEXT("SetKeyTransform", "Move hitbox key"));
	return Access->SetKeyTransform(*Track, KeyIndex, NewTransform);
}

FString FGSTN_TimelineEditorModel::CopyKeysToText(TConstArrayView<FGSTN_Selection> Keys) const
{
	const FGSTN_CombatTimelineData* Data = Context.Get();
	if (Data == nullptr)
	{
		return FString();
	}
	TArray<FKeyEntry> Entries = GatherKeys(*Data, Keys);
	if (Entries.IsEmpty())
	{
		return FString();
	}

	// One track per copy: the first key's. Keys of other tracks have a different layout and would not paste back
	const int32 TrackIndex = Entries[0].Track;
	Entries.RemoveAll([TrackIndex](const FKeyEntry& Entry) { return Entry.Track != TrackIndex; });
	Entries.StableSort([](const FKeyEntry& A, const FKeyEntry& B) { return A.Frame < B.Frame; });

	const IGSTN_KeyedTrackAccess* Access = FindAccess(*Data, TrackIndex);
	const FGSTN_TimelineTrack* Track = Data->Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack>();
	const UScriptStruct* KeyStruct = Access->GetKeyStruct();

	FString Text = FString::Printf(TEXT("%s\t%s\t%s"), ClipboardHeader, *Data->Tracks[TrackIndex].GetScriptStruct()->GetName(), *KeyStruct->GetName());
	for (const FKeyEntry& Entry : Entries)
	{
		FString KeyText;
		KeyStruct->ExportText(KeyText, Access->GetKey(*Track, Entry.Key), nullptr, nullptr, PPF_None, nullptr);
		Text += FString::Printf(TEXT("\n%d\t%s"), Entry.Frame - Entries[0].Frame, *KeyText);
	}
	return Text;
}

bool FGSTN_TimelineEditorModel::CopyKeys(TConstArrayView<FGSTN_Selection> Keys) const
{
	const FString Text = CopyKeysToText(Keys);
	if (Text.IsEmpty())
	{
		return false;
	}
	FPlatformApplicationMisc::ClipboardCopy(*Text);
	return true;
}

int32 FGSTN_TimelineEditorModel::PasteKeysFromText(int32 TrackIndex, int32 Frame, const FString& Text)
{
	FGSTN_CombatTimelineData* Data = Context.Get();
	const IGSTN_KeyedTrackAccess* Access = Data ? FindAccess(*Data, TrackIndex) : nullptr;
	FGSTN_TimelineTrack* Track = MutableTrack(Data, TrackIndex);
	if (Access == nullptr || Track == nullptr)
	{
		return 0;
	}

	// The system clipboard may hand back Windows line ends
	TArray<FString> Lines;
	Text.Replace(TEXT("\r\n"), TEXT("\n")).ParseIntoArray(Lines, TEXT("\n"), false);
	TArray<FString> Header;
	if (Lines.Num() < 2)
	{
		return 0;
	}
	Lines[0].ParseIntoArray(Header, TEXT("\t"), false);
	const UScriptStruct* KeyStruct = Access->GetKeyStruct();
	if (Header.Num() != 3 || Header[0] != ClipboardHeader
		|| Header[1] != Data->Tracks[TrackIndex].GetScriptStruct()->GetName() || Header[2] != KeyStruct->GetName())
	{
		return 0;
	}

	// Read every key before changing anything: text that is half good pastes nothing
	struct FPastedKey
	{
		int32 RelativeFrame = 0;
		TSharedPtr<FStructOnScope> Key;
	};
	TArray<FPastedKey> Pasted;
	for (int32 i = 1; i < Lines.Num(); ++i)
	{
		const FString Line = Lines[i].TrimEnd();
		int32 Tab = INDEX_NONE;
		if (Line.IsEmpty())
		{
			continue;
		}
		if (!Line.FindChar(TEXT('\t'), Tab) || !FCString::IsNumeric(*Line.Left(Tab)))
		{
			return 0;
		}
		FPastedKey Entry;
		Entry.RelativeFrame = FCString::Atoi(*Line.Left(Tab));
		Entry.Key = MakeShared<FStructOnScope>(KeyStruct);
		if (KeyStruct->ImportText(*Line.Mid(Tab + 1), Entry.Key->GetStructMemory(), nullptr, PPF_None, GWarn, KeyStruct->GetName()) == nullptr)
		{
			return 0;
		}
		Pasted.Add(MoveTemp(Entry));
	}
	if (Pasted.IsEmpty())
	{
		return 0;
	}

	const int32 LastFrame = Data->TotalFrames > 0 ? Data->TotalFrames - 1 : UnboundedFrames;
	TArray<int32> NewIndices;
	TArray<int32> OldToNew;
	{
		FChangeScope Scope(*this, LOCTEXT("PasteKeys", "Paste keys"));
		for (const FPastedKey& Entry : Pasted)
		{
			NewIndices.Add(Access->AppendKey(*Track, FMath::Clamp(Frame + Entry.RelativeFrame, 0, LastFrame), Entry.Key->GetStructMemory()));
		}
		Access->SortKeys(*Track, OldToNew);
		RemapSelectionKeys(TrackIndex, OldToNew);

		TArray<FGSTN_Selection> PastedSelection;
		for (const int32 NewIndex : NewIndices)
		{
			if (OldToNew.IsValidIndex(NewIndex))
			{
				PastedSelection.Add(FGSTN_Selection::Key(TrackIndex, OldToNew[NewIndex]));
			}
		}
		CommitSelection(MoveTemp(PastedSelection));
	}
	return Pasted.Num();
}

int32 FGSTN_TimelineEditorModel::PasteKeys(int32 TrackIndex, int32 Frame)
{
	FString Text;
	FPlatformApplicationMisc::ClipboardPaste(Text);
	return PasteKeysFromText(TrackIndex, Frame, Text);
}

// Drags

bool FGSTN_TimelineEditorModel::BeginDrag(const FText& Description)
{
	return Drag.Begin(Description, Context);
}

void FGSTN_TimelineEditorModel::EndDrag()
{
	if (!Drag.IsActive())
	{
		return;
	}
	const bool bChanged = Drag.HasTransaction();
	Drag.End();
	if (bChanged)
	{
		// The final, non-interactive notification: what was announced as Interactive during the drag is now settled
		NotifyDataChanged();
	}
}

void FGSTN_TimelineEditorModel::CancelDrag()
{
	if (Drag.Cancel())
	{
		Revalidate();
		OnDataChanged.Broadcast();
	}
}

#undef LOCTEXT_NAMESPACE

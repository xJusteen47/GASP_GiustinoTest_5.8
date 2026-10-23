// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_TimelineSelectionProxy.h"

#include "Timeline/GSTN_TimelineEditorModel.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

void UGSTN_TimelineSelectionProxy::Bind(const TSharedPtr<FGSTN_TimelineEditorModel>& InModel, int32 InTrackIndex)
{
	Unbind();
	const FGSTN_CombatTimelineData* Data = InModel.IsValid() ? InModel->GetData() : nullptr;
	if (Data == nullptr || !Data->Tracks.IsValidIndex(InTrackIndex))
	{
		return;
	}
	Model = InModel;
	TrackIndex = InTrackIndex;
	Track = Data->Tracks[InTrackIndex];
}

void UGSTN_TimelineSelectionProxy::Unbind()
{
	// An interactive edit that never saw its committed change must not leave the model's drag open: it would swallow every later edit
	if (bOwnsDrag)
	{
		if (const TSharedPtr<FGSTN_TimelineEditorModel> Pinned = Model.Pin())
		{
			Pinned->EndDrag();
		}
		bOwnsDrag = false;
	}
	Track.Reset();
	TrackIndex = INDEX_NONE;
	Model.Reset();
}

bool UGSTN_TimelineSelectionProxy::Pull()
{
	const TSharedPtr<FGSTN_TimelineEditorModel> Pinned = Model.Pin();
	const FGSTN_CombatTimelineData* Data = Pinned.IsValid() ? Pinned->GetData() : nullptr;
	if (Data == nullptr || !Data->Tracks.IsValidIndex(TrackIndex))
	{
		const bool bWasBound = Track.IsValid();
		Unbind();
		return bWasBound;
	}
	const TInstancedStruct<FGSTN_TimelineTrack>& Current = Data->Tracks[TrackIndex];
	if (Track.IsValid() && Track == Current)
	{
		return false;
	}
	Track = Current;
	return true;
}

bool UGSTN_TimelineSelectionProxy::Commit(EPropertyChangeType::Type ChangeType)
{
	const TSharedPtr<FGSTN_TimelineEditorModel> Pinned = Model.Pin();
	const FGSTN_CombatTimelineData* Data = Pinned.IsValid() ? Pinned->GetData() : nullptr;
	if (Data == nullptr || !Track.IsValid() || !Data->Tracks.IsValidIndex(TrackIndex) || Data->Tracks[TrackIndex].GetScriptStruct() != Track.GetScriptStruct())
	{
		return false;
	}

	const FText Description = LOCTEXT("EditTrackProperties", "Edit timeline track");
	const bool bInteractive = ChangeType == EPropertyChangeType::Interactive;
	bool bChanged = false;
	if (!(Data->Tracks[TrackIndex] == Track))
	{
		if (bInteractive && !Pinned->IsDragging())
		{
			bOwnsDrag = Pinned->BeginDrag(Description);
		}
		bChanged = Pinned->ApplyTrackEdit(TrackIndex, [this](FGSTN_TimelineTrack& Destination)
		{
			Track.GetScriptStruct()->CopyScriptStruct(&Destination, Track.GetMemory());
		}, Description);
	}
	if (!bInteractive && bOwnsDrag)
	{
		Pinned->EndDrag();
		bOwnsDrag = false;
	}
	return bChanged;
}

TSharedPtr<FStructOnScope> UGSTN_TimelineSelectionProxy::MakeStructData()
{
	if (!Track.IsValid())
	{
		return nullptr;
	}
	return MakeShared<FStructOnScope>(Track.GetScriptStruct(), Track.GetMutableMemory());
}

#undef LOCTEXT_NAMESPACE

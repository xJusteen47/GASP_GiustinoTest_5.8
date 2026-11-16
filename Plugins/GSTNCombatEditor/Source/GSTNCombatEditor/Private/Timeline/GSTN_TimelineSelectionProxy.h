// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "UObject/Object.h"
#include "UObject/StructOnScope.h"
#include "GSTN_TimelineSelectionProxy.generated.h"

class FGSTN_TimelineEditorModel;

/**
 * What the details panel edits (combat-m5-editor 5.1): a transient copy of the selected track, so the panel never holds a pointer
 * into the timeline's array (an add, a delete or an undo moves the memory it would point at) and every edit goes back through
 * the model's ApplyTrackEdit (one transaction, the owner notified) instead of being written by the property editor.
 *
 * The flow:  Bind() on selection  ->  Pull() when the data changed behind the panel  ->  the details view edits the copy
 * ->  Commit() applies the copy to the track through the model.
 *
 * An interactive change (a slider being dragged) opens a model drag, so the whole drag is one undo step like the gestures of the
 * lanes; the committed change (the mouse released, Enter pressed) closes it.
 *
 * The copy covers the whole track struct, so a field the panel does not show is carried unchanged, and the keys of a keyed track
 * (E5b) are edited by their own gestures, not here.
 */
UCLASS(Transient)
class UGSTN_TimelineSelectionProxy : public UObject
{
	GENERATED_BODY()

public:
	/** Points the proxy at a track of the model and copies it. A bad index unbinds */
	void Bind(const TSharedPtr<FGSTN_TimelineEditorModel>& InModel, int32 InTrackIndex);

	/** Forgets the track: the details view then shows nothing */
	void Unbind();

	bool IsBound() const { return Track.IsValid() && Model.IsValid(); }
	int32 GetTrackIndex() const { return TrackIndex; }

	/** The copy being edited, null when unbound */
	const FGSTN_TimelineTrack* GetTrackCopy() const { return Track.GetPtr<FGSTN_TimelineTrack>(); }
	const UScriptStruct* GetTrackStruct() const { return Track.GetScriptStruct(); }

	/**
	 * Re-reads the track from the model. Returns true when the copy changed (the details view must then be told), false when it
	 * already matched or the track is gone, in which case the proxy unbinds.
	 */
	bool Pull();

	/**
	 * Writes the copy to the track through ApplyTrackEdit. Returns whether the data changed. Does nothing when the copy equals the
	 * track. Interactive: the change joins a drag that is opened on the first one and closed by the next committed change.
	 */
	bool Commit(EPropertyChangeType::Type ChangeType = EPropertyChangeType::ValueSet);

	/** The copy as the struct the details view shows. It points into this proxy, so the proxy must outlive the view's use of it */
	TSharedPtr<FStructOnScope> MakeStructData();

private:
	/** The copy of the track. A UPROPERTY so a hard reference inside a track (none today) is seen by the collector */
	UPROPERTY()
	TInstancedStruct<FGSTN_TimelineTrack> Track;

	UPROPERTY()
	int32 TrackIndex = INDEX_NONE;

	TWeakPtr<FGSTN_TimelineEditorModel> Model;

	/** This proxy opened the model's drag for an interactive change and has not closed it yet */
	bool bOwnsDrag = false;
};

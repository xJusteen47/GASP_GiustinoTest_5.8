// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineTypes.h"

class UScriptStruct;

/**
 * How the editor model reaches the keys of a track that has them (the hitbox track of M4, with its position, rotation and
 * shape over time) without knowing the track's struct. The model owns the gestures (move, duplicate, copy, paste, undo,
 * clamping, selection) and the track's adapter owns the layout of the keys, so the keyed tracks that arrive with M4 and
 * M4b plug in with one registration and no change to the model.
 *
 * A track may keep its keys in several lanes (the hitbox track: one lane per hitbox spec, each with its own path). Key indices stay flat over
 * the track, lane after lane, each lane sorted by frame, so a selection is still (track, key index). The lane API below has defaults for a track
 * with one lane, so an adapter that has none ignores it.
 *
 * Contract:
 *  - Frames are absolute timeline frames. An adapter whose keys store a frame relative to the track's window converts.
 *  - Nothing here opens a transaction, notifies or clamps: the model does, so every adapter behaves the same way.
 *  - Reads never change the track. SetKeyFrame and AppendKey leave the keys unsorted; the model calls SortKeys afterwards and
 *    uses the returned mapping to keep its selection on the same keys.
 */
class IGSTN_KeyedTrackAccess
{
public:
	virtual ~IGSTN_KeyedTrackAccess() = default;

	/** The struct of one key. The clipboard text of a key is this struct's ExportText */
	virtual const UScriptStruct* GetKeyStruct() const = 0;

	virtual int32 GetKeyCount(const FGSTN_TimelineTrack& Track) const = 0;

	/** Memory of the key, of type GetKeyStruct(). Valid until the track is next edited */
	virtual const void* GetKey(const FGSTN_TimelineTrack& Track, int32 KeyIndex) const = 0;

	virtual int32 GetKeyFrame(const FGSTN_TimelineTrack& Track, int32 KeyIndex) const = 0;

	virtual void SetKeyFrame(FGSTN_TimelineTrack& Track, int32 KeyIndex, int32 Frame) const = 0;

	/**
	 * Appends a key at Frame and returns its index. KeyData is a copy of a key (duplicate, paste); when null the new key's
	 * values are interpolated from the neighbouring keys at Frame by the same pure function the runtime evaluates, so a key
	 * added on the curve does not change the curve.
	 */
	virtual int32 AppendKey(FGSTN_TimelineTrack& Track, int32 Frame, const void* KeyData) const = 0;

	/** How many lanes the track keeps its keys in; the lane of a key is GetKeyLane. 1 for a track with one path */
	virtual int32 GetLaneCount(const FGSTN_TimelineTrack& Track) const { return 1; }

	virtual int32 GetKeyLane(const FGSTN_TimelineTrack& Track, int32 KeyIndex) const { return 0; }

	/**
	 * AppendKey into a lane. The returned index is where the key is in the track after the call, and the keys of later lanes have moved up by one:
	 * a caller that appends several keys does it in ascending lane order, so the indices it holds stay valid. Default: the only lane.
	 */
	virtual int32 AppendKeyInLane(FGSTN_TimelineTrack& Track, int32 Lane, int32 Frame, const void* KeyData) const { return AppendKey(Track, Frame, KeyData); }

	virtual void RemoveKey(FGSTN_TimelineTrack& Track, int32 KeyIndex) const = 0;

	/** Stable sort by frame. OutOldToNew has one entry per key: where the key that was at index i is now */
	virtual void SortKeys(FGSTN_TimelineTrack& Track, TArray<int32>& OutOldToNew) const = 0;

	/** The key's pose in root space for the gizmo (E7). False for a track whose keys have none */
	virtual bool GetKeyTransform(const FGSTN_TimelineTrack& Track, int32 KeyIndex, FTransform& OutTransform) const { return false; }

	/** False when the key has no pose */
	virtual bool SetKeyTransform(FGSTN_TimelineTrack& Track, int32 KeyIndex, const FTransform& NewTransform) const { return false; }
};

/** Which adapter serves which track struct. Registered by the module that knows the keyed track (the hitbox track editor), removed on shutdown. */
class GSTNCOMBATEDITOR_API FGSTN_KeyedTrackAccessRegistry
{
public:
	static void Register(const UScriptStruct* TrackStruct, TSharedRef<const IGSTN_KeyedTrackAccess> Access);
	static void Unregister(const UScriptStruct* TrackStruct);

	/** The adapter for the track struct or its nearest registered base, null when the track has no keys */
	static const IGSTN_KeyedTrackAccess* Find(const UScriptStruct* TrackStruct);
};

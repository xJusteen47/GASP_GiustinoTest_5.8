// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_KeyedTrackAccess.h"
#include "Timeline/GSTN_TimelineTypes.h"

/**
 * How the editor model reaches the keys of a hitbox track (E5b). A hitbox track is one window with several hitboxes (FGSTN_HitboxSpec), each with
 * its own path of keys, so the keys are kept in lanes, one per hitbox:
 *
 *  - Key indices are flat over the track: the keys of hitbox 0 in order, then those of hitbox 1, and so on. A selection is (track, flat index).
 *  - The frame the model sees is absolute. A hitbox key stores its frame counted from the track's first frame (FGSTN_HitboxKey), so a track moves
 *    without re-keying; this adapter converts both ways and nothing else in the editor knows.
 *  - A key added on the curve gets the values the runtime would evaluate there (UGSTN_HitShapeUtils::EvaluateKeys, the same function the hit
 *    code reads), so adding a key does not change the path. Auto-key of the gizmos (E7) relies on it.
 *  - A new key in a track with no hitbox creates the first one (a sphere with default hit data).
 *
 * The one thing the contract of IGSTN_KeyedTrackAccess does not give: GetKey returns a pointer into the track and stays valid until the next edit,
 * which holds here because the keys live in the specs' arrays.
 */
class FGSTN_HitboxKeyedAccess final : public IGSTN_KeyedTrackAccess
{
public:
	/** The hitbox (lane) and the key inside it that a flat index stands for. False when the index is out of range */
	static bool Locate(const FGSTN_TimelineTrack_Hitbox& Track, int32 FlatIndex, int32& OutSpec, int32& OutKeyInSpec);

	/** The flat index of a key of a hitbox */
	static int32 ToFlat(const FGSTN_TimelineTrack_Hitbox& Track, int32 Spec, int32 KeyInSpec);

	/** A hitbox of a new track: a small sphere a little in front of the root, with one key on the first frame */
	static FGSTN_HitboxSpec MakeDefaultSpec();

	virtual const UScriptStruct* GetKeyStruct() const override;
	virtual int32 GetKeyCount(const FGSTN_TimelineTrack& Track) const override;
	virtual const void* GetKey(const FGSTN_TimelineTrack& Track, int32 KeyIndex) const override;
	virtual int32 GetKeyFrame(const FGSTN_TimelineTrack& Track, int32 KeyIndex) const override;
	virtual void SetKeyFrame(FGSTN_TimelineTrack& Track, int32 KeyIndex, int32 Frame) const override;
	virtual int32 AppendKey(FGSTN_TimelineTrack& Track, int32 Frame, const void* KeyData) const override;
	virtual int32 GetLaneCount(const FGSTN_TimelineTrack& Track) const override;
	virtual int32 GetKeyLane(const FGSTN_TimelineTrack& Track, int32 KeyIndex) const override;
	virtual int32 AppendKeyInLane(FGSTN_TimelineTrack& Track, int32 Lane, int32 Frame, const void* KeyData) const override;
	virtual void RemoveKey(FGSTN_TimelineTrack& Track, int32 KeyIndex) const override;
	virtual void SortKeys(FGSTN_TimelineTrack& Track, TArray<int32>& OutOldToNew) const override;
	virtual bool GetKeyTransform(const FGSTN_TimelineTrack& Track, int32 KeyIndex, FTransform& OutTransform) const override;
	virtual bool SetKeyTransform(FGSTN_TimelineTrack& Track, int32 KeyIndex, const FTransform& NewTransform) const override;
};

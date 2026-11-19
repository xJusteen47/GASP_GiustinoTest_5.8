// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"

class FGSTN_TimelineEditorModel;
struct FGSTN_CombatTimelineData;

/**
 * One row of the hitbox outliner: a hitbox group (a Hitbox track, Spec INDEX_NONE) or one hitbox inside it (Spec = its index in the
 * track's Hitboxes). Indices are positions in the data, like the model's selection: rebuilt after every change of the data.
 */
struct FGSTN_HitboxOutlineItem
{
	int32 Track = INDEX_NONE;
	int32 Spec = INDEX_NONE;

	FGSTN_HitboxOutlineItem() = default;
	FGSTN_HitboxOutlineItem(int32 InTrack, int32 InSpec) : Track(InTrack), Spec(InSpec) {}

	bool IsGroup() const { return Track != INDEX_NONE && Spec == INDEX_NONE; }
	bool IsValid() const { return Track != INDEX_NONE; }
	bool operator==(const FGSTN_HitboxOutlineItem& Other) const { return Track == Other.Track && Spec == Other.Spec; }
};

/**
 * What the hitbox outliner of the timeline editor shows and does, without Slate, so the tests run it headless (owner 2026-11-13: the
 * hitboxes of a move are confusing in the viewport alone; a list of groups and their hitboxes that follows the viewport's selection).
 *
 *  - Groups are the Hitbox tracks in data order, numbered from 1 among the Hitbox tracks only (the lane of the timeline counts the same way).
 *  - Focus selects like a click in the viewport does (the key on the playhead, else the nearest one, so the gizmo appears), and first moves
 *    the playhead inside the group's window when it is outside: a hitbox is only drawn on the frames its group is open.
 *  - FindSelected is the other direction: the row of what the model has selected (a key of a hitbox, or the window of a group).
 */
namespace GSTNHitboxOutline
{
	/** Every group, each followed by its hitboxes */
	TArray<FGSTN_HitboxOutlineItem> Build(const FGSTN_CombatTimelineData& Data);

	/** "Group 2" and "Hitbox 1": the name of the row, 1-based */
	FText GetName(const FGSTN_CombatTimelineData& Data, const FGSTN_HitboxOutlineItem& Item);

	/** The second column: frames and count for a group ("frames 14-18, 2 hitboxes, part Sword"), shape and space for a hitbox ("Sphere 70 cm, socket hand_r") */
	FText GetSummary(const FGSTN_CombatTimelineData& Data, const FGSTN_HitboxOutlineItem& Item);

	/** The row the model's selection points at (the first selected item that is a hitbox key or a Hitbox track's window). False when none */
	bool FindSelected(const FGSTN_TimelineEditorModel& Model, FGSTN_HitboxOutlineItem& OutItem);

	/** Selects the row's group or hitbox in the model and puts the playhead where it is drawn (see the namespace comment). False when the row no longer exists */
	bool Focus(FGSTN_TimelineEditorModel& Model, const FGSTN_HitboxOutlineItem& Item);
}

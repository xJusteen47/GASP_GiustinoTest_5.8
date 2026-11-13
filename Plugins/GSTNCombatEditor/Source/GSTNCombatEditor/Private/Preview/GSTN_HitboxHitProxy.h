// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "HitProxies.h"

/**
 * What the viewport's hit-testing render returns for a hitbox (or a hurtbox in the hurtbox set editor): which shape was drawn under the cursor. The
 * wireframe is drawn with this proxy set, so a click on a line selects the shape without a ray; a click inside a shape's volume is found by the ray
 * test of GSTNGizmo::PickNearest instead (the viewport client does both). Track and Lane mean what the gizmo target says: a track and the hitbox
 * inside it, or 0 and the position of a hurtbox in its set.
 *
 * Priority World, below the widget's own proxies (UI), so the gizmo wins where a handle and a line overlap.
 */
struct HGSTN_HitboxProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	int32 Track = INDEX_NONE;
	int32 Lane = INDEX_NONE;

	HGSTN_HitboxProxy(int32 InTrack, int32 InLane)
		: HHitProxy(HPP_World)
		, Track(InTrack)
		, Lane(InLane)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor() override { return EMouseCursor::Crosshairs; }
};

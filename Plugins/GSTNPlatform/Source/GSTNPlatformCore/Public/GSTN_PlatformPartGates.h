// Platform core: pure data and math, no world, no Mover.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_PlatformShape.h"

/**
 * Which parts gate each shape of a platform, as bits of a part mask (traversal-surfaces.md 4.4): the part a shape names plus every
 * ancestor of that part, since a child cannot stay on a body whose parent limb broke. A shape whose gate is 0 is body: it follows
 * the platform only. Part indices are 0..15, the bits of the parts' BrokenMask.
 *
 * Kept free of any parts type (indices and parent indices only) so the platform plugin does not depend on GSTNParts: the bridges
 * resolve names to indices (GSTNPlatformParts for fighters, GSTNPlatformMinion for minions, plan T8) and both compute the shape mask
 * here, the same way.
 */
struct GSTNPLATFORMCORE_API FGSTN_PlatformPartGates
{
	/** Part bits gating shape id i; 0 = body */
	uint16 Gates[GSTNPlatform::MaxShapes] = {};

	/** The part's bit and its ancestors' bits. ParentIndices[i] is part i's parent or INDEX_NONE; a cycle or a bad index stops the walk */
	static uint16 MakeGate(int32 PartIndex, TConstArrayView<int32> ParentIndices);

	/**
	 * Shapes allowed by a broken mask: bit i set unless a part in Gates[i] is broken. Ids without a gate stay set (body shapes, and ids
	 * the spec does not use, which the platform ignores). What UGSTN_EnemyPlatformComponent::SetShapeMask takes
	 */
	uint8 ComputeEnabledShapes(uint16 BrokenMask) const;

	bool operator==(const FGSTN_PlatformPartGates& Other) const { return FMemory::Memcmp(Gates, Other.Gates, sizeof(Gates)) == 0; }
	bool operator!=(const FGSTN_PlatformPartGates& Other) const { return !(*this == Other); }
};

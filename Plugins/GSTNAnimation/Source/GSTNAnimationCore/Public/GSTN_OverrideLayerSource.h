// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GSTN_OverrideLayerSource.generated.h"

/**
 * The override slots of the main graph (anim-layer-skid-wallslide-piano L1): pose N of its "Blend Poses by int" is the Linked Anim Layer
 * of slot N, 0 is the base pose. The numbers are the contract between the layers and the setup script that builds the seam
 * (Tools/GAS/setup_anim_layers.py, SLOTS): a new kind of override layer takes the next number there, the graph is regenerated, and no other
 * layer changes. When several layers want the pose, the highest priority wins: by default wall > skid > run start.
 */
namespace GSTNOverrideSlot
{
	constexpr int32 None = 0;
	constexpr int32 Wall = 1;
	constexpr int32 Skid = 2;
	constexpr int32 RunStart = 3;

	/** Priority of a slot when its layer does not choose one */
	constexpr int32 DefaultPriority(int32 Slot)
	{
		return Slot == Wall ? 30 : Slot == Skid ? 20 : Slot == RunStart ? 10 : 0;
	}
}

/** One layer's answer this frame, for PickOverride */
struct FGSTN_OverrideCandidate
{
	bool bActive = false;
	int32 Slot = GSTNOverrideSlot::None;
	int32 Priority = 0;
	float BlendTime = 0.15f;
};

/** The winner: the slot to show (None = the base pose), its blend time and its index in the candidates (INDEX_NONE for none) */
struct FGSTN_OverridePick
{
	int32 Slot = GSTNOverrideSlot::None;
	float BlendTime = 0.15f;
	int32 Index = INDEX_NONE;

	/** Pure: the active candidate with a valid slot (>= 1) and the highest priority; on a tie the first wins */
	static GSTNANIMATIONCORE_API FGSTN_OverridePick Pick(TConstArrayView<FGSTN_OverrideCandidate> Candidates);
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGSTN_OverrideLayerSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * A linked anim layer that can take over the character's pose while its own feature runs (a wall mode, later a spin or a drill), see
 * the plugin README and docs/architecture/anim-layers.md. The layer decides when from its own data; the main graph only asks (UGSTN_CharacterAnimInstance),
 * so features bring their animation as a layer class chosen by data and the main graph never names them.
 *
 * Called on the game thread after the linked layers have updated, so the answer is this frame's.
 */
class GSTNANIMATIONCORE_API IGSTN_OverrideLayerSource
{
	GENERATED_BODY()

public:
	/** True while this layer wants to replace the main pose */
	virtual bool IsOverrideLayerActive() const = 0;

	/** Blend time into and out of this layer (s) */
	virtual float GetOverrideBlendTime() const { return 0.15f; }

	/** Which pose of the main graph's Blend Poses by int this layer feeds (GSTNOverrideSlot; 0 is the base pose, never a layer) */
	virtual int32 GetOverrideSlot() const { return GSTNOverrideSlot::Wall; }

	/** When more than one layer is active, the highest priority wins (default: by slot, wall > skid > run start) */
	virtual int32 GetOverridePriority() const { return GSTNOverrideSlot::DefaultPriority(GetOverrideSlot()); }
};

// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitShape.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Rules/GSTN_KitExtension.h"
#include "GSTN_HurtboxPose.generated.h"

struct FGSTN_CombatTimelineData;

/** One shape of a kit's movement hurtbox (FGSTN_MovementHurtboxExtension): root-relative, counted as a hurtbox of the set */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_MovementHurtboxShape
{
	GENERATED_BODY()

	/** Label only (editor, debug draw) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox")
	FName Name = NAME_None;

	/** Root-relative, like FGSTN_Hurtbox::Shape. The default is a body capsule from the feet to the shoulders that has no gap between the legs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox")
	FGSTN_HitShape Shape = MakeDefaultBody();

	/** The hurtbox of the set a hit on this shape counts as (priority, reaction scale, state, part). None = the set's first one */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox")
	FName CountsAs = NAME_None;

	static FGSTN_HitShape MakeDefaultBody()
	{
		FGSTN_HitShape Body;
		Body.Type = EGSTN_ShapeType::Capsule;
		Body.Extent = FVector(38.0, 38.0, 80.0);
		Body.Local = FTransform(FVector(0.0, 0.0, -10.0));
		return Body;
	}
};

/**
 * A kit's own movement hurtbox (owner 2026-11-16: "the idle / basic movement hurtbox could change per kit"): a kit with a tower shield is wider, a
 * light kit slimmer. Read only while no move runs. When it has shapes, they replace the set's SimpleOnly hurtboxes (the body volume) at the
 * Simple level; a kit without this extension keeps the set's. Copied into the loadout's table set when it is built, so it changes with the
 * loadout revision the simulation runs and a rollback across a kit swap reads the right one.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Movement Hurtbox"))
struct GSTNCOMBATCORE_API FGSTN_MovementHurtboxExtension : public FGSTN_KitExtension
{
	GENERATED_BODY()

	/** On = MovementDetail below replaces the set's own level while this kit is equipped */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox")
	bool bOverrideDetail = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox", meta = (EditCondition = "bOverrideDetail"))
	EGSTN_HurtboxDetail MovementDetail = EGSTN_HurtboxDetail::Simple;

	/** The kit's body volume(s) at the Simple level. Empty = the set's SimpleOnly hurtboxes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox")
	TArray<FGSTN_MovementHurtboxShape> Shapes;
};

/** A shape that counts as a set hurtbox for one frame: a move's Hurtbox track shape or a kit's movement shape. Root-relative */
struct FGSTN_PosedHurtboxExtension
{
	FGSTN_HitShape Shape;

	/** The set hurtbox a hit on it counts as (an index into the set) */
	int32 HurtboxIndex = 0;
};

/** What FGSTN_HurtboxPose::Build reads */
struct FGSTN_HurtboxPoseInput
{
	const UGSTN_HurtboxSetDefinition* Set = nullptr;

	/** The move running at the frame, null outside moves (idle, locomotion, a recoil) */
	const FGSTN_CombatTimelineData* Timeline = nullptr;

	/** The timeline frame (paused frames already removed), with Timeline */
	int32 TimelineFrame = 0;

	/** The equipped kit's movement hurtbox, null without one. Read only when Timeline is null */
	const FGSTN_MovementHurtboxExtension* KitMovement = nullptr;
};

/**
 * Where a combatant's hurtboxes are at one frame (owner 2026-11-16: hurtboxes follow the bones per move). A value of the snapshot entry and of the
 * lag compensation record, derived from the end-of-previous-frame state like the hurt states: the running timeline and its frame, which are sync
 * state, and data (the set, the timeline's baked bones, the kit). So every machine and every resimulation computes the same bits.
 *
 * Empty arrays mean "as the set says": Shapes empty = every hurtbox at its own Shape, Present empty = every hurtbox there. So a combatant whose set
 * has only Both, non-following hurtboxes and no kit shapes costs nothing: the old behaviour, bit for bit. Inline storage covers a 16-hurtbox set
 * without heap traffic.
 */
struct GSTNCOMBATCORE_API FGSTN_HurtboxPose
{
	/** Most extensions in one frame; more are dropped in track order, the same on every machine */
	static constexpr int32 MaxExtensions = 8;

	/** Root-relative shape per set hurtbox this frame. Empty = the set's shapes */
	TArray<FGSTN_HitShape, TInlineAllocator<16>> Shapes;

	/** Per set hurtbox: does it take hits at this frame's detail level? Empty = all do */
	TArray<bool, TInlineAllocator<16>> Present;

	/** Shapes that count as a set hurtbox: the move's Hurtbox tracks, or the kit's movement shapes */
	TArray<FGSTN_PosedHurtboxExtension, TInlineAllocator<4>> Extensions;

	void Reset()
	{
		Shapes.Reset();
		Present.Reset();
		Extensions.Reset();
	}

	bool IsDefault() const { return Shapes.IsEmpty() && Present.IsEmpty() && Extensions.IsEmpty(); }

	/** The shape of set hurtbox Index this frame (the set's own when not posed) */
	const FGSTN_HitShape& GetShape(const UGSTN_HurtboxSetDefinition& Set, int32 Index) const
	{
		return Shapes.IsValidIndex(Index) ? Shapes[Index] : Set.Hurtboxes[Index].Shape;
	}

	/** Does set hurtbox Index take hits through its own shape this frame (its extensions are tested on their own) */
	bool IsPresent(int32 Index) const
	{
		return !Present.IsValidIndex(Index) || Present[Index];
	}

	/** The radius around the root that holds every present shape and every extension, for the broadphase */
	double ComputeBoundRadius(const UGSTN_HurtboxSetDefinition* Set) const;

	/**
	 * Fills Out for one frame. Pure. In order: the detail level (the move's override, else the set's MoveDetail during a move and MovementDetail
	 * outside, a kit's override outside moves); the bones (a bFollowBone hurtbox whose bone the timeline baked is moved by the bone's motion from its
	 * reference); the extensions (the open Hurtbox tracks of the move; outside moves the kit's shapes, which replace the SimpleOnly hurtboxes)
	 */
	static void Build(const FGSTN_HurtboxPoseInput& Input, FGSTN_HurtboxPose& Out);

	/** The set index a CountsAs name resolves to: the hurtbox with that name, else 0 (the set's first) */
	static int32 ResolveCountsAs(const UGSTN_HurtboxSetDefinition& Set, FName CountsAs);
};

// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitShape.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Misc/Optional.h"

struct FGSTN_CombatTimelineData;

// The maths of the viewport gizmos and of picking (combat-m5-editor 5.5, plan m5 E7), with no widget, no viewport and no UObject in it, so the
// automation tests drive it directly and the viewport client is only the wiring.
//
// The conventions are the runtime's (GSTN_CombatTick_Attacker BuildCarrier): a hitbox is placed at Shape.Local x Key x Space, where Key is the
// path of the hitbox evaluated at the frame (UGSTN_HitShapeUtils::EvaluateKeys) and Space is what the hitbox's EGSTN_HitboxSpace stands for at
// that frame. In the preview the root is the world origin, so "world" and "root" differ only when a test gives the root a rotation.
namespace GSTNGizmo
{
	/** Extent change per widget scale unit. The widget reports scale as the pixels dragged along an axis, so 100 pixels double the shape */
	constexpr double ScalePerWidgetUnit = 0.01;

	/** A shape is never scaled below this (cm): a zero extent would make a hitbox that cannot be picked or seen again */
	constexpr double MinExtent = 1.0;

	// Spaces

	/**
	 * The transform a hitbox's keys are relative to, at one frame (the runtime's Space x Root, without the key).
	 *  Root: the root. BakedSocket: the socket's root-relative transform (baked, else the live socket the caller resolved) then the root; a socket
	 *  nobody could place falls back to the root, so the hitbox is still drawn and can be moved. WorldAtMoveStart: the transform at the start of
	 *  the move, the preview origin. Projectile: identity (M5; the projectile system does not exist).
	 */
	FTransform SpaceTransform(EGSTN_HitboxSpace Space, const FTransform& Root, const TOptional<FTransform>& SocketRootRelative, const FTransform& MoveOrigin = FTransform::Identity);

	// Drag mapping

	/** A whole drag as the viewport reports it: the sum of the widget's deltas since the mouse went down, all in world space (scale in widget units) */
	struct FDragDelta
	{
		FVector Translation = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
		FVector Scale = FVector::ZeroVector;

		/** Shift held at the start: the translation moves every key of the hitbox, the whole path */
		bool bOffsetAllKeys = false;

		/** Adds one widget delta. Rotations compose in world space, the latest last */
		void Accumulate(const FVector& InTranslation, const FQuat& InRotation, const FVector& InScale);

		bool IsZero() const { return Translation.IsNearlyZero(1e-9) && Rotation.IsIdentity(1e-9) && Scale.IsNearlyZero(1e-9); }
	};

	/** A world-space vector (a drag, an offset) in the space a key is expressed in: the inverse of the space transform without its translation */
	FVector WorldTranslationToKey(const FVector& WorldDelta, const FTransform& Space);

	/** The key's rotation after a rotation of the world-space gizmo by WorldDelta: Space^-1 x WorldDelta x Space x Key. The key stays in its space */
	FQuat WorldRotationToKey(const FQuat& KeyRotation, const FQuat& SpaceRotation, const FQuat& WorldDelta);

	/**
	 * The key after the translation and rotation of a drag. StartKey is the key as it was when the mouse went down: the result is always computed from
	 * it with the whole accumulated delta, never from the previous result, so a long drag does not drift. The key's own scale is left alone: the scale
	 * gizmo edits the shape's extent (ScaleExtent), never a transform scale.
	 */
	FTransform ApplyDragToKey(const FTransform& StartKey, const FTransform& Space, const FDragDelta& Delta);

	/**
	 * The extent after a scale drag: widget scale units are additive, each axis grows by (1 + units x ScalePerWidgetUnit), at least MinExtent. A
	 * sphere has one radius, so it takes the axis that was dragged furthest for all three; a capsule keeps its radius (X) and its half height
	 * (Z, hemispheres included) and never goes below the radius; a box scales per axis.
	 */
	FVector ScaleExtent(const FGSTN_HitShape& StartShape, const FVector& ScaleUnits);

	// Mirror

	/**
	 * The key mirrored across the YZ plane of the root (x -> -x), whatever space it lives in: the key is taken to root space, mirrored, and put back.
	 * Only position and rotation mirror; the scale is kept. Mirroring twice gives the key back.
	 */
	FTransform MirrorKeyAcrossRootYZ(const FTransform& Key, const FTransform& Space, const FTransform& Root);

	// Which key a drag edits

	enum class EDragKeyAction : uint8
	{
		/** The hitbox has no key to edit and auto-key is off */
		None,
		/** A key sits on the playhead frame: edit it */
		UseKeyAtFrame,
		/** No key on the frame, auto-key on: create one with the interpolated values first, then edit it */
		CreateAtFrame,
		/** No key on the frame, auto-key off: edit the nearest one (the earlier of two equally near) */
		EditNearest
	};

	struct FDragKeyResolution
	{
		EDragKeyAction Action = EDragKeyAction::None;

		/** Index inside the hitbox's keys: the key to edit, INDEX_NONE for None and CreateAtFrame */
		int32 KeyInLane = INDEX_NONE;
	};

	/** Keys are sorted by Frame (counted from the first frame of the track); RelativeFrame is the playhead counted the same way */
	FDragKeyResolution ResolveDragKey(TConstArrayView<FGSTN_HitboxKey> Keys, int32 RelativeFrame, bool bAutoKey);

	/** The key a click selects: the one on the frame, else the nearest. INDEX_NONE for a hitbox with no keys */
	int32 FindKeyForPick(TConstArrayView<FGSTN_HitboxKey> Keys, int32 RelativeFrame);

	// Picking

	/** One shape the viewport can pick: whatever Track and Lane stand for to the owner (a hitbox of a track; a hurtbox of a set with Track 0) */
	struct FPickCandidate
	{
		int32 Track = INDEX_NONE;
		int32 Lane = INDEX_NONE;
		FGSTN_HitShape Shape;
		FTransform Carrier = FTransform::Identity;
	};

	/**
	 * The nearest entry of the ray into the shape placed at Shape.Local x Carrier, extents as the drawing has them (GetEffectiveExtent). A ray that
	 * starts inside hits at 0. Direction need not be normalised. False for a miss or a shape that is behind the ray.
	 */
	bool RayVsShape(const FVector& Origin, const FVector& Direction, const FGSTN_HitShape& Shape, const FTransform& Carrier, double& OutDistance);

	/**
	 * The index of the candidate the ray reaches first, INDEX_NONE if it reaches none. When two are reached at the same distance (the camera inside
	 * both) the smaller one wins, so a small hitbox inside a big one stays pickable.
	 */
	int32 PickNearest(const FVector& Origin, const FVector& Direction, TConstArrayView<FPickCandidate> Candidates, double* OutDistance = nullptr);

	// Hitboxes active at a frame

	/** Where the live socket of the previewed mesh is, relative to the root, for a socket that is not baked. False when the mesh has none */
	using FSocketResolver = TFunctionRef<bool(FName Socket, FTransform& OutRootRelative)>;

	/** A hitbox of the timeline that is active at a frame, ready to draw, pick and move */
	struct FActiveHitbox
	{
		int32 Track = INDEX_NONE;
		int32 Lane = INDEX_NONE;
		uint8 HitGroup = 0;

		/** The shape with the key's transform folded into its Local placement (and so the key's scale in its extent), as the hit code sees it */
		FGSTN_HitShape Shape;

		/** What Shape is relative to: the hitbox's space at the frame */
		FTransform Space = FTransform::Identity;
	};

	/**
	 * The hitboxes of every Hitbox track whose window holds Frame (absolute timeline frame). A baked socket is looked up in the data, then in the
	 * live resolver. Appends to Out (the caller resets it).
	 */
	void CollectActiveHitboxes(const FGSTN_CombatTimelineData& Data, int32 Frame, const FTransform& Root, const FSocketResolver& LiveSocket, TArray<FActiveHitbox>& Out);
}

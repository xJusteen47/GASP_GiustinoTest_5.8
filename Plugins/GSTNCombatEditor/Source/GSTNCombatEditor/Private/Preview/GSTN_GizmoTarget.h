// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Preview/GSTN_GizmoMath.h"

/**
 * What the gizmos of the preview viewport move (combat-m5-editor 5.5). The viewport client knows the widget, the mouse and the camera; the target
 * knows the data: which thing is selected, where its gizmo sits, how a drag writes back, what a click on a shape selects. Two targets exist: the
 * hitbox key of the timeline editor (FGSTN_HitboxGizmoTarget) and the shape of a hurtbox in the hurtbox set editor (FGSTN_HurtboxGizmoTarget).
 *
 * All positions are world space; in the preview the root is the world origin. A drag is Begin, any number of Apply with the whole accumulated delta
 * (never an increment), then End, or Cancel for Escape. Everything the target writes goes through the model of its editor, so a drag is one undo step.
 */
class IGSTN_GizmoTarget
{
public:
	virtual ~IGSTN_GizmoTarget() = default;

	/** Where the gizmo sits and how its local axes point, now. False: nothing is selected (or it is not active at the playhead), so there is no gizmo */
	virtual bool GetWidgetFrame(FVector& OutLocation, FQuat& OutLocalRotation) const = 0;

	/** The shapes a click can reach, drawn at the playhead. Appends */
	virtual void CollectPickables(TArray<GSTNGizmo::FPickCandidate>& Out) const = 0;

	/** A click on a shape: selects it (and, for a hitbox, the key the click is most likely about). False when nothing changed */
	virtual bool Pick(int32 Track, int32 Lane) = 0;

	/** Mouse down on a handle: opens the drag (one undo step) and, for a hitbox with auto-key, makes the key to edit. False: nothing to drag */
	virtual bool BeginDrag(bool bOffsetAllKeys) = 0;

	/** The whole drag so far, applied from the state at BeginDrag. False when it changed nothing */
	virtual bool ApplyDelta(const GSTNGizmo::FDragDelta& Delta) = 0;

	/** Mouse up: the drag becomes its undo step */
	virtual void EndDrag() = 0;

	/** Escape: data and history as before BeginDrag */
	virtual void CancelDrag() = 0;

	/** The target works on keys over time (a hitbox), so the viewport offers Add key, Mirror and Auto-key. A hurtbox shape has none */
	virtual bool HasKeys() const { return false; }

	virtual bool IsAutoKeyEnabled() const { return false; }
	virtual void SetAutoKeyEnabled(bool bEnabled) {}

	/** K: a key for the selected hitbox at the playhead, with the values the path has there. False when nothing was added */
	virtual bool AddKeyAtPlayhead() { return false; }

	/** The selected key mirrored across the root's YZ plane, one undo step. False when nothing changed */
	virtual bool MirrorSelectedKey() { return false; }

	/** One line for the viewport: what the gizmo is on and what a drag will do (auto-key creates a key, auto-key off edits the nearest one). Empty: nothing to say */
	virtual FText GetStatusText() const { return FText::GetEmpty(); }
};

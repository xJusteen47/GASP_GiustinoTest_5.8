// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitShape.h"
#include "Preview/GSTN_GizmoTarget.h"

class FGSTN_HurtboxEditModel;

/**
 * The gizmo of the selected hurtbox in the hurtbox set editor (combat-m5-editor 8 "gizmos on the selected shape"). A hurtbox has no keys and no
 * space: its shape sits at Shape.Local in the root's space, which is the world in the preview, so the maths is the hitbox's with the identity as
 * space. Move and rotate write Shape.Local, scale writes Shape.Extent (never the Local scale). A drag is an edit session of the model
 * (FGSTN_HurtboxEditModel::SetShape through BeginEdit/EndEdit), one undo step; Escape restores through CancelEdit.
 *
 * Only a single selected hurtbox has a gizmo. Picking uses Track 0 and the position of the hurtbox in the set as Lane.
 */
class FGSTN_HurtboxGizmoTarget final : public IGSTN_GizmoTarget
{
public:
	explicit FGSTN_HurtboxGizmoTarget(const TSharedRef<FGSTN_HurtboxEditModel>& InModel);

	//~ IGSTN_GizmoTarget
	virtual bool GetWidgetFrame(FVector& OutLocation, FQuat& OutLocalRotation) const override;
	virtual void CollectPickables(TArray<GSTNGizmo::FPickCandidate>& Out) const override;
	virtual bool Pick(int32 Track, int32 Lane) override;
	virtual bool BeginDrag(bool bOffsetAllKeys) override;
	virtual bool ApplyDelta(const GSTNGizmo::FDragDelta& Delta) override;
	virtual void EndDrag() override;
	virtual void CancelDrag() override;
	virtual FText GetStatusText() const override;

private:
	TSharedRef<FGSTN_HurtboxEditModel> Model;

	bool bDragOpen = false;
	int32 DragIndex = INDEX_NONE;
	FGSTN_HitShape StartShape;
};

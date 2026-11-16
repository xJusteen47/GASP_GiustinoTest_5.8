// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Preview/GSTN_GizmoTarget.h"
#include "Timeline/GSTN_TimelineTypes.h"

class FGSTN_TimelineEditorModel;
class FGSTN_TimelinePreviewScene;

/**
 * The gizmo of a hitbox key in the timeline editor (plan m5 E7). The gizmo sits on the path of the hitbox that the selected key belongs to,
 * evaluated at the playhead (not on the key itself), so it follows the shape through the move; a drag edits the key on the playhead frame.
 *
 *  - Auto-key on (the default): a hitbox with no key on the playhead frame gets one first, with the values the path has there, in the same undo step as
 *    the drag. Off: the drag edits the nearest key.
 *  - Shift held at the start of a translate drag: every key of the hitbox moves by the same offset (the whole path), no key is made.
 *  - Scale never touches a transform scale: it changes the extent of the hitbox's shape (FGSTN_HitShape::Extent), see GSTNGizmo::ScaleExtent.
 *
 * Everything is written through the model (SetKeyTransform, AddKey, ApplyTrackEdit inside the model's drag), which owns clamping and transactions.
 */
class FGSTN_HitboxGizmoTarget final : public IGSTN_GizmoTarget
{
public:
	FGSTN_HitboxGizmoTarget(const TSharedRef<FGSTN_TimelineEditorModel>& InModel, const TWeakPtr<FGSTN_TimelinePreviewScene>& InScene);

	/** A hitbox of a Hitbox track, and the key of the selection inside it */
	struct FLane
	{
		int32 Track = INDEX_NONE;
		int32 Spec = INDEX_NONE;
		int32 KeyInSpec = INDEX_NONE;

		bool IsValid() const { return Track != INDEX_NONE && Spec != INDEX_NONE; }
	};

	/** The hitbox of the first selected key that belongs to a Hitbox track. False when the selection holds none */
	static bool FindSelectedLane(const FGSTN_TimelineEditorModel& Model, FLane& OutLane);

	/** The preview's root: the origin (the character stands still in the preview) */
	static FTransform GetRootTransform() { return FTransform::Identity; }

	/** The live socket of the previewed mesh relative to the root, for a BakedSocket hitbox that has not been baked. False if the mesh has no such socket */
	static bool ResolveLiveSocket(const FGSTN_TimelinePreviewScene* Scene, FName Socket, FTransform& OutRootRelative);

	//~ IGSTN_GizmoTarget
	virtual bool GetWidgetFrame(FVector& OutLocation, FQuat& OutLocalRotation) const override;
	virtual void CollectPickables(TArray<GSTNGizmo::FPickCandidate>& Out) const override;
	virtual bool Pick(int32 Track, int32 Lane) override;
	virtual bool BeginDrag(bool bOffsetAllKeys) override;
	virtual bool ApplyDelta(const GSTNGizmo::FDragDelta& Delta) override;
	virtual void EndDrag() override;
	virtual void CancelDrag() override;
	virtual bool HasKeys() const override { return true; }
	virtual bool IsAutoKeyEnabled() const override;
	virtual void SetAutoKeyEnabled(bool bEnabled) override;
	virtual bool AddKeyAtPlayhead() override;
	virtual bool MirrorSelectedKey() override;
	virtual FText GetStatusText() const override;

private:
	const FGSTN_TimelineTrack_Hitbox* GetHitboxTrack(int32 TrackIndex) const;

	/** The space of a hitbox at an absolute frame (socket placement included) */
	FTransform ComputeSpace(const FGSTN_HitboxSpec& Spec, int32 Frame) const;

	/**
	 * Opens the model's drag and settles which key it edits (see the class comment), keeping the state Apply starts from. False, with nothing left
	 * open, when there is no gizmo, the playhead is outside the window, or auto-key is off and the hitbox has no key
	 */
	bool OpenDrag(bool bInOffsetAll, const FText& Description);

	TSharedRef<FGSTN_TimelineEditorModel> Model;
	TWeakPtr<FGSTN_TimelinePreviewScene> Scene;

	// The drag in progress: the state as it was at BeginDrag, because every Apply starts from it
	bool bDragOpen = false;
	bool bOffsetAll = false;
	FLane DragLane;
	int32 DragKeyFlat = INDEX_NONE;
	FTransform DragSpace = FTransform::Identity;
	FTransform StartKey = FTransform::Identity;
	FGSTN_HitShape StartShape;
	TArray<FGSTN_HitboxKey> StartKeys;
};

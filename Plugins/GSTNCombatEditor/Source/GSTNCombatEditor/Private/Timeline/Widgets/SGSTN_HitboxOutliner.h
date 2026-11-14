// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_HitboxOutline.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class FGSTN_TimelineEditorModel;

/**
 * The Hitboxes tab of the timeline editor: the hitbox groups of the move (the Hitbox tracks) with their hitboxes under them, as a tree.
 * A view over the model, like the hurtbox list of the hurtbox set editor:
 *  - a click on a row focuses it (GSTNHitboxOutline::Focus): the group or the hitbox is selected, the gizmo goes on it, and the playhead
 *    moves inside the group's frames when it was outside, so the shape is drawn;
 *  - a click on a hitbox in the viewport (or on a lane of the timeline) selects its row here, expands its group and scrolls it into view.
 * It never edits: the Details tab and the viewport gizmo do.
 */
class SGSTN_HitboxOutliner : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_HitboxOutliner) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelineEditorModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SGSTN_HitboxOutliner() override;

	/** Groups listed at the top level, for a test */
	int32 GetGroupCount() const { return Roots.Num(); }

private:
	using FItemPtr = TSharedPtr<FGSTN_HitboxOutlineItem>;

	/** Rebuilds the rows when the shape of the data changed (groups or hitboxes added, removed, reordered); the cells read the data by themselves */
	void Rebuild();
	void SyncSelectionToTree();
	void HandleTreeSelectionChanged(FItemPtr Item, ESelectInfo::Type SelectInfo);
	void HandleDoubleClick(FItemPtr Item);
	TSharedRef<ITableRow> OnGenerateRow(FItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(FItemPtr Item, TArray<FItemPtr>& OutChildren);
	FItemPtr FindRow(const FGSTN_HitboxOutlineItem& Item) const;

	TSharedPtr<FGSTN_TimelineEditorModel> Model;
	TSharedPtr<STreeView<FItemPtr>> Tree;
	TArray<FItemPtr> Roots;
	TMap<int32, TArray<FItemPtr>> ChildrenByTrack;

	/** The outline the rows were built from: a rebuild only when it differs */
	TArray<FGSTN_HitboxOutlineItem> Built;

	FDelegateHandle DataHandle;
	FDelegateHandle SelectionHandle;

	/** The tree is being set from the model: its own selection callback must not write back */
	bool bSyncing = false;
};

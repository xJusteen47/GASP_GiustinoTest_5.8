// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineSelectionProxy.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class FPropertyEditorModule;
class IStructureDetailsView;
struct FPropertyChangedEvent;

/**
 * The Details tab of the timeline editor (combat-m5-editor 5.1): the fields of the selected track, shown through a structure
 * details view over a transient copy (UGSTN_TimelineSelectionProxy). It follows the model's selection and its data; an edit goes
 * back through the model's ApplyTrackEdit, so the panel writes nothing itself and an undo of the edit is the editor's undo.
 *
 * It shows one track at a time. With nothing selected, or tracks of several rows, it says so instead of guessing.
 *
 * A selected hitbox (a key of a Hitbox track, picked in the timeline, the viewport or the Hitboxes tab) gets two views of its own, owner 2026-11-16:
 * "Hitbox" with that hitbox's fields only (shape, hit data, parenting...) and "Key" with the selected key (offset, rotation, scale: the numbers the
 * viewport gizmo can only drag). The group's view stays above, without its Hitboxes array, so nothing is edited three menus deep. All three views
 * edit the same transient copy and commit through the model, so an edit in any of them is one undo step.
 */
class SGSTN_TimelineDetails : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_TimelineDetails) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelineEditorModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SGSTN_TimelineDetails() override;

	/** The row whose track is shown: the one track every selected item belongs to, INDEX_NONE for none or several */
	static int32 ResolveShownTrack(const TArray<FGSTN_Selection>& Selection);

	/** The proxy, for tests and for the toolkit's debugging */
	UGSTN_TimelineSelectionProxy* GetProxy() const { return Proxy.Get(); }

private:
	/** Selection or data changed: bind the proxy to the right track, or refresh its copy */
	void Refresh();

	/** The details view finished a change (interactive or committed) to the copy */
	void HandleFinishedChanging(const FPropertyChangedEvent& Event);

	/** Points the views at the proxy's current copy */
	void ShowProxy();

	/** The hitbox and key the selection points at inside the shown track: (spec, key in spec), INDEX_NONE for none */
	void ResolveFocus(int32 TrackIndex, int32& OutSpec, int32& OutKey) const;

	TSharedRef<IStructureDetailsView> MakeView(FPropertyEditorModule& PropertyEditor);

	TSharedPtr<FGSTN_TimelineEditorModel> Model;
	TStrongObjectPtr<UGSTN_TimelineSelectionProxy> Proxy;
	TSharedPtr<IStructureDetailsView> View;
	TSharedPtr<IStructureDetailsView> HitboxView;
	TSharedPtr<IStructureDetailsView> KeyView;
	FDelegateHandle DataHandle;
	FDelegateHandle SelectionHandle;
};

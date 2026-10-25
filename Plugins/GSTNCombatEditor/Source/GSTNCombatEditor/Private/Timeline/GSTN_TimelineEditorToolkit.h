// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Timeline/GSTN_FindingsStore.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Validation/GSTN_ValidationReport.h"

class FGSTN_PreviewController;
class FGSTN_TimelineEditorModel;
class FGSTN_TimelineUndoClient;
class SDockTab;
class UBlueprint;

/**
 * The asset editor of a timeline (combat-m5-editor 5.1): a standalone (or world-centric) window with four tabs, Viewport, Timeline,
 * Details and Frame Data, over one FGSTN_TimelineEditorModel. It owns the model, the undo client that keeps the model honest after
 * an undo, and the lease that makes it the only editor of its target; the widgets of the tabs only call the model.
 *
 * What the tabs hold today: Timeline is the panel of E4 with the montage-length bar and the toolbar additions; Details edits the
 * selected track through the selection proxy; Frame Data is the summary line. Viewport is the 3D preview scene (E6) of the controller the toolkit owns.
 *
 * Save is the standard one of the asset editor: it saves the object GetObjectsCurrentlyBeingEdited returns, which is the target of the
 * context (the asset; the Blueprint in inline mode). The layout is saved by the editor under the layout name and restored when the
 * asset is opened again.
 *
 * Inline mode (E13) is the same window on the inline timeline of an ability Blueprint. The toolkit is registered with the asset editor
 * subsystem under the Blueprint but says it is not the primary editor of it, so opening the Blueprint from the Content Browser still
 * opens the Blueprint editor and does not focus this one. It listens to the compiles of the Blueprint: the default object is a new one
 * after each, the editor refreshes from it, and closes with a message when the Blueprint no longer has an inline timeline. A strip above
 * the timeline names the Blueprint and offers Extract to timeline asset.
 */
class FGSTN_TimelineEditorToolkit : public FAssetEditorToolkit
{
public:
	/** The name the layout is saved under. Change the number when the tabs change, so a saved layout of the old shape is not restored over the new one */
	static const FName LayoutName;

	static const FName ViewportTabId;
	static const FName TimelineTabId;
	static const FName DetailsTabId;
	static const FName FrameDataTabId;

	FGSTN_TimelineEditorToolkit();
	virtual ~FGSTN_TimelineEditorToolkit() override;

	/**
	 * Opens the editor on the context. False, with nothing opened, when the context is not valid or the target already has an editor
	 * (that editor is brought to the front: the second toolkit is refused, combat-m5-editor 4.1).
	 */
	bool InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, const FGSTN_TimelineEditContext& InContext);

	const TSharedPtr<FGSTN_TimelineEditorModel>& GetModel() const { return Model; }

	/** The open editor of a target (the timeline asset, or the Blueprint in inline mode), null when none is open. The validation panel goes to a finding through it */
	static TSharedPtr<FGSTN_TimelineEditorToolkit> FindOpenFor(const UObject* Target);

	/** The findings of the last Validate, listed by the Frame Data tab */
	const TSharedRef<FGSTN_FindingsStore>& GetFindings() const { return Findings; }
	const FGSTN_TimelineEditContext& GetContext() const { return Context; }

	/**
	* Validate: runs the timeline rules on the data as it is now, at the model's tick rate with the montage loaded, and writes the findings
	* to the Message Log (category GSTN Combat, a page named after the asset) and to the Frame Data tab (GetFindings). The Validate button
	* and F7 call it. Returns the report so a test can read it; nothing is changed in the data.
	 */
	FGSTN_ValidationReport ValidateNow();

	/**
	 * Runs the same rules as ValidateNow, silently: the Frame Data tab and the lane markers are updated and the Message Log is left alone. The toolkit
	 * calls it 250 ms after the last edit (and once when it opens), so the tab shows the findings of the timeline as it is without a button press.
	 */
	FGSTN_ValidationReport RefreshFindings();

	/** Delay between the last edit and the silent re-validation */
	static constexpr float FindingsDebounceSeconds = 0.25f;

	/** Is a silent re-validation waiting for its 250 ms? For a test */
	bool IsFindingsRefreshPending() const { return FindingsTickerHandle.IsValid(); }

	/** The standalone default layout, apart so a test can read its shape without opening a window */
	static TSharedRef<FTabManager::FLayout> MakeDefaultLayout();

	/** Inline mode: after a compile of the Blueprint. Refreshes, or closes the window (next tick) with a message when the inline timeline is gone */
	void HandleBlueprintCompiled(UBlueprint* Blueprint);

	/**
	 * Inline mode: Extract to timeline asset. Asks where to put the asset (Content Browser save dialog), creates it, moves the inline
	 * timeline into it with GSTNInlineTimeline::ExtractToAsset, closes this window and opens the asset in its own editor.
	 */
	void ExtractToAsset();

	//~ FAssetEditorToolkit
	virtual bool IsPrimaryEditor() const override { return !Context.IsInline(); }
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void OnClose() override;

private:
	TSharedRef<SDockTab> SpawnViewportTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTimelineTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnFrameDataTab(const FSpawnTabArgs& Args);

	/** The data changed: the findings describe the timeline as it was, and a refresh is scheduled for after the edits settle */
	void HandleDataChanged();

	void CancelFindingsRefresh();

	/** A finding of the Frame Data tab was double-clicked: select its track, put the playhead on its frame */
	void GoToFinding(const FGSTN_ValidationFinding& Finding);

	/** Lets go of the target and the undo client. Safe to call twice */
	void Release();

	FGSTN_TimelineEditContext Context;

	/** Inline mode: the Blueprint whose compiles this toolkit listens to */
	TWeakObjectPtr<UBlueprint> ObservedBlueprint;

	TSharedPtr<FGSTN_TimelineEditorModel> Model;

	/** The preview scene and its subject. Declared after the model it listens to, so it is destroyed first */
	TSharedPtr<FGSTN_PreviewController> Preview;

	/** The pending debounced refresh, invalid when there is none */
	FTSTicker::FDelegateHandle FindingsTickerHandle;

	/** What the last Validate found, for the Frame Data tab. Marked stale when the data changes, and refreshed 250 ms later */
	TSharedRef<FGSTN_FindingsStore> Findings = MakeShared<FGSTN_FindingsStore>();

	/** Declared after the model it points at, so it is destroyed first */
	TUniquePtr<FGSTN_TimelineUndoClient> UndoClient;

	/** Held while the toolkit is open: the one editor of the target */
	TSharedPtr<FGSTN_TimelineContextLease> Lease;
};

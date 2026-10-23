// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Toolkits/AssetEditorToolkit.h"

class FGSTN_TimelineEditorModel;
class FGSTN_TimelineUndoClient;
class SDockTab;

/**
 * The asset editor of a timeline (combat-m5-editor 5.1): a standalone (or world-centric) window with four tabs, Viewport, Timeline,
 * Details and Frame Data, over one FGSTN_TimelineEditorModel. It owns the model, the undo client that keeps the model honest after
 * an undo, and the lease that makes it the only editor of its target; the widgets of the tabs only call the model.
 *
 * What the tabs hold today: Timeline is the panel of E4 with the montage-length bar and the toolbar additions; Details edits the
 * selected track through the selection proxy; Frame Data is the summary line. Viewport is a placeholder until the preview scene (E6).
 *
 * Save is the standard one of the asset editor: it saves the object GetObjectsCurrentlyBeingEdited returns, which is the target of the
 * context (the asset; the Blueprint once inline mode arrives in E13). The layout is saved by the editor under the layout name and
 * restored when the asset is opened again.
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
	const FGSTN_TimelineEditContext& GetContext() const { return Context; }

	/** The standalone default layout, apart so a test can read its shape without opening a window */
	static TSharedRef<FTabManager::FLayout> MakeDefaultLayout();

	//~ FAssetEditorToolkit
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

	/** Lets go of the target and the undo client. Safe to call twice */
	void Release();

	FGSTN_TimelineEditContext Context;
	TSharedPtr<FGSTN_TimelineEditorModel> Model;

	/** Declared after the model it points at, so it is destroyed first */
	TUniquePtr<FGSTN_TimelineUndoClient> UndoClient;

	/** Held while the toolkit is open: the one editor of the target */
	TSharedPtr<FGSTN_TimelineContextLease> Lease;
};

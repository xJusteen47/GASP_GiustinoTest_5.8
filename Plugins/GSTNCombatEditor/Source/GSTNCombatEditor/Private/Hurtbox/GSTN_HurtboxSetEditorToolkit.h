// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Hurtbox/GSTN_HurtboxEditModel.h"
#include "Hurtbox/GSTN_HurtboxGenerator.h"
#include "Hurtbox/GSTN_HurtboxMirror.h"
#include "Timeline/GSTN_FindingsStore.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Validation/GSTN_ValidationReport.h"

class FGSTN_HurtboxUndoClient;
class FGSTN_PreviewController;
class FPrimitiveDrawInterface;
class SDockTab;
class UGSTN_HurtboxSetDefinition;

/**
 * The asset editor of a hurtbox set (combat-m5-editor 8, plan m5 E9): the same preview scene as the timeline editor with the set's shapes drawn over the
 * mesh, a list of the hurtboxes, the details of the selected one, the live findings, and the two tools that make a set quickly: Generate from the mesh's
 * Physics Asset and Mirror. Every edit goes through FGSTN_HurtboxEditModel (one transaction each), so undo is the editor's.
 *
 * The mesh is the set's editor-only PreviewMesh; with none, the preview subject (kit and skin) the timeline editor also falls back to.
 * Any skeletal mesh works, a person or a turret: nothing here assumes a bone. Mirroring is offered only where the skeleton has recognisable left/right
 * names (see GSTNHurtboxMirror) and says why when it is not.
 *
 * Gizmos on the selected shape wait for the viewport work of E7 (S4 spike); until then a shape is edited numerically in the details.
 */
class FGSTN_HurtboxSetEditorToolkit : public FAssetEditorToolkit
{
public:
	/** The layout name; change the number when the tabs change */
	static const FName LayoutName;

	static const FName ViewportTabId;
	static const FName HurtboxesTabId;
	static const FName DetailsTabId;
	static const FName FindingsTabId;

	FGSTN_HurtboxSetEditorToolkit();
	virtual ~FGSTN_HurtboxSetEditorToolkit() override;

	/** Opens the editor on a set. False, nothing opened, when the set is null */
	bool InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UGSTN_HurtboxSetDefinition* InSet);

	const TSharedPtr<FGSTN_HurtboxEditModel>& GetModel() const { return Model; }
	const TSharedPtr<FGSTN_PreviewController>& GetPreview() const { return Preview; }
	const TSharedRef<FGSTN_FindingsStore>& GetFindings() const { return Findings; }

	/** The default layout, apart so a test can read its shape without opening a window */
	static TSharedRef<FTabManager::FLayout> MakeDefaultLayout();

	/**
	 * Generate from the Physics Asset of the preview mesh. Replace all asks first when the set has hurtboxes. Returns what it did (all zero when it could
	 * not run: no mesh, no Physics Asset, or the designer said no); the reason is shown in a notification
	 */
	FGSTN_RegenerateSummary GenerateFromPhysicsAsset(EGSTN_RegenerateMode Mode);

	/** Mirror the selected hurtboxes to the other side. Overwriting existing counterparts asks first. Returns how many hurtboxes were written */
	int32 MirrorSelection();

	/** Mirroring for the mesh and settings as they are now: on or off, the pairs, and the sentence that says why */
	const GSTNHurtboxMirror::FResolvedConfig& GetMirrorConfig() const { return MirrorConfig; }

	/** Adds a small sphere at the root, named Hurtbox (unique), and selects it */
	void AddHurtbox();

	/** Removes the selected hurtboxes after a confirmation */
	void RemoveSelected();

	/** Re-reads the findings now (the set, the mesh, the settings' sane radius) */
	FGSTN_ValidationReport RefreshFindings();

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
	TSharedRef<SDockTab> SpawnHurtboxesTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnFindingsTab(const FSpawnTabArgs& Args);

	/** The set, the mesh or the settings changed: the findings, the mirror state and the preview mesh follow */
	void HandleSetChanged();
	void HandleSubjectChanged();
	void RefreshMirrorConfig();

	/** The mesh of the set's PreviewMesh field goes to the preview, when it names one the preview does not show */
	void SyncPreviewFromSet();

	void DrawHurtboxes(FPrimitiveDrawInterface* PDI);
	void GoToFinding(const FGSTN_ValidationFinding& Finding);
	void Notify(const FText& Message, float Seconds = 5.f) const;

	TWeakObjectPtr<UGSTN_HurtboxSetDefinition> Set;
	TSharedPtr<FGSTN_HurtboxEditModel> Model;

	/** Declared after the model it points at, so it is destroyed first */
	TUniquePtr<FGSTN_HurtboxUndoClient> UndoClient;

	TSharedPtr<FGSTN_PreviewController> Preview;
	TSharedRef<FGSTN_FindingsStore> Findings = MakeShared<FGSTN_FindingsStore>();
	GSTNHurtboxMirror::FResolvedConfig MirrorConfig;

	FDelegateHandle ChangedHandle;
	FDelegateHandle SubjectHandle;
};

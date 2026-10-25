// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "Hurtbox/GSTN_HurtboxGenerator.h"
#include "Hurtbox/GSTN_HurtboxMirror.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FScopedTransaction;
class USkeletalMesh;

// This header and its source include no UI, widget or viewport type: the toolkit, its list, its details and the tests all call the model, and only
// the model writes the set (combat-m5-editor 4.2 applied to the hurtbox set editor, plan m5 E9).

/**
 * The state and the mutations of one open hurtbox set editor. Same rules as the timeline model: a mutation that would change nothing opens no
 * transaction; otherwise it opens one scoped transaction with a description, records the set, changes it, marks the package dirty and tells the
 * listeners. Inside an edit session (BeginEdit, for a slider the designer is dragging in the details) the transaction opens at the first real
 * change and every later change joins it, so a drag is one undo step.
 *
 * The set is held weakly; every call resolves it again. Hurtboxes are addressed by position, which is what a hit carries (HurtboxIndex), so
 * nothing here reorders the array: add appends, remove closes the gap (and says so in its comment, since it shifts later indices).
 */
class FGSTN_HurtboxEditModel : public TSharedFromThis<FGSTN_HurtboxEditModel>
{
public:
	explicit FGSTN_HurtboxEditModel(UGSTN_HurtboxSetDefinition* InSet);
	FGSTN_HurtboxEditModel(const FGSTN_HurtboxEditModel&) = delete;
	FGSTN_HurtboxEditModel& operator=(const FGSTN_HurtboxEditModel&) = delete;
	~FGSTN_HurtboxEditModel();

	UGSTN_HurtboxSetDefinition* GetSet() const { return Set.Get(); }
	bool IsValid() const { return Set.IsValid(); }

	/** The hurtboxes, null when the set is gone */
	const TArray<FGSTN_Hurtbox>* GetHurtboxes() const;
	int32 Num() const;

	// Selection (positions in the set). Editor state: no transaction

	const TArray<int32>& GetSelection() const { return Selection; }
	void SetSelection(TConstArrayView<int32> NewSelection);
	void ClearSelection() { SetSelection(TConstArrayView<int32>()); }

	/** The one selected position, INDEX_NONE for none or several */
	int32 GetSingleSelection() const { return Selection.Num() == 1 ? Selection[0] : INDEX_NONE; }

	// Mutations

	/** Appends a hurtbox (its name made unique if taken) and selects it. Returns its index */
	int32 AddHurtbox(const FGSTN_Hurtbox& New);

	/** Removes the hurtboxes at these positions; later ones shift down. Returns how many went */
	int32 RemoveHurtboxes(TConstArrayView<int32> Indices);

	/** Replaces one hurtbox with the value, unclamped (an invalid value is allowed and shown by the findings). False if the index is bad or nothing changed */
	bool SetHurtbox(int32 Index, const FGSTN_Hurtbox& Value, const FText& Description = FText());

	/** Replaces the shape of one hurtbox (what a gizmo drag will call) */
	bool SetShape(int32 Index, const FGSTN_HitShape& Shape);

	/** Replaces the whole array and the entity defaults untouched: the generator's and the mirror's way in. One step */
	bool SetHurtboxes(const TArray<FGSTN_Hurtbox>& NewHurtboxes, const FText& Description);

	/** Regenerates from a generation by name (see EGSTN_RegenerateMode). One step. Returns what happened */
	FGSTN_RegenerateSummary ApplyGeneration(const FGSTN_HurtboxGenResult& Generation, EGSTN_RegenerateMode Mode);

	/** Writes a mirror plan: overwritten hurtboxes in place, new ones appended. One step. Returns how many were written */
	int32 ApplyMirror(const GSTNHurtboxMirror::FPlan& Plan);

	/** The mesh the set was authored on (editor-only field of the set) */
	bool SetPreviewMesh(const TSoftObjectPtr<USkeletalMesh>& Mesh);

	/** A name no hurtbox of the set has, starting from Base ("Hurtbox", "Hurtbox_2"...) */
	FName MakeUniqueName(FName Base) const;

	// Edit sessions (interactive changes)

	/** The next mutations until EndEdit are one undo step. False if a session is already open */
	bool BeginEdit(const FText& Description);
	void EndEdit();
	bool IsEditing() const { return bSession; }

	/** After an undo or redo that touched the set: drops the selection that no longer exists and tells the listeners */
	void HandleUndoRedo();

	/** The set changed (every mutation, undo and redo) */
	FSimpleMulticastDelegate OnChanged;
	FSimpleMulticastDelegate OnSelectionChanged;

private:
	/** Runs Edit inside the transaction of this mutation (or the session's). Edit returns whether it changed anything; a no-change opens no step */
	bool Mutate(const FText& Description, TFunctionRef<bool(UGSTN_HurtboxSetDefinition&)> Edit);

	void Revalidate();

	TWeakObjectPtr<UGSTN_HurtboxSetDefinition> Set;
	TArray<int32> Selection;

	bool bSession = false;
	FText SessionDescription;
	TUniquePtr<FScopedTransaction> SessionTransaction;
};

/** Connects a hurtbox set editor to the editor's undo: after an undo or redo that touched the set the model revalidates and tells its listeners */
class FGSTN_HurtboxUndoClient : public FEditorUndoClient
{
public:
	explicit FGSTN_HurtboxUndoClient(FGSTN_HurtboxEditModel& InModel);
	virtual ~FGSTN_HurtboxUndoClient() override;

	FGSTN_HurtboxUndoClient(const FGSTN_HurtboxUndoClient&) = delete;
	FGSTN_HurtboxUndoClient& operator=(const FGSTN_HurtboxUndoClient&) = delete;

	//~ FEditorUndoClient
	virtual bool MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const override;
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

private:
	FGSTN_HurtboxEditModel& Model;
};

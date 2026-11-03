// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "Misc/Optional.h"
#include "Timeline/GSTN_TimelineEditContext.h"

class FGSTN_TimelineEditorModel;

/**
 * One drag, one undo step (combat-m5-editor 4.3). Begin on mouse-down starts the drag; the first mutation inside it opens the
 * transaction (EnsureTransaction), and every later mutation joins it, so each mouse move edits the data live and the whole drag
 * is a single entry in the undo history. End on mouse-up closes it.
 *
 * The transaction opens at the first change, not at mouse-down: a click that moves nothing, or a drag that only meets the clamp,
 * must not leave an empty step in the history.
 *
 * Cancel (Escape) has to do more than the editor's CancelTransaction: that call only drops the undo record and leaves the
 * edited data as the drag left it. So Begin keeps a copy of the data, and Cancel drops the record and puts the copy back, which
 * leaves the data and the history exactly as they were before the drag. The copy is the whole timeline data: a timeline is
 * small, and a drag can touch any of it.
 */
class FGSTN_TimelineDragTransaction
{
public:
	FGSTN_TimelineDragTransaction() = default;
	FGSTN_TimelineDragTransaction(const FGSTN_TimelineDragTransaction&) = delete;
	FGSTN_TimelineDragTransaction& operator=(const FGSTN_TimelineDragTransaction&) = delete;

	/** An abandoned drag is cancelled, never left open: an open transaction would swallow every later edit */
	~FGSTN_TimelineDragTransaction();

	/** Starts a drag and keeps a copy of the data. False when a drag is already running, the context is invalid or no editor transaction buffer exists */
	bool Begin(const FText& Description, const FGSTN_TimelineEditContext& InContext);

	/** Called by a mutation inside the drag before it edits: opens the drag's transaction the first time, does nothing after */
	void EnsureTransaction();

	/** Closes the transaction if a change opened one: the drag becomes one undo step */
	void End();

	/** Drops the transaction, if open, and restores the data as it was at Begin. False when no drag was running */
	bool Cancel();

	/** A drag is running (between Begin and End or Cancel) */
	bool IsActive() const { return bActive; }

	/** A change has opened the transaction */
	bool HasTransaction() const { return TransactionIndex != INDEX_NONE; }

private:
	bool bActive = false;
	int32 TransactionIndex = INDEX_NONE;
	FText Description;
	FGSTN_TimelineEditContext Context;
	TOptional<FGSTN_CombatTimelineData> Snapshot;
};

/**
 * Connects a model to the editor's undo system: after an undo or redo that touched the model's owner it revalidates the
 * selection and the playhead and tells the listeners the data changed (combat-m5-editor 4.3). The toolkit owns one, and so does a test;
 * it registers itself on construction and unregisters on destruction.
 */
class FGSTN_TimelineUndoClient : public FEditorUndoClient
{
public:
	explicit FGSTN_TimelineUndoClient(FGSTN_TimelineEditorModel& InModel);
	virtual ~FGSTN_TimelineUndoClient() override;

	FGSTN_TimelineUndoClient(const FGSTN_TimelineUndoClient&) = delete;
	FGSTN_TimelineUndoClient& operator=(const FGSTN_TimelineUndoClient&) = delete;

	//~ FEditorUndoClient
	virtual bool MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const override;
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

private:
	FGSTN_TimelineEditorModel& Model;
};

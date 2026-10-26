// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_TimelineUndo.h"

#include "Editor.h"
#include "Timeline/GSTN_TimelineEditorModel.h"

FGSTN_TimelineDragTransaction::~FGSTN_TimelineDragTransaction()
{
	if (IsActive())
	{
		Cancel();
	}
}

bool FGSTN_TimelineDragTransaction::Begin(const FText& InDescription, const FGSTN_TimelineEditContext& InContext)
{
	const FGSTN_CombatTimelineData* Data = InContext.Get();
	if (IsActive() || Data == nullptr || GEditor == nullptr || GEditor->Trans == nullptr)
	{
		return false;
	}

	Context = InContext;
	Description = InDescription;
	Snapshot = *Data;
	bActive = true;
	return true;
}

void FGSTN_TimelineDragTransaction::EnsureTransaction()
{
	if (bActive && !HasTransaction() && GEditor)
	{
		TransactionIndex = GEditor->BeginTransaction(TEXT("GSTNCombatEditor"), Description, Context.GetOwner());
	}
}

void FGSTN_TimelineDragTransaction::End()
{
	if (!bActive)
	{
		return;
	}
	const bool bOpen = HasTransaction();
	bActive = false;
	TransactionIndex = INDEX_NONE;
	Snapshot.Reset();
	if (bOpen && GEditor)
	{
		GEditor->EndTransaction();
	}
}

bool FGSTN_TimelineDragTransaction::Cancel()
{
	if (!bActive)
	{
		return false;
	}

	const bool bOpen = HasTransaction();
	const int32 Index = TransactionIndex;
	bActive = false;
	TransactionIndex = INDEX_NONE;
	if (bOpen && GEditor)
	{
		GEditor->CancelTransaction(Index);
	}

	// The record is gone but the data is not: put the copy back, outside any transaction, so nothing new reaches the history.
	// Without an open transaction nothing was edited, and the data already equals the copy
	if (bOpen)
	{
		if (FGSTN_CombatTimelineData* Data = Context.Get(); Data != nullptr && Snapshot.IsSet())
		{
			*Data = Snapshot.GetValue();
			Context.NotifyChanged();
		}
	}
	Snapshot.Reset();
	return true;
}

FGSTN_TimelineUndoClient::FGSTN_TimelineUndoClient(FGSTN_TimelineEditorModel& InModel)
	: Model(InModel)
{
	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}
}

FGSTN_TimelineUndoClient::~FGSTN_TimelineUndoClient()
{
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
}

bool FGSTN_TimelineUndoClient::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
{
	const UObject* Owner = Model.GetContext().GetOwner();
	if (Owner == nullptr)
	{
		return false;
	}
	for (const TPair<UObject*, FTransactionObjectEvent>& Entry : TransactionObjectContexts)
	{
		if (Entry.Key == Owner)
		{
			return true;
		}
	}
	return false;
}

void FGSTN_TimelineUndoClient::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
		Model.HandleUndoRedo();
	}
}

void FGSTN_TimelineUndoClient::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
		Model.HandleUndoRedo();
	}
}

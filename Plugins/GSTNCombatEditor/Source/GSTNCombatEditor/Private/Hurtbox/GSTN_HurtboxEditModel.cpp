// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Hurtbox/GSTN_HurtboxEditModel.h"

#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace
{
	/** The struct has no operator==: two hurtboxes are equal when their exported text is, which covers every field the details panel can change */
	bool AreEqual(const FGSTN_Hurtbox& A, const FGSTN_Hurtbox& B)
	{
		FString TextA;
		FString TextB;
		FGSTN_Hurtbox::StaticStruct()->ExportText(TextA, &A, nullptr, nullptr, PPF_None, nullptr);
		FGSTN_Hurtbox::StaticStruct()->ExportText(TextB, &B, nullptr, nullptr, PPF_None, nullptr);
		return TextA == TextB;
	}

	bool AreEqual(const TArray<FGSTN_Hurtbox>& A, const TArray<FGSTN_Hurtbox>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (!AreEqual(A[Index], B[Index]))
			{
				return false;
			}
		}
		return true;
	}
}

FGSTN_HurtboxEditModel::FGSTN_HurtboxEditModel(UGSTN_HurtboxSetDefinition* InSet)
	: Set(InSet)
{
	// An asset that was not created transactional (an old one, a script) would record nothing and undo would have nothing to restore
	if (InSet != nullptr)
	{
		InSet->SetFlags(RF_Transactional);
	}
}

FGSTN_HurtboxEditModel::~FGSTN_HurtboxEditModel()
{
	// An open session must not outlive the model: its transaction would swallow every later edit of the editor
	EndEdit();
}

const TArray<FGSTN_Hurtbox>* FGSTN_HurtboxEditModel::GetHurtboxes() const
{
	const UGSTN_HurtboxSetDefinition* Resolved = Set.Get();
	return Resolved != nullptr ? &Resolved->Hurtboxes : nullptr;
}

int32 FGSTN_HurtboxEditModel::Num() const
{
	const TArray<FGSTN_Hurtbox>* Hurtboxes = GetHurtboxes();
	return Hurtboxes != nullptr ? Hurtboxes->Num() : 0;
}

void FGSTN_HurtboxEditModel::SetSelection(TConstArrayView<int32> NewSelection)
{
	const int32 Count = Num();
	TArray<int32> Clean;
	for (const int32 Index : NewSelection)
	{
		if (Index >= 0 && Index < Count)
		{
			Clean.AddUnique(Index);
		}
	}
	Clean.Sort();
	if (Clean != Selection)
	{
		Selection = MoveTemp(Clean);
		OnSelectionChanged.Broadcast();
	}
}

void FGSTN_HurtboxEditModel::Revalidate()
{
	SetSelection(Selection);
}

bool FGSTN_HurtboxEditModel::BeginEdit(const FText& Description)
{
	if (bSession)
	{
		return false;
	}
	bSession = true;
	SessionDescription = Description;
	const TArray<FGSTN_Hurtbox>* Hurtboxes = GetHurtboxes();
	SessionSnapshot = Hurtboxes != nullptr ? *Hurtboxes : TArray<FGSTN_Hurtbox>();
	return true;
}

void FGSTN_HurtboxEditModel::EndEdit()
{
	bSession = false;
	SessionTransaction.Reset();
	SessionSnapshot.Reset();
}

void FGSTN_HurtboxEditModel::CancelEdit()
{
	if (!bSession)
	{
		return;
	}
	const bool bOpen = SessionTransaction.IsValid();
	if (bOpen)
	{
		SessionTransaction->Cancel();
	}
	bSession = false;
	SessionTransaction.Reset();

	// The record is gone and the data is not: put the copy back, outside any transaction, so nothing new reaches the history
	if (UGSTN_HurtboxSetDefinition* Resolved = Set.Get(); Resolved != nullptr && bOpen)
	{
		Resolved->Hurtboxes = SessionSnapshot;
		Resolved->PostEditChange();
		Revalidate();
		OnChanged.Broadcast();
	}
	SessionSnapshot.Reset();
}

bool FGSTN_HurtboxEditModel::Mutate(const FText& Description, TFunctionRef<bool(UGSTN_HurtboxSetDefinition&)> Edit)
{
	UGSTN_HurtboxSetDefinition* Resolved = Set.Get();
	if (Resolved == nullptr)
	{
		return false;
	}

	// The transaction opens before the edit (the record is of the data as it was) and is dropped if the edit turns out to change nothing, so a
	// no-op leaves no empty step in the undo history. Inside a session the first change opens the session's transaction and the rest join it
	TUniquePtr<FScopedTransaction> OwnTransaction;
	if (bSession)
	{
		if (!SessionTransaction.IsValid())
		{
			SessionTransaction = MakeUnique<FScopedTransaction>(SessionDescription.IsEmpty() ? Description : SessionDescription);
		}
	}
	else
	{
		OwnTransaction = MakeUnique<FScopedTransaction>(Description);
	}
	Resolved->Modify();

	if (!Edit(*Resolved))
	{
		if (OwnTransaction.IsValid())
		{
			OwnTransaction->Cancel();
		}
		return false;
	}
	Resolved->MarkPackageDirty();
	Resolved->PostEditChange();
	Revalidate();
	OnChanged.Broadcast();
	return true;
}

FName FGSTN_HurtboxEditModel::MakeUniqueName(FName Base) const
{
	const TArray<FGSTN_Hurtbox>* Hurtboxes = GetHurtboxes();
	auto IsTaken = [Hurtboxes](FName Name)
	{
		return Hurtboxes != nullptr && Hurtboxes->ContainsByPredicate([Name](const FGSTN_Hurtbox& Existing) { return Existing.Name == Name; });
	};
	if (Base.IsNone())
	{
		Base = TEXT("Hurtbox");
	}
	FName Candidate = Base;
	for (int32 Suffix = 2; IsTaken(Candidate); ++Suffix)
	{
		Candidate = FName(*FString::Printf(TEXT("%s_%d"), *Base.ToString(), Suffix));
	}
	return Candidate;
}

int32 FGSTN_HurtboxEditModel::AddHurtbox(const FGSTN_Hurtbox& New)
{
	int32 NewIndex = INDEX_NONE;
	Mutate(LOCTEXT("AddHurtbox", "Add hurtbox"), [&](UGSTN_HurtboxSetDefinition& Edited)
	{
		FGSTN_Hurtbox Added = New;
		Added.Name = MakeUniqueName(New.Name);
		NewIndex = Edited.Hurtboxes.Add(Added);
		return true;
	});
	if (NewIndex != INDEX_NONE)
	{
		const int32 One[] = { NewIndex };
		SetSelection(One);
	}
	return NewIndex;
}

int32 FGSTN_HurtboxEditModel::RemoveHurtboxes(TConstArrayView<int32> Indices)
{
	TArray<int32> Sorted;
	Sorted.Append(Indices.GetData(), Indices.Num());
	Sorted.Sort([](int32 A, int32 B) { return A > B; });
	int32 Removed = 0;
	Mutate(LOCTEXT("RemoveHurtboxes", "Remove hurtboxes"), [&](UGSTN_HurtboxSetDefinition& Edited)
	{
		int32 Last = INDEX_NONE;
		for (const int32 Index : Sorted)
		{
			if (Index != Last && Edited.Hurtboxes.IsValidIndex(Index))
			{
				Edited.Hurtboxes.RemoveAt(Index);
				++Removed;
			}
			Last = Index;
		}
		return Removed > 0;
	});
	return Removed;
}

bool FGSTN_HurtboxEditModel::SetHurtbox(int32 Index, const FGSTN_Hurtbox& Value, const FText& Description)
{
	const TArray<FGSTN_Hurtbox>* Hurtboxes = GetHurtboxes();
	if (Hurtboxes == nullptr || !Hurtboxes->IsValidIndex(Index))
	{
		return false;
	}
	return Mutate(Description.IsEmpty() ? LOCTEXT("EditHurtbox", "Edit hurtbox") : Description, [&](UGSTN_HurtboxSetDefinition& Edited)
	{
		FGSTN_Hurtbox& Target = Edited.Hurtboxes[Index];
		if (AreEqual(Target, Value))
		{
			return false;
		}
		Target = Value;
		return true;
	});
}

bool FGSTN_HurtboxEditModel::SetShape(int32 Index, const FGSTN_HitShape& Shape)
{
	const TArray<FGSTN_Hurtbox>* Hurtboxes = GetHurtboxes();
	if (Hurtboxes == nullptr || !Hurtboxes->IsValidIndex(Index))
	{
		return false;
	}
	FGSTN_Hurtbox Changed = (*Hurtboxes)[Index];
	Changed.Shape = Shape;
	return SetHurtbox(Index, Changed, LOCTEXT("EditHurtboxShape", "Edit hurtbox shape"));
}

bool FGSTN_HurtboxEditModel::SetHurtboxes(const TArray<FGSTN_Hurtbox>& NewHurtboxes, const FText& Description)
{
	return Mutate(Description, [&](UGSTN_HurtboxSetDefinition& Edited)
	{
		if (AreEqual(Edited.Hurtboxes, NewHurtboxes))
		{
			return false;
		}
		Edited.Hurtboxes = NewHurtboxes;
		return true;
	});
}

FGSTN_RegenerateSummary FGSTN_HurtboxEditModel::ApplyGeneration(const FGSTN_HurtboxGenResult& Generation, EGSTN_RegenerateMode Mode)
{
	FGSTN_RegenerateSummary Summary;
	const TArray<FGSTN_Hurtbox>* Existing = GetHurtboxes();
	if (Existing == nullptr)
	{
		return Summary;
	}
	const TArray<FGSTN_Hurtbox> Merged = GSTNHurtboxGenerator::Merge(*Existing, Generation.Hurtboxes, Mode, &Summary);
	const FText Description = Mode == EGSTN_RegenerateMode::ReplaceAll
		? LOCTEXT("GenerateReplaceAll", "Generate hurtboxes (replace all)")
		: LOCTEXT("GenerateKeep", "Generate hurtboxes");
	SetHurtboxes(Merged, Description);
	return Summary;
}

int32 FGSTN_HurtboxEditModel::ApplyMirror(const GSTNHurtboxMirror::FPlan& Plan)
{
	const TArray<FGSTN_Hurtbox>* Existing = GetHurtboxes();
	if (Existing == nullptr || Plan.Entries.IsEmpty())
	{
		return 0;
	}
	const TArray<FGSTN_Hurtbox> Mirrored = GSTNHurtboxMirror::Apply(*Existing, Plan);
	return SetHurtboxes(Mirrored, LOCTEXT("MirrorHurtboxes", "Mirror hurtboxes")) ? Plan.Entries.Num() : 0;
}

bool FGSTN_HurtboxEditModel::SetPreviewMesh(const TSoftObjectPtr<USkeletalMesh>& Mesh)
{
	return Mutate(LOCTEXT("SetPreviewMesh", "Set hurtbox preview mesh"), [&](UGSTN_HurtboxSetDefinition& Edited)
	{
#if WITH_EDITORONLY_DATA
		if (Edited.PreviewMesh == Mesh)
		{
			return false;
		}
		Edited.PreviewMesh = Mesh;
		return true;
#else
		return false;
#endif
	});
}

void FGSTN_HurtboxEditModel::HandleUndoRedo()
{
	Revalidate();
	OnChanged.Broadcast();
}

FGSTN_HurtboxUndoClient::FGSTN_HurtboxUndoClient(FGSTN_HurtboxEditModel& InModel)
	: Model(InModel)
{
	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}
}

FGSTN_HurtboxUndoClient::~FGSTN_HurtboxUndoClient()
{
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
}

bool FGSTN_HurtboxUndoClient::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
{
	const UObject* Owner = Model.GetSet();
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

void FGSTN_HurtboxUndoClient::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
		Model.HandleUndoRedo();
	}
}

void FGSTN_HurtboxUndoClient::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
		Model.HandleUndoRedo();
	}
}

#undef LOCTEXT_NAMESPACE

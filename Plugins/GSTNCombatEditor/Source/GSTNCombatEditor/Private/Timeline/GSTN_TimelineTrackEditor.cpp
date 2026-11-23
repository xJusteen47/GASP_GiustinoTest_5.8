// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_TimelineTrackEditor.h"

#include "Settings/GSTN_CombatEditorSettings.h"
#include "Timeline/GSTN_KeyedTrackAccess.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace GSTNTrackEditorRegistry
{
	/** True for a struct a track editor can serve: a strict child of FGSTN_TimelineTrack */
	bool IsTrackStruct(const UScriptStruct* Struct)
	{
		const UScriptStruct* Base = FGSTN_TimelineTrack::StaticStruct();
		return Struct != nullptr && Struct != Base && Struct->IsChildOf(Base);
	}

	/** The row of a track struct nobody registered: a window bar named by the struct, everything else in the details panel */
	class FGenericTrackEditor final : public IGSTN_TrackEditor
	{
	public:
		explicit FGenericTrackEditor(const UScriptStruct* InStruct) : Struct(InStruct) {}

		virtual const UScriptStruct* GetTrackStruct() const override { return Struct.Get(); }

		virtual FText GetDisplayName() const override
		{
			const UScriptStruct* Resolved = Struct.Get();
			return Resolved ? Resolved->GetDisplayNameText() : LOCTEXT("GenericTrackName", "Track");
		}

	private:
		TWeakObjectPtr<const UScriptStruct> Struct;
	};
}

FLinearColor IGSTN_TrackEditor::GetColor() const
{
	const UScriptStruct* Struct = GetTrackStruct();
	return UGSTN_CombatEditorSettings::Get().GetTrackColor(Struct ? Struct->GetFName() : NAME_None);
}

FText IGSTN_TrackEditor::GetRowLabel(const FGSTN_TimelineTrack& Track) const
{
	return GetDisplayName();
}

bool IGSTN_TrackEditor::HasKeys() const
{
	return FGSTN_KeyedTrackAccessRegistry::Find(GetTrackStruct()) != nullptr;
}

FGSTN_TrackEditorRegistry& FGSTN_TrackEditorRegistry::Get()
{
	static FGSTN_TrackEditorRegistry Instance;
	return Instance;
}

void FGSTN_TrackEditorRegistry::Register(TSharedRef<const IGSTN_TrackEditor> Editor)
{
	const UScriptStruct* Struct = Editor->GetTrackStruct();
	if (!GSTNTrackEditorRegistry::IsTrackStruct(Struct))
	{
		return;
	}
	Editors.Add(Struct, Editor);
	OnChanged.Broadcast();
}

bool FGSTN_TrackEditorRegistry::Unregister(const UScriptStruct* TrackStruct)
{
	if (Editors.Remove(TrackStruct) == 0)
	{
		return false;
	}
	OnChanged.Broadcast();
	return true;
}

TSharedPtr<const IGSTN_TrackEditor> FGSTN_TrackEditorRegistry::Find(const UScriptStruct* TrackStruct) const
{
	// The exact struct first, then its bases: a game's own child of a built-in track keeps the built-in row
	for (const UStruct* Struct = TrackStruct; Struct != nullptr; Struct = Struct->GetSuperStruct())
	{
		const UScriptStruct* Script = Cast<UScriptStruct>(Struct);
		if (!GSTNTrackEditorRegistry::IsTrackStruct(Script))
		{
			break;
		}
		if (const TSharedRef<const IGSTN_TrackEditor>* Found = Editors.Find(Script))
		{
			return *Found;
		}
	}
	return nullptr;
}

TSharedRef<const IGSTN_TrackEditor> FGSTN_TrackEditorRegistry::FindOrGeneric(const UScriptStruct* TrackStruct) const
{
	if (const TSharedPtr<const IGSTN_TrackEditor> Registered = Find(TrackStruct))
	{
		return Registered.ToSharedRef();
	}

	// Entries of structs that were unloaded since are dropped on the way
	for (auto It = GenericEditors.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	const TWeakObjectPtr<const UScriptStruct> Key(TrackStruct);
	if (const TSharedRef<const IGSTN_TrackEditor>* Existing = GenericEditors.Find(Key))
	{
		return *Existing;
	}
	return GenericEditors.Add(Key, MakeShared<GSTNTrackEditorRegistry::FGenericTrackEditor>(TrackStruct));
}

void FGSTN_TrackEditorRegistry::GetAll(TArray<TSharedRef<const IGSTN_TrackEditor>>& OutEditors) const
{
	OutEditors.Reset();
	for (const TPair<const UScriptStruct*, TSharedRef<const IGSTN_TrackEditor>>& Pair : Editors)
	{
		OutEditors.Add(Pair.Value);
	}
	OutEditors.Sort([](const TSharedRef<const IGSTN_TrackEditor>& A, const TSharedRef<const IGSTN_TrackEditor>& B)
	{
		return A->GetDisplayName().CompareTo(B->GetDisplayName()) < 0;
	});
}

void FGSTN_TrackEditorRegistry::GetUnregisteredTrackStructs(TArray<const UScriptStruct*>& OutStructs) const
{
	OutStructs.Reset();
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		const UScriptStruct* Struct = *It;
		if (GSTNTrackEditorRegistry::IsTrackStruct(Struct) && !Struct->HasMetaData(TEXT("Hidden")) && !IsRegistered(Struct))
		{
			OutStructs.Add(Struct);
		}
	}
	OutStructs.Sort([](const UScriptStruct& A, const UScriptStruct& B) { return A.GetDisplayNameText().CompareTo(B.GetDisplayNameText()) < 0; });
}

#undef LOCTEXT_NAMESPACE

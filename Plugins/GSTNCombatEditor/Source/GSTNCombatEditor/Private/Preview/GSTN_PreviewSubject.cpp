// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Preview/GSTN_PreviewSubject.h"

#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_SkinDefinition.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

FGSTN_ResolvedSubject GSTNPreviewSubject::Resolve(const FGSTN_PreviewSubject& Subject)
{
	FGSTN_ResolvedSubject Out;
	Out.Label = LOCTEXT("SubjectNone", "No subject");

	switch (Subject.Kind)
	{
	case EGSTN_PreviewSubjectKind::KitSkin:
	{
		const UGSTN_PrimaryKitDefinition* Kit = Subject.Kit.IsNull() ? nullptr : Subject.Kit.LoadSynchronous();
		const UGSTN_SkinDefinition* Skin = Subject.Skin.IsNull() ? nullptr : Subject.Skin.LoadSynchronous();
		if (Kit == nullptr)
		{
			Out.Problems.Add(LOCTEXT("ProblemNoKit", "Pick a kit."));
		}
		if (Skin == nullptr)
		{
			Out.Problems.Add(LOCTEXT("ProblemNoSkin", "Pick a skin of the kit."));
		}
		else
		{
			if (Kit != nullptr && !Kit->CompatibleSkins.IsEmpty() && !Kit->CompatibleSkins.Matches(Skin->SkinTags))
			{
				Out.Problems.Add(LOCTEXT("ProblemSkinNotCompatible", "The skin does not match the kit's CompatibleSkins query. It is shown anyway."));
			}
			Out.Mesh = Skin->Mesh.IsNull() ? nullptr : Skin->Mesh.LoadSynchronous();
			if (Out.Mesh == nullptr)
			{
				Out.Problems.Add(LOCTEXT("ProblemSkinNoMesh", "The skin has no mesh (it keeps the character's own, which the kit does not name)."));
			}
			if (!Skin->AnimClass.IsNull())
			{
				Out.AnimClass = Skin->AnimClass.LoadSynchronous();
			}
		}
		if (!Out.AnimClass && Kit != nullptr && !Kit->LocomotionAnimLayer.IsNull())
		{
			Out.AnimClass = Kit->LocomotionAnimLayer.LoadSynchronous();
		}
		Out.Label = FText::Format(LOCTEXT("SubjectKitSkinLabel", "{0} / {1}"),
			FText::FromString(Kit ? Kit->GetName() : TEXT("?")), FText::FromString(Skin ? Skin->GetName() : TEXT("?")));
		break;
	}
	case EGSTN_PreviewSubjectKind::SkeletalMesh:
		if (Subject.Mesh.IsNull())
		{
			Out.Problems.Add(LOCTEXT("ProblemNoMesh", "Pick a skeletal mesh."));
		}
		else
		{
			Out.Mesh = Subject.Mesh.LoadSynchronous();
			if (Out.Mesh == nullptr)
			{
				Out.Problems.Add(FText::Format(LOCTEXT("ProblemMeshNotLoaded", "The mesh {0} could not be loaded."), FText::FromString(Subject.Mesh.ToString())));
			}
			else
			{
				Out.Label = FText::FromString(Out.Mesh->GetName());
			}
		}
		if (!Subject.AnimClass.IsNull())
		{
			Out.AnimClass = Subject.AnimClass.LoadSynchronous();
		}
		break;
	default:
		break;
	}

	for (const FGSTN_PreviewAttachment& Attachment : Subject.Attachments)
	{
		if (Attachment.Mesh.IsNull())
		{
			// A row being filled in: nothing to show and nothing to complain about yet
			continue;
		}
		UObject* Loaded = Attachment.Mesh.LoadSynchronous();
		if (Loaded == nullptr || !(Loaded->IsA<USkeletalMesh>() || Loaded->IsA<UStaticMesh>()))
		{
			Out.Problems.Add(FText::Format(LOCTEXT("ProblemAttachment", "The attachment {0} is not a skeletal or static mesh that loads."), FText::FromString(Attachment.Mesh.ToString())));
			continue;
		}
		FGSTN_ResolvedAttachment& Resolved = Out.Attachments.AddDefaulted_GetRef();
		Resolved.Mesh = Loaded;
		Resolved.ParentSocket = Attachment.ParentSocket;
		Resolved.Relative = Attachment.Relative;
	}
	return Out;
}

FQuat GSTNPreviewSubject::MeshToRootRotation(EAxis::Type MeshForwardAxis)
{
	switch (MeshForwardAxis)
	{
	case EAxis::Y:
		// Mesh +Y to root +X is a quarter turn about the up axis, clockwise seen from above (-90 degrees of yaw)
		return FQuat(FVector::UpVector, -UE_HALF_PI);
	case EAxis::Z:
		// A mesh that faces up: tip it forward so +Z becomes +X
		return FQuat(FVector::RightVector, UE_HALF_PI);
	default:
		return FQuat::Identity;
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------

namespace
{
	UGSTN_CombatEditorPreviewState& State()
	{
		return *GetMutableDefault<UGSTN_CombatEditorPreviewState>();
	}

	bool IsBlank(const FGSTN_PreviewSubject& Subject)
	{
		return Subject.Kind == EGSTN_PreviewSubjectKind::None && Subject.Attachments.IsEmpty();
	}
}

bool UGSTN_CombatEditorPreviewState::Find(const FSoftObjectPath& Asset, FGSTN_PreviewSubject& OutSubject)
{
	for (const FGSTN_PreviewSubjectEntry& Entry : State().Entries)
	{
		if (Entry.Asset == Asset)
		{
			OutSubject = Entry.Subject;
			return true;
		}
	}
	return false;
}

void UGSTN_CombatEditorPreviewState::Store(const FSoftObjectPath& Asset, const FGSTN_PreviewSubject& Subject)
{
	if (IsBlank(Subject))
	{
		Remove(Asset);
		return;
	}
	UGSTN_CombatEditorPreviewState& Self = State();
	FGSTN_PreviewSubjectEntry* Existing = Self.Entries.FindByPredicate([&Asset](const FGSTN_PreviewSubjectEntry& Entry) { return Entry.Asset == Asset; });
	if (Existing == nullptr)
	{
		Existing = &Self.Entries.AddDefaulted_GetRef();
		Existing->Asset = Asset;
	}
	Existing->Subject = Subject;
	Self.SaveConfig();
}

void UGSTN_CombatEditorPreviewState::Remove(const FSoftObjectPath& Asset)
{
	UGSTN_CombatEditorPreviewState& Self = State();
	if (Self.Entries.RemoveAll([&Asset](const FGSTN_PreviewSubjectEntry& Entry) { return Entry.Asset == Asset; }) > 0)
	{
		Self.SaveConfig();
	}
}

void UGSTN_CombatEditorPreviewState::ReloadFromDisk()
{
	State().Entries.Reset();
	State().ReloadConfig();
}

#undef LOCTEXT_NAMESPACE

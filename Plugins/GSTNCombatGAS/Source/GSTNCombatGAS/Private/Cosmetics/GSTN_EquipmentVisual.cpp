// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Cosmetics/GSTN_EquipmentVisual.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"

namespace GSTNEquipment
{
	const FName Socket_ImplantBack(TEXT("Implant_Back"));
	const FName Socket_ImplantHandL(TEXT("Implant_Hand_L"));
	const FName Socket_ImplantHandR(TEXT("Implant_Hand_R"));
	const FName Socket_ImplantFootL(TEXT("Implant_Foot_L"));
	const FName Socket_ImplantFootR(TEXT("Implant_Foot_R"));
	const FName Socket_WeaponHandR(TEXT("Weapon_Hand_R"));
	const FName Socket_ConnectorBackpack(TEXT("Connector_Backpack"));
	const FName Slot_ImplantBack(TEXT("Implant.Back"));
}

namespace
{
	/** A weapon's parent chain is short; the cap only guards a cycle the weapon validator already reports */
	constexpr int32 MaxParentDepth = 16;

	void MergeParameters(FGSTN_AttachmentRequest& Into, const FGSTN_AttachmentRequest& From)
	{
		for (const TPair<FName, FLinearColor>& Pair : From.VectorParameters)
		{
			Into.VectorParameters.Add(Pair.Key, Pair.Value);
		}
		for (const TPair<FName, float>& Pair : From.ScalarParameters)
		{
			Into.ScalarParameters.Add(Pair.Key, Pair.Value);
		}
	}
}

void FGSTN_EquipmentVisualUtils::GatherLoadoutVisuals(const UGSTN_PrimaryKitDefinition* Kit, const UGSTN_WeaponDefinition* PrimaryWeapon, TArray<FGSTN_AttachmentRequest>& OutRequests)
{
	if (Kit)
	{
		for (const FGSTN_AttachmentRequest& Part : Kit->Visual.Parts)
		{
			FGSTN_AttachmentRequest& Added = OutRequests.Add_GetRef(Part);
			if (Added.VisualSlot.IsNone() && Added.ParentSlot.IsNone() && Added.Socket == GSTNEquipment::Socket_ImplantBack)
			{
				Added.VisualSlot = GSTNEquipment::Slot_ImplantBack;
			}
		}
	}

	// The weapon's own look, else the nearest loaded parent's: a variant weapon that only changes numbers keeps its parent's sword
	const UGSTN_WeaponDefinition* Weapon = PrimaryWeapon;
	for (int32 Depth = 0; Weapon && Depth < MaxParentDepth; ++Depth)
	{
		if (!Weapon->Visual.IsEmpty())
		{
			OutRequests.Append(Weapon->Visual.Parts);
			break;
		}
		Weapon = Weapon->ParentWeapon.Get();
	}
}

void FGSTN_EquipmentVisualUtils::Compose(const TArray<FGSTN_AttachmentRequest>& Requests, TArray<FGSTN_AttachmentRequest>& OutPieces)
{
	OutPieces.Reset();
	for (const FGSTN_AttachmentRequest& Request : Requests)
	{
		const bool bHasSlot = !Request.VisualSlot.IsNone();
		if (Request.Mesh.IsNull())
		{
			// A recolour: every piece made so far under the slot takes the parameters; nothing is built for it
			if (bHasSlot && Request.HasMaterialParameters())
			{
				for (FGSTN_AttachmentRequest& Piece : OutPieces)
				{
					if (Piece.VisualSlot == Request.VisualSlot)
					{
						MergeParameters(Piece, Request);
					}
				}
			}
			continue;
		}

		if (Request.bReplacesSlot && bHasSlot)
		{
			FGSTN_AttachmentRequest* Target = nullptr;
			for (int32 Index = OutPieces.Num() - 1; Index >= 0 && !Target; --Index)
			{
				Target = OutPieces[Index].VisualSlot == Request.VisualSlot ? &OutPieces[Index] : nullptr;
			}
			if (Target)
			{
				Target->Mesh = Request.Mesh;
				Target->Offset = Request.Offset;
				Target->bShownByDefault = Request.bShownByDefault;
				if (!Request.Socket.IsNone())
				{
					Target->Socket = Request.Socket;
					Target->ParentSlot = Request.ParentSlot;
				}
				MergeParameters(*Target, Request);
				continue;
			}
		}

		FGSTN_AttachmentRequest& Added = OutPieces.Add_GetRef(Request);
		Added.bReplacesSlot = false;
		if (!Added.ParentSlot.IsNone())
		{
			const FName Parent = Added.ParentSlot;
			const bool bParentMade = OutPieces.ContainsByPredicate([Parent, &Added](const FGSTN_AttachmentRequest& Piece)
			{
				return &Piece != &Added && Piece.VisualSlot == Parent;
			});
			if (!bParentMade)
			{
				Added.ParentSlot = NAME_None;
			}
		}
	}
}

bool FGSTN_EquipmentVisualUtils::MeshHasSocket(const UObject* Mesh, FName Socket)
{
	if (const UStaticMesh* Static = Cast<UStaticMesh>(Mesh))
	{
		return Static->FindSocket(Socket) != nullptr;
	}
	if (const USkeletalMesh* Skeletal = Cast<USkeletalMesh>(Mesh))
	{
		return Skeletal->FindSocket(Socket) != nullptr;
	}
	return false;
}

void FGSTN_EquipmentVisualUtils::Validate(const FGSTN_EquipmentVisual& Visual, const FString& Owner, TArray<FText>& OutWarnings)
{
	auto AddWarning = [&OutWarnings, &Owner](const FString& Text) { OutWarnings.Add(FText::FromString(FString::Printf(TEXT("%s Visual: %s"), *Owner, *Text))); };

	for (int32 Index = 0; Index < Visual.Parts.Num(); ++Index)
	{
		const FGSTN_AttachmentRequest& Part = Visual.Parts[Index];
		if (Part.Mesh.IsNull() && (Part.VisualSlot.IsNone() || !Part.HasMaterialParameters()))
		{
			AddWarning(FString::Printf(TEXT("part %d has no mesh and recolours nothing (a recolour needs a VisualSlot and a material parameter)."), Index));
			continue;
		}
		if (Part.bReplacesSlot && Part.VisualSlot.IsNone())
		{
			AddWarning(FString::Printf(TEXT("part %d replaces a slot but names none: it is added instead."), Index));
		}
		const bool bBackImplant = Part.ParentSlot.IsNone() && (Part.VisualSlot == GSTNEquipment::Slot_ImplantBack
			|| (Part.VisualSlot.IsNone() && Part.Socket == GSTNEquipment::Socket_ImplantBack));
		if (bBackImplant && !Part.Mesh.IsNull())
		{
			// Loaded only in the editor: a game build validating a loadout must not hitch on a cosmetic mesh
			const UObject* Mesh = GIsEditor ? Part.Mesh.LoadSynchronous() : Part.Mesh.Get();
			if (Mesh && !MeshHasSocket(Mesh, GSTNEquipment::Socket_ConnectorBackpack))
			{
				AddWarning(FString::Printf(TEXT("the back implant %s has no %s socket: a backpack will sit at its origin."),
					*Mesh->GetName(), *GSTNEquipment::Socket_ConnectorBackpack.ToString()));
			}
		}
	}
}

namespace
{
	/** Drawn in game: visible, not hidden in game, with a mesh asset */
	bool IsShownInGame(const USkeletalMeshComponent* Mesh)
	{
		return Mesh && Mesh->GetSkeletalMeshAsset() && Mesh->IsVisible() && !Mesh->bHiddenInGame;
	}

	USkeletalMeshComponent* FindShownMesh(const AActor& Actor)
	{
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(&Actor);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (IsShownInGame(Mesh))
			{
				return Mesh;
			}
		}
		return nullptr;
	}
}

USkeletalMeshComponent* FGSTN_EquipmentVisualUtils::FindShownBodyMesh(const AActor& Pawn, USkeletalMeshComponent* Preferred)
{
	if (IsShownInGame(Preferred))
	{
		return Preferred;
	}
	if (USkeletalMeshComponent* Own = FindShownMesh(Pawn))
	{
		return Own;
	}
	TInlineComponentArray<UChildActorComponent*> Children(&Pawn);
	for (const UChildActorComponent* Child : Children)
	{
		const AActor* ChildActor = Child ? Child->GetChildActor() : nullptr;
		if (USkeletalMeshComponent* Shown = ChildActor ? FindShownMesh(*ChildActor) : nullptr)
		{
			return Shown;
		}
	}
	return Preferred;
}

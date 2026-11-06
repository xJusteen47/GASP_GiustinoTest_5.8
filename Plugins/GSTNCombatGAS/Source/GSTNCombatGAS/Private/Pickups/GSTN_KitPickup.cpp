// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Pickups/GSTN_KitPickup.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Net/UnrealNetwork.h"
#include "Pickups/GSTN_KitPlaceholderLook.h"

AGSTN_KitPickup::AGSTN_KitPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// A kit on the ground is part of the map's furniture: every player must see it wherever they stand
	bAlwaysRelevant = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(RootComponent);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(30.f);
}

void AGSTN_KitPickup::SetLoadout(const FGSTN_LoadoutData& NewLoadout)
{
	if (!HasAuthority())
	{
		return;
	}
	Loadout = NewLoadout;
	// The authority never gets the OnRep of what it sets (a listen host shows the pickup too)
	RefreshLook();
	OnKitChanged();
}

void AGSTN_KitPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshLook();
}

void AGSTN_KitPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGSTN_KitPickup, Loadout);
}

void AGSTN_KitPickup::OnRep_Loadout()
{
	RefreshLook();
	OnKitChanged();
}

void AGSTN_KitPickup::RefreshLook()
{
	const bool bHasKit = !IsEmpty();
	if (Mesh)
	{
		// No mesh of its own: the kit's stand-in box, the same colour the kit has on a player's back
		// The tag marks the box as ours, so a later kit recolours it (a designer's own mesh is never touched)
		static const FName PlaceholderTag(TEXT("GSTN_KitPlaceholder"));
		if (Mesh->ComponentHasTag(PlaceholderTag) || !Mesh->GetStaticMesh())
		{
			if (!Mesh->ComponentHasTag(PlaceholderTag))
			{
				Mesh->ComponentTags.Add(PlaceholderTag);
				Mesh->SetRelativeLocation(FVector(0.f, 0.f, 25.f));
				Mesh->SetRelativeScale3D(FVector(0.5f));
			}
			UGSTN_KitPlaceholderLookSubsystem::ApplyPlaceholder(Mesh, bHasKit ? Loadout.PrimaryKit : FPrimaryAssetId());
		}
		Mesh->SetVisibility(bHasKit);
	}
	if (Label)
	{
		Label->SetVisibility(bHasKit);
		Label->SetText(bHasKit ? FText::FromName(Loadout.PrimaryKit.PrimaryAssetName) : FText::GetEmpty());
	}
}

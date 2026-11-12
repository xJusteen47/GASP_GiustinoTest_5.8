// Equipment pickups (M11 G2).

#include "Pickups/GSTN_EquipmentPickup.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "Net/UnrealNetwork.h"
#include "Pickups/GSTN_KitPlaceholderLook.h"
#include "Pickups/GSTN_PickupPayload.h"

AGSTN_EquipmentPickup::AGSTN_EquipmentPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// Equipment on the ground is part of the map's furniture: every player must see it wherever they stand
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

const FGSTN_PickupPayload* AGSTN_EquipmentPickup::GetPayloadBase() const
{
	return Payload.GetPtr<FGSTN_PickupPayload>();
}

bool AGSTN_EquipmentPickup::IsEmpty() const
{
	const FGSTN_PickupPayload* Base = GetPayloadBase();
	return !Base || Base->IsEmpty();
}

FPrimaryAssetId AGSTN_EquipmentPickup::GetDisplayId() const
{
	const FGSTN_PickupPayload* Base = GetPayloadBase();
	return Base && !Base->IsEmpty() ? Base->GetDisplayId() : FPrimaryAssetId();
}

void AGSTN_EquipmentPickup::SetPayload(const FInstancedStruct& NewPayload)
{
	if (!HasAuthority())
	{
		return;
	}
	Payload = NewPayload;
	// The authority never gets the OnRep of what it sets (a listen host shows the pickup too)
	NotifyPayloadChanged();
}

bool AGSTN_EquipmentPickup::Take(UGSTN_LoadoutComponent& Component, TArray<FString>& OutErrors)
{
	OutErrors.Reset();
	const FGSTN_PickupPayload* Base = GetPayloadBase();
	if (!Base || Base->IsEmpty())
	{
		OutErrors.Add(TEXT("Pickup: nothing lies here"));
		return false;
	}
	// Copies: AcceptLoadout replaces the applied loadout a reference would point at, SetPayload the payload Base points into
	const FGSTN_LoadoutData Previous = Component.GetApplied();
	FGSTN_LoadoutData NewLoadout;
	FInstancedStruct LeftBehind;
	if (!Base->MakeChange(Previous, NewLoadout, LeftBehind, OutErrors))
	{
		return false;
	}
	if (!Component.AcceptLoadout(NewLoadout, OutErrors))
	{
		return false;
	}
	SetPayload(LeftBehind);
	return true;
}

void AGSTN_EquipmentPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshLook();
}

void AGSTN_EquipmentPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGSTN_EquipmentPickup, Payload);
}

void AGSTN_EquipmentPickup::OnRep_Payload()
{
	NotifyPayloadChanged();
}

void AGSTN_EquipmentPickup::NotifyPayloadChanged()
{
	RefreshLook();
	OnPayloadChanged();
	K2_OnPayloadChanged();
}

void AGSTN_EquipmentPickup::RefreshLook()
{
	const FPrimaryAssetId Shown = GetDisplayId();
	const bool bHasSomething = Shown.IsValid();
	if (Mesh)
	{
		// No mesh of its own: the stand-in box, the same colour the kit has on a player's back (a weapon gets its own colour the same way).
		// The tag marks the box as ours, so a later payload recolours it (a designer's own mesh is never touched)
		static const FName PlaceholderTag(TEXT("GSTN_KitPlaceholder"));
		if (Mesh->ComponentHasTag(PlaceholderTag) || !Mesh->GetStaticMesh())
		{
			if (!Mesh->ComponentHasTag(PlaceholderTag))
			{
				Mesh->ComponentTags.Add(PlaceholderTag);
				Mesh->SetRelativeLocation(FVector(0.f, 0.f, 25.f));
				Mesh->SetRelativeScale3D(FVector(0.5f));
			}
			UGSTN_KitPlaceholderLookSubsystem::ApplyPlaceholder(Mesh, Shown);
		}
		Mesh->SetVisibility(bHasSomething);
	}
	if (Label)
	{
		Label->SetVisibility(bHasSomething);
		Label->SetText(bHasSomething ? FText::FromName(Shown.PrimaryAssetName) : FText::GetEmpty());
	}
}

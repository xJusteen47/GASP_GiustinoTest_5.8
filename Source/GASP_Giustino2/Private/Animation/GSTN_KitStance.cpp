// Kit -> upper-body stance glue (presentation only).

#include "Animation/GSTN_KitStance.h"

#include "GSTN_CharacterAnimInstance.h"
#include "GSTN_StanceTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Loadout/GSTN_LoadoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_KitStance)

UGSTN_KitStanceComponent::UGSTN_KitStanceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UGSTN_StanceDefinition* UGSTN_KitStanceComponent::ResolveKitStance(const FGSTN_KitExtensionList& Extensions)
{
	const FGSTN_StanceKitExtension* Extension = Extensions.Find<FGSTN_StanceKitExtension>();
	return Extension ? Extension->Stance.LoadSynchronous() : nullptr;
}

void UGSTN_KitStanceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UGSTN_LoadoutComponent* Loadout = GetOwner() ? GetOwner()->FindComponentByClass<UGSTN_LoadoutComponent>() : nullptr)
	{
		Loadout->OnLoadoutApplied.AddDynamic(this, &UGSTN_KitStanceComponent::HandleLoadoutApplied);
		if (Loadout->HasLoadout())
		{
			// Applied before this component began play (a client whose loadout replicated first)
			HandleLoadoutApplied(Loadout, 0);
			return;
		}
	}
	Apply(DefaultStance.LoadSynchronous());
}

void UGSTN_KitStanceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGSTN_LoadoutComponent* Loadout = GetOwner() ? GetOwner()->FindComponentByClass<UGSTN_LoadoutComponent>() : nullptr)
	{
		Loadout->OnLoadoutApplied.RemoveDynamic(this, &UGSTN_KitStanceComponent::HandleLoadoutApplied);
	}
	Super::EndPlay(EndPlayReason);
}

void UGSTN_KitStanceComponent::HandleLoadoutApplied(UGSTN_LoadoutComponent* Component, int32 Revision)
{
	UGSTN_StanceDefinition* Stance = nullptr;
	if (Component && Component->HasLoadout())
	{
		if (const UGSTN_PrimaryKitDefinition* Kit = Component->GetResolvedLoadout().Kit)
		{
			Stance = ResolveKitStance(Kit->Extensions);
		}
	}
	Apply(Stance ? Stance : DefaultStance.LoadSynchronous());
}

void UGSTN_KitStanceComponent::Apply(UGSTN_StanceDefinition* Stance)
{
	TArray<USkeletalMeshComponent*> Meshes;
	if (AActor* Owner = GetOwner())
	{
		Owner->GetComponents(Meshes);
	}
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (UGSTN_CharacterAnimInstance* Instance = Cast<UGSTN_CharacterAnimInstance>(Mesh->GetAnimInstance()))
		{
			Instance->SetStance(Stance ? Stance : Instance->DefaultStance.Get());
		}
	}
}

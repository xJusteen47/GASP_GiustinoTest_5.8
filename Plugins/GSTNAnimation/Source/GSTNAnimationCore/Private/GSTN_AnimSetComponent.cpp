// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GSTN_AnimSetComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "GSTN_AnimSetUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimSetComponent)

const FName UGSTN_AnimSetComponent::CharacterSourceName(TEXT("Character"));

namespace GSTNAnimSetComponentPrivate
{
	static FGSTN_AnimSetSource MakeSource(FName SourceName, int32 SourcePriority, UGSTN_AnimSetCollection* Collection)
	{
		FGSTN_AnimSetSource Source;
		Source.SourceName = SourceName;
		Source.SourcePriority = SourcePriority;
		Source.Collection = Collection;
		return Source;
	}
}

UGSTN_AnimSetComponent::UGSTN_AnimSetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UGSTN_AnimSetComponent::BeginPlay()
{
	Super::BeginPlay();

	CosmeticTags.AppendTags(DefaultCosmeticTags);
	if (DefaultCollection && !Sources.ContainsByPredicate([](const FGSTN_AnimSetSource& Source) { return Source.SourceName == CharacterSourceName; }))
	{
		Sources.Add(GSTNAnimSetComponentPrivate::MakeSource(CharacterSourceName, GSTNAnimSet::PriorityCharacter, DefaultCollection));
	}

	if (USkeletalMeshComponent* Mesh = GetOrFindMesh())
	{
		Mesh->OnAnimInitialized.AddUniqueDynamic(this, &UGSTN_AnimSetComponent::HandleAnimInitialized);
	}
	RequestRefresh();
}

void UGSTN_AnimSetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	++RequestSerial;
	if (LoadHandle.IsValid())
	{
		LoadHandle->CancelHandle();
		LoadHandle.Reset();
	}
	if (USkeletalMeshComponent* Mesh = TargetMesh.Get())
	{
		Mesh->OnAnimInitialized.RemoveDynamic(this, &UGSTN_AnimSetComponent::HandleAnimInitialized);
	}
	Super::EndPlay(EndPlayReason);
}

void UGSTN_AnimSetComponent::SetSource(FName SourceName, UGSTN_AnimSetCollection* Collection, int32 SourcePriority)
{
	const int32 Existing = Sources.IndexOfByPredicate([SourceName](const FGSTN_AnimSetSource& Source) { return Source.SourceName == SourceName; });
	if (!Collection)
	{
		if (Existing == INDEX_NONE)
		{
			return;
		}
		Sources.RemoveAt(Existing);
	}
	else if (Existing != INDEX_NONE)
	{
		if (Sources[Existing].Collection == Collection && Sources[Existing].SourcePriority == SourcePriority)
		{
			return;
		}
		Sources[Existing].Collection = Collection;
		Sources[Existing].SourcePriority = SourcePriority;
	}
	else
	{
		Sources.Add(GSTNAnimSetComponentPrivate::MakeSource(SourceName, SourcePriority, Collection));
	}

	if (HasBegunPlay())
	{
		RequestRefresh();
	}
}

void UGSTN_AnimSetComponent::SetCosmeticTags(const FGameplayTagContainer& Tags)
{
	if (CosmeticTags == Tags)
	{
		return;
	}
	CosmeticTags = Tags;
	if (HasBegunPlay())
	{
		RequestRefresh();
	}
}

void UGSTN_AnimSetComponent::SetTargetMesh(USkeletalMeshComponent* Mesh)
{
	if (TargetMesh.Get() == Mesh)
	{
		return;
	}
	if (USkeletalMeshComponent* Old = TargetMesh.Get())
	{
		Old->OnAnimInitialized.RemoveDynamic(this, &UGSTN_AnimSetComponent::HandleAnimInitialized);
		for (UClass* LayerClass : LinkedLayerClasses)
		{
			Old->UnlinkAnimClassLayers(LayerClass);
		}
	}
	LinkedLayerClasses.Reset();
	TargetMesh = Mesh;
	if (Mesh && HasBegunPlay())
	{
		Mesh->OnAnimInitialized.AddUniqueDynamic(this, &UGSTN_AnimSetComponent::HandleAnimInitialized);
		RequestRefresh();
	}
}

UGSTN_AnimSetDefinition* UGSTN_AnimSetComponent::FindSet(FGameplayTag SetTag) const
{
	const TObjectPtr<UGSTN_AnimSetDefinition>* Found = Winners.Find(SetTag);
	return Found ? Found->Get() : nullptr;
}

UAnimMontage* UGSTN_AnimSetComponent::ResolveVisualMontage(UAnimMontage* Reference) const
{
	const TObjectPtr<UAnimMontage>* Found = Reference ? VisualMontages.Find(Reference) : nullptr;
	return Found && *Found ? Found->Get() : Reference;
}

void UGSTN_AnimSetComponent::RequestRefresh()
{
	const int32 Serial = ++RequestSerial;
	if (LoadHandle.IsValid())
	{
		LoadHandle->CancelHandle();
		LoadHandle.Reset();
	}

	// Stage 1: every set any source names (the sets are small data assets; their layers and montages load in stage 2, winners only)
	TArray<FSoftObjectPath> SetPaths;
	for (const FGSTN_AnimSetSource& Source : Sources)
	{
		if (Source.Collection)
		{
			for (const FGSTN_AnimSetRule& Rule : Source.Collection->Rules)
			{
				if (!Rule.Set.IsNull())
				{
					SetPaths.AddUnique(Rule.Set.ToSoftObjectPath());
				}
			}
		}
	}

	if (SetPaths.IsEmpty())
	{
		OnSetsLoaded(Serial);
		return;
	}
	LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(SetPaths, FStreamableDelegate::CreateWeakLambda(this, [this, Serial]()
	{
		OnSetsLoaded(Serial);
	}));
}

void UGSTN_AnimSetComponent::OnSetsLoaded(int32 Serial)
{
	if (Serial != RequestSerial)
	{
		return;
	}

	TMap<FGameplayTag, UGSTN_AnimSetDefinition*> NewWinners = UGSTN_AnimSetUtils::ResolveWinners(Sources, CosmeticTags);
	TMap<FGameplayTag, TSoftClassPtr<UAnimInstance>> LayerClasses = UGSTN_AnimSetUtils::ResolveLayerClasses(Sources, CosmeticTags);

	// Stage 2: the layer classes and the winners' montages
	TArray<FSoftObjectPath> ContentPaths;
	for (const TPair<FGameplayTag, TSoftClassPtr<UAnimInstance>>& Layer : LayerClasses)
	{
		ContentPaths.AddUnique(Layer.Value.ToSoftObjectPath());
	}
	for (const TPair<FGameplayTag, UGSTN_AnimSetDefinition*>& Winner : NewWinners)
	{
		for (const FGSTN_MontageOverride& Override : Winner.Value->MontageOverrides)
		{
			if (!Override.Reference.IsNull() && !Override.Visual.IsNull())
			{
				ContentPaths.AddUnique(Override.Reference.ToSoftObjectPath());
				ContentPaths.AddUnique(Override.Visual.ToSoftObjectPath());
			}
		}
	}

	if (ContentPaths.IsEmpty())
	{
		OnContentLoaded(Serial, MoveTemp(LayerClasses), MoveTemp(NewWinners));
		return;
	}
	LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(ContentPaths, FStreamableDelegate::CreateWeakLambda(this,
		[this, Serial, LayerClasses, NewWinners]()
		{
			OnContentLoaded(Serial, LayerClasses, NewWinners);
		}));
}

void UGSTN_AnimSetComponent::OnContentLoaded(int32 Serial, TMap<FGameplayTag, TSoftClassPtr<UAnimInstance>> LayerClasses, TMap<FGameplayTag, UGSTN_AnimSetDefinition*> NewWinners)
{
	if (Serial != RequestSerial)
	{
		return;
	}
	LoadHandle.Reset();

	Winners.Reset();
	VisualMontages.Reset();
	for (const TPair<FGameplayTag, UGSTN_AnimSetDefinition*>& Winner : NewWinners)
	{
		Winners.Add(Winner.Key, Winner.Value);
		for (const FGSTN_MontageOverride& Override : Winner.Value->MontageOverrides)
		{
			UAnimMontage* Reference = Override.Reference.Get();
			UAnimMontage* Visual = Override.Visual.Get();
			if (Reference && Visual)
			{
				VisualMontages.Add(Reference, Visual);
			}
		}
	}

	TArray<UClass*> NewClasses;
	for (const TPair<FGameplayTag, TSoftClassPtr<UAnimInstance>>& Layer : LayerClasses)
	{
		if (UClass* LayerClass = Layer.Value.Get())
		{
			NewClasses.AddUnique(LayerClass);
		}
	}
	LinkLayers(NewClasses);

	OnAnimSetsChanged.Broadcast();
}

void UGSTN_AnimSetComponent::LinkLayers(const TArray<UClass*>& NewClasses)
{
	USkeletalMeshComponent* Mesh = GetOrFindMesh();
	if (!Mesh)
	{
		LinkedLayerClasses.Reset();
		return;
	}

	for (UClass* OldClass : LinkedLayerClasses)
	{
		if (OldClass && !NewClasses.Contains(OldClass))
		{
			Mesh->UnlinkAnimClassLayers(OldClass);
		}
	}

	// Linking a class replaces whatever implemented the same layer interfaces before, so a variant swaps its base cleanly
	LinkedLayerClasses.Reset();
	for (UClass* LayerClass : NewClasses)
	{
		Mesh->LinkAnimClassLayers(TSubclassOf<UAnimInstance>(LayerClass));
		LinkedLayerClasses.Add(LayerClass);
	}
}

USkeletalMeshComponent* UGSTN_AnimSetComponent::GetOrFindMesh()
{
	if (USkeletalMeshComponent* Mesh = TargetMesh.Get())
	{
		return Mesh;
	}

	if (const AActor* Owner = GetOwner())
	{
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(Owner);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetAnimClass())
			{
				TargetMesh = Mesh;
				return Mesh;
			}
		}
	}
	return nullptr;
}

void UGSTN_AnimSetComponent::HandleAnimInitialized()
{
	// A new anim instance (e.g. a skin's AnimClass) dropped the links: link the applied classes again
	TArray<UClass*> Classes;
	for (UClass* LayerClass : LinkedLayerClasses)
	{
		Classes.Add(LayerClass);
	}
	LinkedLayerClasses.Reset();
	LinkLayers(Classes);
}

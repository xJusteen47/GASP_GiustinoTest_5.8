// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GSTN_AnimSetTypes.h"
#include "GSTN_AnimSetComponent.generated.h"

class UAnimMontage;
class USkeletalMeshComponent;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSTN_OnAnimSetsChanged);

/**
 * Picks the anim set of every feature slot for one character and applies it to its mesh (anim-layers.md section 4). Presentation only:
 * nothing is replicated or predicted; every machine resolves the same sets from the same replicated loadout and cosmetics, and no
 * simulation code may call this component (a timeline names its reference montage; only the code that plays it on the mesh asks
 * ResolveVisualMontage).
 *
 * Sources are collections with a priority (GSTNAnimSet::PriorityCharacter < Kit < Weapon < Skin): the character's DefaultCollection, and
 * whatever the loadout code registers with SetSource when it applies a kit, a weapon or a skin. Cosmetic tags (skin tags plus Anim.Trait.*
 * leaves such as Anim.Trait.Body.Female or Anim.Trait.Build.Heavy) decide which rules match.
 *
 * Any change is resolved off the critical path: the sets are loaded asynchronously, the winners chosen (UGSTN_AnimSetUtils), then their layer
 * classes and montages loaded, then the layers linked on the mesh (LinkAnimClassLayers) and OnAnimSetsChanged broadcast. A newer change while
 * loading supersedes the older one. When the mesh's anim class is replaced (a skin with its own AnimClass), the layers are linked again.
 */
UCLASS(ClassGroup = "Animation", meta = (BlueprintSpawnableComponent))
class GSTNANIMATIONCORE_API UGSTN_AnimSetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_AnimSetComponent();

	/** The character's own anim sets, registered as source "Character" at GSTNAnimSet::PriorityCharacter */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Anim Sets")
	TObjectPtr<UGSTN_AnimSetCollection> DefaultCollection;

	/** Cosmetic tags before any skin sets them (e.g. the body of the default character) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Anim Sets")
	FGameplayTagContainer DefaultCosmeticTags;

	/** Fires after a change has been applied to the mesh (layers linked, montage map rebuilt) */
	UPROPERTY(BlueprintAssignable, Category = "Animation|Anim Sets")
	FGSTN_OnAnimSetsChanged OnAnimSetsChanged;

	/** Adds, replaces or (Collection null) removes the source SourceName */
	UFUNCTION(BlueprintCallable, Category = "Animation|Anim Sets")
	void SetSource(FName SourceName, UGSTN_AnimSetCollection* Collection, int32 SourcePriority);

	/** Replaces the cosmetic tags the rules match against */
	UFUNCTION(BlueprintCallable, Category = "Animation|Anim Sets")
	void SetCosmeticTags(const FGameplayTagContainer& Tags);

	/** The mesh the layers are linked on. Default: the owner's first skeletal mesh component with an anim class */
	UFUNCTION(BlueprintCallable, Category = "Animation|Anim Sets")
	void SetTargetMesh(USkeletalMeshComponent* Mesh);

	/** The applied winner of a slot, null when none */
	UFUNCTION(BlueprintPure, Category = "Animation|Anim Sets")
	UGSTN_AnimSetDefinition* FindSet(FGameplayTag SetTag) const;

	/** What the mesh should play for Reference: the applied winners' override, else Reference itself. Presentation only */
	UFUNCTION(BlueprintPure, Category = "Animation|Anim Sets")
	UAnimMontage* ResolveVisualMontage(UAnimMontage* Reference) const;

	const FGameplayTagContainer& GetCosmeticTags() const { return CosmeticTags; }
	TConstArrayView<FGSTN_AnimSetSource> GetSources() const { return Sources; }

	/** Source name of DefaultCollection */
	static const FName CharacterSourceName;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TArray<FGSTN_AnimSetSource> Sources;

	UPROPERTY(Transient)
	FGameplayTagContainer CosmeticTags;

	UPROPERTY(Transient)
	TWeakObjectPtr<USkeletalMeshComponent> TargetMesh;

	/** Applied winners per slot */
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UGSTN_AnimSetDefinition>> Winners;

	/** Layer classes currently linked by this component */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UClass>> LinkedLayerClasses;

	/** Reference -> visual montage of the applied winners */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UAnimMontage>, TObjectPtr<UAnimMontage>> VisualMontages;

	TSharedPtr<FStreamableHandle> LoadHandle;

	/** Increments on every change, so a load that finishes after a newer change is dropped */
	int32 RequestSerial = 0;

	void RequestRefresh();
	void OnSetsLoaded(int32 Serial);
	void OnContentLoaded(int32 Serial, TMap<FGameplayTag, TSoftClassPtr<UAnimInstance>> LayerClasses, TMap<FGameplayTag, UGSTN_AnimSetDefinition*> NewWinners);
	void LinkLayers(const TArray<UClass*>& NewClasses);
	USkeletalMeshComponent* GetOrFindMesh();

	UFUNCTION()
	void HandleAnimInitialized();
};

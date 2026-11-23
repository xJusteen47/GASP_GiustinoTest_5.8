// Kit -> upper-body stance glue (presentation only): the project module is the one place that sees both the combat kits and the anim plugin.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rules/GSTN_KitExtension.h"
#include "GSTN_KitStance.generated.h"

class UGSTN_LoadoutComponent;
class UGSTN_StanceDefinition;

/**
 * The upper-body stance of a kit (or weapon), as an entry of its extension list, so the kit asset never names the anim plugin. The stance
 * asset carries the per-kit settings: the chosen idle, the return timing (with the feet or slightly before), the FFXIV-style delay, fades
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Stance (upper body)"))
struct GASP_GIUSTINO2_API FGSTN_StanceKitExtension : public FGSTN_KitExtension
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stance")
	TSoftObjectPtr<UGSTN_StanceDefinition> Stance;
};

/**
 * Gives the character's main anim instance (UGSTN_CharacterAnimInstance) the stance of its applied kit, or DefaultStance when the kit names
 * none. Every machine resolves the same stance from the same replicated loadout; nothing is replicated or predicted, no simulation reads it.
 * Add it to the character Blueprint next to the loadout component.
 */
UCLASS(ClassGroup = "Animation", meta = (BlueprintSpawnableComponent))
class GASP_GIUSTINO2_API UGSTN_KitStanceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_KitStanceComponent();

	/** Used when the kit has no stance extension (null = the anim instance's own DefaultStance) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Stance")
	TSoftObjectPtr<UGSTN_StanceDefinition> DefaultStance;

	/** The stance a kit's extension list names (the first FGSTN_StanceKitExtension), loaded now; null when it names none */
	static UGSTN_StanceDefinition* ResolveKitStance(const FGSTN_KitExtensionList& Extensions);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleLoadoutApplied(UGSTN_LoadoutComponent* Component, int32 Revision);

	void Apply(UGSTN_StanceDefinition* Stance);
};

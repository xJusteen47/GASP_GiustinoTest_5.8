// Bridge between parts and minions (plan P16).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_PartsMinionExtension.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "GSTN_PartsMinionSettings.generated.h"

/**
 * Which minion types lose parts (combat-parts.md §5.5). The extension is attached here, by the type tag of UGSTN_MinionDefinition, and
 * not as a field of the definition: GSTNMinion must not depend on GSTNParts, and the bridge plugin owns the whole edge. The bridge
 * subsystem reads this map once per world; a type absent from it costs nothing (one lookup the first time the type activates, then a
 * per-slot flag). Code and tests can add an exact definition at runtime with UGSTN_PartsMinionSubsystem::RegisterDefinition, which wins
 * over the tag.
 */
UCLASS(config = GSTNPartsMinion, defaultconfig, meta = (DisplayName = "GSTN Parts Minion"))
class GSTNPARTSMINION_API UGSTN_PartsMinionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	static const UGSTN_PartsMinionSettings& Get() { return *GetDefault<UGSTN_PartsMinionSettings>(); }

	/** Minion type tag (a leaf under Minion.Type) to its part set and lethal parts. The set must have bForMinions (at most 8 parts) */
	UPROPERTY(Config, EditAnywhere, Category = "Parts|Minion", meta = (Categories = "Minion.Type"))
	TMap<FGameplayTag, FGSTN_PartsMinionExtension> TypeExtensions;
};

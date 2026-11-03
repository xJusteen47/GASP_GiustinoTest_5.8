// Piggyback, editor part (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "GSTN_PairMoveAbilityValidator.generated.h"

class UGSTN_PairMoveDefinition;

/**
 * A Blueprint of a pair move's initiator ability (a UGSTN_GA_PairMove child, combat-m7-piggyback.md 4 and 9). The definition asset validates
 * itself against its abilities' timelines, but only when the definition is saved: editing the ability's timeline afterwards (moving the
 * PartnerMove key, shortening the move) would break the pair unseen. This validator runs the definition's checks again when the ability is
 * saved, and checks that the two name each other:
 *  - no PairMove: a warning (nothing can check the pair; the ability may be a base other Blueprints derive from);
 *  - the definition's InitiatorAbility is not this class nor a parent of it: an error (the registry plays another ability than this one);
 *  - every finding of UGSTN_PairMoveDefinition::Validate: an error, prefixed with the definition's name.
 */
UCLASS()
class UGSTN_PairMoveAbilityValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	/** The checks above on AbilityClass and the definition it names (null = none), the asset aside: what the validator and the tests run */
	static void CheckPairMoveAbility(const UClass* AbilityClass, const UGSTN_PairMoveDefinition* Definition, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) override;
};

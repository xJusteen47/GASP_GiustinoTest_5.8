// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_AnimSetTypes.h"
#include "GSTN_AnimSetUtils.generated.h"

/**
 * Stateless resolution of anim sets (anim-layers.md section 4) and the timing check of montage overrides. Everything here works on
 * loaded objects: a rule whose set is not loaded is skipped, so the caller (UGSTN_AnimSetComponent) loads first and resolves after.
 */
UCLASS()
class GSTNANIMATIONCORE_API UGSTN_AnimSetUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * The winning set of every slot. A rule competes when its set is loaded, has a valid SetTag and When matches CosmeticTags (an empty
	 * query always matches). Order: higher SourcePriority, then higher rule Priority; on a full tie the earlier source in Sources, then the
	 * earlier rule, wins. Deterministic, so every machine picks the same sets from the same replicated cosmetics
	 */
	static TMap<FGameplayTag, UGSTN_AnimSetDefinition*> ResolveWinners(TConstArrayView<FGSTN_AnimSetSource> Sources, const FGameplayTagContainer& CosmeticTags);

	/**
	 * The layer class of each slot: the winner's, or when the winner has none, the best set of that slot with one (sets that only override
	 * montages do not unlink a lower source's layer). Slots where no competing set has a layer class are absent
	 */
	static TMap<FGameplayTag, TSoftClassPtr<UAnimInstance>> ResolveLayerClasses(TConstArrayView<FGSTN_AnimSetSource> Sources, const FGameplayTagContainer& CosmeticTags);

	/**
	 * Whether Visual may stand in for Reference: same section names in the same order, each section length within MaxSectionLengthDelta,
	 * same rate scale, and no root motion when the reference has none. Appends one line per problem to OutProblems
	 */
	static bool ValidateMontageOverride(const UAnimMontage& Reference, const UAnimMontage& Visual, TArray<FText>& OutProblems);
};

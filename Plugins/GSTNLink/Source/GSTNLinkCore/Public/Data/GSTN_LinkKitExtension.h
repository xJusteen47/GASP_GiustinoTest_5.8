// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_GrabRules.h"
#include "GameplayTagContainer.h"
#include "Rules/GSTN_KitExtension.h"
#include "GSTN_LinkKitExtension.generated.h"

class UAnimInstance;

/**
 * What a kit plays while one of its entities holds a role of a link: the animation layer of the role (m6 spec 4). The anim layer is
 * chosen by the kit, not by the link, so two kits grab with different poses and the link type knows nothing about animation.
 * Soft, so a dedicated server never loads it.
 */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_KitLinkAnims
{
	GENERATED_BODY()

	/** The animation layer linked into the kit's main animation blueprint while the role is held */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Animation")
	TSoftClassPtr<UAnimInstance> AnimLayer;
};

/**
 * Gives a fighter its link behaviour, by data (m6 spec 1b). It is an entry of a kit's or weapon's extension array, so the kit asset
 * never grows a property that names links; a combat build without this plugin keeps the entry and ignores it with a warning.
 *
 * A kit WITHOUT this entry grabs and latches with the defaults of the rules the link type names; a kit with an entry uses the rules
 * below instead. The animation layers are per role tag, so one kit can carry the roles of several link types.
 */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_LinkKitExtension : public FGSTN_KitExtension
{
	GENERATED_BODY()

	/** The kit's own grab rules (weight limit, mash, immunity frames, input timings) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link")
	FGSTN_GrabRules GrabRules;

	/** The kit's own latch rules (the weight a victim must reach) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link")
	FGSTN_LatchRules LatchRules;

	/** The animation layer of each role the kit can hold, by role tag. A role with no entry plays nothing extra */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Animation", meta = (ForceInlineRow))
	TMap<FGameplayTag, FGSTN_KitLinkAnims> AnimsByRole;
};

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
 * A kit's four grab throws (M11 K2, Smash style: in a grab the stick picks the throw). Each entry is a timeline ability of the kit (a child of
 * the combat base ability with a link outcome key Throw). An empty entry falls back to the project's common set (UGSTN_LinkSettings::
 * FallbackThrows); a set entry replaces what the kit's binding table binds to that throw. Soft paths: the pure part names no GAS class.
 */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_LinkThrowSet
{
	GENERATED_BODY()

	/** Command.Link.Throw.Forward */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Throws", meta = (MetaClass = "/Script/GameplayAbilities.GameplayAbility"))
	FSoftClassPath Forward;

	/** Command.Link.Throw.Back */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Throws", meta = (MetaClass = "/Script/GameplayAbilities.GameplayAbility"))
	FSoftClassPath Back;

	/** Command.Link.Throw.Up */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Throws", meta = (MetaClass = "/Script/GameplayAbilities.GameplayAbility"))
	FSoftClassPath Up;

	/** Command.Link.Throw.Down */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Throws", meta = (MetaClass = "/Script/GameplayAbilities.GameplayAbility"))
	FSoftClassPath Down;

	/** The entry of a throw command (Command.Link.Throw.*), null for any other tag */
	const FSoftClassPath* Find(const FGameplayTag& ThrowCommand) const;
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

	/** The kit's own four throws (M11 K2); an empty entry takes the project's fallback set. A kit with this entry grabs only if it binds the grab */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link|Throws")
	FGSTN_LinkThrowSet Throws;
};

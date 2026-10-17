// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "GameplayTagContainer.h"

/**
 * Button tag -> bit lookup: the part of an input config the pattern matcher needs. Conditions name buttons by tag so slot
 * sets work with any config; the matcher binds the two at evaluation through this map (at most 32 integer compares per
 * button reference, no allocation).
 *
 * Immutable once built, so it can be shared by every fighter that uses the same config. An unknown tag has mask 0, which
 * no condition can satisfy: a slot that names a button the config does not have simply never fires (the slot set
 * validation reports it).
 */
struct GSTNCOMBATCORE_API FGSTN_ButtonMap
{
	FGSTN_ButtonMap() = default;

	/** Bit i is the button with tag Tags[i]; at most 32 */
	explicit FGSTN_ButtonMap(TConstArrayView<FGameplayTag> InTags);

	/** The six standard buttons (GSTNInputTags::Button_*) in bit order Attack, Strong, Special, Crouch, Shield, Jump */
	static FGSTN_ButtonMap MakeStandard();

	/** Bit of a button, INDEX_NONE if the map does not have it */
	int32 GetIndex(const FGameplayTag& Button) const;

	/** Mask of a button, 0 if unknown */
	uint32 GetMask(const FGameplayTag& Button) const;

	/** Mask of every button of the container the map knows */
	uint32 GetMask(const FGameplayTagContainer& Buttons) const;

	/** Tag of a bit, invalid if the map has fewer buttons */
	FGameplayTag GetTag(int32 Index) const { return Index >= 0 && Index < Num ? Tags[Index] : FGameplayTag(); }

	int32 GetNum() const { return Num; }

private:
	TStaticArray<FGameplayTag, 32> Tags;
	int32 Num = 0;
};

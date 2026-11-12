// Bridge between backpacks and pickups (combat-loadout-mods.md §10, §12).

#pragma once

#include "CoreMinimal.h"
#include "Pickups/GSTN_PickupPayload.h"
#include "GSTN_BackpackPickupPayload.generated.h"

/**
 * A backpack on the ground. One at a time (spec §10): the taker's backpack is replaced and lies here instead; with none, the pickup
 * empties. Kit, weapons and the rest of the loadout are untouched. Like every payload it only builds the new loadout: the loadout
 * component applies it on the server as a new revision, never predicted.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Backpack"))
struct GSTNBACKPACKPICKUPS_API FGSTN_BackpackPickupPayload : public FGSTN_PickupPayload
{
	GENERATED_BODY()

	FGSTN_BackpackPickupPayload() = default;
	explicit FGSTN_BackpackPickupPayload(const FSoftObjectPath& InBackpack) : Backpack(InBackpack) {}

	/** The backpack asset (UGSTN_BackpackDefinition) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (AllowedClasses = "/Script/GSTNBackpackGAS.GSTN_BackpackDefinition"))
	FSoftObjectPath Backpack;

	virtual bool IsEmpty() const override { return !Backpack.IsValid(); }

	/** Type GSTNBackpack (UGSTN_BackpackDefinition::AssetType) and the asset's name: the placeholder colour and label come from it */
	virtual FPrimaryAssetId GetDisplayId() const override;

	virtual bool MakeChange(const FGSTN_LoadoutData& Current, FGSTN_LoadoutData& OutNew, FInstancedStruct& OutLeftBehind, TArray<FString>& OutErrors) const override;
};

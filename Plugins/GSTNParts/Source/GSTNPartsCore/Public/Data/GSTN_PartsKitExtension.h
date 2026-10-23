// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Rules/GSTN_KitExtension.h"
#include "GSTN_PartsKitExtension.generated.h"

class UGSTN_PartSetDefinition;

/**
 * Gives a fighter a part set, by data (combat-parts.md §2). It is an entry of a kit's or weapon's extension array, so the kit
 * asset never grows a property that names parts; a combat build without this plugin keeps the entry and ignores it with a
 * warning. Parts are attached to an entity by this data, never by code in the combat core.
 *
 * The reference is soft: a dedicated server and a client both resolve it before the pawn's first simulation tick and must
 * resolve the same set. Nothing checks that yet (the part count is not on the wire); the connect check belongs to the network
 * module of this plugin, which is not built.
 */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartsKitExtension : public FGSTN_KitExtension
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	TSoftObjectPtr<UGSTN_PartSetDefinition> PartSet;
};

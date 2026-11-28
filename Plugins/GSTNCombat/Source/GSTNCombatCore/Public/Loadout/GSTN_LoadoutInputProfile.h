// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Input/GSTN_InputProfile.h"
#include "Loadout/GSTN_LoadoutExtensionData.h"
#include "GSTN_LoadoutInputProfile.generated.h"

/**
 * The player's input profile (FGSTN_InputProfile: device, button overrides of remappable functions, picked commands for virtual commands)
 * in the loadout: the handshake of the rollback plan's F1. Sent to the host once per change (UGSTN_LoadoutComponent::ServerSetInputProfile),
 * replicated, part of the revision, so every machine compiles the player's command table from it and adopts it on the same frame. The
 * per-frame input command does not change.
 *
 * Same life as FGSTN_LoadoutInputFeelExt: it belongs to the player, stays with them on a kit swap, and the host copies it into a request
 * that lacks it. No entry = a gamepad with the layout's defaults. Its own versioned payload with bounded counts, because it comes from a
 * client and is kept in saves.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_LoadoutInputProfileExt : public FGSTN_LoadoutExtensionData
{
	GENERATED_BODY()

	static const FName WireId;

	/** Payload layout written by SerializePayload */
	static constexpr uint8 PayloadVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Profile")
	FGSTN_InputProfile Profile;

	virtual bool SerializePayload(FArchive& Ar) override;
	virtual bool StaysWithPlayerOnKitSwap() const override { return true; }

	/** The profile a loadout asks for, sanitized against the active button layout; the default profile when there is no entry */
	static FGSTN_InputProfile Resolve(const TArray<FInstancedStruct>& Extensions);
};

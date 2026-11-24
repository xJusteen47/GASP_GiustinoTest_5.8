#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GSTN_ExperienceTypes.generated.h"

GSTNEXPERIENCE_API DECLARE_LOG_CATEGORY_EXTERN(LogGSTNExperience, Log, All);

/**
 * Where an Experience is in its load. The order is fixed and is the same on the host and on every client
 * (game-modes.md 2.2 steps 1-2): Unloaded -> Loading (asset bundles) -> LoadingGameFeatures -> ExecutingActions -> Loaded.
 * "Loaded" is the spec's "Active": every Game Feature is active and every action has run, so simulation-relevant data is final.
 * Deactivating only appears when the world ends.
 */
UENUM(BlueprintType)
enum class EGSTN_ExperienceLoadState : uint8
{
	Unloaded,
	Loading,
	LoadingGameFeatures,
	ExecutingActions,
	Loaded,
	Deactivating
};

/**
 * The one rule behind "no pawn simulates before the Experience is Active" (spike G4). Pure so the Network Prediction adapters, the
 * game mode and the tests all ask the same question. A world with no Experience assigned is not gated (classic GameMode, existing
 * harness maps): the gate only closes once an Experience is assigned and stays closed until it is Loaded.
 */
struct FGSTN_ExperienceGate
{
	static bool IsOpen(bool bExperienceAssigned, EGSTN_ExperienceLoadState State)
	{
		return !bExperienceAssigned || State == EGSTN_ExperienceLoadState::Loaded;
	}
};

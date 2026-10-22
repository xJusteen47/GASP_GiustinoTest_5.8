// Native stand-ins for Blueprint classes, used only by the NP automation tests.

#pragma once

#include "CoreMinimal.h"
#include "Context/GSTN_InputContextProvider.h"
#include "GSTN_InputNPTestObjects.generated.h"

/** Answers like a Blueprint provider would: constants and a fingerprint-driven event. */
UCLASS(Hidden)
class UGSTN_TestBlueprintContextProvider : public UGSTN_BlueprintInputContextProvider
{
	GENERATED_BODY()

public:
	virtual float GetBlueprintFacingYaw_Implementation() const override { return 90.f; }
	virtual FGameplayTagContainer GetBlueprintContextTags_Implementation() const override;
	virtual uint8 GetBlueprintContextFingerprint_Implementation() const override { return 2; }
	virtual FGameplayTagContainer GetBlueprintFrameEvents_Implementation(uint8 PreviousFingerprint, uint8 CurrentFingerprint) const override;
};

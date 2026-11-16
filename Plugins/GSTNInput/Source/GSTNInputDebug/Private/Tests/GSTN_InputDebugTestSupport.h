// Test-only helpers of GSTNInputDebug.

#pragma once

#include "CoreMinimal.h"
#include "Display/GSTN_InputDisplayLayout.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_InputScript.h"
#include "Input/GSTN_InputSettings.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNInputDisplayTest
{
	/** Six buttons with no tag in bit order: the letters A to F of the notation need no tag, and this module knows no game's buttons */
	inline TArray<FGameplayTag> ButtonTags()
	{
		return { FGameplayTag(), FGameplayTag(), FGameplayTag(), FGameplayTag(), FGameplayTag(), FGameplayTag() };
	}

	/** Settings with the recommended values written out, so the result does not move with a designer's config (as the Core tests do) */
	inline TStrongObjectPtr<UGSTN_InputSettings> MakeSettings()
	{
		TStrongObjectPtr<UGSTN_InputSettings> Settings(NewObject<UGSTN_InputSettings>(GetTransientPackage()));
		Settings->DefaultBufferFrames = 8;
		Settings->bHoldBufferByDefault = true;
		Settings->MaxHoldBufferFrames = 0;
		Settings->DefaultSimultaneityFrames = 3;
		Settings->SimultaneousFrames = 2;
		Settings->BufferConflictPolicy = EGSTN_BufferConflictPolicy::MostRecent;
		Settings->bChordGraceByDefault = false;
		Settings->ChordGraceFrames = 3;
		Settings->StickDeadZone = 0.2f;
		Settings->AimZone = 0.5f;
		Settings->FlickFrames = 3;
		Settings->DiagonalHalfAngle = 22.5f;
		Settings->WorldReferenceYaw = 0.f;
		Settings->CommandTiers.Empty();
		Settings->ExclusiveButtons.Empty();
		return Settings;
	}

	/** Plays a notation into display data the way the display would get it from a pawn, without resolving a command */
	inline bool Play(FAutomationTestBase& Test, const TCHAR* Notation, const UGSTN_InputSettings& Settings, FGSTN_InputDisplayData& OutData)
	{
		FGSTN_InputScript Script(ButtonTags());
		FString Error;
		const bool bOk = Script.Parse(Notation, Error) && Script.Advance(OutData.History, Settings, Script.GetLastFrame());
		Test.TestTrue(*FString::Printf(TEXT("script plays: %s (%s)"), Notation, *Error), bOk);
		return bOk;
	}
}


#endif // WITH_DEV_AUTOMATION_TESTS

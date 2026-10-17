// Helpers shared by the Core automation tests.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Input/GSTN_CombatInputSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

/** Flags of every Core test: pure logic, runs in the editor and in headless commandlets. */
#define GSTN_CORE_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNTest
{
	/**
	 * Settings with the documented defaults written out. A new object copies the class defaults, which come from the
	 * project's config and so change when a designer tunes them: tests must not depend on that.
	 */
	inline TStrongObjectPtr<UGSTN_CombatInputSettings> MakeSettings()
	{
		TStrongObjectPtr<UGSTN_CombatInputSettings> Settings(NewObject<UGSTN_CombatInputSettings>(GetTransientPackage()));
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
		return Settings;
	}

	/** Stick vector of a given angle (0 = up, clockwise) and magnitude */
	inline FVector2D StickAt(float AngleDeg, float Magnitude = 1.f)
	{
		const float Radians = FMath::DegreesToRadians(AngleDeg);
		return FVector2D(FMath::Sin(Radians), FMath::Cos(Radians)) * Magnitude;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS

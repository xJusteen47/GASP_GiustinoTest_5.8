// Helpers shared by the combat Core automation tests. The generic input tests have their own (GSTNInputCore), with
// test-only tags; these use the real combat tags and the standard button order.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_InputHistory.h"
#include "Input/GSTN_InputScript.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_InputSlot.h"
#include "Input/GSTN_SlotResolver.h"

#if WITH_DEV_AUTOMATION_TESTS

/** Flags of every Core test: pure logic, runs in the editor and in headless commandlets. */
#define GSTN_CORE_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNCombatTest
{
	/** The six standard buttons in bit order Attack, Strong, Special, Crouch, Shield, Jump (letters A to F of the scripts) */
	inline TArray<FGameplayTag> ButtonTags()
	{
		return { GSTNCombatButtonTags::Button_Attack.GetTag(), GSTNCombatButtonTags::Button_Strong.GetTag(), GSTNCombatButtonTags::Button_Special.GetTag(),
			GSTNCombatButtonTags::Button_Crouch.GetTag(), GSTNCombatButtonTags::Button_Shield.GetTag(), GSTNCombatButtonTags::Button_Jump.GetTag() };
	}

	/**
	 * Settings with the recommended combat values written out (tiers and exclusive buttons of combat.md 8.2). A new object
	 * copies the class defaults, which come from the project's config and so change when a designer tunes them.
	 */
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
		Settings->CommandTiers.Add(GSTNCommandTags::Attack, 100);
		Settings->CommandTiers.Add(GSTNCommandTags::Strong, 200);
		Settings->CommandTiers.Add(GSTNCommandTags::Special, 300);
		Settings->CommandTiers.Add(GSTNCommandTags::Combo, 350);
		Settings->CommandTiers.Add(GSTNCommandTags::Super, 400);
		Settings->ExclusiveButtons = { GSTNCombatButtonTags::Button_Attack.GetTag(), GSTNCombatButtonTags::Button_Strong.GetTag(), GSTNCombatButtonTags::Button_Special.GetTag() };
		return Settings;
	}

	/** Plays a notation from frame 0 to its last token's frame with the standard buttons */
	inline FGSTN_InputHistory PlayTo(FAutomationTestBase& Test, const TCHAR* Notation, const UGSTN_InputSettings& Settings)
	{
		FGSTN_InputScript Script(ButtonTags());
		FString Error;
		FGSTN_InputHistory History;
		const bool bOk = Script.Parse(Notation, Error) && Script.Advance(History, Settings, Script.GetLastFrame());
		Test.TestTrue(*FString::Printf(TEXT("script plays: %s (%s)"), Notation, *Error), bOk);
		return History;
	}

	/** Resolves with the standard buttons at the history's latest frame; every command can execute */
	inline FGSTN_SlotResolution ResolveSlots(const TArray<FGSTN_InputSlotSpec>& Slots, const FGSTN_InputHistory& History, const UGSTN_InputSettings& Settings)
	{
		static const FGSTN_ButtonMap Map(ButtonTags());
		TArray<const FGSTN_InputSlotSpec*> Pointers;
		for (const FGSTN_InputSlotSpec& Spec : Slots)
		{
			Pointers.Add(&Spec);
		}
		FGSTN_ResolveParams Params;
		Params.Buttons = &Map;
		Params.Now = History.GetLatestFrame();
		return FGSTN_SlotResolver::Resolve(Pointers, History, Params, Settings, [](int32) { return true; });
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS

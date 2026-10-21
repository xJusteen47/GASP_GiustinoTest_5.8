// Helpers shared by the Core automation tests.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_BufferResetRules.h"
#include "Input/GSTN_InputTags.h"
#include "Tests/GSTN_TestTags.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_CommandBindings.h"
#include "Input/GSTN_InputConditions.h"
#include "Input/GSTN_InputHistory.h"
#include "Input/GSTN_InputMatcher.h"
#include "Input/GSTN_InputPattern.h"
#include "Input/GSTN_InputScript.h"
#include "Input/GSTN_InputSlot.h"
#include "Input/GSTN_SlotResolver.h"

#if WITH_DEV_AUTOMATION_TESTS

/** Flags of every Core test: pure logic, runs in the editor and in headless commandlets. */
#define GSTN_CORE_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNTest
{
	using FCondition = TInstancedStruct<FGSTN_InputCondition>;
	using EDir = EGSTN_StickDirection;

	/**
	 * Settings with the documented defaults written out. A new object copies the class defaults, which come from the
	 * project's config and so change when a designer tunes them: tests must not depend on that.
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
		Settings->CommandTiers.Add(GSTNTestTags::Attack, 100);
		Settings->CommandTiers.Add(GSTNTestTags::Strong, 200);
		Settings->CommandTiers.Add(GSTNTestTags::Special, 300);
		Settings->CommandTiers.Add(GSTNTestTags::Combo, 350);
		Settings->CommandTiers.Add(GSTNTestTags::Super, 400);
		Settings->ExclusiveButtons = { GSTNTestTags::Button_Attack.GetTag(), GSTNTestTags::Button_Strong.GetTag(), GSTNTestTags::Button_Special.GetTag() };
		return Settings;
	}

	/** Stick vector of a given angle (0 = up, clockwise) and magnitude */
	inline FVector2D StickAt(float AngleDeg, float Magnitude = 1.f)
	{
		const float Radians = FMath::DegreesToRadians(AngleDeg);
		return FVector2D(FMath::Sin(Radians), FMath::Cos(Radians)) * Magnitude;
	}

	// ---- Test buttons (bit order Attack, Strong, Special, Crouch, Shield, Jump = letters A to F of the scripts)

	inline TArray<FGameplayTag> ButtonTags()
	{
		return { GSTNTestTags::Button_Attack.GetTag(), GSTNTestTags::Button_Strong.GetTag(), GSTNTestTags::Button_Special.GetTag(),
			GSTNTestTags::Button_Crouch.GetTag(), GSTNTestTags::Button_Shield.GetTag(), GSTNTestTags::Button_Jump.GetTag() };
	}

	inline FGSTN_ButtonMap MakeButtonMap() { return FGSTN_ButtonMap(ButtonTags()); }

	inline FGameplayTag Attack() { return GSTNTestTags::Button_Attack.GetTag(); }
	inline FGameplayTag Strong() { return GSTNTestTags::Button_Strong.GetTag(); }
	inline FGameplayTag Special() { return GSTNTestTags::Button_Special.GetTag(); }
	inline FGameplayTag Crouch() { return GSTNTestTags::Button_Crouch.GetTag(); }
	inline FGameplayTag Jump() { return GSTNTestTags::Button_Jump.GetTag(); }

	// ---- Condition builders

	/**
	 * Wraps a condition struct. TInstancedStruct::Make(Condition) would pick the variadic overload, whose type defaults to
	 * the base struct: it would slice the condition into a base instance that never matches anything.
	 */
	template<typename T>
	FCondition MakeCondition(const T& Condition)
	{
		return FCondition::Make<T>(Condition);
	}

	inline FCondition Press(const FGameplayTag& Button)
	{
		FGSTN_InputCondition_Press Condition;
		Condition.Buttons.AddTag(Button);
		return MakeCondition(Condition);
	}

	inline FCondition PressAny(const FGameplayTag& First, const FGameplayTag& Second)
	{
		FGSTN_InputCondition_Press Condition;
		Condition.Buttons.AddTag(First);
		Condition.Buttons.AddTag(Second);
		return MakeCondition(Condition);
	}

	inline FCondition Release(const FGameplayTag& Button, int32 MinHeldFrames = 0)
	{
		FGSTN_InputCondition_Release Condition;
		Condition.Button = Button;
		Condition.MinHeldFrames = MinHeldFrames;
		return MakeCondition(Condition);
	}

	inline FCondition Hold(const FGameplayTag& Button, int32 MinFrames = 0)
	{
		FGSTN_InputCondition_Hold Condition;
		Condition.Button = Button;
		Condition.MinFrames = MinFrames;
		return MakeCondition(Condition);
	}

	inline FCondition Chord(const FGameplayTag& First, const FGameplayTag& Second, int32 WithinFrames = 2)
	{
		FGSTN_InputCondition_Chord Condition;
		Condition.Buttons.AddTag(First);
		Condition.Buttons.AddTag(Second);
		Condition.WithinFrames = WithinFrames;
		return MakeCondition(Condition);
	}

	inline FCondition Mash(const FGameplayTag& Button, int32 Count, int32 WithinFrames)
	{
		FGSTN_InputCondition_Mash Condition;
		Condition.Button = Button;
		Condition.Count = Count;
		Condition.WithinFrames = WithinFrames;
		return MakeCondition(Condition);
	}

	inline FCondition Direction(EGSTN_DirectionSpace Space, FGSTN_DirectionMask Mask, EGSTN_StickZone MinZone = EGSTN_StickZone::Neutral,
		EGSTN_StickZone MaxZone = EGSTN_StickZone::Tilt, bool bRequireFlick = false)
	{
		FGSTN_InputCondition_Direction Condition;
		Condition.Space = Space;
		Condition.Directions = Mask;
		Condition.MinZone = MinZone;
		Condition.MaxZone = MaxZone;
		Condition.bRequireFlick = bRequireFlick;
		return MakeCondition(Condition);
	}

	inline FCondition Motion(EGSTN_DirectionSpace Space, std::initializer_list<FGSTN_DirectionMask> Sequence, int32 MaxBetween = 12, int32 MaxTotal = 30)
	{
		FGSTN_InputCondition_Motion Condition;
		Condition.Space = Space;
		for (const FGSTN_DirectionMask Mask : Sequence)
		{
			Condition.Sequence.Add(FGSTN_DirectionSet(Mask));
		}
		Condition.MaxFramesBetween = MaxBetween;
		Condition.MaxTotalFrames = MaxTotal;
		return MakeCondition(Condition);
	}

	inline FCondition Spin(EGSTN_SpinRotation Rotation, int32 MaxFrames = 30, int32 MaxSkipped = 1, float MinDegrees = 360.f)
	{
		FGSTN_InputCondition_Spin Condition;
		Condition.Rotation = Rotation;
		Condition.MaxFrames = MaxFrames;
		Condition.MaxSkippedSectors = MaxSkipped;
		Condition.MinDegrees = MinDegrees;
		return MakeCondition(Condition);
	}

	inline FCondition Charge(EGSTN_DirectionSpace Space, FGSTN_DirectionMask Mask, int32 MinFrames, int32 MaxRelease)
	{
		FGSTN_InputCondition_Charge Condition;
		Condition.Space = Space;
		Condition.Directions = Mask;
		Condition.MinFrames = MinFrames;
		Condition.MaxReleaseFrames = MaxRelease;
		return MakeCondition(Condition);
	}

	// ---- Patterns

	inline FGSTN_InputStep Step(std::initializer_list<FCondition> Conditions, int32 SimultaneityFrames = -1, int32 MaxGapFrames = 15)
	{
		FGSTN_InputStep Result;
		for (const FCondition& Condition : Conditions)
		{
			Result.AllOf.Add(Condition);
		}
		Result.SimultaneityFrames = SimultaneityFrames;
		Result.MaxGapFrames = MaxGapFrames;
		return Result;
	}

	inline FGSTN_InputPattern Pattern(std::initializer_list<FGSTN_InputStep> Steps)
	{
		FGSTN_InputPattern Result;
		for (const FGSTN_InputStep& Step : Steps)
		{
			Result.Steps.Add(Step);
		}
		return Result;
	}

	/** One step pattern */
	inline FGSTN_InputPattern Pattern(std::initializer_list<FCondition> Conditions)
	{
		return Pattern({ Step(Conditions) });
	}

	// ---- Slots

	inline FGSTN_InputSlotSpec Slot(const FGameplayTag& Command, const FGSTN_InputPattern& Pattern)
	{
		FGSTN_InputSlotSpec Spec;
		Spec.Command = Command;
		Spec.Pattern = Pattern;
		return Spec;
	}

	/** Resolves with the test buttons at the history's latest frame; every command can execute unless CanExecute says otherwise */
	inline FGSTN_SlotResolution ResolveSlots(const TArray<FGSTN_InputSlotSpec>& Slots, const FGSTN_InputHistory& History,
		const UGSTN_InputSettings& Settings,
		TFunctionRef<bool(int32)> CanExecute = [](int32) { return true; })
	{
		static const FGSTN_ButtonMap Map = GSTNTest::MakeButtonMap();
		TArray<const FGSTN_InputSlotSpec*> Pointers;
		for (const FGSTN_InputSlotSpec& Spec : Slots)
		{
			Pointers.Add(&Spec);
		}
		FGSTN_ResolveParams Params;
		Params.Buttons = &Map;
		Params.Now = History.GetLatestFrame();
		return FGSTN_SlotResolver::Resolve(Pointers, History, Params, Settings, CanExecute);
	}

	/** Marks the events a resolution asked to consume */
	inline void ConsumeEvents(FGSTN_InputHistory& History, const FGSTN_SlotResolution& Resolution)
	{
		for (const uint8 Slot : Resolution.EventsToConsume)
		{
			History.Consume(Slot);
		}
	}

	// ---- Histories

	/** Plays a notation from frame 0 to Now (Now defaults to the last token's frame) */
	inline FGSTN_InputHistory PlayTo(FAutomationTestBase& Test, const TCHAR* Notation, const UGSTN_InputSettings& Settings, int32 Now = INDEX_NONE)
	{
		FGSTN_InputScript Script(GSTNTest::ButtonTags());
		FString Error;
		FGSTN_InputHistory History;
		const bool bOk = Script.Parse(Notation, Error) && Script.Advance(History, Settings, Now == INDEX_NONE ? Script.GetLastFrame() : Now);
		Test.TestTrue(*FString::Printf(TEXT("script plays: %s (%s)"), Notation, *Error), bOk);
		return History;
	}

	/** Matches with the settings' buffer defaults at the history's latest frame; Customize may change the params */
	inline bool MatchNow(const FGSTN_InputPattern& Pattern, const FGSTN_InputHistory& History, const UGSTN_InputSettings& Settings,
		FGSTN_PatternMatch* OutMatch = nullptr, TFunctionRef<void(FGSTN_MatchParams&)> Customize = [](FGSTN_MatchParams&) {})
	{
		static const FGSTN_ButtonMap Map = GSTNTest::MakeButtonMap();
		FGSTN_MatchParams Params = FGSTN_MatchParams::FromSettings(Settings, Map, History.GetLatestFrame());
		Customize(Params);
		FGSTN_PatternMatch Match;
		const bool bMatched = FGSTN_InputMatcher::Match(Pattern, History, Params, Match);
		if (OutMatch)
		{
			*OutMatch = Match;
		}
		return bMatched;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS

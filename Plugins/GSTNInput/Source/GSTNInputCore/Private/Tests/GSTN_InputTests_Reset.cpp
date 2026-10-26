#include "Tests/GSTN_TestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNTest;

namespace
{
	using FRule = TInstancedStruct<FGSTN_BufferResetRule>;

	FRule OnEvent(const FGameplayTag& Event, std::initializer_list<FGameplayTag> Buttons = {})
	{
		FGSTN_BufferResetRule_OnEvent Rule;
		Rule.EventTag = Event;
		for (const FGameplayTag& Button : Buttons)
		{
			Rule.Buttons.AddTag(Button);
		}
		return FRule::Make<FGSTN_BufferResetRule_OnEvent>(Rule);
	}

	/** Number of unconsumed presses of a button */
	int32 CountLivePresses(const FGSTN_InputHistory& History, int32 Button)
	{
		int32 Count = 0;
		for (int32 Age = 0; Age < History.GetNumButtonEvents(); ++Age)
		{
			const FGSTN_ButtonEvent& Event = History.GetButtonEvent(History.GetButtonSlot(Age));
			Count += (!Event.bRelease && !Event.bConsumed && Event.Button == Button) ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ResetOnEventTest, "GSTNInput.Core.Input.Reset.OnEvent", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ResetOnEventTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const FGSTN_ButtonMap Map = GSTNTest::MakeButtonMap();

	// Any registered tag works as an event name: the rules only compare tags.
	const FGameplayTag Event = GSTNInputTags::Context_Airborne.GetTag();

	FGSTN_BufferFrameEvents Fired;
	Fired.EventTags.AddTag(Event);
	const FGSTN_BufferFrameEvents Quiet;

	// Only the masked button is flushed.
	{
		FGSTN_InputHistory History = PlayTo(*this, TEXT("5:+A 5:+B"), *Settings);
		const FRule Rules[] = { OnEvent(Event, { Attack() }) };
		TestFalse(TEXT("no event, no flush"), FGSTN_BufferResetRules::Apply(History, Map, Rules, Quiet, 5));
		TestEqual(TEXT("A untouched"), CountLivePresses(History, 0), 1);
		TestTrue(TEXT("the rule fires"), FGSTN_BufferResetRules::Apply(History, Map, Rules, Fired, 5));
		TestEqual(TEXT("A flushed"), CountLivePresses(History, 0), 0);
		TestEqual(TEXT("B left alone"), CountLivePresses(History, 1), 1);
	}

	// No filter: every button.
	{
		FGSTN_InputHistory History = PlayTo(*this, TEXT("5:+A 5:+B"), *Settings);
		const FRule Rules[] = { OnEvent(Event) };
		FGSTN_BufferResetRules::Apply(History, Map, Rules, Fired, 5);
		TestEqual(TEXT("A flushed"), CountLivePresses(History, 0), 0);
		TestEqual(TEXT("B flushed"), CountLivePresses(History, 1), 0);
	}

	// A flush ends the hold buffer: the button is still down, but the press is gone.
	{
		const FGSTN_InputPattern PressA = Pattern({ Press(Attack()) });
		FGSTN_InputHistory History = PlayTo(*this, TEXT("0:+A"), *Settings, 30);
		TestTrue(TEXT("held press matches through the hold buffer"), MatchNow(PressA, History, *Settings));
		const FRule Rules[] = { OnEvent(Event) };
		FGSTN_BufferResetRules::Apply(History, Map, Rules, Fired, 30);
		TestFalse(TEXT("after the flush it does not"), MatchNow(PressA, History, *Settings));
		TestEqual(TEXT("the button is still held"), History.GetHeldButtons(), 1u);

		// A new press works again.
		FGSTN_InputSample Sample;
		Sample.Frame = 31;
		Sample.Held = 1u;
		History.Append(Sample);
		TestFalse(TEXT("holding is not pressing"), MatchNow(PressA, History, *Settings));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ResetTagAndIdleTest, "GSTNInput.Core.Input.Reset.TagAndIdle", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ResetTagAndIdleTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const FGSTN_ButtonMap Map = GSTNTest::MakeButtonMap();
	const FGameplayTag Tag = GSTNInputTags::Context_Airborne.GetTag();

	// OnTagAdded.
	{
		FGSTN_BufferResetRule_OnTagAdded Rule;
		Rule.Tag = Tag;
		const FRule Rules[] = { FRule::Make<FGSTN_BufferResetRule_OnTagAdded>(Rule) };

		FGSTN_InputHistory History = PlayTo(*this, TEXT("5:+A"), *Settings);
		FGSTN_BufferFrameEvents Events;
		TestFalse(TEXT("tag not added"), FGSTN_BufferResetRules::Apply(History, Map, Rules, Events, 5));
		Events.TagsAdded.AddTag(Tag);
		TestTrue(TEXT("tag added"), FGSTN_BufferResetRules::Apply(History, Map, Rules, Events, 5));
		TestEqual(TEXT("press flushed"), CountLivePresses(History, 0), 0);

		FGameplayTagContainer Watched;
		FGSTN_BufferResetRules::GetWatchedTags(Rules, Watched);
		TestTrue(TEXT("the rule asks for the tag to be watched"), Watched.HasTagExact(Tag));
	}

	// AfterIdleFrames: counted from the last button or stick event.
	{
		FGSTN_BufferResetRule_AfterIdleFrames Rule;
		Rule.Frames = 10;
		const FRule Rules[] = { FRule::Make<FGSTN_BufferResetRule_AfterIdleFrames>(Rule) };
		const FGSTN_BufferFrameEvents Events;

		FGSTN_InputHistory History = PlayTo(*this, TEXT("5:+A"), *Settings, 14);
		TestFalse(TEXT("9 idle frames"), FGSTN_BufferResetRules::Apply(History, Map, Rules, Events, 14));
		TestEqual(TEXT("press kept"), CountLivePresses(History, 0), 1);
		TestTrue(TEXT("10 idle frames"), FGSTN_BufferResetRules::Apply(History, Map, Rules, Events, 15));
		TestEqual(TEXT("press flushed"), CountLivePresses(History, 0), 0);

		// A stick event counts as activity.
		FGSTN_InputHistory Stick = PlayTo(*this, TEXT("5:+A 10:8t"), *Settings, 15);
		TestFalse(TEXT("the stick moved 5 frames ago"), FGSTN_BufferResetRules::Apply(Stick, Map, Rules, Events, 15));
		TestEqual(TEXT("press kept"), CountLivePresses(Stick, 0), 1);
	}

	// Lists apply in order: global, then weapon, then move.
	{
		FGSTN_InputHistory History = PlayTo(*this, TEXT("5:+A 5:+B"), *Settings);
		const FGameplayTag Event = Tag;
		FGSTN_BufferFrameEvents Events;
		Events.EventTags.AddTag(Event);
		const FRule Global[] = { OnEvent(Event, { Attack() }) };
		const FRule Move[] = { OnEvent(Event, { Strong() }) };
		FGSTN_BufferResetRules::Apply(History, Map, Global, Events, 5);
		TestEqual(TEXT("global list flushed A"), CountLivePresses(History, 0), 0);
		TestEqual(TEXT("B still there"), CountLivePresses(History, 1), 1);
		FGSTN_BufferResetRules::Apply(History, Map, Move, Events, 5);
		TestEqual(TEXT("move list flushed B"), CountLivePresses(History, 1), 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

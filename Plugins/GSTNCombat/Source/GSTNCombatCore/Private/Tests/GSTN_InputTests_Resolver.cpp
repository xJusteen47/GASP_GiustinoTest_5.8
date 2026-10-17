#include "Tests/GSTN_TestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNTest;

namespace
{
	// A few standard commands as slots of one set.
	FGSTN_InputSlotSpec AttackNeutral() { return Slot(GSTNCommandTags::Attack_Neutral, Pattern({ Press(Attack()) })); }
	FGSTN_InputSlotSpec SpecialNeutral() { return Slot(GSTNCommandTags::Special_Neutral, Pattern({ Press(Special()) })); }
	FGSTN_InputSlotSpec ComboNeutral() { return Slot(GSTNCommandTags::Combo_AttackSpecial_Neutral, Pattern({ Chord(Attack(), Special(), 2) })); }

	FString Describe(const FGSTN_SlotResolution& Resolution)
	{
		return Resolution.SlotIndex == INDEX_NONE ? (Resolution.bDeferredForChord ? FString(TEXT("deferred")) : FString(TEXT("none"))) : FString::FromInt(Resolution.SlotIndex);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ResolverSpecificityTest, "GSTNCombat.Core.Input.Resolver.Specificity", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ResolverSpecificityTest::RunTest(const FString& Parameters)
{
	using namespace GSTN_DirectionMasks;
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = MakeSettings();

	// The same tier (attack): the more demanding pattern wins.
	const TArray<FGSTN_InputSlotSpec> Slots = {
		Slot(GSTNCommandTags::Attack_Neutral, Pattern({ Press(Attack()) })),
		Slot(GSTNCommandTags::Attack_Directional, Pattern({ Press(Attack()), Direction(EGSTN_DirectionSpace::Facing, AnyTilted, EGSTN_StickZone::Tilt) })),
		Slot(GSTNCommandTags::Attack_CrouchDirectional, Pattern({ Step({ Motion(EGSTN_DirectionSpace::Facing, { FacingBack, FacingForward }) }), Step({ Press(Attack()) }) })),
	};

	TestEqual(TEXT("motion + A beats direction + A"), ResolveSlots(Slots, PlayTo(*this, TEXT("0:2t 3:8t 5:+A"), *Settings), *Settings).SlotIndex, 2);
	TestEqual(TEXT("direction + A beats A"), ResolveSlots(Slots, PlayTo(*this, TEXT("0:8t 5:+A"), *Settings), *Settings).SlotIndex, 1);
	TestEqual(TEXT("A alone"), ResolveSlots(Slots, PlayTo(*this, TEXT("5:+A"), *Settings), *Settings).SlotIndex, 0);
	TestEqual(TEXT("nothing pressed"), ResolveSlots(Slots, PlayTo(*this, TEXT("0:8t 5:5"), *Settings), *Settings).SlotIndex, static_cast<int32>(INDEX_NONE));

	// Equal rank: the slot order decides (the validator reports such ties).
	const TArray<FGSTN_InputSlotSpec> Tie = { AttackNeutral(), Slot(GSTNCommandTags::Attack_Directional, Pattern({ Press(Attack()) })) };
	TestEqual(TEXT("equal rank goes to the first slot"), ResolveSlots(Tie, PlayTo(*this, TEXT("5:+A"), *Settings), *Settings).SlotIndex, 0);

	// The manual offset breaks it the other way.
	TArray<FGSTN_InputSlotSpec> Offset = Tie;
	Offset[1].PriorityOffset = 1;
	TestEqual(TEXT("offset raises the second slot"), ResolveSlots(Offset, PlayTo(*this, TEXT("5:+A"), *Settings), *Settings).SlotIndex, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ResolverConsumptionTest, "GSTNCombat.Core.Input.Resolver.Consumption", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ResolverConsumptionTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = MakeSettings();
	const TArray<FGSTN_InputSlotSpec> Slots = { AttackNeutral() };

	// A held press: it fires once, the next frame nothing is left to fire, even though the button is still down.
	FGSTN_InputScript Script;
	FString Error;
	FGSTN_InputHistory History;
	TestTrue(TEXT("script"), Script.Parse(TEXT("5:+A"), Error) && Script.Advance(History, *Settings, 5));
	const FGSTN_SlotResolution First = ResolveSlots(Slots, History, *Settings);
	TestEqual(TEXT("fires"), First.SlotIndex, 0);
	ConsumeEvents(History, First);
	TestTrue(TEXT("next frame"), Script.Advance(History, *Settings, 6));
	TestEqual(TEXT("a consumed press does not fire again"), ResolveSlots(Slots, History, *Settings).SlotIndex, static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("much later, still held"), Script.Advance(History, *Settings, 40));
	TestEqual(TEXT("not even through the hold buffer"), ResolveSlots(Slots, History, *Settings).SlotIndex, static_cast<int32>(INDEX_NONE));

	// Without consuming it would have fired again.
	FGSTN_InputHistory Unconsumed = PlayTo(*this, TEXT("5:+A"), *Settings, 6);
	TestEqual(TEXT("an unconsumed press fires on the next frame too"), ResolveSlots(Slots, Unconsumed, *Settings).SlotIndex, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ResolverUnavailableTest, "GSTNCombat.Core.Input.Resolver.Unavailable", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ResolverUnavailableTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = MakeSettings();
	const FGSTN_InputHistory History = PlayTo(*this, TEXT("5:+A"), *Settings);

	// Slot 0 is the higher command (a special on the same button), slot 1 the plain one.
	TArray<FGSTN_InputSlotSpec> Slots = { Slot(GSTNCommandTags::Special_Neutral, Pattern({ Press(Attack()) })), AttackNeutral() };
	const auto OnlyTheSecond = [](int32 Index) { return Index == 1; };

	// The higher command cannot execute: it keeps its press, nothing below it fires.
	TestEqual(TEXT("the plain command cannot steal the press"), ResolveSlots(Slots, History, *Settings, FGameplayTagContainer(), OnlyTheSecond).SlotIndex, static_cast<int32>(INDEX_NONE));

	// With fall through, the plain command takes over.
	Slots[0].bFallThroughWhenUnavailable = true;
	TestEqual(TEXT("fall through"), ResolveSlots(Slots, History, *Settings, FGameplayTagContainer(), OnlyTheSecond).SlotIndex, 1);

	// And when the higher command can execute, it wins.
	TestEqual(TEXT("available"), ResolveSlots(Slots, History, *Settings).SlotIndex, 0);

	// CanExecute is asked once per slot, never for a slot that did not match.
	TArray<int32> Asked;
	ResolveSlots(Slots, PlayTo(*this, TEXT("5:+C"), *Settings), *Settings, FGameplayTagContainer(), [&Asked](int32 Index) { Asked.Add(Index); return true; });
	TestEqual(TEXT("no slot matched, nobody asked"), Asked.Num(), 0);
	Asked.Reset();
	ResolveSlots(Slots, History, *Settings, FGameplayTagContainer(), [&Asked](int32 Index) { Asked.Add(Index); return false; });
	TestEqual(TEXT("both asked once"), Asked.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ResolverReservationTest, "GSTNCombat.Core.Input.Resolver.ReservationUsesOlderPress", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ResolverReservationTest::RunTest(const FString& Parameters)
{
	// Two presses of A are buffered. The higher slot cannot execute and reserves the newest; the lower slot may still use the older.
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = MakeSettings();
	const FGSTN_InputHistory History = PlayTo(*this, TEXT("4:*A 5:*A"), *Settings);
	const TArray<FGSTN_InputSlotSpec> Slots = { Slot(GSTNCommandTags::Special_Neutral, Pattern({ Press(Attack()) })), AttackNeutral() };

	const FGSTN_SlotResolution Resolution = ResolveSlots(Slots, History, *Settings, FGameplayTagContainer(), [](int32 Index) { return Index == 1; });
	TestEqual(TEXT("the lower slot uses the older press"), Resolution.SlotIndex, 1);
	if (Resolution.SlotIndex == 1)
	{
		TestEqual(TEXT("it is the press of frame 4"), Resolution.Match.StepAnchors.Last(), 4);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ResolverContextTest, "GSTNCombat.Core.Input.Resolver.Context", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ResolverContextTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = MakeSettings();
	const FGSTN_InputHistory History = PlayTo(*this, TEXT("5:+A"), *Settings);

	FGSTN_InputSlotSpec Air = Slot(GSTNCommandTags::Air_Attack_Neutral, Pattern({ Press(Attack()) }));
	Air.Context.RequireTags.AddTag(GSTNCommandTags::Context_Airborne.GetTag());
	FGSTN_InputSlotSpec Ground = AttackNeutral();
	Ground.Context.IgnoreTags.AddTag(GSTNCommandTags::Context_Airborne.GetTag());
	const TArray<FGSTN_InputSlotSpec> Slots = { Air, Ground };

	FGameplayTagContainer Airborne;
	Airborne.AddTag(GSTNCommandTags::Context_Airborne.GetTag());
	TestEqual(TEXT("grounded picks the ground slot"), ResolveSlots(Slots, History, *Settings).SlotIndex, 1);
	TestEqual(TEXT("airborne picks the air slot"), ResolveSlots(Slots, History, *Settings, Airborne).SlotIndex, 0);

	// An airborne character with only a ground slot has nothing to fire.
	const TArray<FGSTN_InputSlotSpec> GroundOnly = { Ground };
	TestEqual(TEXT("airborne with a ground only slot"), ResolveSlots(GroundOnly, History, *Settings, Airborne).SlotIndex, static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ResolverTiersTest, "GSTNCombat.Core.Input.Resolver.TiersAndPressGroups", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ResolverTiersTest::RunTest(const FString& Parameters)
{
	using namespace GSTN_DirectionMasks;
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = MakeSettings();
	const TArray<FGSTN_InputSlotSpec> Slots = { AttackNeutral(), SpecialNeutral() };
	const auto Winner = [&](const TCHAR* Notation) { return ResolveSlots(Slots, PlayTo(*this, Notation, *Settings), *Settings).SlotIndex; };

	// Pressed together, or one frame apart: the special (higher tier).
	TestEqual(TEXT("attack and special together"), Winner(TEXT("5:+A 5:+C")), 1);
	TestEqual(TEXT("special one frame after the attack"), Winner(TEXT("5:+A 6:+C")), 1);
	TestEqual(TEXT("attack one frame after the special"), Winner(TEXT("5:+C 6:+A")), 1);

	// Further apart than the press group (2 frames): the most recent press competes alone.
	TestEqual(TEXT("attack 4 frames after the special: the attack"), Winner(TEXT("5:+C 9:+A")), 0);
	TestEqual(TEXT("special 4 frames after the attack: the special"), Winner(TEXT("5:+A 9:+C")), 1);

	Settings->BufferConflictPolicy = EGSTN_BufferConflictPolicy::HighestPriority;
	TestEqual(TEXT("HighestPriority: the older special beats the newer attack"), Winner(TEXT("5:+C 9:+A")), 1);
	Settings->BufferConflictPolicy = EGSTN_BufferConflictPolicy::Oldest;
	TestEqual(TEXT("Oldest: the older press competes alone"), Winner(TEXT("5:+A 9:+C")), 0);
	Settings->BufferConflictPolicy = EGSTN_BufferConflictPolicy::MostRecent;

	// A tier beats the pattern: a crouch-directional attack loses to a neutral special.
	const TArray<FGSTN_InputSlotSpec> CrouchSlots = {
		Slot(GSTNCommandTags::Attack_CrouchDirectional, Pattern({ Press(Attack()), Hold(Crouch()), Direction(EGSTN_DirectionSpace::Facing, AnyTilted, EGSTN_StickZone::Tilt) })),
		SpecialNeutral() };
	TestEqual(TEXT("tier before specificity"), ResolveSlots(CrouchSlots, PlayTo(*this, TEXT("0:+D 0:8t 5:+A 5:+C"), *Settings), *Settings).SlotIndex, 1);
	TestEqual(TEXT("without the special, the crouch attack"), ResolveSlots(CrouchSlots, PlayTo(*this, TEXT("0:+D 0:8t 5:+A"), *Settings), *Settings).SlotIndex, 0);

	// The chord command beats both single buttons.
	const TArray<FGSTN_InputSlotSpec> WithCombo = { AttackNeutral(), SpecialNeutral(), ComboNeutral() };
	TestEqual(TEXT("chord command"), ResolveSlots(WithCombo, PlayTo(*this, TEXT("5:+A 5:+C"), *Settings), *Settings).SlotIndex, 2);
	TestEqual(TEXT("chord command with the buttons a frame apart"), ResolveSlots(WithCombo, PlayTo(*this, TEXT("5:+A 6:+C"), *Settings), *Settings).SlotIndex, 2);
	TestEqual(TEXT("one button is still its own command"), ResolveSlots(WithCombo, PlayTo(*this, TEXT("5:+A"), *Settings), *Settings).SlotIndex, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ResolverLoserConsumptionTest, "GSTNCombat.Core.Input.Resolver.LoserConsumption", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ResolverLoserConsumptionTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = MakeSettings();
	const TArray<FGSTN_InputSlotSpec> Slots = { AttackNeutral(), SpecialNeutral() };

	// Attack and special together: the special fires and the attack press goes with it.
	FGSTN_InputHistory History = PlayTo(*this, TEXT("5:+A 5:+C"), *Settings);
	const FGSTN_SlotResolution Resolution = ResolveSlots(Slots, History, *Settings);
	TestEqual(TEXT("special wins"), Resolution.SlotIndex, 1);
	TestEqual(TEXT("both presses are consumed"), Resolution.EventsToConsume.Num(), 2);
	ConsumeEvents(History, Resolution);
	TestEqual(TEXT("the losing attack does not come out later"), ResolveSlots(Slots, History, *Settings).SlotIndex, static_cast<int32>(INDEX_NONE));

	// A press of a button that is not exclusive (jump) in the same group is left alone and fires on its own next.
	const TArray<FGSTN_InputSlotSpec> WithJump = { AttackNeutral(), Slot(GSTNCommandTags::Attack_Directional, Pattern({ Press(Jump()) })) };
	FGSTN_InputHistory Both = PlayTo(*this, TEXT("5:+A 5:+F"), *Settings);
	const FGSTN_SlotResolution First = ResolveSlots(WithJump, Both, *Settings);
	TestEqual(TEXT("the first slot wins the tie"), First.SlotIndex, 0);
	TestEqual(TEXT("only the attack press is consumed"), First.EventsToConsume.Num(), 1);
	ConsumeEvents(Both, First);
	TestEqual(TEXT("the jump press is still there"), ResolveSlots(WithJump, Both, *Settings).SlotIndex, 1);

	// A press older than the group stays buffered: it is not a loser of this group.
	FGSTN_InputHistory Older = PlayTo(*this, TEXT("5:+C 9:+A"), *Settings);
	const FGSTN_SlotResolution Attack = ResolveSlots(Slots, Older, *Settings);
	TestEqual(TEXT("the recent attack"), Attack.SlotIndex, 0);
	TestEqual(TEXT("the old special press is not consumed"), Attack.EventsToConsume.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ResolverChordGraceTest, "GSTNCombat.Core.Input.Resolver.ChordGrace", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ResolverChordGraceTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = MakeSettings();
	TArray<FGSTN_InputSlotSpec> Slots = { AttackNeutral(), ComboNeutral() };

	// Off by default: the attack fires at once.
	TestEqual(TEXT("grace off"), ResolveSlots(Slots, PlayTo(*this, TEXT("5:+A"), *Settings), *Settings).SlotIndex, 0);

	// On for the attack slot: it waits ChordGraceFrames (3) for the second button.
	Slots[0].ChordGrace = EGSTN_OverrideBool::On;
	const auto Resolve = [&](const TCHAR* Notation, int32 Now) { return Describe(ResolveSlots(Slots, PlayTo(*this, Notation, *Settings, Now), *Settings)); };
	TestEqual(TEXT("press frame"), Resolve(TEXT("5:+A"), 5), FString(TEXT("deferred")));
	TestEqual(TEXT("one frame later"), Resolve(TEXT("5:+A"), 6), FString(TEXT("deferred")));
	TestEqual(TEXT("two frames later"), Resolve(TEXT("5:+A"), 7), FString(TEXT("deferred")));
	TestEqual(TEXT("grace over: the attack alone"), Resolve(TEXT("5:+A"), 8), FString(TEXT("0")));

	// The second button inside the grace turns it into the chord command.
	TestEqual(TEXT("the chord completes"), Resolve(TEXT("5:+A 6:+C"), 6), FString(TEXT("1")));
	TestEqual(TEXT("the chord completes on the same frame"), Resolve(TEXT("5:+A 5:+C"), 5), FString(TEXT("1")));

	// Grace by default in the settings.
	Slots[0].ChordGrace = EGSTN_OverrideBool::Default;
	Settings->bChordGraceByDefault = true;
	TestEqual(TEXT("grace on by the settings"), Resolve(TEXT("5:+A"), 5), FString(TEXT("deferred")));
	Slots[0].ChordGrace = EGSTN_OverrideBool::Off;
	TestEqual(TEXT("the slot opts out"), Resolve(TEXT("5:+A"), 5), FString(TEXT("0")));

	// A button that is in no chord is never delayed.
	Slots[0].ChordGrace = EGSTN_OverrideBool::On;
	const TArray<FGSTN_InputSlotSpec> Lonely = { Slots[0] };
	TestEqual(TEXT("no chord, no wait"), ResolveSlots(Lonely, PlayTo(*this, TEXT("5:+A"), *Settings), *Settings).SlotIndex, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ResolverDeterminismTest, "GSTNCombat.Core.Input.Resolver.Determinism", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ResolverDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace GSTN_DirectionMasks;
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = MakeSettings();
	const TArray<FGSTN_InputSlotSpec> Slots = {
		AttackNeutral(), SpecialNeutral(), ComboNeutral(),
		Slot(GSTNCommandTags::Attack_Directional, Pattern({ Press(Attack()), Direction(EGSTN_DirectionSpace::Facing, AnyTilted, EGSTN_StickZone::Tilt) })),
		Slot(GSTNCommandTags::Attack_CrouchDirectional, Pattern({ Step({ Motion(EGSTN_DirectionSpace::Facing, { FacingBack, FacingForward }) }), Step({ Press(Attack()) }) })),
	};

	// Resolve and consume every frame of a busy session; fork it halfway (history and script are plain values) and play the
	// rest on both copies, which must give the same decisions: the stand-in for a rollback and re-simulation.
	FGSTN_InputScript Script;
	FString Error;
	TestTrue(TEXT("script parses"), Script.Parse(TEXT("0:2t 3:8t 5:+A 8:+C 9:-A 12:*A 13:6t 14:+A 15:*B 18:-A 20:-C 22:*C 25:5 26:*A"), Error));

	const auto PlayFrames = [&](FGSTN_InputScript& PlayedScript, FGSTN_InputHistory& History, int32 From, int32 To)
	{
		FString Log;
		for (int32 Frame = From; Frame <= To; ++Frame)
		{
			if (!PlayedScript.Advance(History, *Settings, Frame))
			{
				AddError(TEXT("the script is invalid"));
				break;
			}
			const FGSTN_SlotResolution Resolution = ResolveSlots(Slots, History, *Settings);
			ConsumeEvents(History, Resolution);
			Log += FString::Printf(TEXT("%d:%s "), Frame, *Describe(Resolution));
		}
		return Log;
	};

	FGSTN_InputHistory History;
	const FString FirstHalf = PlayFrames(Script, History, 0, 9);
	FGSTN_InputHistory ForkHistory = History;
	FGSTN_InputScript ForkScript = Script;
	const FString Original = PlayFrames(Script, History, 10, 30);
	const FString Replayed = PlayFrames(ForkScript, ForkHistory, 10, 30);
	TestEqual(TEXT("the forked session decides the same"), Replayed, Original);
	TestTrue(TEXT("both histories end equal"), History == ForkHistory);
	TestTrue(TEXT("something fired"), (FirstHalf + Original).Contains(TEXT(":0 ")) || (FirstHalf + Original).Contains(TEXT(":4 ")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

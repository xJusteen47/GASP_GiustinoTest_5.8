// Automation tests of the part restore (P7b, combat-parts.md §4.5): the request is an infinite effect on the ability system, the parts tick
// extension consumes it at the victim's next combat tick.

#include "GSTN_PartsNPTestFixture.h"
#include "Effects/GSTN_GEComponent_RestoreParts.h"
#include "Events/GSTN_PartEvents.h"
#include "Data/GSTN_PenetrationTable.h"
#include "Events/GSTN_SimEvent.h"
#include "GSTN_PartsTags.h"
#include "Math/GSTN_PartMath.h"
#include "Sim/GSTN_PartsTickExtension.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTickExtension.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNPartsNPTest;

namespace
{
	constexpr uint8 AtCap = GSTN_PartsLimits::DamageAtCap;

	FGSTN_PartRestoreRequest MakeRequest(uint16 Mask, float Fraction = 0.f)
	{
		FGSTN_PartRestoreRequest Request;
		Request.PartMask = Mask;
		Request.DamageFraction = Fraction;
		return Request;
	}

	/** The state of an arm that broke at serial 1 (damage at the cap, the hand cascaded with it) */
	FGSTN_PartsState ArmBrokenState()
	{
		return MakeState((1u << Arm) | (1u << Hand), 1, { { Arm, AtCap }, { Hand, AtCap } });
	}

	/** One tick of the extension at the hook that consumes the requests; returns the events the frame's outbox got */
	int32 TickRestore(FGSTN_PartsTickExtension& Parts, FGSTN_CombatTickContext& Context, FGSTN_SimEventOutbox& Outbox)
	{
		Context.Outbox = &Outbox;
		const int32 Before = Outbox.Events.Num();
		Parts.OnCombatHook(EGSTN_CombatHookPoint::AfterClocks, Context);
		return Outbox.Events.Num() - Before;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsNPRestoreClearsMaskTest, "GSTNParts.NP.Restore.ClearsMaskAtFPlus1", GSTNPartsNPTest::TestFlags)
bool FGSTN_PartsNPRestoreClearsMaskTest::RunTest(const FString& Parameters)
{
	FPartsFixture Fixture;
	FRig Rig;
	FGSTN_PartsAttach::ApplyPartSet(Rig.Owner, Fixture.Set.Get(), Rig.ASC);

	FGSTN_PartsTickExtension Parts;
	FGSTN_CombatSyncState In, Out;
	SetFragment(In.Fragments, ArmBrokenState(), Fixture.Num());
	SetFragment(Out.Fragments, ArmBrokenState(), Fixture.Num());
	FGSTN_CombatTickContext Context(11, In, Out);
	Context.Owner = Rig.Owner;
	Context.AbilitySystem = Rig.ASC;
	FGSTN_SimEventOutbox Outbox;

	// Nothing asked: nothing changes and no event is made
	TestEqual(TEXT("no request, no event"), TickRestore(Parts, Context, Outbox), 0);
	TestTrue(TEXT("no request, the state is the one the frame started with"), ReadState(Out.Fragments) == ArmBrokenState());

	// The arm only, a quarter of the cap kept: the request is on the ability system until the tick that consumes it
	TestTrue(TEXT("the request is applied"), FGSTN_PartRestoreQueue::Apply(*Rig.ASC, nullptr, MakeRequest(1u << Arm, 0.25f), Rig.Owner));
	TArray<FGSTN_PendingPartRestore> Pending;
	FGSTN_PartRestoreQueue::Collect(*Rig.ASC, Pending);
	TestEqual(TEXT("one request is pending"), Pending.Num(), 1);
	if (Pending.Num() == 1)
	{
		TestEqual(TEXT("it carries the mask of the call"), Pending[0].Request.GetMask16(), static_cast<uint16>(1u << Arm));
		TestTrue(TEXT("and its fraction"), FMath::IsNearlyEqual(Pending[0].Request.DamageFraction, 0.25f, 1e-4f));
	}
	TestTrue(TEXT("a request on the ability system changes no state by itself"), ReadState(Out.Fragments) == ArmBrokenState());

	TestEqual(TEXT("one Restored event for the one restored part"), TickRestore(Parts, Context, Outbox), 1);
	const FGSTN_PartsState After = ReadState(Out.Fragments);
	TestFalse(TEXT("the arm's bit is clear"), After.IsBroken(Arm));
	TestTrue(TEXT("the hand, a child that broke with it, stays broken: only the masked parts and their ancestors come back"), After.IsBroken(Hand));
	TestEqual(TEXT("the arm keeps the quarter"), static_cast<int32>(After.PartDamage[Arm]), FMath::CeilToInt(0.25f * AtCap));
	TestEqual(TEXT("the serial moved on"), static_cast<int32>(After.BreakSerial), 2);
	TestEqual(TEXT("untouched parts keep their damage"), After.PartDamage[Hand], AtCap);

	FGSTN_PartRestoreQueue::Collect(*Rig.ASC, Pending);
	TestTrue(TEXT("the request is consumed: the effect is gone"), Pending.IsEmpty());
	TestEqual(TEXT("a second tick finds nothing and adds no event"), TickRestore(Parts, Context, Outbox), 0);
	TestTrue(TEXT("and the state stays"), ReadState(Out.Fragments) == After);

	// The event: type, frame, target, payload with the new serial
	if (Outbox.Events.Num() == 1)
	{
		const FGSTN_SimEvent& Event = Outbox.Events[0];
		TestTrue(TEXT("the event is Restored"), Event.EventType == GSTNPartsTags::Event_Part_Restored);
		TestEqual(TEXT("on the frame being simulated"), Event.Frame, 11);
		TestTrue(TEXT("about the restored entity"), Event.Target.Get() == Rig.Owner);
		const FGSTN_PartEventPayload Payload = FGSTN_PartEvents::GetPayload(Event);
		TestEqual(TEXT("payload: the part"), static_cast<int32>(Payload.PartIndex), Arm);
		TestEqual(TEXT("payload: the serial after the restore"), static_cast<int32>(Payload.BreakSerial), 2);
	}

	// The tags follow the state in the same frame (BeforeTagProjection reads the frame's output)
	Parts.OnCombatHook(EGSTN_CombatHookPoint::BeforeTagProjection, Context);
	TestEqual(TEXT("a part is still broken (the hand)"), Rig.ASC->GetTagCount(GSTNPartsTags::Part_Broken), 1);

	// The default request, every part, full repair
	TestTrue(TEXT("a second request is applied"), FGSTN_PartRestoreQueue::Apply(*Rig.ASC, nullptr, MakeRequest(0, 0.f), nullptr));
	TickRestore(Parts, Context, Outbox);
	const FGSTN_PartsState All = ReadState(Out.Fragments);
	TestEqual(TEXT("mask 0 restores every part"), All.BrokenMask, static_cast<uint16>(0));
	TestEqual(TEXT("full repair"), static_cast<int32>(All.PartDamage[Hand]), 0);
	Parts.OnCombatHook(EGSTN_CombatHookPoint::BeforeTagProjection, Context);
	TestEqual(TEXT("Part.Broken goes with the mask"), Rig.ASC->GetTagCount(GSTNPartsTags::Part_Broken), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsNPRestoreRollbackTest, "GSTNParts.NP.Restore.RollbackRedeliveryRestoresOnce", GSTNPartsNPTest::TestFlags)
bool FGSTN_PartsNPRestoreRollbackTest::RunTest(const FString& Parameters)
{
	FPartsFixture Fixture;
	FRig Rig;
	FGSTN_PartsAttach::ApplyPartSet(Rig.Owner, Fixture.Set.Get(), Rig.ASC);
	FGSTN_PartsTickExtension Parts;

	// The frame is simulated, then rolled back (the ability system and the combat state go back together: the effect is on the target again) and
	// simulated again: the same state and the same one event, no second restore
	FGSTN_PartsState First;
	int32 FirstEvents = 0;
	for (int32 Run = 0; Run < 3; ++Run)
	{
		FGSTN_CombatSyncState In, Out;
		SetFragment(In.Fragments, ArmBrokenState(), Fixture.Num());
		SetFragment(Out.Fragments, ArmBrokenState(), Fixture.Num());
		FGSTN_CombatTickContext Context(20, In, Out);
		Context.Owner = Rig.Owner;
		Context.AbilitySystem = Rig.ASC;
		FGSTN_SimEventOutbox Outbox;

		TestTrue(TEXT("the effect is on the target after the rollback"), FGSTN_PartRestoreQueue::Apply(*Rig.ASC, nullptr, MakeRequest(1u << Arm), nullptr));
		const int32 Events = TickRestore(Parts, Context, Outbox);
		if (Run == 0)
		{
			First = ReadState(Out.Fragments);
			FirstEvents = Events;
			TestEqual(TEXT("the first run restores and tells once"), Events, 1);
		}
		else
		{
			TestTrue(TEXT("a re-simulated frame ends in the same state"), ReadState(Out.Fragments) == First);
			TestEqual(TEXT("with the same one event"), Events, FirstEvents);
		}
		TestEqual(TEXT("the serial moved exactly one step per run"), static_cast<int32>(ReadState(Out.Fragments).BreakSerial), 2);
	}

	// The same frame twice with no rollback in between (the effect is gone): the state is already restored and a second tick changes nothing
	FGSTN_CombatSyncState In, Out;
	SetFragment(Out.Fragments, First, Fixture.Num());
	FGSTN_CombatTickContext Context(21, In, Out);
	Context.Owner = Rig.Owner;
	Context.AbilitySystem = Rig.ASC;
	FGSTN_SimEventOutbox Outbox;
	TestEqual(TEXT("no effect, no event"), TickRestore(Parts, Context, Outbox), 0);
	TestTrue(TEXT("and no change"), ReadState(Out.Fragments) == First);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsNPRestoreThenBreakTest, "GSTNParts.NP.Restore.RestoreThenBreakNewSerial", GSTNPartsNPTest::TestFlags)
bool FGSTN_PartsNPRestoreThenBreakTest::RunTest(const FString& Parameters)
{
	FPartsFixture Fixture;
	FRig Rig;
	FGSTN_PartsAttach::ApplyPartSet(Rig.Owner, Fixture.Set.Get(), Rig.ASC);
	FGSTN_PartsTickExtension Parts;

	FGSTN_CombatSyncState In, Out;
	SetFragment(Out.Fragments, ArmBrokenState(), Fixture.Num());
	FGSTN_CombatTickContext Context(30, In, Out);
	Context.Owner = Rig.Owner;
	Context.AbilitySystem = Rig.ASC;
	FGSTN_SimEventOutbox Outbox;

	FGSTN_PartRestoreQueue::Apply(*Rig.ASC, nullptr, MakeRequest(1u << Arm), nullptr);
	TickRestore(Parts, Context, Outbox);
	FGSTN_PartsState State = ReadState(Out.Fragments);
	TestEqual(TEXT("restored at serial 2"), static_cast<int32>(State.BreakSerial), 2);

	// The arm breaks again: an overwhelming hit on it, the way the part math decides it
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();
	FGSTN_PartHitInput Input;
	Input.Damage = 10000.f;
	Input.PartIndex = Arm;
	Input.Penetration = 15;
	const FGSTN_PartHitResult Hit = UGSTN_PartMath::ApplyHit(Fixture.Set->GetParts(), Bands, State, Input);
	TestTrue(TEXT("the arm broke again"), (Hit.NewlyBrokenMask & (1u << Arm)) != 0);
	TestEqual(TEXT("under a new serial"), static_cast<int32>(State.BreakSerial), 3);

	// The keys of the three events differ, so a consumer that deduplicates by (entity, part, serial) shows all of them
	FGSTN_SimEvent Restored;
	Restored.Target = Rig.Owner;
	FGSTN_SimEventOutbox Made;
	FGSTN_PartEvents::AppendRestored(Rig.Owner, nullptr, 30, 1u << Arm, 2, Made);
	FGSTN_SimEvent Broken;
	Broken.EventType = GSTNPartsTags::Event_Part_Broken;
	Broken.Target = Rig.Owner;
	Broken.Magnitude = FGSTN_PartEventPayload::Make(Arm, 3, 0).Pack();
	FGSTN_SimEvent FirstBroken = Broken;
	FirstBroken.Magnitude = FGSTN_PartEventPayload::Make(Arm, 1, 0).Pack();
	if (Made.Events.Num() == 1)
	{
		TestFalse(TEXT("the restore's key is not the first break's"), FGSTN_PartEvents::MakeKey(Made.Events[0]) == FGSTN_PartEvents::MakeKey(FirstBroken));
		TestFalse(TEXT("nor the second break's"), FGSTN_PartEvents::MakeKey(Made.Events[0]) == FGSTN_PartEvents::MakeKey(Broken));
	}
	TestFalse(TEXT("the two breaks differ"), FGSTN_PartEvents::MakeKey(FirstBroken) == FGSTN_PartEvents::MakeKey(Broken));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsNPRestoreIrreversibleTest, "GSTNParts.NP.Restore.IrreversibleEffectsStay", GSTNPartsNPTest::TestFlags)
bool FGSTN_PartsNPRestoreIrreversibleTest::RunTest(const FString& Parameters)
{
	FPartsFixture Fixture;
	// The one-shot effects of the arm: a launch and a stagger. They happened; a restore does not undo them (no effect of them is left to undo)
	FGSTN_PartBreakEffect_Launch Launch;
	Launch.Speed = 500.f;
	Fixture.Set->Parts[Arm].OnBreak.Add(FInstancedStruct::Make(Launch));
	FGSTN_PartBreakEffect_Stagger Stagger;
	Stagger.Frames = 20;
	Fixture.Set->Parts[Arm].OnBreak.Add(FInstancedStruct::Make(Stagger));

	FRig Rig;
	FGSTN_PartsAttach::ApplyPartSet(Rig.Owner, Fixture.Set.Get(), Rig.ASC);
	FGSTN_PartsTickExtension Parts;

	FGSTN_CombatSyncState In, Out;
	SetFragment(In.Fragments, ArmBrokenState(), Fixture.Num());
	SetFragment(Out.Fragments, ArmBrokenState(), Fixture.Num());
	FGSTN_CombatTickContext Context(40, In, Out);
	Context.Owner = Rig.Owner;
	Context.AbilitySystem = Rig.ASC;
	FGSTN_SimEventOutbox Outbox;

	Parts.OnCombatHook(EGSTN_CombatHookPoint::BeforeCandidates, Context);
	TestTrue(TEXT("while broken the arm's hit group is masked"), Context.HitGroupMask.Has(ArmHitGroup));

	FGSTN_PartRestoreQueue::Apply(*Rig.ASC, nullptr, MakeRequest(1u << Arm), nullptr);
	TickRestore(Parts, Context, Outbox);

	// The projections reverse by themselves: the next frame reads the restored fragment
	FGSTN_CombatSyncState Next = Out;
	FGSTN_CombatSyncState NextOut;
	FGSTN_CombatTickContext NextContext(41, Next, NextOut);
	NextContext.Owner = Rig.Owner;
	Parts.OnCombatHook(EGSTN_CombatHookPoint::BeforeCandidates, NextContext);
	TestFalse(TEXT("derived effects reverse: the hit group is back"), NextContext.HitGroupMask.Has(ArmHitGroup));

	// The one-shots left no effect on the entity, so nothing is removed or re-applied by a restore
	FGameplayEffectQuery Everything;
	TestEqual(TEXT("no effect is left on the ability system: the restore request was the only one and it is consumed"), Rig.ASC->GetActiveEffects(Everything).Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

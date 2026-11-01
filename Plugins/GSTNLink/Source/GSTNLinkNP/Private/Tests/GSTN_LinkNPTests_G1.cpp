// Automation tests of the M7 G1 follow-ups of the link plugin: the optimistic initiator accept (GSTNLink.NP.Predict.*), the role link effects
// (GSTNLink.NP.LinkEffect.*), the interact offers the registry compiles and the probe projects (GSTNLink.NP.Interact.*), and the team and kit
// extensions of the request sides (GSTNLink.NP.Request.*). No world: the extension's rules on fragments, snapshot entries and views.

#include "GSTN_LinkNPTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/GSTN_LinkKitExtension.h"
#include "GSTN_InteractTypes.h"
#include "GSTN_LinkInteractProvider.h"
#include "GSTN_LinkTags.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitResolution.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Movement/GSTN_LinkExitRequest.h"
#include "Pipeline/GSTN_HitStep_LinkRequest.h"
#include "Pipeline/GSTN_LinkHitOutcome.h"
#include "Rules/GSTN_LinkRule_Team.h"
#include "Sim/GSTN_LinkAttach.h"
#include "Sim/GSTN_LinkEffects.h"
#include "Sim/GSTN_LinkInboxKinds.h"
#include "Sim/GSTN_LinkTickExtension.h"
#include "StructUtils/InstancedStruct.h"

// One namespace per test file: the unity build merges the files of this module, so their helpers must not share a scope
namespace GSTNLinkNPTests_G1
{

using namespace GSTNLinkNPTest;

namespace
{
	// Test-only types (test builds only; no production data names them): a type whose initiator predicts (parked initiator, both roles acting,
	// the target carrying an effect), and two interact offers of other meanings at priorities around the grab's 100
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TestPredict, "Combat.Link.Type.GSTNTestOnlyNPPredict");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TestPredictA, "Combat.Link.Role.GSTNTestOnlyNPPredictA");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TestPredictB, "Combat.Link.Role.GSTNTestOnlyNPPredictB");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TestOfferLow, "Combat.Link.Type.GSTNTestOnlyNPOfferLow");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TestOfferHigh, "Combat.Link.Type.GSTNTestOnlyNPOfferHigh");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TestOfferA, "Combat.Link.Role.GSTNTestOnlyNPOfferA");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TestOfferB, "Combat.Link.Role.GSTNTestOnlyNPOfferB");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TestMeaningLow, "Interact.Target.GSTNTestOnlyNPLow");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TestMeaningHigh, "Interact.Target.GSTNTestOnlyNPHigh");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TestMagnitude, "Combat.Link.GSTNTestOnlyNPMagnitude");

	constexpr int32 SelfId = 21;
	constexpr int32 TargetId = 22;
	constexpr int32 OtherId = 23;
	constexpr int32 HitFrame = 400;

	UGSTN_LinkTypeDefinition* NewDefinition(const FGameplayTag& Type, const FGameplayTag& RoleA, const FGameplayTag& RoleB)
	{
		UGSTN_LinkTypeDefinition* Definition = NewObject<UGSTN_LinkTypeDefinition>(GetTransientPackage());
		Definition->AddToRoot();
		Definition->LinkType = Type;
		Definition->InitiatorRole.RoleTag = RoleA;
		Definition->TargetRole.RoleTag = RoleB;
		return Definition;
	}

	/** The predicting type (and the test grab and latch of the fixture) in the global registry for the scope */
	struct FScopedPredictType
	{
		FScopedLinkTypes Types;
		UGSTN_LinkTypeDefinition* Predict = nullptr;
		uint8 Index = 0;

		FScopedPredictType()
		{
			Predict = NewDefinition(TestPredict, TestPredictA, TestPredictB);
			Predict->InitiatorRole.bParked = true;
			Predict->InitiatorRole.bActsWhileLinked = true;
			Predict->TargetRole.bActsWhileLinked = true;
			Predict->TargetRole.LinkEffect.Effect = FSoftClassPath(UGSTN_GE_LinkExit::StaticClass());
			Predict->TargetRole.LinkEffect.SetByCallerTag = TestMagnitude;
			Predict->bInitiatorPredicts = true;
			Predict->PendingFrames = 2;
			UGSTN_LinkRegistry::Get()->AddDefinition(Predict);
			Index = UGSTN_LinkRegistry::Get()->GetTypeIndex(TestPredict);
		}

		~FScopedPredictType()
		{
			UGSTN_LinkRegistry::Get()->RemoveDefinition(Predict);
			Predict->RemoveFromRoot();
		}
	};

	/** The two offered types: Low (priority 40, ally-only team rule) and High (priority 200, any team); every kit is offered both */
	struct FScopedOffers
	{
		UGSTN_LinkTypeDefinition* Low = nullptr;
		UGSTN_LinkTypeDefinition* High = nullptr;

		FScopedOffers()
		{
			Low = NewDefinition(TestOfferLow, TestOfferA, TestOfferB);
			Low->InteractOffer.Meaning = TestMeaningLow;
			Low->InteractOffer.Priority = 40;
			Low->InteractOffer.ReachCm = 150.f;
			Low->Rules.Add(NewObject<UGSTN_LinkRule_Team>(Low));
			High = NewDefinition(TestOfferHigh, TestOfferA, TestOfferB);
			High->InteractOffer.Meaning = TestMeaningHigh;
			High->InteractOffer.Priority = 200;
			UGSTN_LinkRegistry::Get()->AddDefinition(Low);
			UGSTN_LinkRegistry::Get()->AddDefinition(High);
		}

		~FScopedOffers()
		{
			UGSTN_LinkRegistry::Get()->RemoveDefinition(Low);
			UGSTN_LinkRegistry::Get()->RemoveDefinition(High);
			Low->RemoveFromRoot();
			High->RemoveFromRoot();
		}
	};

	/** A predicted request of the predicting type from SelfId to TargetId at HitFrame, on an initiator that carried Serial before */
	FGSTN_LinkFragment Requested(uint8 Serial = 4)
	{
		FGSTN_LinkFragment Fragment;
		Fragment.Links.Body.Serial = Serial;
		FGSTN_LinkTickExtension::MarkPredicted(Fragment, *UGSTN_LinkRegistry::Get(), TestPredict, TargetId, HitFrame);
		return Fragment;
	}

	/** The target's snapshot extension as the initiator reads it (end of the previous frame) */
	FGSTN_LinkSnapshotExt TargetExtOf(const FGSTN_LinkFragment& Target)
	{
		FGSTN_LinkSnapshotExt Ext;
		Ext.SetFromFragment(Target);
		return Ext;
	}

	bool Accept(FGSTN_LinkFragment& Fragment, int32 Frame, const FGSTN_LinkSnapshotExt* Target)
	{
		return FGSTN_LinkTickExtension::StepOptimisticAccept(Fragment, SelfId, Frame, UGSTN_LinkRegistry::Get(),
			[Target](int32 Id) { return Id == TargetId ? Target : nullptr; });
	}

	FGSTN_LinkFragment Linked(uint8 TypeIndex, uint8 Role, int32 Partner, uint8 Serial, int32 LinkFrame, uint8 Payload = 0)
	{
		FGSTN_LinkFragment Fragment = MakeLinked(TypeIndex, Role, Partner, Serial, LinkFrame);
		Fragment.Links.Body.Payload = Payload;
		return Fragment;
	}

	/** A snapshot entry with hurtboxes (the probe skips an entity without), a team and a body index */
	FGSTN_SnapshotEntry ProbeEntry(int32 Id, const FVector& Location, uint8 Team, uint16 BodyIndex, const UGSTN_HurtboxSetDefinition* Hurtboxes)
	{
		FGSTN_SnapshotEntry Entry = MakeEntry(Id, FTransform(Location), BodyIndex, nullptr);
		Entry.Team = FGenericTeamId(Team);
		Entry.Hurtboxes = Hurtboxes;
		Entry.BoundRadius = 30.f;
		Entry.Reaction.WeightScaleQ = 64;
		return Entry;
	}
}

// ---- Optimistic accept

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkPredictAcceptTest, "GSTNLink.NP.Predict.OptimisticAcceptAtPendingFrames", Flags)
bool FGSTN_LinkPredictAcceptTest::RunTest(const FString& Parameters)
{
	FScopedPredictType Scope;
	const FGSTN_LinkSnapshotExt IdleTarget = TargetExtOf(FGSTN_LinkFragment());

	FGSTN_LinkFragment Fragment = Requested(4);
	TestTrue(TEXT("F: pending, predicted"), Fragment.Links.Body.Phase == EGSTN_LinkPhase::Pending && Fragment.Links.Body.HasFlag(GSTNLinkFlags::Predicted));
	TestFalse(TEXT("F+1: one frame is not PendingFrames"), Accept(Fragment, HitFrame + 1, &IdleTarget));
	TestTrue(TEXT("F+1: still pending"), Fragment.Links.Body.Phase == EGSTN_LinkPhase::Pending);
	TestFalse(TEXT("F+2 without the target in the snapshot: waits"), Accept(Fragment, HitFrame + 2, nullptr));

	TestTrue(TEXT("F+2: accepted"), Accept(Fragment, HitFrame + 2, &IdleTarget));
	const FGSTN_LinkState& Body = Fragment.Links.Body;
	TestTrue(TEXT("active role 1 on the target"), Body.IsActive() && Body.Role == 1 && Body.PartnerId.Id == TargetId);
	TestTrue(TEXT("the Predicted flag stays"), Body.HasFlag(GSTNLinkFlags::Predicted));
	TestEqual(TEXT("the serial the target will form"), static_cast<int32>(Body.Serial), static_cast<int32>(FGSTN_LinkState::NextSerial(0, 4)));
	TestEqual(TEXT("the link frame the target will form on"), Body.LinkFrame, HitFrame + 1);
	TestTrue(TEXT("parked from the accept on (the simulation's own edge)"), FGSTN_LinkTickExtension::IsParked(&Fragment, UGSTN_LinkRegistry::Get()));
	TestFalse(TEXT("idempotent: a re-simulated accept changes nothing"), Accept(Fragment, HitFrame + 3, &IdleTarget));

	// The authority formed the same link from the response: equal but for the flag, which is not a misprediction
	FGSTN_LinkFragment Authority = Linked(Scope.Index, 1, TargetId, Body.Serial, HitFrame + 1);
	TestFalse(TEXT("no reconcile against the confirmed link"), Fragment.ShouldReconcile(Authority));
	Authority.Links.Body.Serial = static_cast<uint8>(Body.Serial + 1);
	TestTrue(TEXT("a mispredicted serial reconciles"), Fragment.ShouldReconcile(Authority));

	// A target that already holds role 2 with this entity (a machine that simulates it) gives its own serial and frame
	FGSTN_LinkFragment Fresh = Requested(4);
	const FGSTN_LinkSnapshotExt Formed = TargetExtOf(Linked(Scope.Index, 2, SelfId, 9, HitFrame + 1));
	TestTrue(TEXT("accepted on a formed target"), Accept(Fresh, HitFrame + 2, &Formed));
	TestTrue(TEXT("its serial and link frame"), Fresh.Links.Body.Serial == 9 && Fresh.Links.Body.LinkFrame == HitFrame + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkPredictRefusalTest, "GSTNLink.NP.Predict.RefusalRollsBackToIdle", Flags)
bool FGSTN_LinkPredictRefusalTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkFragmentRegistration::Register();
	FScopedPredictType Scope;

	// The target holds a link with someone else at F+1 (a second initiator came first): the request is cleared as a refusal, the serial kept
	FGSTN_LinkFragment Fragment = Requested(7);
	const FGSTN_LinkSnapshotExt Busy = TargetExtOf(Linked(Scope.Index, 2, OtherId, 3, HitFrame));
	TestTrue(TEXT("changed"), Accept(Fragment, HitFrame + 2, &Busy));
	TestTrue(TEXT("idle again"), Fragment.Links.Body.IsIdle());
	TestEqual(TEXT("serial kept"), static_cast<int32>(Fragment.Links.Body.Serial), 7);

	// Accepted optimistically, then the authority's state says nothing formed (the target refused on a disagreement): one rollback to idle
	FGSTN_LinkFragment Optimistic = Requested(7);
	const FGSTN_LinkSnapshotExt Idle = TargetExtOf(FGSTN_LinkFragment());
	Accept(Optimistic, HitFrame + 2, &Idle);
	FGSTN_LinkFragment Authority;
	Authority.Links.Body.Serial = 7;
	TestTrue(TEXT("the authority's idle state reconciles the optimistic link"), Optimistic.ShouldReconcile(Authority));

	// A refused response (a machine that simulates the target, with PendingFrames below 2) clears the optimistic link too; like every reset it
	// keeps the serial the state carried (the predicted one), so a late entry of that link stays old
	FGSTN_LinkResponseEntry Refused;
	Refused.bAccepted = false;
	Refused.LinkTypeIndex = Scope.Index;
	const uint8 PredictedSerial = Optimistic.Links.Body.Serial;
	TestTrue(TEXT("a refusal clears the optimistic accept"), FGSTN_LinkInboxHandlers::ApplyResponse(Optimistic, Refused, TargetId));
	TestTrue(TEXT("cleared: idle, the predicted serial kept"), Optimistic.Links.Body.IsIdle() && Optimistic.Links.Body.Serial == PredictedSerial);

	// The link effect of the role that was optimistically active is removed on the rollback frame
	FGSTN_LinkFragment Before = Requested(7);
	Accept(Before, HitFrame + 2, &Idle);
	TArray<FGSTN_LinkEffects::FOp, TInlineAllocator<2>> Ops;
	FGSTN_LinkEffects::ComputeOps(&Before, &Authority, UGSTN_LinkRegistry::Get(), Ops);
	TestEqual(TEXT("the initiator's role carries no effect: nothing to remove"), Ops.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkPredictConfirmTest, "GSTNLink.NP.Predict.ConfirmClearsFlag", Flags)
bool FGSTN_LinkPredictConfirmTest::RunTest(const FString& Parameters)
{
	FScopedPredictType Scope;
	const FGSTN_LinkSnapshotExt Idle = TargetExtOf(FGSTN_LinkFragment());

	FGSTN_LinkFragment Fragment = Requested(4);
	Fragment.Links.Body.Payload = 51;
	Accept(Fragment, HitFrame + 2, &Idle);

	// The real response (same serial): the flag goes, the response's payload and frame are kept
	FGSTN_LinkResponseEntry Response;
	Response.bAccepted = true;
	Response.LinkTypeIndex = Scope.Index;
	Response.Serial = Fragment.Links.Body.Serial;
	Response.LinkFrame = HitFrame + 1;
	Response.Payload = 51;
	TestTrue(TEXT("the response confirms"), FGSTN_LinkInboxHandlers::ApplyResponse(Fragment, Response, TargetId));
	TestFalse(TEXT("flag cleared"), Fragment.Links.Body.HasFlag(GSTNLinkFlags::Predicted));
	TestTrue(TEXT("equal to the authority's link"), Fragment == Linked(Scope.Index, 1, TargetId, Response.Serial, HitFrame + 1, 51));
	TestFalse(TEXT("a second arrival changes nothing"), FGSTN_LinkInboxHandlers::ApplyResponse(Fragment, Response, TargetId));

	// A response whose serial differs from the predicted one corrects it
	FGSTN_LinkFragment Wrong = Requested(4);
	Accept(Wrong, HitFrame + 2, &Idle);
	Response.Serial = 77;
	TestTrue(TEXT("a different serial replaces the optimistic link"), FGSTN_LinkInboxHandlers::ApplyResponse(Wrong, Response, TargetId));
	TestTrue(TEXT("the response's serial, no flag"), Wrong.Links.Body.Serial == 77 && !Wrong.Links.Body.HasFlag(GSTNLinkFlags::Predicted));

	// The link effect does not flicker on the confirmation: same type, role and serial
	FGSTN_LinkFragment TargetBefore = Linked(Scope.Index, 2, SelfId, 5, HitFrame + 1, 51);
	TArray<FGSTN_LinkEffects::FOp, TInlineAllocator<2>> Ops;
	FGSTN_LinkEffects::ComputeOps(&TargetBefore, &TargetBefore, UGSTN_LinkRegistry::Get(), Ops);
	TestEqual(TEXT("no effect operation while the link holds"), Ops.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkPredictGrabUnchangedTest, "GSTNLink.NP.Predict.GrabUnchanged", Flags)
bool FGSTN_LinkPredictGrabUnchangedTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkFragmentRegistration::Register();
	FScopedLinkTypes Types;
	const FGSTN_LinkSnapshotExt Idle = TargetExtOf(FGSTN_LinkFragment());

	// A grab and a latch (no bInitiatorPredicts) keep their M6 sequence frame by frame: Pending until the response or the 12-frame timeout
	for (const FGameplayTag& Type : { TestGrab.GetTag(), TestLatch.GetTag() })
	{
		FGSTN_LinkFragment Fragment;
		FGSTN_LinkTickExtension::MarkPredicted(Fragment, *UGSTN_LinkRegistry::Get(), Type, TargetId, HitFrame);
		const FGSTN_LinkFragment AtRequest = Fragment;
		const int32 Timeout = 12;
		for (int32 Frame = HitFrame + 1; Frame <= HitFrame + Timeout; ++Frame)
		{
			const FGSTN_LinkFragment Before = Fragment;
			TestFalse(FString::Printf(TEXT("%s frame +%d: no optimistic accept"), *Type.ToString(), Frame - HitFrame), Accept(Fragment, Frame, &Idle));
			FGSTN_LinkTickExtension::StepLinkTick(Fragment, Frame, Timeout);
			if (Frame < HitFrame + Timeout)
			{
				TestEqual(FString::Printf(TEXT("%s frame +%d: the pending request, bytes unchanged"), *Type.ToString(), Frame - HitFrame), PayloadBytes(Fragment), PayloadBytes(AtRequest));
				TestTrue(FString::Printf(TEXT("%s frame +%d: state unchanged"), *Type.ToString(), Frame - HitFrame), Fragment == Before);
			}
		}
		TestTrue(FString::Printf(TEXT("%s: cleared by the timeout"), *Type.ToString()), Fragment.Links.Body.IsIdle());

		// The response path of M6, the payload 0 of a rule that writes none
		FGSTN_LinkFragment Responded;
		FGSTN_LinkTickExtension::MarkPredicted(Responded, *UGSTN_LinkRegistry::Get(), Type, TargetId, HitFrame);
		FGSTN_LinkResponseEntry Response;
		Response.bAccepted = true;
		Response.LinkTypeIndex = UGSTN_LinkRegistry::Get()->GetTypeIndex(Type);
		Response.Serial = 1;
		Response.LinkFrame = HitFrame + 1;
		FGSTN_LinkInboxHandlers::ApplyResponse(Responded, Response, TargetId);
		TestTrue(FString::Printf(TEXT("%s: the M6 formed state"), *Type.ToString()), Responded == Linked(Response.LinkTypeIndex, 1, TargetId, 1, HitFrame + 1));
	}
	return true;
}

// ---- Link effects

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkEffectAppliedTest, "GSTNLink.NP.LinkEffect.AppliedOnActive", Flags)
bool FGSTN_LinkEffectAppliedTest::RunTest(const FString& Parameters)
{
	FScopedPredictType Scope;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	TArray<FGSTN_LinkEffects::FOp, TInlineAllocator<2>> Ops;

	// The target role carries the effect: applied on the frame its link becomes Active, with the link's payload
	const FGSTN_LinkFragment Target = Linked(Scope.Index, 2, SelfId, 5, HitFrame + 1, 128);
	FGSTN_LinkEffects::ComputeOps(nullptr, &Target, Registry, Ops);
	TestTrue(TEXT("one apply, the target's role and payload"), Ops.Num() == 1 && Ops[0].bApply && Ops[0].Role == 2 && Ops[0].Payload == 128
		&& Ops[0].Channel == EGSTN_LinkChannel::Body && Ops[0].LinkTypeIndex == Scope.Index);
	FGSTN_LinkEffects::ComputeOps(&Target, &Target, Registry, Ops);
	TestEqual(TEXT("nothing while it holds"), Ops.Num(), 0);

	// The initiator's role names none: nothing, optimistic accept included
	const FGSTN_LinkFragment Initiator = Linked(Scope.Index, 1, TargetId, 5, HitFrame + 1);
	FGSTN_LinkEffects::ComputeOps(nullptr, &Initiator, Registry, Ops);
	TestEqual(TEXT("a role without an effect"), Ops.Num(), 0);
	FGSTN_LinkFragment Pending = Requested();
	FGSTN_LinkEffects::ComputeOps(nullptr, &Pending, Registry, Ops);
	TestEqual(TEXT("a pending request applies nothing"), Ops.Num(), 0);

	// A grab of M6 carries no effect: the projection of a grab is unchanged
	FScopedLinkTypes Types;
	const FGSTN_LinkFragment Grabbed = Linked(Types.GrabIndex, 2, SelfId, 5, HitFrame);
	FGSTN_LinkEffects::ComputeOps(nullptr, &Grabbed, Registry, Ops);
	TestEqual(TEXT("grab: no effect"), Ops.Num(), 0);

	// The class resolves to the gameplay effect the definition names
	TestTrue(TEXT("effect class resolves"), FGSTN_LinkEffects::ResolveEffectClass(Scope.Predict->TargetRole.LinkEffect) == UGSTN_GE_LinkExit::StaticClass());
	TestTrue(TEXT("an empty definition resolves to nothing"), FGSTN_LinkEffects::ResolveEffectClass(FGSTN_LinkEffectDef()) == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkEffectRemovedTest, "GSTNLink.NP.LinkEffect.RemovedOnEveryBreakReason", Flags)
bool FGSTN_LinkEffectRemovedTest::RunTest(const FString& Parameters)
{
	FScopedPredictType Scope;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const FGSTN_LinkFragment Live = Linked(Scope.Index, 2, SelfId, 5, HitFrame + 1, 200);
	TArray<FGSTN_LinkEffects::FOp, TInlineAllocator<2>> Ops;

	// Every generic reason and a plugin's: the state resets with the reason (watchdog commands, throws, outcome keys); the effect goes
	for (int32 Reason = 1; Reason <= static_cast<int32>(EGSTN_LinkBreakReason::FirstPlugin); ++Reason)
	{
		FGSTN_LinkFragment Ended = Live;
		Ended.Links.Body.Reset();
		Ended.Links.Body.BreakReason = static_cast<uint8>(Reason);
		FGSTN_LinkEffects::ComputeOps(&Live, &Ended, Registry, Ops);
		TestTrue(FString::Printf(TEXT("reason %d: one removal of the role's effect"), Reason), Ops.Num() == 1 && !Ops[0].bApply && Ops[0].Role == 2
			&& Ops[0].LinkTypeIndex == Scope.Index);
	}

	// The exit phase is not Active any more: removed there already
	FGSTN_LinkFragment Exiting = Live;
	Exiting.Links.Body.Phase = EGSTN_LinkPhase::Exiting;
	FGSTN_LinkEffects::ComputeOps(&Live, &Exiting, Registry, Ops);
	TestTrue(TEXT("exiting: removed"), Ops.Num() == 1 && !Ops[0].bApply);

	// The fragment gone (an entity whose fragment was dropped): removed too
	FGSTN_LinkEffects::ComputeOps(&Live, nullptr, Registry, Ops);
	TestTrue(TEXT("no fragment: removed"), Ops.Num() == 1 && !Ops[0].bApply);

	// A new link of another serial on the same frame: the old one removed first, then the new one applied
	FGSTN_LinkFragment Next = Linked(Scope.Index, 2, OtherId, 6, HitFrame + 9, 10);
	FGSTN_LinkEffects::ComputeOps(&Live, &Next, Registry, Ops);
	TestTrue(TEXT("remove then apply"), Ops.Num() == 2 && !Ops[0].bApply && Ops[1].bApply && Ops[1].Payload == 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkEffectResimTest, "GSTNLink.NP.LinkEffect.IdempotentOnResim", Flags)
bool FGSTN_LinkEffectResimTest::RunTest(const FString& Parameters)
{
	FScopedPredictType Scope;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();

	// A re-simulated frame starts from the restored previous fragment and writes the same one: the same operations, every time
	const FGSTN_LinkFragment Idle;
	const FGSTN_LinkFragment Formed = Linked(Scope.Index, 2, SelfId, 5, HitFrame + 1, 64);
	FGSTN_LinkFragment Ended = Formed;
	Ended.Links.Body.Reset();
	const TArray<TPair<const FGSTN_LinkFragment*, const FGSTN_LinkFragment*>> Frames = { { &Idle, &Formed }, { &Formed, &Formed }, { &Formed, &Ended }, { &Ended, &Ended } };
	TArray<int32> FirstPass;
	for (int32 Pass = 0; Pass < 3; ++Pass)
	{
		TArray<int32> Signature;
		for (const TPair<const FGSTN_LinkFragment*, const FGSTN_LinkFragment*>& Step : Frames)
		{
			TArray<FGSTN_LinkEffects::FOp, TInlineAllocator<2>> Ops;
			FGSTN_LinkEffects::ComputeOps(Step.Key, Step.Value, Registry, Ops);
			for (const FGSTN_LinkEffects::FOp& Op : Ops)
			{
				Signature.Add((Op.bApply ? 1000 : 0) + Op.Role * 100 + Op.Payload);
			}
			Signature.Add(-1);
		}
		if (Pass == 0)
		{
			FirstPass = Signature;
			TestTrue(TEXT("apply on formation, nothing while held, remove on the end, nothing after"), Signature == TArray<int32>({ 1000 + 264, -1, -1, 264, -1, -1 }));
		}
		else
		{
			TestTrue(FString::Printf(TEXT("pass %d equals the first"), Pass), Signature == FirstPass);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkEffectMagnitudeTest, "GSTNLink.NP.LinkEffect.PayloadToMagnitude", Flags)
bool FGSTN_LinkEffectMagnitudeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("0 -> 0"), FGSTN_LinkEffects::Magnitude(0), 0.f);
	TestEqual(TEXT("255 -> 1"), FGSTN_LinkEffects::Magnitude(255), 1.f);
	TestTrue(TEXT("128 -> 128/255"), FMath::IsNearlyEqual(FGSTN_LinkEffects::Magnitude(128), 128.f / 255.f));
	for (int32 Payload = 1; Payload <= 255; ++Payload)
	{
		if (FGSTN_LinkEffects::Magnitude(static_cast<uint8>(Payload)) <= FGSTN_LinkEffects::Magnitude(static_cast<uint8>(Payload - 1)))
		{
			AddError(FString::Printf(TEXT("magnitude not increasing at %d"), Payload));
			break;
		}
	}
	return true;
}

// ---- Interact offers

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkInteractSecondOfferTest, "GSTNLink.NP.Interact.SecondOfferProjectsItsMeaning", Flags)
bool FGSTN_LinkInteractSecondOfferTest::RunTest(const FString& Parameters)
{
	FScopedOffers Offers;
	// High (priority 200, any team) is disabled for this test by a lock on every candidate it could take; Low is ally-only
	Offers.High->RequestRequirements.RequireTags.AddTag(GSTNLinkTags::Immunity_Latch);
	UGSTN_LinkRegistry::Get()->RebuildFromAssetManager();
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();

	const FGSTN_InteractMeaningTable& Meanings = Registry->GetInteractMeanings();
	TestTrue(TEXT("the registry's meanings hold the new meaning"), Meanings.IndexOf(TestMeaningLow) != 0);
	TestEqual(TEXT("at its offer's priority"), Meanings.DefaultPriorityOf(Meanings.IndexOf(TestMeaningLow)), 40);

	// A kit without a link extension: no grab offer, the two test offers (they require no kit extension)
	FGSTN_KitExtensionList NoLinkKit;
	const uint16 BodyIndex = FGSTN_LinkAttach::AddForTest({}, nullptr, &NoLinkKit);
	TArray<FGSTN_LinkInteractOffer, TInlineAllocator<4>> Made;
	FGSTN_LinkTickExtension::MakeOffersFor(*Registry, FGSTN_LinkAttach::At(BodyIndex), Made);
	TestFalse(TEXT("no grab offer for a kit without a link extension"), Made.ContainsByPredicate([](const FGSTN_LinkInteractOffer& Offer) { return Offer.Meaning == GSTNLinkTags::Interact_Target_Grab; }));
	TestTrue(TEXT("the low offer is there"), Made.ContainsByPredicate([&Offers](const FGSTN_LinkInteractOffer& Offer) { return Offer.Definition == Offers.Low && Offer.ReachCm == 150.f; }));

	UGSTN_HurtboxSetDefinition* Hurtboxes = NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage());
	FGCObjectScopeGuard Guard(Hurtboxes);
	const FGSTN_SnapshotEntry Self = ProbeEntry(SelfId, FVector::ZeroVector, 1, BodyIndex, Hurtboxes);
	const FGSTN_LinkTickExtension Extension;

	// An ally in front: offered by Low, projected under Low's meaning
	TArray<FGSTN_SnapshotEntry> Entries = { Self, ProbeEntry(TargetId, FVector(80.f, 0.f, 0.f), 1, 0, Hurtboxes) };
	TestEqual(TEXT("the ally projects the second offer's meaning"), Extension.ResolveInteractTarget(Self, Entries, HitFrame, FVector::ZeroVector), TestMeaningLow.GetTag());

	// An enemy at the same place: the ally-only rule refuses it in the pre-filter, so nothing is projected
	Entries[1] = ProbeEntry(TargetId, FVector(80.f, 0.f, 0.f), 2, 0, Hurtboxes);
	TestFalse(TEXT("the enemy projects nothing"), Extension.ResolveInteractTarget(Self, Entries, HitFrame, FVector::ZeroVector).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkInteractPriorityTest, "GSTNLink.NP.Interact.OfferPriorityOrder", Flags)
bool FGSTN_LinkInteractPriorityTest::RunTest(const FString& Parameters)
{
	FScopedOffers Offers;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();

	// Registry order: priority, higher first, then the type tag's name
	const TConstArrayView<FGSTN_LinkOfferEntry> Entries = Registry->GetInteractOffers();
	for (int32 Index = 1; Index < Entries.Num(); ++Index)
	{
		const FGSTN_LinkOfferEntry& A = Entries[Index - 1];
		const FGSTN_LinkOfferEntry& B = Entries[Index];
		TestTrue(FString::Printf(TEXT("offer %d is not after offer %d"), Index - 1, Index), A.Offer.Priority > B.Offer.Priority
			|| (A.Offer.Priority == B.Offer.Priority && A.Definition->LinkType.GetTagName().ToString().Compare(B.Definition->LinkType.GetTagName().ToString(), ESearchCase::CaseSensitive) < 0));
	}
	const int32 HighAt = Entries.IndexOfByPredicate([&Offers](const FGSTN_LinkOfferEntry& Entry) { return Entry.Definition == Offers.High; });
	const int32 LowAt = Entries.IndexOfByPredicate([&Offers](const FGSTN_LinkOfferEntry& Entry) { return Entry.Definition == Offers.Low; });
	TestTrue(TEXT("both test offers registered, high before low"), HighAt != INDEX_NONE && LowAt != INDEX_NONE && HighAt < LowAt);
	const int32 GrabAt = Entries.IndexOfByPredicate([](const FGSTN_LinkOfferEntry& Entry) { return Entry.Definition->LinkType == GSTNLinkTags::Type_Grab; });
	if (GrabAt != INDEX_NONE)
	{
		TestTrue(TEXT("the grab (100) sits between them"), HighAt < GrabAt && GrabAt < LowAt);
		TestEqual(TEXT("the grab's meaning priority stays the M6 default 100"), Registry->GetInteractMeanings().DefaultPriorityOf(
			Registry->GetInteractMeanings().IndexOf(GSTNLinkTags::Interact_Target_Grab)), 100);
	}

	// Per kit: the offers follow the registry order; a type's alternatives come right before it under its meaning
	FScopedLinkTypes Types;
	Offers.High->Alternatives.Add(TestLatch);
	FGSTN_LinkKitExtension LinkKit;
	TArray<FGSTN_LinkInteractOffer, TInlineAllocator<4>> Made;
	FGSTN_LinkInteractProvider::MakeOffers(*Registry, nullptr, &LinkKit, Made);
	const int32 MadeHigh = Made.IndexOfByPredicate([&Offers](const FGSTN_LinkInteractOffer& Offer) { return Offer.Definition == Offers.High; });
	const int32 MadeLow = Made.IndexOfByPredicate([&Offers](const FGSTN_LinkInteractOffer& Offer) { return Offer.Definition == Offers.Low; });
	TestTrue(TEXT("high's alternative right before it, same meaning"), MadeHigh > 0 && Made[MadeHigh - 1].Definition == Types.Latch && Made[MadeHigh - 1].Meaning == TestMeaningHigh);
	TestTrue(TEXT("high before low"), MadeHigh != INDEX_NONE && MadeLow != INDEX_NONE && MadeHigh < MadeLow);
	Offers.High->Alternatives.Reset();

	// A required kit extension the kit does not carry removes the offer
	Offers.Low->InteractOffer.RequiredKitExtension = FGSTN_LinkKitExtension::StaticStruct();
	UGSTN_LinkRegistry::Get()->RebuildFromAssetManager();
	FGSTN_KitExtensionList NoLinkKit;
	Made.Reset();
	FGSTN_LinkInteractProvider::MakeOffers(*Registry, &NoLinkKit, nullptr, Made);
	TestFalse(TEXT("required extension missing: not offered"), Made.ContainsByPredicate([&Offers](const FGSTN_LinkInteractOffer& Offer) { return Offer.Definition == Offers.Low; }));
	NoLinkKit.ExtensionData.Add(FInstancedStruct::Make(FGSTN_LinkKitExtension()));
	Made.Reset();
	FGSTN_LinkInteractProvider::MakeOffers(*Registry, &NoLinkKit, nullptr, Made);
	TestTrue(TEXT("carried in the kit's array: offered"), Made.ContainsByPredicate([&Offers](const FGSTN_LinkInteractOffer& Offer) { return Offer.Definition == Offers.Low; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkInteractAllyOnlyTest, "GSTNLink.NP.Interact.AllyOnlyRulePreFilter", Flags)
bool FGSTN_LinkInteractAllyOnlyTest::RunTest(const FString& Parameters)
{
	FScopedOffers Offers;
	FGSTN_LinkInteractOffer Offer;
	Offer.Definition = Offers.Low;
	Offer.Meaning = TestMeaningLow;
	const FGSTN_LinkInteractOffer Made[] = { Offer };

	FGSTN_LinkInteractSource Source;
	Source.Interactor.EntityId = SelfId;
	Source.Interactor.TeamId = 1;
	Source.Offers = Made;
	Source.Registry = UGSTN_LinkRegistry::Get();

	auto Decide = [&Source](uint8 Team, EGSTN_LinkRefusal& OutRefusal)
	{
		FGSTN_LinkInteractEntity Entity;
		Entity.Side.EntityId = TargetId;
		Entity.Side.TeamId = Team;
		FGSTN_LinkRuleResult Refusal;
		const int32 Index = FGSTN_LinkInteractProvider::FindAcceptedOffer(Source, Entity, HitFrame, &Refusal);
		OutRefusal = Refusal.Refusal;
		return Index;
	};
	EGSTN_LinkRefusal Refusal = EGSTN_LinkRefusal::None;
	TestEqual(TEXT("an ally is offered"), Decide(1, Refusal), 0);
	TestEqual(TEXT("an enemy is not"), Decide(2, Refusal), static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("refused for Team"), Refusal == EGSTN_LinkRefusal::Team);
	TestEqual(TEXT("an entity without a team is nobody's ally"), Decide(255, Refusal), static_cast<int32>(INDEX_NONE));
	Source.Interactor.TeamId = 255;
	TestEqual(TEXT("nor is a teamless interactor"), Decide(255, Refusal), static_cast<int32>(INDEX_NONE));

	// The same rule on the hit path decides the same from the hit's teams
	UGSTN_LinkRule_Team* Rule = CastChecked<UGSTN_LinkRule_Team>(Offers.Low->Rules[0]);
	FGSTN_LinkRequestContext Context;
	Context.Definition = Offers.Low;
	Context.Initiator.EntityId = SelfId;
	Context.Initiator.TeamId = 3;
	Context.Target.EntityId = TargetId;
	Context.Target.TeamId = 3;
	TestTrue(TEXT("allies accepted"), Rule->Evaluate(Context).bAccepted);
	Rule->Requirement = EGSTN_LinkTeamRequirement::EnemiesOnly;
	TestTrue(TEXT("enemies-only refuses allies"), Rule->Evaluate(Context).Refusal == EGSTN_LinkRefusal::Team);
	Rule->Requirement = EGSTN_LinkTeamRequirement::Any;
	TestTrue(TEXT("any accepts"), Rule->Evaluate(Context).bAccepted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkRequestSideTest, "GSTNLink.NP.Request.SideCarriesTeamAndKit", Flags)
bool FGSTN_LinkRequestSideTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;

	// Each side's kit: an extension array holding a link extension (as a kit asset does)
	FGSTN_KitExtensionList InitiatorKit;
	FGSTN_LinkKitExtension InitiatorLink;
	InitiatorLink.GrabRules.MaxGrabWeight = 321.f;
	InitiatorKit.ExtensionData.Add(FInstancedStruct::Make(InitiatorLink));
	FGSTN_KitExtensionList TargetKit;
	const uint16 InitiatorBody = FGSTN_LinkAttach::AddForTest({}, nullptr, &InitiatorKit);
	const uint16 TargetBody = FGSTN_LinkAttach::AddForTest({}, nullptr, &TargetKit);

	// The hit path: the teams of the hit and of the victim, the kits of both bodies
	FGSTN_LinkSnapshotExt InitiatorExt;
	InitiatorExt.BodyIndex = InitiatorBody;
	FGSTN_LinkSnapshotExt TargetExt;
	TargetExt.BodyIndex = TargetBody;
	FGSTN_HitContextView View;
	View.Incoming.Hit.HitType = GSTNCombatHitTags::HitType_Grab;
	View.Incoming.Hit.LinkType = TestGrab;
	View.Incoming.Frame = HitFrame;
	View.Incoming.AttackerEntityId = SelfId;
	View.Incoming.AttackerTeamId = 4;
	View.Victim.EntityId = TargetId;
	View.Victim.TeamId = 6;
	FGSTN_HitVictimView::FSnapshotExtensionRef Ref;
	Ref.TypeId = GSTNFeatureIds::Link;
	Ref.Data = &TargetExt;
	View.Victim.SetSnapshotExtensions(&Ref, 1);
	View.Incoming.AttackerExtensions[0].TypeId = GSTNFeatureIds::Link;
	View.Incoming.AttackerExtensions[0].Data = &InitiatorExt;
	FGSTN_LinkRequestContext Context;
	FGSTN_LinkStepUtils::MakeRequestContext(View, Context);
	TestEqual(TEXT("initiator team from the hit"), static_cast<int32>(Context.Initiator.TeamId), 4);
	TestEqual(TEXT("target team from the victim"), static_cast<int32>(Context.Target.TeamId), 6);
	TestTrue(TEXT("initiator kit array"), Context.Initiator.KitExtensions == &InitiatorKit);
	TestTrue(TEXT("target kit array"), Context.Target.KitExtensions == &TargetKit);
	const FGSTN_LinkKitExtension* Found = Context.Initiator.FindKitExtension<FGSTN_LinkKitExtension>();
	TestTrue(TEXT("a rule finds its kit struct by type"), Found && Found->GrabRules.MaxGrabWeight == 321.f);
	TestTrue(TEXT("the typed grab rules still come from the same kit"), Context.InitiatorGrabRules && Context.InitiatorGrabRules->MaxGrabWeight == 321.f);
	TestTrue(TEXT("a missing struct is null"), Context.Target.FindKitExtension<FGSTN_LinkKitExtension>() == nullptr);
	TestFalse(TEXT("teams 4 and 6 are not allies"), Context.Initiator.IsAllyOf(Context.Target));

	// The interact path builds the same side from the snapshot entry
	FGSTN_SnapshotEntry Entry = MakeEntry(SelfId, FTransform::Identity, InitiatorBody, nullptr);
	Entry.Team = FGenericTeamId(4);
	const FGSTN_LinkRequestSide Side = FGSTN_LinkTickExtension::MakeRequestSide(Entry);
	TestTrue(TEXT("snapshot side: team and kit"), Side.TeamId == 4 && Side.KitExtensions == &InitiatorKit && Side.EntityId == SelfId);
	return true;
}

} // namespace GSTNLinkNPTests_G1

#endif

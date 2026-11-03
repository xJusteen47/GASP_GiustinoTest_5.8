// Automation tests of the link plugin: the fake link host (plan F5b).

#include "GSTN_LinkSettings.h"
#include "GSTN_LinkTestUtils.h"
#include "Host/GSTN_FakeLinkHost.h"
#include "Misc/AutomationTest.h"
#include "Rules/GSTN_LinkRule_Grab.h"
#include "State/GSTN_LinkMath.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A grab type that parks the grabbed entity 50 cm ahead of the grabber, with the grab rule listed */
	UGSTN_LinkTypeDefinition* MakeHostGrabDefinition()
	{
		UGSTN_LinkTypeDefinition* Definition = GSTNLinkTest::MakeGrabDefinition();
		Definition->TargetRole.SocketOffset = FTransform(FRotator::ZeroRotator, FVector(50.0, 0.0, 0.0));
		Definition->Rules.Add(NewObject<UGSTN_LinkRule_Grab>(Definition));
		return Definition;
	}

	int32 Count(const FGSTN_FakeLinkHost& Host, const FGameplayTag& Event)
	{
		int32 Found = 0;
		for (const FGSTN_LinkEventRecord& Record : Host.GetEvents())
		{
			Found += Record.Event == Event ? 1 : 0;
		}
		return Found;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkFakeHostFormBreakTest, "GSTNLink.Core.FakeHost.FormBreakSynchronous", GSTNLinkTest::Flags)
bool FGSTN_LinkFakeHostFormBreakTest::RunTest(const FString& Parameters)
{
	UGSTN_LinkTypeDefinition* Grab = MakeHostGrabDefinition();
	FGSTN_FakeLinkHost Host;
	const uint8 GrabIndex = Host.RegisterType(*Grab);
	TestEqual(TEXT("The first type takes index 1: 0 means no link"), static_cast<int32>(GrabIndex), 1);
	TestTrue(TEXT("The index resolves to the definition"), Host.GetLinkDefinition(GrabIndex) == Grab);
	TestNull(TEXT("Index 0 resolves to nothing"), Host.GetLinkDefinition(0));

	const FGSTN_EntityRef A = Host.AddFighter(FGameplayTagContainer(), 100.f, FTransform(FRotator::ZeroRotator, FVector(0.0, 0.0, 0.0)));
	const FGSTN_EntityRef B = Host.AddFighter(FGameplayTagContainer(), 100.f, FTransform(FRotator::ZeroRotator, FVector(150.0, 0.0, 0.0)));
	const FGSTN_EntityRef C = Host.AddFighter(FGameplayTagContainer(), 100.f, FTransform(FRotator::ZeroRotator, FVector(300.0, 0.0, 0.0)));
	Host.SetFrame(10);

	// The formation is executed inside the call
	const FGSTN_LinkRuleResult Formed = Host.RequestLink(A, B, GrabIndex, true);
	TestTrue(TEXT("The grab is accepted"), Formed.bAccepted);
	FGSTN_LinkEntityView ViewA;
	FGSTN_LinkEntityView ViewB;
	TestTrue(TEXT("A is visible through the host interface"), Host.GetEntityView(A, ViewA));
	TestTrue(TEXT("B is visible through the host interface"), Host.GetEntityView(B, ViewB));
	TestEqual(TEXT("The grabber holds role 1 at once"), static_cast<int32>(ViewA.Links.Body.Role), 1);
	TestEqual(TEXT("The grabbed holds role 2 at once"), static_cast<int32>(ViewB.Links.Body.Role), 2);
	TestTrue(TEXT("Both are active"), ViewA.Links.Body.IsActive() && ViewB.Links.Body.IsActive());
	TestTrue(TEXT("They name each other"), ViewA.Links.Body.PartnerId == B && ViewB.Links.Body.PartnerId == A);
	TestEqual(TEXT("They share the serial, 1 for the first link"), static_cast<int32>(ViewA.Links.Body.Serial), 1);
	TestEqual(TEXT("... on both sides"), static_cast<int32>(ViewB.Links.Body.Serial), 1);
	TestEqual(TEXT("The link frame is the formation frame"), ViewA.Links.Body.LinkFrame, 10);
	TestEqual(TEXT("The grabber has no mash end"), ViewA.Links.Body.EndFrame, 0);
	const FGSTN_MashRule& Mash = Host.GrabRules.Mash;
	TestEqual(TEXT("The grabbed has the mash end of its damage"), ViewB.Links.Body.EndFrame, 10 + UGSTN_LinkMath::MashInitialFrames(Mash, 0.f));
	TestTrue(TEXT("The grabbed stands where the definition parks it"), ViewB.Transform.GetLocation().Equals(FVector(50.0, 0.0, 0.0), 0.001));
	TestEqual(TEXT("Only the Body slot was used"), static_cast<int32>(ViewA.Links.Hand.Phase), static_cast<int32>(EGSTN_LinkPhase::None));
	TestEqual(TEXT("LinkFormed was published once"), Count(Host, GSTNLinkTags::Event_LinkFormed), 1);

	// A second grabber on the same victim is refused in the same call
	const FGSTN_LinkRuleResult Second = Host.RequestLink(C, B, GrabIndex);
	TestFalse(TEXT("A second grab on the victim is refused"), Second.bAccepted);
	TestEqual(TEXT("Because the victim is busy"), static_cast<int32>(Second.Refusal), static_cast<int32>(EGSTN_LinkRefusal::TargetBusy));
	TestEqual(TEXT("And no second event"), Count(Host, GSTNLinkTags::Event_LinkFormed), 1);

	// A break is executed inside the call: both slots clear, the victim is placed by the exit placer, the events are out
	Host.AdvanceFrames(5);
	TestTrue(TEXT("The release is executed"), Host.RequestBreak(A, EGSTN_LinkChannel::Body, EGSTN_LinkBreakReason::Release));
	Host.GetEntityView(A, ViewA);
	Host.GetEntityView(B, ViewB);
	TestTrue(TEXT("The grabber is free at once"), ViewA.Links.Body.IsIdle());
	TestTrue(TEXT("The grabbed is free at once"), ViewB.Links.Body.IsIdle());
	TestEqual(TEXT("The serial of the ended link is kept"), static_cast<int32>(ViewB.Links.Body.Serial), 1);
	TestEqual(TEXT("LinkBroken was published once"), Count(Host, GSTNLinkTags::Event_LinkBroken), 1);
	TestEqual(TEXT("A release is no escape"), Count(Host, GSTNLinkTags::Event_GrabEscaped), 0);
	TestEqual(TEXT("The break carries its reason"), static_cast<int32>(Host.GetEvents().Last().Reason), static_cast<int32>(EGSTN_LinkBreakReason::Release));
	TestTrue(TEXT("The victim was placed on the ring around the grabber, ahead of it"), ViewB.Transform.GetLocation().Equals(FVector(120.0, 0.0, 0.0), 0.001));
	TestFalse(TEXT("A second break finds nothing to break"), Host.RequestBreak(A, EGSTN_LinkChannel::Body, EGSTN_LinkBreakReason::Release));

	// The released victim is immune for a while: a regrab is refused until the frames pass
	const FGSTN_LinkRuleResult Regrab = Host.RequestLink(A, B, GrabIndex);
	TestEqual(TEXT("An instant regrab is refused by the immunity frame"), static_cast<int32>(Regrab.Refusal), static_cast<int32>(EGSTN_LinkRefusal::Immunity));
	Host.AdvanceFrames(Host.GrabRules.ImmunityFramesAfterRelease);
	TestTrue(TEXT("After the immunity the regrab is accepted"), Host.RequestLink(A, B, GrabIndex).bAccepted);
	Host.GetEntityView(A, ViewA);
	TestEqual(TEXT("The new link has the next serial"), static_cast<int32>(ViewA.Links.Body.Serial), 2);

	// Step runs the break rules in the same call: a timeout and a mash escape
	Grab->MaxFrames = 40;
	FGSTN_FakeLinkHost Timed;
	const uint8 TimedIndex = Timed.RegisterType(*Grab);
	const FGSTN_EntityRef T1 = Timed.AddFighter(FGameplayTagContainer(), 100.f, FTransform::Identity);
	const FGSTN_EntityRef T2 = Timed.AddFighter(FGameplayTagContainer(), 100.f, FTransform(FRotator::ZeroRotator, FVector(100.0, 0.0, 0.0)));
	Timed.SetFrame(0);
	TestTrue(TEXT("The timed grab forms"), Timed.RequestLink(T1, T2, TimedIndex).bAccepted);
	Timed.SetFrame(39);
	TestEqual(TEXT("Frame 39: nothing breaks"), Timed.Step().Breaks, 0);
	Timed.SetFrame(40);
	TestEqual(TEXT("Frame 40: the timeout breaks it"), Timed.Step().Breaks, 1);
	TestEqual(TEXT("The reason is Timeout"), static_cast<int32>(Timed.GetEvents().Last().Reason), static_cast<int32>(EGSTN_LinkBreakReason::Timeout));

	Grab->MaxFrames = 0;
	FGSTN_FakeLinkHost Mashed;
	const uint8 MashedIndex = Mashed.RegisterType(*Grab);
	const FGSTN_EntityRef M1 = Mashed.AddFighter(FGameplayTagContainer(), 100.f, FTransform::Identity);
	const FGSTN_EntityRef M2 = Mashed.AddFighter(FGameplayTagContainer(), 100.f, FTransform(FRotator::ZeroRotator, FVector(100.0, 0.0, 0.0)));
	Mashed.SetFrame(0);
	Mashed.RequestLink(M1, M2, MashedIndex, true);
	const int32 EndFrame = Mashed.FindEntity(M2)->Links.Body.EndFrame;
	Mashed.SetFrame(EndFrame - 1);
	TestEqual(TEXT("One frame before the mash end: held"), Mashed.Step().Breaks, 0);
	Mashed.SetFrame(EndFrame);
	TestEqual(TEXT("On the mash end: free"), Mashed.Step().Breaks, 1);
	TestEqual(TEXT("It is an escape"), Count(Mashed, GSTNLinkTags::Event_GrabEscaped), 1);
	TestTrue(TEXT("Both slots cleared"), Mashed.FindEntity(M1)->Links.Body.IsIdle() && Mashed.FindEntity(M2)->Links.Body.IsIdle());

	// A pummel from the grabber keeps the link, a hit from a third party does not
	Grab->Break.HitBreaks.Reset();
	FGSTN_RoleHitBreak& HitBreak = Grab->Break.HitBreaks.AddDefaulted_GetRef();
	HitBreak.RoleTag = GSTNLinkTags::Role_Grabbed.GetTag();
	FGSTN_FakeLinkHost Hits;
	const uint8 HitsIndex = Hits.RegisterType(*Grab);
	const FGSTN_EntityRef H1 = Hits.AddFighter(FGameplayTagContainer(), 100.f, FTransform::Identity);
	const FGSTN_EntityRef H2 = Hits.AddFighter(FGameplayTagContainer(), 100.f, FTransform(FRotator::ZeroRotator, FVector(100.0, 0.0, 0.0)));
	Hits.RequestLink(H1, H2, HitsIndex);
	Hits.FindEntity(H2)->PendingHit = { true, true, 500.f };
	TestEqual(TEXT("The grabber's pummel keeps the grab"), Hits.Step().Breaks, 0);
	Hits.FindEntity(H2)->PendingHit = { true, false, 500.f };
	TestEqual(TEXT("A third party's hit breaks it"), Hits.Step().Breaks, 1);
	TestEqual(TEXT("With the reason Hit"), static_cast<int32>(Hits.GetEvents().Last().Reason), static_cast<int32>(EGSTN_LinkBreakReason::Hit));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkFakeHostOrphanTest, "GSTNLink.Core.FakeHost.OrphanDetected", GSTNLinkTest::Flags)
bool FGSTN_LinkFakeHostOrphanTest::RunTest(const FString& Parameters)
{
	UGSTN_LinkTypeDefinition* Grab = MakeHostGrabDefinition();
	FGSTN_FakeLinkHost Host;
	const uint8 GrabIndex = Host.RegisterType(*Grab);
	const FGSTN_EntityRef A = Host.AddFighter(FGameplayTagContainer(), 100.f, FTransform::Identity);
	const FGSTN_EntityRef B = Host.AddFighter(FGameplayTagContainer(), 100.f, FTransform(FRotator::ZeroRotator, FVector(100.0, 0.0, 0.0)));
	Host.SetFrame(20);
	TestTrue(TEXT("The grab forms"), Host.RequestLink(A, B, GrabIndex).bAccepted);

	// A healthy pair is no orphan at any frame
	Host.SetFrame(30);
	const FGSTN_FakeLinkHost::FStepResult Healthy = Host.Step();
	TestEqual(TEXT("A consistent pair has no orphan"), Healthy.Orphans, 0);
	TestEqual(TEXT("... and breaks nothing"), Healthy.Breaks, 0);

	// The grabbed side loses its state (a throw, a lost entry): the grabber is left holding a link its partner does not have
	Host.SetLinkState(B, EGSTN_LinkChannel::Body, FGSTN_LinkState());
	const int32 Grace = UGSTN_LinkSettings::Get().OrphanGraceFrames;
	TestEqual(TEXT("The grace is the settings' value, 3 by default"), Grace, 3);

	// Inside the grace after the formation the missing side may still be in flight
	Host.SetFrame(20 + Grace - 1);
	const FGSTN_FakeLinkHost::FStepResult Early = Host.Step();
	TestEqual(TEXT("Inside the grace: not an orphan yet"), Early.Orphans, 0);
	TestTrue(TEXT("The grabber still holds its link"), Host.FindEntity(A)->Links.Body.IsActive());

	Host.SetFrame(20 + Grace);
	const FGSTN_FakeLinkHost::FStepResult Late = Host.Step();
	TestEqual(TEXT("After the grace: one orphan"), Late.Orphans, 1);
	TestTrue(TEXT("The dangling side was broken"), Host.FindEntity(A)->Links.Body.IsIdle());
	TestEqual(TEXT("It was broken as Forced"), static_cast<int32>(Host.GetEvents().Last().Reason), static_cast<int32>(EGSTN_LinkBreakReason::Forced));
	TestEqual(TEXT("LinkBroken was published once"), Count(Host, GSTNLinkTags::Event_LinkBroken), 1);

	// The next step finds nothing more
	Host.AdvanceFrames(1);
	TestEqual(TEXT("A repaired world has no orphan"), Host.Step().Orphans, 0);

	// A partner that holds a DIFFERENT link (another serial) does not make the pair consistent
	FGSTN_FakeLinkHost Mixed;
	const uint8 MixedIndex = Mixed.RegisterType(*Grab);
	const FGSTN_EntityRef X = Mixed.AddFighter(FGameplayTagContainer(), 100.f, FTransform::Identity);
	const FGSTN_EntityRef Y = Mixed.AddFighter(FGameplayTagContainer(), 100.f, FTransform(FRotator::ZeroRotator, FVector(100.0, 0.0, 0.0)));
	Mixed.SetFrame(0);
	Mixed.RequestLink(X, Y, MixedIndex);
	FGSTN_LinkState Other = Mixed.FindEntity(Y)->Links.Body;
	Other.Serial = 77;
	Mixed.SetLinkState(Y, EGSTN_LinkChannel::Body, Other);
	Mixed.SetFrame(Grace);
	// X holds link 1 and Y holds link 77 to X: neither is the other end of the other's link, so both dangle
	TestEqual(TEXT("A stale serial on the partner leaves both sides dangling: two orphans"), Mixed.Step().Orphans, 2);
	TestTrue(TEXT("Both slots are clear afterwards"), Mixed.FindEntity(X)->Links.Body.IsIdle() && Mixed.FindEntity(Y)->Links.Body.IsIdle());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

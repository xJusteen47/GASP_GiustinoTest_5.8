// Automation tests of the link tick extension (plan F10, m6 spec 5.2 step 5 and 6, 7.4): a parked role that does not act vetoes every command
// (no candidates), a role that acts while linked keeps them (the latched enemy fights on, so does the latcher), and the Combat.Link.* tags are
// a function of the link state's Role and Phase only. No world: the extension's rules on a combat sync state.

#include "GSTN_LinkNPTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GSTN_LinkTags.h"
#include "Sim/GSTN_LinkTickExtension.h"
#include "Simulation/GSTN_CombatSimTypes.h"

using namespace GSTNLinkNPTest;

namespace
{
	bool BlocksWith(const FGSTN_LinkFragment& Fragment)
	{
		FGSTN_CombatSyncState In;
		FGSTN_CombatSyncState Out;
		FGSTN_SyncFragmentList::FindOrAdd<FGSTN_LinkFragment>(Out.Fragments) = Fragment;
		FGSTN_CombatTickContext Context(200, In, Out);
		const FGSTN_LinkTickExtension Extension;
		return Extension.BlocksActions(Context);
	}

	FGameplayTagContainer TagsOf(const FGSTN_LinkFragment& Fragment)
	{
		FGameplayTagContainer Tags;
		FGSTN_LinkTickExtension::GatherStateTags(&Fragment, UGSTN_LinkRegistry::Get(), Tags);
		return Tags;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkTickGrabbedNoCandidatesTest, "GSTNLink.NP.Tick.GrabbedHasNoCandidates", Flags)
bool FGSTN_LinkTickGrabbedNoCandidatesTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkFragmentRegistration::Register();
	FScopedLinkTypes Types;

	// The grabbed: parked, does not act. Every command is vetoed while the link is live, the exit frames included
	FGSTN_LinkFragment Grabbed = MakeLinked(Types.GrabIndex, 2, 5, 3, 100);
	TestTrue(TEXT("the grabbed has no candidates"), BlocksWith(Grabbed));
	Grabbed.Links.Body.Phase = EGSTN_LinkPhase::Exiting;
	TestTrue(TEXT("nor while its exit is in flight"), BlocksWith(Grabbed));

	// The grabber acts (pummels and throws are its commands); a predicted request blocks nothing; no fragment blocks nothing
	TestFalse(TEXT("the grabber keeps its candidates"), BlocksWith(MakeLinked(Types.GrabIndex, 1, 6, 3, 100)));
	FGSTN_LinkFragment Predicted;
	TestTrue(TEXT("a grab-like hit marks the request"), FGSTN_LinkTickExtension::MarkPredicted(Predicted, *UGSTN_LinkRegistry::Get(), TestGrab, 6, 100));
	TestFalse(TEXT("a predicted request blocks nothing"), BlocksWith(Predicted));
	TestFalse(TEXT("an idle fragment blocks nothing"), BlocksWith(FGSTN_LinkFragment()));

	// A link that ended (serial kept) frees the entity on the frame it ended
	Grabbed.Links.Body.Reset();
	TestFalse(TEXT("after the link: candidates again"), BlocksWith(Grabbed));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkTickLatchedKeepsCandidatesTest, "GSTNLink.NP.Tick.LatchedKeepsCandidates", Flags)
bool FGSTN_LinkTickLatchedKeepsCandidatesTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkFragmentRegistration::Register();
	FScopedLinkTypes Types;

	// The latch parks the latcher (role 1) but both roles act: the latched enemy fights on, the latcher attacks the enemy it rides
	const FGSTN_LinkFragment Latcher = MakeLinked(Types.LatchIndex, 1, 9, 1, 50);
	const FGSTN_LinkFragment Latched = MakeLinked(Types.LatchIndex, 2, 8, 1, 50);
	TestTrue(TEXT("the latcher is parked"), FGSTN_LinkTickExtension::IsParked(&Latcher, UGSTN_LinkRegistry::Get()));
	TestFalse(TEXT("but keeps its candidates"), BlocksWith(Latcher));
	TestFalse(TEXT("the latched enemy is not parked"), FGSTN_LinkTickExtension::IsParked(&Latched, UGSTN_LinkRegistry::Get()));
	TestFalse(TEXT("and keeps its candidates"), BlocksWith(Latched));

	// Whether a parked role acts is data of its definition, not a branch on the link type: flipping it vetoes the latcher too
	Types.Latch->InitiatorRole.bActsWhileLinked = false;
	TestTrue(TEXT("a non-acting latcher is vetoed by the same rule"), BlocksWith(Latcher));
	Types.Latch->InitiatorRole.bActsWhileLinked = true;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkTickTagsFromRoleTest, "GSTNLink.NP.Tick.TagsFromRole", Flags)
bool FGSTN_LinkTickTagsFromRoleTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;

	const FGameplayTagContainer Grabbed = TagsOf(MakeLinked(Types.GrabIndex, 2, 5, 3, 100));
	TestTrue(TEXT("grabbed: Linked"), Grabbed.HasTagExact(GSTNLinkTags::Linked));
	TestTrue(TEXT("grabbed: its role tag"), Grabbed.HasTagExact(TestGrabbed));
	TestFalse(TEXT("grabbed: not the other role"), Grabbed.HasTagExact(TestGrabber));
	TestEqual(TEXT("grabbed: two tags"), Grabbed.Num(), 2);

	const FGameplayTagContainer Grabber = TagsOf(MakeLinked(Types.GrabIndex, 1, 6, 3, 100));
	TestTrue(TEXT("grabber: Linked and its role"), Grabber.HasTagExact(GSTNLinkTags::Linked) && Grabber.HasTagExact(TestGrabber));

	FGSTN_LinkFragment Predicted;
	FGSTN_LinkTickExtension::MarkPredicted(Predicted, *UGSTN_LinkRegistry::Get(), TestGrab, 6, 100);
	const FGameplayTagContainer Pending = TagsOf(Predicted);
	TestTrue(TEXT("predicted: Pending"), Pending.HasTagExact(GSTNLinkTags::Pending));
	TestFalse(TEXT("predicted: no role (the grab confirm is the hitstop's, m6 spec 7.2)"), Pending.HasTagExact(TestGrabber) || Pending.HasTagExact(GSTNLinkTags::Linked));

	TestEqual(TEXT("idle: no tag"), TagsOf(FGSTN_LinkFragment()).Num(), 0);

	// The projection clears what it can project: every registered role tag is in the set it writes, so an ended role is removed
	FGameplayTagContainer Projectable;
	FGSTN_LinkTickExtension::GatherProjectableTags(UGSTN_LinkRegistry::Get(), Projectable);
	TestTrue(TEXT("the projectable set has every test role"), Projectable.HasTagExact(TestGrabber) && Projectable.HasTagExact(TestGrabbed)
		&& Projectable.HasTagExact(TestLatcher) && Projectable.HasTagExact(TestLatched));
	TestTrue(TEXT("and Linked and Pending"), Projectable.HasTagExact(GSTNLinkTags::Linked) && Projectable.HasTagExact(GSTNLinkTags::Pending));
	return true;
}

#endif

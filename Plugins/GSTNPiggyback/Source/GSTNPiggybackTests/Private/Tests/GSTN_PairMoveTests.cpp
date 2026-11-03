// Tests of the pair-move data (M7 G8): definition validation against hand-built timelines (spec 9: missing abilities, partner offset past the
// initiator, OutcomeFrame outside the timeline, roles) and the registry as the mount type's action provider (id ordering, clashes, resolution).
// No world: the definitions are transient objects and the abilities are the plugin's mount moves (any combat timeline ability will do).

#include "Data/GSTN_LinkOutcomeTrack.h"
#include "GSTN_MountAbilities.h"
#include "GSTN_PairMoveDefinition.h"
#include "GSTN_PairMoveRegistry.h"
#include "GSTN_PiggybackTags.h"
#include "GSTN_Placer_Mount.h"
#include "Misc/AutomationTest.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNPairMoveTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A timeline of Total frames with one PartnerMove key naming Id at KeyFrame (INDEX_NONE = no key) */
	FGSTN_CombatTimelineData MakeTimeline(int32 Total, int32 Id = INDEX_NONE, int32 KeyFrame = 0)
	{
		FGSTN_CombatTimelineData Timeline;
		Timeline.TotalFrames = Total;
		Timeline.FirstActionableFrame = Total;
		if (Id != INDEX_NONE)
		{
			FGSTN_LinkOutcomeTrack Track;
			Track.StartFrame = 0;
			Track.EndFrame = Total;
			FGSTN_LinkOutcomeKey Key;
			Key.Frame = KeyFrame;
			Key.Outcome = EGSTN_LinkOutcome::PartnerMove;
			Key.PartnerMoveId = Id;
			Track.Keys.Add(Key);
			Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_LinkOutcomeTrack>(Track));
		}
		return Timeline;
	}

	/** A valid carrier-initiated pair move like PM_ThrowRider: id 7, partner at +12, Break at 14 */
	UGSTN_PairMoveDefinition* MakeDefinition(int32 Id = 7)
	{
		UGSTN_PairMoveDefinition* Definition = NewObject<UGSTN_PairMoveDefinition>(GetTransientPackage());
		Definition->PairTag = GSTNPiggybackTags::PairMove_ThrowRider;
		Definition->PairMoveId = Id;
		Definition->InitiatorRole = EGSTN_PairRole::Carrier;
		Definition->InitiatorAbility = UGSTN_GA_Drop::StaticClass();
		Definition->PartnerAbility = UGSTN_GA_Dismount::StaticClass();
		Definition->PartnerStartOffsetFrames = 12;
		Definition->Outcome = EGSTN_LinkActionOutcome::Break;
		Definition->OutcomeFrame = 14;
		Definition->ExitMode = EGSTN_ExitMode::AtAnchor;
		return Definition;
	}

	TArray<FString> Validate(const UGSTN_PairMoveDefinition* Definition, const FGSTN_CombatTimelineData* Initiator, const FGSTN_CombatTimelineData* Partner)
	{
		TArray<FString> Errors;
		Definition->ValidateWithTimelines(Initiator, Partner, Errors);
		return Errors;
	}

	bool AnyContains(const TArray<FString>& Errors, const TCHAR* Text)
	{
		return Errors.ContainsByPredicate([Text](const FString& Error) { return Error.Contains(Text); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PairMoveValidTest, "GSTNPiggyback.Runtime.PairMove.ValidDefinitionHasNoError", GSTNPairMoveTests::Flags)
bool FGSTN_PairMoveValidTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPairMoveTests;
	const FGSTN_CombatTimelineData Initiator = MakeTimeline(44, 7, 0);
	const FGSTN_CombatTimelineData Partner = MakeTimeline(40);
	const TArray<FString> Errors = Validate(MakeDefinition(), &Initiator, &Partner);
	TestEqual(TEXT("no error"), Errors.Num(), 0);
	for (const FString& Error : Errors)
	{
		AddInfo(Error);
	}
	TestEqual(TEXT("key frame found"), UGSTN_PairMoveDefinition::FindPartnerMoveKeyFrame(MakeTimeline(30, 7, 3), 7), 3);
	TestEqual(TEXT("another id is not the key"), UGSTN_PairMoveDefinition::FindPartnerMoveKeyFrame(MakeTimeline(30, 7, 3), 8), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PairMoveInvalidTest, "GSTNPiggyback.Runtime.PairMove.ValidationCases", GSTNPairMoveTests::Flags)
bool FGSTN_PairMoveInvalidTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPairMoveTests;
	const FGSTN_CombatTimelineData Initiator = MakeTimeline(44, 7, 0);
	const FGSTN_CombatTimelineData Partner = MakeTimeline(40);

	TestTrue(TEXT("missing initiator"), AnyContains(Validate(MakeDefinition(), nullptr, &Partner), TEXT("no initiator ability")));
	TestTrue(TEXT("missing partner"), AnyContains(Validate(MakeDefinition(), &Initiator, nullptr), TEXT("no partner ability")));
	const FGSTN_CombatTimelineData EmptyPartner = MakeTimeline(0);
	TestTrue(TEXT("empty partner timeline"), AnyContains(Validate(MakeDefinition(), &Initiator, &EmptyPartner), TEXT("timeline is empty")));

	const FGSTN_CombatTimelineData NoKey = MakeTimeline(44);
	TestTrue(TEXT("no PartnerMove key"), AnyContains(Validate(MakeDefinition(), &NoKey, &Partner), TEXT("no PartnerMove key")));
	const FGSTN_CombatTimelineData OtherKey = MakeTimeline(44, 8, 0);
	TestTrue(TEXT("key of another id"), AnyContains(Validate(MakeDefinition(), &OtherKey, &Partner), TEXT("no PartnerMove key")));

	// Offset past the initiator: key 0 + 12 on a 12-frame initiator; offset 0 counts as 1
	UGSTN_PairMoveDefinition* Offset = MakeDefinition();
	Offset->OutcomeFrame = 5;
	const FGSTN_CombatTimelineData Short = MakeTimeline(12, 7, 0);
	TestTrue(TEXT("offset past the initiator"), AnyContains(Validate(Offset, &Short, &Partner), TEXT("past the initiator")));
	Offset->PartnerStartOffsetFrames = 0;
	const FGSTN_CombatTimelineData KeyAtEnd = MakeTimeline(12, 7, 11);
	Offset->OutcomeFrame = 11;
	TestTrue(TEXT("offset 0 is 1: past the last frame"), AnyContains(Validate(Offset, &KeyAtEnd, &Partner), TEXT("past the initiator")));

	UGSTN_PairMoveDefinition* Outside = MakeDefinition();
	Outside->OutcomeFrame = 44;
	TestTrue(TEXT("OutcomeFrame outside"), AnyContains(Validate(Outside, &Initiator, &Partner), TEXT("outside the initiator timeline")));
	const FGSTN_CombatTimelineData LateKey = MakeTimeline(44, 7, 20);
	UGSTN_PairMoveDefinition* Early = MakeDefinition();
	Early->PartnerStartOffsetFrames = 1;
	TestTrue(TEXT("OutcomeFrame before the key"), AnyContains(Validate(Early, &LateKey, &Partner), TEXT("before the PartnerMove key")));
	UGSTN_PairMoveDefinition* NoFrame = MakeDefinition();
	NoFrame->OutcomeFrame = INDEX_NONE;
	TestTrue(TEXT("Break without a frame"), AnyContains(Validate(NoFrame, &Initiator, &Partner), TEXT("needs an OutcomeFrame")));
	NoFrame->Outcome = EGSTN_LinkActionOutcome::KeepLink;
	TestEqual(TEXT("KeepLink without a frame is fine"), Validate(NoFrame, &Initiator, &Partner).Num(), 0);
	UGSTN_PairMoveDefinition* Swap = MakeDefinition();
	Swap->Outcome = EGSTN_LinkActionOutcome::SwapRoles;
	TestTrue(TEXT("SwapRoles reserved"), AnyContains(Validate(Swap, &Initiator, &Partner), TEXT("SwapRoles")));

	// Roles: the carrier initiator may not require the rider role; the rider partner may not require the carrier role nor ignore its own
	UGSTN_PairMoveDefinition* Roles = MakeDefinition();
	Roles->InitiatorRequirements.RequireTags.AddTag(GSTNPiggybackTags::Role_Rider);
	TestTrue(TEXT("initiator requires the partner role"), AnyContains(Validate(Roles, &Initiator, &Partner), TEXT("initiator's requirements")));
	Roles->InitiatorRequirements = FGSTN_TagRequirements();
	Roles->InitiatorRequirements.RequireTags.AddTag(GSTNPiggybackTags::Role_Carrier);
	Roles->PartnerRequirements.IgnoreTags.AddTag(GSTNPiggybackTags::Role_Rider);
	TestTrue(TEXT("partner ignores its own role"), AnyContains(Validate(Roles, &Initiator, &Partner), TEXT("partner's requirements")));
	Roles->PartnerRequirements = FGSTN_TagRequirements();
	Roles->InitiatorRole = EGSTN_PairRole::Rider;
	Roles->PartnerRequirements.RequireTags.AddTag(GSTNPiggybackTags::Role_Rider);
	TestTrue(TEXT("rider initiator: carrier partner may not require the rider role"), AnyContains(Validate(Roles, &Initiator, &Partner), TEXT("partner's requirements")));

	UGSTN_PairMoveDefinition* BadId = MakeDefinition(0);
	const FGSTN_CombatTimelineData KeyZero = MakeTimeline(44, 0, 0);
	TestTrue(TEXT("id out of range"), AnyContains(Validate(BadId, &KeyZero, &Partner), TEXT("outside 1..32767")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PairMoveRegistryTest, "GSTNPiggyback.Runtime.PairMove.RegistryOrderAndResolve", GSTNPairMoveTests::Flags)
bool FGSTN_PairMoveRegistryTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPairMoveTests;
	TStrongObjectPtr<UGSTN_PairMoveRegistry> Registry(NewObject<UGSTN_PairMoveRegistry>(GetTransientPackage()));
	UGSTN_PairMoveDefinition* Id9 = MakeDefinition(9);
	UGSTN_PairMoveDefinition* Id3 = MakeDefinition(3);
	Id3->PairTag = GSTNPiggybackTags::PairMove_BoostJump;
	UGSTN_PairMoveDefinition* Id5 = MakeDefinition(5);

	// Two different add orders give the same listing (id order)
	TestTrue(TEXT("add 9"), Registry->AddDefinition(Id9));
	TestTrue(TEXT("add 3"), Registry->AddDefinition(Id3));
	TestTrue(TEXT("add 5"), Registry->AddDefinition(Id5));
	TArray<const UGSTN_PairMoveDefinition*> Listing = Registry->GetDefinitions();
	TestTrue(TEXT("ordered by id"), Listing.Num() == 3 && Listing[0] == Id3 && Listing[1] == Id5 && Listing[2] == Id9);
	TStrongObjectPtr<UGSTN_PairMoveRegistry> Other(NewObject<UGSTN_PairMoveRegistry>(GetTransientPackage()));
	UGSTN_PairMoveSet* Set = NewObject<UGSTN_PairMoveSet>(GetTransientPackage());
	Set->PairMoves = { Id5, Id9, Id3 };
	TestEqual(TEXT("set adds all"), Other->AddSet(Set), 3);
	TestTrue(TEXT("same listing from the set"), Other->GetDefinitions() == Listing);

	// Idempotent for the same definition; a clash of ids is refused and the first stays
	TestTrue(TEXT("same definition again"), Registry->AddDefinition(Id5));
	UGSTN_PairMoveDefinition* Clash = MakeDefinition(5);
	FString Error;
	AddExpectedMessage(TEXT("already held"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("clash refused"), Registry->AddDefinition(Clash, &Error));
	TestTrue(TEXT("first wins"), Registry->FindById(5) == Id5);
	UGSTN_PairMoveDefinition* NoAbility = MakeDefinition(11);
	NoAbility->PartnerAbility = nullptr;
	AddExpectedMessage(TEXT("missing ability"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("missing ability refused"), Registry->AddDefinition(NoAbility));
	TestEqual(TEXT("still three"), Registry->GetDefinitions().Num(), 3);
	TestTrue(TEXT("by tag"), Registry->FindByTag(GSTNPiggybackTags::PairMove_BoostJump) == Id3);

	// Resolution: the action GSTNLink plays
	FGSTN_LinkActionInfo Info;
	TestFalse(TEXT("unknown id"), Registry->ResolveLinkAction(4, Info));
	TestTrue(TEXT("known id"), Registry->ResolveLinkAction(9, Info));
	TestTrue(TEXT("partner ability"), Info.PartnerAbility == UGSTN_GA_Dismount::StaticClass());
	TestEqual(TEXT("offset"), Info.PartnerStartOffsetFrames, 12);
	TestEqual(TEXT("outcome frame"), Info.OutcomeFrame, 14);
	TestTrue(TEXT("break with the outcome reason"), Info.Outcome == EGSTN_LinkActionOutcome::Break && Info.BreakReason == GSTNPiggybackTags::BreakReason_Outcome.GetTag());
	const UGSTN_Placer_Mount* Placer = Cast<UGSTN_Placer_Mount>(Info.ExitPlacer);
	TestTrue(TEXT("mount placer at the anchor"), Placer && Placer->Mode == EGSTN_ExitMode::AtAnchor);
	TestTrue(TEXT("cooperative partner locks commands"), Info.bPartnerLocksCommands);
	Id3->PartnerStartOffsetFrames = 0;
	Id3->PartnerMode = EGSTN_PairPartnerMode::Interruptible;
	TestTrue(TEXT("resolve 3"), Registry->ResolveLinkAction(3, Info));
	TestEqual(TEXT("offset 0 becomes 1"), Info.PartnerStartOffsetFrames, 1);
	TestFalse(TEXT("interruptible partner keeps its commands"), Info.bPartnerLocksCommands);
	return true;
}

#endif

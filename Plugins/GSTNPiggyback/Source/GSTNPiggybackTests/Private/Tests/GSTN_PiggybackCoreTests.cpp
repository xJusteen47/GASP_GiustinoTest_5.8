// Automation tests of the piggyback plugin's pure part (plan G4): no world, no ability system, no project content (spec 2b, isolation test b).

#include "GameplayTagsManager.h"
#include "GSTN_LinkTags.h"
#include "GSTN_MountUtils.h"
#include "GSTN_PiggybackTags.h"
#include "Misc/AutomationTest.h"
#include "State/GSTN_LinkMath.h"

#include <cmath>
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNPiggybackTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A request every check accepts: both kits allowed, nobody linked, no cooldown, in range, light enough */
	FGSTN_MountEligibilityInput MakeAcceptedInput()
	{
		FGSTN_MountEligibilityInput Input;
		Input.bCarrierCanBeMounted = true;
		Input.CarrierMaxCarryWeight = 100.0;
		Input.MountMaxDistanceCm = 150.0;
		Input.bRiderCanRide = true;
		Input.RiderWeight = 80.0;
		Input.RiderCooldownEndFrame = 0;
		Input.Frame = 500;
		Input.DistanceCm = 90.0;
		return Input;
	}

	bool TransformsNear(const FTransform& A, const FTransform& B, double Tolerance = 1.e-3)
	{
		return A.GetLocation().Equals(B.GetLocation(), Tolerance) && A.GetRotation().Equals(B.GetRotation(), 1.e-5);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PiggybackEligibilityEveryVerdictTest, "GSTNPiggyback.Core.Eligibility.EveryVerdict", GSTNPiggybackTest::Flags)
bool FGSTN_PiggybackEligibilityEveryVerdictTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPiggybackTest;
	TestEqual(TEXT("A valid request is accepted"), UGSTN_MountUtils::EvaluateEligibility(MakeAcceptedInput()), EGSTN_MountVerdict::Accept);

	// Each check failing alone gives its own verdict
	struct FCase
	{
		const TCHAR* Name;
		TFunction<void(FGSTN_MountEligibilityInput&)> Break;
		EGSTN_MountVerdict Expected;
	};
	const FCase Cases[] = {
		{ TEXT("carrier kit"), [](FGSTN_MountEligibilityInput& In) { In.bCarrierCanBeMounted = false; }, EGSTN_MountVerdict::NotAllowedKit },
		{ TEXT("rider kit"), [](FGSTN_MountEligibilityInput& In) { In.bRiderCanRide = false; }, EGSTN_MountVerdict::RiderIneligible },
		{ TEXT("carrier linked"), [](FGSTN_MountEligibilityInput& In) { In.bCarrierLinked = true; }, EGSTN_MountVerdict::AlreadyLinked },
		{ TEXT("rider linked"), [](FGSTN_MountEligibilityInput& In) { In.bRiderLinked = true; }, EGSTN_MountVerdict::AlreadyLinked },
		{ TEXT("cooldown"), [](FGSTN_MountEligibilityInput& In) { In.RiderCooldownEndFrame = In.Frame + 1; }, EGSTN_MountVerdict::Cooldown },
		{ TEXT("hitstun"), [](FGSTN_MountEligibilityInput& In) { In.bCarrierInHitstun = true; }, EGSTN_MountVerdict::CarrierHitstun },
		{ TEXT("busy or locked"), [](FGSTN_MountEligibilityInput& In) { In.bCarrierBusyOrLocked = true; }, EGSTN_MountVerdict::CarrierBusyLock },
		{ TEXT("range"), [](FGSTN_MountEligibilityInput& In) { In.DistanceCm = 150.5; }, EGSTN_MountVerdict::OutOfRange },
		{ TEXT("NaN distance"), [](FGSTN_MountEligibilityInput& In) { In.DistanceCm = std::numeric_limits<double>::quiet_NaN(); }, EGSTN_MountVerdict::OutOfRange },
		{ TEXT("weight"), [](FGSTN_MountEligibilityInput& In) { In.RiderWeight = 120.0; }, EGSTN_MountVerdict::WeightTooHigh },
	};
	for (const FCase& Case : Cases)
	{
		FGSTN_MountEligibilityInput Input = MakeAcceptedInput();
		Case.Break(Input);
		TestEqual(FString::Printf(TEXT("Only %s fails"), Case.Name), UGSTN_MountUtils::EvaluateEligibility(Input), Case.Expected);
	}

	// Precedence: with every check failing, removing the winning failure each time walks the documented order
	FGSTN_MountEligibilityInput All = MakeAcceptedInput();
	for (const FCase& Case : Cases)
	{
		Case.Break(All);
	}
	TestEqual(TEXT("Everything fails: the carrier kit first"), UGSTN_MountUtils::EvaluateEligibility(All), EGSTN_MountVerdict::NotAllowedKit);
	All.bCarrierCanBeMounted = true;
	TestEqual(TEXT("Then the rider kit"), UGSTN_MountUtils::EvaluateEligibility(All), EGSTN_MountVerdict::RiderIneligible);
	All.bRiderCanRide = true;
	TestEqual(TEXT("Then a link"), UGSTN_MountUtils::EvaluateEligibility(All), EGSTN_MountVerdict::AlreadyLinked);
	All.bCarrierLinked = false;
	All.bRiderLinked = false;
	TestEqual(TEXT("Then the cooldown"), UGSTN_MountUtils::EvaluateEligibility(All), EGSTN_MountVerdict::Cooldown);
	All.RiderCooldownEndFrame = 0;
	TestEqual(TEXT("Then hitstun"), UGSTN_MountUtils::EvaluateEligibility(All), EGSTN_MountVerdict::CarrierHitstun);
	All.bCarrierInHitstun = false;
	TestEqual(TEXT("Then busy or locked"), UGSTN_MountUtils::EvaluateEligibility(All), EGSTN_MountVerdict::CarrierBusyLock);
	All.bCarrierBusyOrLocked = false;
	TestEqual(TEXT("Then range"), UGSTN_MountUtils::EvaluateEligibility(All), EGSTN_MountVerdict::OutOfRange);
	All.DistanceCm = 10.0;
	TestEqual(TEXT("Then weight"), UGSTN_MountUtils::EvaluateEligibility(All), EGSTN_MountVerdict::WeightTooHigh);
	All.RiderWeight = 10.0;
	TestEqual(TEXT("Then nothing"), UGSTN_MountUtils::EvaluateEligibility(All), EGSTN_MountVerdict::Accept);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PiggybackEligibilityWeightBoundaryTest, "GSTNPiggyback.Core.Eligibility.WeightBoundary", GSTNPiggybackTest::Flags)
bool FGSTN_PiggybackEligibilityWeightBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPiggybackTest;
	FGSTN_MountEligibilityInput Input = MakeAcceptedInput();

	Input.RiderWeight = Input.CarrierMaxCarryWeight;
	TestEqual(TEXT("Weight equal to the capacity is accepted"), UGSTN_MountUtils::EvaluateEligibility(Input), EGSTN_MountVerdict::Accept);
	Input.RiderWeight = std::nextafter(Input.CarrierMaxCarryWeight, 1.e9);
	TestEqual(TEXT("The next double above the capacity is refused"), UGSTN_MountUtils::EvaluateEligibility(Input), EGSTN_MountVerdict::WeightTooHigh);
	Input.RiderWeight = std::nextafter(Input.CarrierMaxCarryWeight, 0.0);
	TestEqual(TEXT("The next double below the capacity is accepted"), UGSTN_MountUtils::EvaluateEligibility(Input), EGSTN_MountVerdict::Accept);

	Input.CarrierMaxCarryWeight = 0.0;
	Input.RiderWeight = 0.0;
	TestEqual(TEXT("No capacity carries a weightless rider"), UGSTN_MountUtils::EvaluateEligibility(Input), EGSTN_MountVerdict::Accept);
	Input.RiderWeight = 0.01;
	TestEqual(TEXT("No capacity refuses any weight"), UGSTN_MountUtils::EvaluateEligibility(Input), EGSTN_MountVerdict::WeightTooHigh);
	Input.RiderWeight = std::numeric_limits<double>::quiet_NaN();
	TestEqual(TEXT("A NaN weight is refused"), UGSTN_MountUtils::EvaluateEligibility(Input), EGSTN_MountVerdict::WeightTooHigh);

	// The other two boundaries of the same rule: equal range and the cooldown's own end frame are accepted
	Input = MakeAcceptedInput();
	Input.DistanceCm = Input.MountMaxDistanceCm;
	TestEqual(TEXT("Distance equal to the range is accepted"), UGSTN_MountUtils::EvaluateEligibility(Input), EGSTN_MountVerdict::Accept);
	Input.RiderCooldownEndFrame = Input.Frame;
	TestEqual(TEXT("The cooldown's end frame is accepted"), UGSTN_MountUtils::EvaluateEligibility(Input), EGSTN_MountVerdict::Accept);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PiggybackCarryRatioTest, "GSTNPiggyback.Core.CarryRatio.ClampAndQuantise", GSTNPiggybackTest::Flags)
bool FGSTN_PiggybackCarryRatioTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("No weight is 0"), UGSTN_MountUtils::ComputeCarryRatio(0.0, 100.0), static_cast<uint8>(0));
	TestEqual(TEXT("Full capacity is 255"), UGSTN_MountUtils::ComputeCarryRatio(100.0, 100.0), static_cast<uint8>(255));
	TestEqual(TEXT("Over capacity clamps to 255"), UGSTN_MountUtils::ComputeCarryRatio(250.0, 100.0), static_cast<uint8>(255));
	TestEqual(TEXT("A negative weight clamps to 0"), UGSTN_MountUtils::ComputeCarryRatio(-5.0, 100.0), static_cast<uint8>(0));
	TestEqual(TEXT("Half rounds half up (127.5 -> 128)"), UGSTN_MountUtils::ComputeCarryRatio(50.0, 100.0), static_cast<uint8>(128));
	TestEqual(TEXT("A quarter rounds to the nearest step (63.75 -> 64)"), UGSTN_MountUtils::ComputeCarryRatio(25.0, 100.0), static_cast<uint8>(64));
	TestEqual(TEXT("One step"), UGSTN_MountUtils::ComputeCarryRatio(1.0, 255.0), static_cast<uint8>(1));
	TestEqual(TEXT("A NaN weight is 0"), UGSTN_MountUtils::ComputeCarryRatio(std::numeric_limits<double>::quiet_NaN(), 100.0), static_cast<uint8>(0));
	TestEqual(TEXT("No capacity, some weight: full"), UGSTN_MountUtils::ComputeCarryRatio(10.0, 0.0), static_cast<uint8>(255));
	TestEqual(TEXT("No capacity, no weight: empty"), UGSTN_MountUtils::ComputeCarryRatio(0.0, 0.0), static_cast<uint8>(0));
	TestEqual(TEXT("Negative capacity counts as none"), UGSTN_MountUtils::ComputeCarryRatio(10.0, -50.0), static_cast<uint8>(255));

	TestEqual(TEXT("0 is exactly 0"), UGSTN_MountUtils::CarryRatioToUnit(0), 0.0);
	TestEqual(TEXT("255 is exactly 1"), UGSTN_MountUtils::CarryRatioToUnit(255), 1.0);

	FGSTN_CarryTuning Full;
	Full.SpeedPenalty = 0.4f;
	Full.AccelPenalty = 0.2f;
	Full.JumpPenalty = 0.3f;
	Full.AirJumpsRemoved = 2;
	const FGSTN_CarryTuning AtFull = UGSTN_MountUtils::ScaleCarryTuning(Full, 255);
	TestEqual(TEXT("Full load keeps the speed penalty"), AtFull.SpeedPenalty, Full.SpeedPenalty);
	TestEqual(TEXT("Full load keeps the jump penalty"), AtFull.JumpPenalty, Full.JumpPenalty);
	TestEqual(TEXT("Full load removes every air jump"), AtFull.AirJumpsRemoved, 2);
	const FGSTN_CarryTuning AtNone = UGSTN_MountUtils::ScaleCarryTuning(Full, 0);
	TestEqual(TEXT("No load, no speed penalty"), AtNone.SpeedPenalty, 0.f);
	TestEqual(TEXT("No load, no air jump removed"), AtNone.AirJumpsRemoved, 0);
	const FGSTN_CarryTuning AtHalf = UGSTN_MountUtils::ScaleCarryTuning(Full, 128);
	TestTrue(TEXT("Half load scales the accel penalty linearly"), FMath::IsNearlyEqual(AtHalf.AccelPenalty, 0.2f * 128.f / 255.f, 1.e-6f));
	TestEqual(TEXT("Half load removes 2 * 128 / 255 = 1 air jump (toward zero)"), AtHalf.AirJumpsRemoved, 1);
	TestEqual(TEXT("Just under full removes 1 of 2 air jumps"), UGSTN_MountUtils::ScaleCarryTuning(Full, 254).AirJumpsRemoved, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PiggybackAnchorComposeTest, "GSTNPiggyback.Core.Anchor.Compose", GSTNPiggybackTest::Flags)
bool FGSTN_PiggybackAnchorComposeTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPiggybackTest;
	// Carrier at (100, 0, 0) facing +Y (yaw 90); socket 50 cm up; the rider 20 cm behind and 10 cm above the socket
	const FTransform Carrier(FRotator(0.0, 90.0, 0.0), FVector(100.0, 0.0, 0.0));
	const FTransform Socket(FVector(0.0, 0.0, 50.0));
	const FTransform Offset(FVector(-20.0, 0.0, 10.0));

	const FTransform Anchor = UGSTN_MountUtils::ComposeAnchor(Carrier, Socket, Offset, 0.0);
	TestTrue(TEXT("Anchor location: carrier local (-20, 0, 60) turned by 90 degrees"), Anchor.GetLocation().Equals(FVector(100.0, -20.0, 60.0), 1.e-6));
	TestTrue(TEXT("Anchor faces the carrier's way"), Anchor.GetRotation().Equals(Carrier.GetRotation(), 1.e-9));

	const FTransform Root = UGSTN_MountUtils::ComposeAnchor(Carrier, FTransform::Identity, FTransform::Identity, 0.0);
	TestTrue(TEXT("A root mount with no offset is the carrier itself"), TransformsNear(Root, Carrier, 1.e-9));

	const FTransform Turned = UGSTN_MountUtils::ComposeAnchor(Carrier, Socket, Offset, 45.0);
	TestTrue(TEXT("The rider's turn does not move the anchor"), Turned.GetLocation().Equals(Anchor.GetLocation(), 1.e-6));
	TestTrue(TEXT("The rider's turn adds to the carrier's yaw"), FMath::IsNearlyEqual(Turned.Rotator().Yaw, 135.0, 1.e-4));

	// The same composition as GSTNLink's parked placement plus its local yaw (45 degrees is exactly 32 quanta of 1/256 turn)
	const int16 YawQ = UGSTN_LinkMath::LocalYawQFromDeg(45.0);
	TestEqual(TEXT("45 degrees is a whole number of link yaw quanta"), UGSTN_LinkMath::LocalYawDeg(YawQ), 45.0);
	TestTrue(TEXT("Equal to UGSTN_LinkMath::ComposeLocalYaw of the unturned anchor"), TransformsNear(Turned, UGSTN_LinkMath::ComposeLocalYaw(Anchor, YawQ), 1.e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PiggybackExitCandidateOrderTest, "GSTNPiggyback.Core.Exit.CandidateOrder", GSTNPiggybackTest::Flags)
bool FGSTN_PiggybackExitCandidateOrderTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPiggybackTest;
	const FTransform Carrier = FTransform::Identity;
	const FTransform Anchor(FVector(-10.0, 0.0, 80.0));
	const FVector Back(-100.0, 0.0, 0.0);
	const FVector Left(0.0, -100.0, 0.0);
	const FVector Right(0.0, 100.0, 0.0);
	const FVector Front(100.0, 0.0, 0.0);

	auto CheckOrder = [this](const TCHAR* Mode, const TArray<FTransform>& Got, const TArray<FVector>& Expected)
	{
		if (!TestEqual(FString::Printf(TEXT("%s: candidate count"), Mode), Got.Num(), Expected.Num()))
		{
			return;
		}
		for (int32 Index = 0; Index < Expected.Num(); ++Index)
		{
			TestTrue(FString::Printf(TEXT("%s: candidate %d at %s"), Mode, Index, *Expected[Index].ToString()), Got[Index].GetLocation().Equals(Expected[Index], 1.e-6));
		}
	};

	CheckOrder(TEXT("BehindCarrier"), UGSTN_MountUtils::ExitCandidates(Anchor, Carrier, EGSTN_ExitMode::BehindCarrier, 100.0), { Back, Left, Right, Front });
	CheckOrder(TEXT("SideOfCarrier"), UGSTN_MountUtils::ExitCandidates(Anchor, Carrier, EGSTN_ExitMode::SideOfCarrier, 100.0), { Right, Left, Back, Front });
	const TArray<FTransform> AtAnchor = UGSTN_MountUtils::ExitCandidates(Anchor, Carrier, EGSTN_ExitMode::AtAnchor, 100.0);
	CheckOrder(TEXT("AtAnchor"), AtAnchor, { Anchor.GetLocation(), Back, Left, Right, Front });
	TestTrue(TEXT("AtAnchor: the first candidate is the anchor itself"), AtAnchor.Num() > 0 && TransformsNear(AtAnchor[0], Anchor, 1.e-9));

	// The candidates follow the carrier: turned 90 degrees and moved, "behind" is its own -X, and every candidate faces its way
	const FTransform Turned(FRotator(0.0, 90.0, 0.0), FVector(500.0, 0.0, 0.0));
	const TArray<FTransform> TurnedBehind = UGSTN_MountUtils::ExitCandidates(Anchor, Turned, EGSTN_ExitMode::BehindCarrier, 100.0);
	CheckOrder(TEXT("BehindCarrier turned"), TurnedBehind, { FVector(500.0, -100.0, 0.0), FVector(600.0, 0.0, 0.0), FVector(400.0, 0.0, 0.0), FVector(500.0, 100.0, 0.0) });
	for (const FTransform& Candidate : TurnedBehind)
	{
		TestTrue(TEXT("Each candidate faces the carrier's way"), Candidate.GetRotation().Equals(Turned.GetRotation(), 1.e-9));
	}

	// Any gravity: on a carrier standing upside down the candidates stay on its plane
	const FTransform Flipped(FRotator(0.0, 0.0, 180.0), FVector(0.0, 0.0, 300.0));
	for (const FTransform& Candidate : UGSTN_MountUtils::ExitCandidates(Anchor, Flipped, EGSTN_ExitMode::SideOfCarrier, 100.0))
	{
		TestTrue(TEXT("Candidates of a flipped carrier stay on its plane"), FMath::IsNearlyEqual(Candidate.GetLocation().Z, 300.0, 1.e-6));
	}

	const TArray<FTransform> Negative = UGSTN_MountUtils::ExitCandidates(Anchor, Carrier, EGSTN_ExitMode::BehindCarrier, -50.0);
	CheckOrder(TEXT("Negative distance"), Negative, { FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PiggybackCreditSplitTest, "GSTNPiggyback.Core.Credit.Split", GSTNPiggybackTest::Flags)
bool FGSTN_PiggybackCreditSplitTest::RunTest(const FString& Parameters)
{
	const FGSTN_CreditSplit OwnerOnly = UGSTN_MountUtils::SplitCredit(0, 30.0);
	TestEqual(TEXT("Share 0: all to the owner"), OwnerOnly.OwnerAmount, 30.0);
	TestEqual(TEXT("Share 0: nothing to the partner"), OwnerOnly.PartnerAmount, 0.0);

	const FGSTN_CreditSplit PartnerOnly = UGSTN_MountUtils::SplitCredit(255, 30.0);
	TestEqual(TEXT("Share 255: nothing to the owner"), PartnerOnly.OwnerAmount, 0.0);
	TestEqual(TEXT("Share 255: all to the partner"), PartnerOnly.PartnerAmount, 30.0);

	TestEqual(TEXT("Half is share 128"), UGSTN_MountUtils::MakePartnerShare(0.5), static_cast<uint8>(128));
	TestEqual(TEXT("A share below 0 clamps to 0"), UGSTN_MountUtils::MakePartnerShare(-1.0), static_cast<uint8>(0));
	TestEqual(TEXT("A share above 1 clamps to 255"), UGSTN_MountUtils::MakePartnerShare(2.0), static_cast<uint8>(255));

	const FGSTN_CreditSplit Half = UGSTN_MountUtils::SplitCredit(128, 100.0);
	TestTrue(TEXT("Share 128: the partner gets 128/255 of the amount"), FMath::IsNearlyEqual(Half.PartnerAmount, 100.0 * 128.0 / 255.0, 1.e-12));
	TestEqual(TEXT("Share 128: the parts add up to the amount"), Half.OwnerAmount + Half.PartnerAmount, 100.0);

	const FGSTN_CreditSplit Third = UGSTN_MountUtils::SplitCredit(85, 12.0);
	TestTrue(TEXT("Share 85 is a third"), FMath::IsNearlyEqual(Third.PartnerAmount, 4.0, 1.e-12));
	TestTrue(TEXT("Share 85: the owner keeps two thirds"), FMath::IsNearlyEqual(Third.OwnerAmount, 8.0, 1.e-12));

	const FGSTN_CreditSplit Negative = UGSTN_MountUtils::SplitCredit(128, -10.0);
	TestTrue(TEXT("A negative amount (a cost) is split the same way"), FMath::IsNearlyEqual(Negative.OwnerAmount + Negative.PartnerAmount, -10.0, 1.e-12) && Negative.PartnerAmount < 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PiggybackTagsRegisteredTest, "GSTNPiggyback.Core.Tags.Registered", GSTNPiggybackTest::Flags)
bool FGSTN_PiggybackTagsRegisteredTest::RunTest(const FString& Parameters)
{
	TArray<FGameplayTag> Tags;
	GSTNPiggybackTags::GetDeclaredTags(Tags);
	TestEqual(TEXT("Every native tag is listed"), Tags.Num(), 21);

	TArray<FGameplayTag> LinkTags;
	GSTNLinkTags::GetDeclaredTags(LinkTags);

	const UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	TSet<FGameplayTag> Seen;
	for (const FGameplayTag& Tag : Tags)
	{
		const FString TagString = Tag.ToString();
		TestTrue(FString::Printf(TEXT("%s is registered"), *TagString), Tag.IsValid());
		bool bAlreadySeen = false;
		Seen.Add(Tag, &bAlreadySeen);
		TestFalse(FString::Printf(TEXT("%s is listed once"), *TagString), bAlreadySeen);
		// A generic link tag is reused, never redeclared (GSTNLink owns it)
		TestFalse(FString::Printf(TEXT("%s is not one of GSTNLink's tags"), *TagString), LinkTags.Contains(Tag));
		if (!Tag.IsValid())
		{
			continue;
		}

#if WITH_EDITOR // tag comments and sources are editor-only data (a Game target has no GetTagEditorData)
		FString Comment;
		FName Source;
		bool bExplicit = false;
		bool bRestricted = false;
		bool bAllowNonRestrictedChildren = false;
		TestTrue(FString::Printf(TEXT("%s is known to the tags manager"), *TagString),
			Manager.GetTagEditorData(Tag.GetTagName(), Comment, Source, bExplicit, bRestricted, bAllowNonRestrictedChildren));
		TestTrue(FString::Printf(TEXT("%s is declared explicitly"), *TagString), bExplicit);
		// The comment is the documentation (modularity-and-tags 4.8: at least 10 characters)
		TestTrue(FString::Printf(TEXT("%s has a comment of at least 10 characters"), *TagString), Comment.TrimStartAndEnd().Len() >= 10);
#endif
	}

	// The generic reasons a mount also ends with are GSTNLink's and exist with the link plugin alone
	const FGameplayTag Generic[] = { GSTNLinkTags::BreakReason_Timeout, GSTNLinkTags::BreakReason_Distance, GSTNLinkTags::BreakReason_Throw,
		GSTNLinkTags::BreakReason_OwnDeath, GSTNLinkTags::BreakReason_PartnerDeath, GSTNLinkTags::BreakReason_Forced };
	for (const FGameplayTag& Reason : Generic)
	{
		TestTrue(FString::Printf(TEXT("Generic reason %s exists (GSTNLink)"), *Reason.ToString()), Reason.IsValid() && LinkTags.Contains(Reason));
	}

	// The command slots of the plugin's own tag ini, registered by the Core module at startup
	TArray<FGameplayTag> Commands;
	GSTNPiggybackTags::GetCommandSlotTags(Commands);
	TestEqual(TEXT("Three command slots"), Commands.Num(), 3);
	for (const FGameplayTag& Command : Commands)
	{
		TestTrue(TEXT("Command.Link.Mount/Dismount/Drop come from GSTNPiggybackCommands.ini"), Command.IsValid());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

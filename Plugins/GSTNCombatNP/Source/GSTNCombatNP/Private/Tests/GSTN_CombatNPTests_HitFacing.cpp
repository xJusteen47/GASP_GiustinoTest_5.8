// Automation tests of the victim's turn on the hit frame (react-hook-4 review, owner 2026-11-14): FGSTN_CombatVictim::ResolveHitFacing over
// a hand built result and sync core, no world. Names the rules flags bTurnBackAlongKnockback and bTurnToAttackerWithoutKnockback.

#include "Misc/AutomationTest.h"
#include "Rules/GSTN_CombatRules.h"
#include "Simulation/GSTN_CombatHitState.h"
#include "Simulation/GSTN_CombatVictim.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** A victim at the origin facing +X, hit once by an attacker standing behind it (-X) and facing +X */
	struct FHitFacingFixture
	{
		TStrongObjectPtr<UGSTN_CombatRules> Rules{NewObject<UGSTN_CombatRules>()};
		FGSTN_CombatSyncCore Core;
		FGSTN_VictimResult Result;
		FGSTN_SnapshotEntry Self;

		FHitFacingFixture(EGSTN_HitReaction Reaction, const FVector& Launch)
		{
			Self.Facing = FVector::ForwardVector;
			FGSTN_VictimHit& Hit = Result.Hits.AddDefaulted_GetRef();
			Hit.bStaggered = true;
			Hit.Reaction = Reaction;
			Hit.Entry.AttackerYaw = 0.f;
			Core.Hit.SetPendingReaction(Reaction);
			Core.Hit.SetPendingLaunch(Launch);
		}

		bool Run(float& Yaw) const { return FGSTN_CombatVictim::ResolveHitFacing(Rules.Get(), Result, Core, Self, nullptr, Yaw); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitFacingBackAlongKnockbackTest, "GSTNCombat.NP.Hits.Facing.BackAlongKnockbackOnHitFrame", GSTN_NP_TEST_FLAGS)
bool FGSTN_HitFacingBackAlongKnockbackTest::RunTest(const FString& Parameters)
{
	// Pushed along +X from behind: the back points along +X, so the victim faces -X (yaw 180), already on the hit frame
	FHitFacingFixture F(EGSTN_HitReaction::Launch, FVector(800.0, 0.0, 600.0));
	float Yaw = 0.f;
	TestTrue(TEXT("bTurnBackAlongKnockback turns on the hit frame"), F.Run(Yaw));
	TestEqual(TEXT("faces against the knockback"), FMath::Abs(FRotator::NormalizeAxis(Yaw)), 180.f, 0.5f);
	F.Rules->bTurnBackAlongKnockback = false;
	F.Rules->bTurnToAttackerWithoutKnockback = false;
	TestFalse(TEXT("both flags off: no turn"), F.Run(Yaw));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitFacingNoPushTest, "GSTNCombat.NP.Hits.Facing.TurnToAttackerWithoutKnockback", GSTN_NP_TEST_FLAGS)
bool FGSTN_HitFacingNoPushTest::RunTest(const FString& Parameters)
{
	// A hit stand from behind: no knockback, the attacker (facing +X) behind the victim: the victim turns to face it
	FHitFacingFixture F(EGSTN_HitReaction::StaggerInPlace, FVector::ZeroVector);
	float Yaw = 0.f;
	TestTrue(TEXT("a back hit with no push turns"), F.Run(Yaw));
	TestEqual(TEXT("faces the attacker"), FMath::Abs(FRotator::NormalizeAxis(Yaw)), 180.f, 0.5f);
	F.Rules->bTurnToAttackerWithoutKnockback = false;
	TestFalse(TEXT("flag off: keeps its facing"), F.Run(Yaw));

	// The same hit from the front (attacker facing -X): inside TurnToAttackerMinAngleDeg, no turn
	FHitFacingFixture Front(EGSTN_HitReaction::StaggerInPlace, FVector::ZeroVector);
	Front.Result.Hits[0].Entry.AttackerYaw = 180.f;
	TestFalse(TEXT("a front hit with no push keeps the facing"), Front.Run(Yaw));
	return true;
}

#endif

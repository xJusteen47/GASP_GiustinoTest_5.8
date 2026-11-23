// Test only (M6 F14, m6 spec 12 item 7): a defensive pipeline step added from this module, outside GSTNDefence and GSTNCombat, through the
// public API only. It is placed in the standard combat rules next to GSTNDefence's Parry and Shield steps; the runner calls it in its place,
// its writes reach the outcome once, and the shield's parry and block still decide as before: no change to either plugin or to the model's tick.

#include "GSTN_ExtTestStep_Brace.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/GSTN_DefenceRules.h"
#include "GSTN_DefenceContentLibrary.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitRunner.h"
#include "Hits/GSTN_HitSteps.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Misc/AutomationTest.h"
#include "Pipeline/GSTN_HitStep_Parry.h"
#include "Pipeline/GSTN_HitStep_Shield.h"
#include "Rules/GSTN_CombatRules.h"
#include "Sim/GSTN_DefenceAttach.h"
#include "Sim/GSTN_DefenceFragment.h"
#include "Sim/GSTN_DefenceSnapshotExt.h"
#include "Sync/GSTN_SyncFragment.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr int32 Bubble = GSTNHurtboxIndex::ExtraIndexBase;

	/** The standard rules with GSTNDefence's steps, and the brace inserted before the Parry step as a consumer's content would place it */
	struct FBraceRig
	{
		FBraceRig()
		{
			Defence.Reset(NewObject<UGSTN_DefenceRules>(GetTransientPackage()));
			Defence->bShieldEnabled = true;
			Defence->Shield.MaxShield = 100.f;
			Defence->Shield.StartupFrames = 1;
			Defence->Shield.ReleaseFrames = 6;
			Defence->Parry.WindowFrames = 5;
			Defence->Parry.Anchor = EGSTN_ParryAnchor::Press;
			Combat.Reset(NewObject<UGSTN_CombatRules>(GetTransientPackage()));
			UGSTN_CombatRules::FillStandard(Combat.Get());
			UGSTN_DefenceContentLibrary::AddDefenceStepsToRules(Combat.Get());
			const int32 Parry = Combat->HitPipeline.IndexOfByPredicate([](const TSoftClassPtr<UGSTN_HitResolutionStep>& Step) { return Step.Get() == UGSTN_HitStep_Parry::StaticClass(); });
			Combat->HitPipeline.Insert(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_ExtTestStep_Brace::StaticClass()), FMath::Max(0, Parry));
			Combat->ResolveHitPipeline();
			Ext.RulesIndex = FGSTN_DefenceAttach::IndexOf(Defence.Get());
			Ext.Flags = GSTNDefenceSnapshotFlags::ShieldEnabled | GSTNDefenceSnapshotFlags::KitHasShield;
		}

		~FBraceRig()
		{
			FGSTN_DefenceAttach::Reset();
		}

		void Raise(int32 Press)
		{
			Ext.Flags |= GSTNDefenceSnapshotFlags::Raised | GSTNDefenceSnapshotFlags::Blocks | GSTNDefenceSnapshotFlags::Bubble;
			Ext.ShieldPressFrame = Press;
			Ext.ShieldReleaseFrame = INDEX_NONE;
			Ext.ShieldValueQ = FGSTN_DefenceFragment::QuantizeValue(100.0);
		}

		FGSTN_HitResolutionContext MakeHit(int32 HurtboxIndex, int32 Frame, FGameplayTag HitType)
		{
			FGSTN_HitResolutionContext Context;
			Context.Incoming.Hit.Damage = 10.f;
			Context.Incoming.Hit.BaseKnockback = 60.f;
			Context.Incoming.Hit.KnockbackGrowth = 1.f;
			Context.Incoming.Hit.HitType = HitType;
			Context.Incoming.Frame = Frame;
			Context.Incoming.Relationship = GSTNCombatHitTags::Relationship_Enemy.GetTag();
			Context.Incoming.HitboxToVictim = FVector::ForwardVector;
			Context.HurtboxIndex = static_cast<uint8>(HurtboxIndex);
			FGSTN_HitVictimView::FSnapshotExtensionRef Ref;
			Ref.TypeId = GSTNFeatureIds::Defence;
			Ref.Data = &Ext;
			Context.Victim.SetSnapshotExtensions(&Ref, 1);
			return Context;
		}

		void Run(FGSTN_HitResolutionContext& Context) const
		{
			FGSTN_HitPipelineRunner::Run(*Combat, Context);
		}

		int32 IndexOfStep(const UClass* Class) const
		{
			return Combat->GetResolvedHitPipeline().IndexOfByPredicate([Class](const TSubclassOf<UGSTN_HitResolutionStep>& Step) { return Step.Get() == Class; });
		}

		TStrongObjectPtr<UGSTN_DefenceRules> Defence;
		TStrongObjectPtr<UGSTN_CombatRules> Combat;
		FGSTN_DefenceSnapshotExt Ext;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ExtTestDefenceOrderTest, "GSTNExtTest.Defence.NewStepResolvesInPlace", Flags)
bool FGSTN_ExtTestDefenceOrderTest::RunTest(const FString& Parameters)
{
	FBraceRig Rig;
	const int32 HurtState = Rig.IndexOfStep(UGSTN_HitStep_HurtState::StaticClass());
	const int32 Brace = Rig.IndexOfStep(UGSTN_ExtTestStep_Brace::StaticClass());
	const int32 Parry = Rig.IndexOfStep(UGSTN_HitStep_Parry::StaticClass());
	const int32 Shield = Rig.IndexOfStep(UGSTN_HitStep_Shield::StaticClass());
	const int32 Knockback = Rig.IndexOfStep(UGSTN_HitStep_ComputeKnockback::StaticClass());
	TestTrue(TEXT("the rules resolved the outside step"), Brace != INDEX_NONE);
	TestTrue(TEXT("HurtState < Brace < Parry < Shield < ComputeKnockback"), HurtState < Brace && Brace < Parry && Parry < Shield && Shield < Knockback);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ExtTestDefenceBraceTest, "GSTNExtTest.Defence.NewStepWritesTheOutcome", Flags)
bool FGSTN_ExtTestDefenceBraceTest::RunTest(const FString& Parameters)
{
	FBraceRig Rig;

	// The same body hit, plain and braceable: the brace halves the knockback the model computes, once, and publishes its event
	FGSTN_HitResolutionContext Plain = Rig.MakeHit(0, 150, GSTNCombatHitTags::HitType_Normal.GetTag());
	Rig.Run(Plain);
	FGSTN_HitResolutionContext Braced = Rig.MakeHit(0, 150, GSTNExtTestDefenceTags::HitType_Braceable.GetTag());
	Rig.Run(Braced);
	TestTrue(TEXT("both land with a knockback"), Plain.Outcome.bHit && Braced.Outcome.bHit && Plain.Outcome.bHasKnockback && Braced.Outcome.bHasKnockback);
	TestTrue(TEXT("the plain hit has some knockback"), Plain.Outcome.Knockback.Knockback > 1.f);
	TestEqual(TEXT("the braced knockback is half, merged once"), Braced.Outcome.Knockback.Knockback, 0.5f * Plain.Outcome.Knockback.Knockback, 0.01f);
	TestEqual(TEXT("damage untouched"), Braced.Outcome.Damage, Plain.Outcome.Damage);
	TestTrue(TEXT("the brace's event reached the outcome"), Braced.Outcome.Events.Contains(GSTNExtTestDefenceTags::Event_Braced.GetTag()));
	TestFalse(TEXT("and only for a braceable hit"), Plain.Outcome.Events.Contains(GSTNExtTestDefenceTags::Event_Braced.GetTag()));

	// Pure: a second run on a fresh context gives the same bits
	FGSTN_HitResolutionContext Again = Rig.MakeHit(0, 150, GSTNExtTestDefenceTags::HitType_Braceable.GetTag());
	Rig.Run(Again);
	TestEqual(TEXT("same knockback again"), Again.Outcome.Knockback.Knockback, Braced.Outcome.Knockback.Knockback);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ExtTestDefenceShieldStillDecidesTest, "GSTNExtTest.Defence.ParryAndBlockUnchanged", Flags)
bool FGSTN_ExtTestDefenceShieldStillDecidesTest::RunTest(const FString& Parameters)
{
	FBraceRig Rig;
	Rig.Raise(200);

	// Press-anchored perfect shield of 5 frames: a braceable hit on the bubble inside it is still parried, after it still blocked
	FGSTN_HitResolutionContext Early = Rig.MakeHit(Bubble, 202, GSTNExtTestDefenceTags::HitType_Braceable.GetTag());
	Rig.Run(Early);
	TestTrue(TEXT("inside the window: parried by GSTNDefence's step"), Early.Outcome.bParried);
	TestEqual(TEXT("no damage"), Early.Outcome.Damage, 0.f);
	TestTrue(TEXT("the brace ran before it"), Early.Outcome.Events.Contains(GSTNExtTestDefenceTags::Event_Braced.GetTag()));

	FGSTN_HitResolutionContext Late = Rig.MakeHit(Bubble, 210, GSTNExtTestDefenceTags::HitType_Braceable.GetTag());
	Rig.Run(Late);
	TestFalse(TEXT("after the window: not parried"), Late.Outcome.bParried);
	TestTrue(TEXT("blocked by GSTNDefence's step"), Late.Outcome.bBlocked);
	TestEqual(TEXT("a block does no damage"), Late.Outcome.Damage, 0.f);
	return true;
}

#endif

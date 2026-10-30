// Automation tests of M6 F8 (m6 spec 5.5, 6 with the owner's revision): the landing from a tumble (stick-steered bounce, downed state, get-ups),
// the optional tech, the DI rules overload and the held-direction SDI drift. No simulation component: the rules of FGSTN_TumbleLanding,
// FGSTN_DIWiring and FGSTN_SDIDrift are driven on hand-made combat states and a stand-in for the model's services, as the F7 tests drive the
// tick extension.

#include "Tests/GSTN_DefenceNPTestFixture.h"
#include "Abilities/GSTN_DefenceAbilities.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitResolution.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "Sim/GSTN_DefenceSnapshotExt.h"
#include "Data/GSTN_DefenceKitExtension.h"
#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"
#include "GSTN_DefenceLibrary.h"
#include "GSTN_DefenceTags.h"
#include "Hits/GSTN_ImpactTypes.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_InputSample.h"
#include "Math/GSTN_DefenceMath.h"
#include "Math/GSTN_DIRulesMath.h"
#include "Math/GSTN_SDIMath.h"
#include "Movement/GSTN_LayeredMove_Freeze.h"
#include "Movement/GSTN_TumbleContact.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Sim/GSTN_DIWiring.h"
#include "Sim/GSTN_SDIDrift.h"
#include "Sim/GSTN_TumbleLanding.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTickExtension.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNDefenceNPTest;

namespace
{
	/** The model's services as the tests see them: every class a rule asked to start Forced */
	struct FFakeServices : IGSTN_CombatTickServices
	{
		TArray<const UClass*> Started;

		/** The state and frame of the tick: a start runs the timeline there, as the model does */
		FGSTN_CombatSyncCore* Core = nullptr;
		int32 Frame = INDEX_NONE;

		virtual bool StartForcedTimeline(const UClass* AbilityClass) override
		{
			if (!AbilityClass)
			{
				return false;
			}
			Started.Add(AbilityClass);
			if (Core)
			{
				FGSTN_CombatTimelineLogic::Start(*Core, Frame, Started.Num(), EGSTN_ActivationSource::Forced);
			}
			return true;
		}
	};

	/** One entity with defence data, a tumbling combat state and a hook context over it */
	struct FLandRig
	{
		explicit FLandRig(bool bTechSet, int32 MaxDownedFrames = 0)
		{
			Rules.Reset(MakeRules());
			Rules->GroundBounce.MaxDownedFrames = MaxDownedFrames;
			FGSTN_DefenceAttach::Apply(Rig.Owner, Rules.Get(), true, FGameplayTagContainer());
			FGSTN_DefenceKitExtension Kit;
			Kit.GetupSet.GroundBounce = UGSTN_GA_GroundBounce::StaticClass();
			Kit.GetupSet.Roll = UGSTN_GA_GetupRoll::StaticClass();
			if (bTechSet)
			{
				for (const EGSTN_TechType Type : { EGSTN_TechType::InPlace, EGSTN_TechType::RollLeft, EGSTN_TechType::RollRight, EGSTN_TechType::WallTech })
				{
					Kit.Tech.Abilities.Add(Type, UGSTN_GA_Tech::StaticClass());
				}
			}
			FGSTN_DefenceAttach::SetKitMoves(Rig.Owner, &Kit);
			Out.Core.ActionState = EGSTN_ActionState::Tumble;
			Out.Core.HitstopStartFrame = 40;
			Out.Core.HitstopEndFrame = 48;
			Out.Core.Hit.HitstunEndFrame = 200;
		}

		const FGSTN_DefenceEntityData& Data() const { return *FGSTN_DefenceAttach::Find(Rig.Owner); }

		/** A context of Frame over In/Out, the shield button on bit 0 */
		FGSTN_CombatTickContext Make(int32 Frame, const FVector2D& Stick = FVector2D::ZeroVector)
		{
			FGSTN_CombatTickContext Context(Frame, In, Out);
			Context.Owner = Rig.Owner;
			Context.AbilitySystem = Rig.ASC;
			Context.Buttons = &Map;
			Context.Services = &Services;
			Services.Core = &Out.Core;
			Services.Frame = Frame;
			Context.Stick = Stick;
			Context.StickOnPlane = FVector(Stick.Y, Stick.X, 0.0);
			return Context;
		}

		/** The shield button pressed on Frame (one frame) in the combat state's history */
		void Press(int32 Frame)
		{
			FGSTN_InputSample Down;
			Down.Frame = Frame;
			Down.Held = 1u;
			Down.Pressed = 1u;
			Out.Core.Input.History.Append(Down);
			FGSTN_InputSample Up;
			Up.Frame = Frame + 1;
			Up.Released = 1u;
			Out.Core.Input.History.Append(Up);
		}

		static FGSTN_TumbleContactSyncState Contact(EGSTN_TumbleContactKind Kind, int32 Frame, const FVector& Normal)
		{
			FGSTN_TumbleContactSyncState Contact;
			Contact.Kind = Kind;
			Contact.ContactFrame = Frame;
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				Contact.NormalQ[Axis] = static_cast<int16>(FMath::RoundToInt(Normal[Axis] * 32767.0));
			}
			Contact.ImpactSpeedQ = 600;
			return Contact;
		}

		FGSTN_DefenceFragment Fragment() const { return ReadFragment(Out.Fragments); }

		FRig Rig;
		TStrongObjectPtr<UGSTN_DefenceRules> Rules;
		FGSTN_CombatSyncState In;
		FGSTN_CombatSyncState Out;
		const FGameplayTag ButtonTags[1] = { GSTNCombatButtonTags::Button_Shield.GetTag() };
		FGSTN_ButtonMap Map{ ButtonTags };
		FFakeServices Services;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceLandingBounceSteeredTest, "GSTNDefence.NP.Landing.BounceSteered", TestFlags)
bool FGSTN_DefenceLandingBounceSteeredTest::RunTest(const FString& Parameters)
{
	FLandRig Land(false);
	FGSTN_CombatTickContext Context = Land.Make(100, FVector2D(0.0, 1.0));
	FGSTN_TumbleLanding::OnContactResolved(Context, Land.Data(), FLandRig::Contact(EGSTN_TumbleContactKind::Floor, 99, FVector::UpVector), EGSTN_ImpactOutcome::None);
	const FGSTN_DefenceFragment Fragment = Land.Fragment();
	TestEqual(TEXT("the landing ends the tumble"), static_cast<int32>(Land.Out.Core.ActionState), static_cast<int32>(EGSTN_ActionState::Actionable));
	TestEqual(TEXT("downed with no automatic get-up (MaxDownedFrames 0)"), Fragment.KnockdownEndFrame, FGSTN_TumbleLanding::NeverEnds);
	TestEqual(TEXT("the ground bounce is pending for this frame"), static_cast<int32>(Fragment.PendingStartKind), static_cast<int32>(GSTNDefencePendingStart::GroundBounce));

	// The bounce follows the stick: forward stick tilts it forward, backward stick backward, neutral straight up, within the steer angle
	const FGSTN_GroundBounceRules& Rules = Land.Rules->GroundBounce;
	const FVector Forward = UGSTN_DefenceMath::GroundBounceVelocity(Rules, FVector::UpVector, FVector(1.0, 0.0, 0.0));
	const FVector Back = UGSTN_DefenceMath::GroundBounceVelocity(Rules, FVector::UpVector, FVector(-1.0, 0.0, 0.0));
	const FVector Neutral = UGSTN_DefenceMath::GroundBounceVelocity(Rules, FVector::UpVector, FVector::ZeroVector);
	TestTrue(TEXT("forward stick, forward bounce"), Forward.X > 1.0 && Forward.Z > 0.0);
	TestTrue(TEXT("backward stick, backward bounce"), Back.X < -1.0 && Back.Z > 0.0);
	TestTrue(TEXT("neutral stick, straight up"), FMath::IsNearlyZero(Neutral.X, 1e-3) && Neutral.Z > 0.0);
	const double Tilt = FMath::RadiansToDegrees(FMath::Acos(Forward.GetSafeNormal().Dot(FVector::UpVector)));
	TestTrue(TEXT("tilted by the steer angle at most"), Tilt <= Rules.GroundBounceSteerDeg + 0.01);

	// AfterClocks of the same frame starts the kit's ground bounce Forced and clears the pending start
	FGSTN_TumbleLanding::AfterClocks(Context, Land.Data());
	TestTrue(TEXT("the ground bounce started Forced"), Land.Services.Started.Num() == 1 && Land.Services.Started[0] == UGSTN_GA_GroundBounce::StaticClass());
	TestEqual(TEXT("the pending start is consumed"), Land.Fragment().PendingStartFrame, static_cast<int32>(INDEX_NONE));

	// A surface impact (a trampoline floor) replaces the landing
	FLandRig Impact(false);
	FGSTN_CombatTickContext ImpactContext = Impact.Make(100);
	FGSTN_TumbleLanding::OnContactResolved(ImpactContext, Impact.Data(), FLandRig::Contact(EGSTN_TumbleContactKind::Floor, 99, FVector::UpVector), EGSTN_ImpactOutcome::Bounce);
	TestEqual(TEXT("an impact outcome: no downed state"), Impact.Fragment().KnockdownEndFrame, static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceLandingDownedRollTest, "GSTNDefence.NP.Landing.DownedThenGetupRoll", TestFlags)
bool FGSTN_DefenceLandingDownedRollTest::RunTest(const FString& Parameters)
{
	FLandRig Land(false);
	FGSTN_CombatTickContext Landing = Land.Make(100);
	FGSTN_TumbleLanding::OnContactResolved(Landing, Land.Data(), FLandRig::Contact(EGSTN_TumbleContactKind::Floor, 99, FVector::UpVector), EGSTN_ImpactOutcome::None);
	FGSTN_TumbleLanding::AfterClocks(Landing, Land.Data());
	Land.Services.Started.Reset();
	// The bounce's timeline has played out
	Land.Out.Core.ActionState = EGSTN_ActionState::Actionable;

	// Lying on the ground, no input: still downed, and the gate admits only the get-ups
	FGSTN_CombatTickContext Still = Land.Make(140);
	FGSTN_TumbleLanding::AfterClocks(Still, Land.Data());
	TestTrue(TEXT("no input: still downed"), Land.Fragment().IsKnockedDown(140));
	TestTrue(TEXT("downed: the gate admits only Combat.State.Knockdown bindings"), FGSTN_TumbleLanding::AdmitsOnly(Still) == GSTNDefenceTags::State_Knockdown.GetTag());
	TestEqual(TEXT("nothing started"), Land.Services.Started.Num(), 0);

	// Holding the stick: the get-up roll starts Forced and the downed state ends on its frame
	FGSTN_CombatTickContext Roll = Land.Make(141, FVector2D(0.8, 0.0));
	FGSTN_TumbleLanding::AfterClocks(Roll, Land.Data());
	TestTrue(TEXT("the roll started Forced"), Land.Services.Started.Num() == 1 && Land.Services.Started[0] == UGSTN_GA_GetupRoll::StaticClass());
	TestFalse(TEXT("up from the roll's frame"), Land.Fragment().IsKnockedDown(141));
	TestFalse(TEXT("the gate is open again"), FGSTN_TumbleLanding::AdmitsOnly(Land.Make(142)).IsValid());

	// During the bounce (a timeline runs) the stick does not roll
	FLandRig Busy(false);
	FGSTN_CombatTickContext BusyLanding = Busy.Make(100);
	FGSTN_TumbleLanding::OnContactResolved(BusyLanding, Busy.Data(), FLandRig::Contact(EGSTN_TumbleContactKind::Floor, 99, FVector::UpVector), EGSTN_ImpactOutcome::None);
	FGSTN_CombatTimelineLogic::Start(Busy.Out.Core, 100, 0, EGSTN_ActivationSource::Forced);
	FGSTN_CombatTickContext BusyRoll = Busy.Make(101, FVector2D(0.8, 0.0));
	FGSTN_TumbleLanding::AfterClocks(BusyRoll, Busy.Data());
	TestFalse(TEXT("no roll while the bounce plays"), Busy.Services.Started.Contains(UGSTN_GA_GetupRoll::StaticClass()));
	TestTrue(TEXT("still downed"), Busy.Fragment().IsKnockedDown(101));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceLandingGetupAttackTest, "GSTNDefence.NP.Landing.GetupAttackFromSlot", TestFlags)
bool FGSTN_DefenceLandingGetupAttackTest::RunTest(const FString& Parameters)
{
	FLandRig Land(false);
	FGSTN_DefenceFragment Downed;
	Downed.KnockdownEndFrame = FGSTN_TumbleLanding::NeverEnds;
	SetFragment(Land.Out.Fragments, Downed);
	Land.Out.Core.ActionState = EGSTN_ActionState::Actionable;

	// A slot binding requiring Combat.State.Knockdown is what the gate admits (the kit's get-up attacks); a plain attack binding is not
	FGSTN_TagRequirements GetupAttack;
	GetupAttack.RequireTags.AddTag(GSTNDefenceTags::State_Knockdown);
	const FGSTN_TagRequirements PlainAttack;
	const FGameplayTag Admit = FGSTN_TumbleLanding::AdmitsOnly(Land.Make(150));
	TestTrue(TEXT("the get-up attack binding is admitted"), GetupAttack.RequireTags.HasTagExact(Admit));
	TestFalse(TEXT("a plain attack binding is not"), PlainAttack.RequireTags.HasTagExact(Admit));

	// The get-up attack starts from its command: the downed state ends on that frame
	FGSTN_CombatTimelineLogic::Start(Land.Out.Core, 150, 2, EGSTN_ActivationSource::Command);
	FGSTN_CombatTickContext Attack = Land.Make(150);
	FGSTN_TumbleLanding::AfterExecute(Attack);
	TestFalse(TEXT("up from the get-up attack's frame"), Land.Fragment().IsKnockedDown(150));

	// A Forced timeline (the bounce) does not get the entity up
	FLandRig Bounce(false);
	SetFragment(Bounce.Out.Fragments, Downed);
	FGSTN_CombatTimelineLogic::Start(Bounce.Out.Core, 150, 1, EGSTN_ActivationSource::Forced);
	FGSTN_CombatTickContext Forced = Bounce.Make(150);
	FGSTN_TumbleLanding::AfterExecute(Forced);
	TestTrue(TEXT("a Forced timeline keeps the entity downed"), Bounce.Fragment().IsKnockedDown(150));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceLandingJumpGetsUpTest, "GSTNDefence.NP.Landing.JumpGetsUp", TestFlags)
bool FGSTN_DefenceLandingJumpGetsUpTest::RunTest(const FString& Parameters)
{
	// The neutral get-up binding (on the jump slot) requires exactly the tag the downed state admits
	const FGSTN_TagRequirements Neutral = UGSTN_DefenceLibrary::GetupNeutralRequirements();
	FLandRig Land(false);
	FGSTN_DefenceFragment Downed;
	Downed.KnockdownEndFrame = FGSTN_TumbleLanding::NeverEnds;
	SetFragment(Land.Out.Fragments, Downed);
	Land.Out.Core.ActionState = EGSTN_ActionState::Actionable;
	TestTrue(TEXT("the jump get-up is admitted while downed"), Neutral.RequireTags.HasTagExact(FGSTN_TumbleLanding::AdmitsOnly(Land.Make(120))));

	// Its command starts it: up on that frame, and the movement lock follows on the tag projection
	FGSTN_CombatTickContext Lying = Land.Make(119);
	FGSTN_TumbleLanding::SyncDownedLock(Lying);
	TestEqual(TEXT("downed: the movement lock is on"), Land.Rig.ASC->GetGameplayEffectCount(UGSTN_GE_DownedLock::StaticClass(), nullptr), 1);
	FGSTN_CombatTimelineLogic::Start(Land.Out.Core, 120, 4, EGSTN_ActivationSource::Command);
	FGSTN_CombatTickContext Jump = Land.Make(120);
	FGSTN_TumbleLanding::AfterExecute(Jump);
	FGSTN_TumbleLanding::SyncDownedLock(Jump);
	TestFalse(TEXT("up from the jump's frame"), Land.Fragment().IsKnockedDown(120));
	TestEqual(TEXT("up: the movement lock is off"), Land.Rig.ASC->GetGameplayEffectCount(UGSTN_GE_DownedLock::StaticClass(), nullptr), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceLandingMaxDownedTest, "GSTNDefence.NP.Landing.MaxDownedFrames", TestFlags)
bool FGSTN_DefenceLandingMaxDownedTest::RunTest(const FString& Parameters)
{
	FLandRig Land(false, 30);
	FGSTN_CombatTickContext Context = Land.Make(100);
	FGSTN_TumbleLanding::OnContactResolved(Context, Land.Data(), FLandRig::Contact(EGSTN_TumbleContactKind::Floor, 99, FVector::UpVector), EGSTN_ImpactOutcome::None);
	const FGSTN_DefenceFragment Fragment = Land.Fragment();
	TestEqual(TEXT("downed for MaxDownedFrames from the contact"), Fragment.KnockdownEndFrame, 99 + 30);
	TestTrue(TEXT("still downed on the last frame"), Fragment.IsKnockedDown(128));
	TestFalse(TEXT("up by itself after it"), Fragment.IsKnockedDown(129));

	// A kit with a tech set that missed: the spec's knockdown with the price of not teching
	FLandRig Missed(true);
	FGSTN_CombatTickContext MissedContext = Missed.Make(100);
	FGSTN_TumbleLanding::OnContactResolved(MissedContext, Missed.Data(), FLandRig::Contact(EGSTN_TumbleContactKind::Floor, 99, FVector::UpVector), EGSTN_ImpactOutcome::None);
	TestEqual(TEXT("missed tech: KnockdownFrames + NoTechKnockdownBonusFrames"), Missed.Fragment().KnockdownEndFrame,
		99 + Missed.Rules->Tech.KnockdownFrames + Missed.Rules->Tech.NoTechKnockdownBonusFrames);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceLandingTechSuccessTest, "GSTNDefence.NP.Landing.TechSuccessCancelsImpact", TestFlags)
bool FGSTN_DefenceLandingTechSuccessTest::RunTest(const FString& Parameters)
{
	// Pressed 4 frames before a floor contact, after the hitstop: a tech in place (neutral stick)
	FLandRig Land(true);
	Land.Press(95);
	FGSTN_CombatTickContext Context = Land.Make(100);
	const bool bConsumed = FGSTN_TumbleLanding::ConsumeContact(Context, Land.Data(), FLandRig::Contact(EGSTN_TumbleContactKind::Floor, 99, FVector::UpVector));
	TestTrue(TEXT("the tech consumes the contact: no impact is resolved"), bConsumed);
	const FGSTN_DefenceFragment Fragment = Land.Fragment();
	TestEqual(TEXT("tech in place"), static_cast<int32>(Fragment.TechType), static_cast<int32>(EGSTN_TechType::InPlace));
	TestEqual(TEXT("teching for the in-place frames"), Fragment.TechEndFrame, 100 + Land.Rules->Tech.InPlaceFrames);
	TestEqual(TEXT("a success clears the lockout"), Fragment.TechLockoutEndFrame, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("the tumble ends"), static_cast<int32>(Land.Out.Core.ActionState), static_cast<int32>(EGSTN_ActionState::Actionable));
	TestEqual(TEXT("not downed"), Fragment.KnockdownEndFrame, static_cast<int32>(INDEX_NONE));
	FGSTN_TumbleLanding::AfterClocks(Context, Land.Data());
	TestTrue(TEXT("the tech ability started Forced"), Land.Services.Started.Num() == 1 && Land.Services.Started[0] == UGSTN_GA_Tech::StaticClass());

	// The kind follows the stick and the contact: a roll to the right, a wall tech
	FLandRig Roll(true);
	Roll.Press(96);
	FGSTN_CombatTickContext RollContext = Roll.Make(100, FVector2D(0.9, 0.0));
	TestTrue(TEXT("roll tech"), FGSTN_TumbleLanding::ConsumeContact(RollContext, Roll.Data(), FLandRig::Contact(EGSTN_TumbleContactKind::Floor, 99, FVector::UpVector)));
	TestEqual(TEXT("stick right: roll right"), static_cast<int32>(Roll.Fragment().TechType), static_cast<int32>(EGSTN_TechType::RollRight));
	FLandRig Wall(true);
	Wall.Press(97);
	FGSTN_CombatTickContext WallContext = Wall.Make(100);
	TestTrue(TEXT("wall tech"), FGSTN_TumbleLanding::ConsumeContact(WallContext, Wall.Data(), FLandRig::Contact(EGSTN_TumbleContactKind::Wall, 99, FVector(-1.0, 0.0, 0.0))));
	TestEqual(TEXT("a wall contact: wall tech"), static_cast<int32>(Wall.Fragment().TechType), static_cast<int32>(EGSTN_TechType::WallTech));

	// The same decision on a re-simulation of the frame from the same state (forced reconcile): the press comes from the history
	FLandRig Again(true);
	Again.Press(95);
	FGSTN_CombatTickContext AgainContext = Again.Make(100);
	FGSTN_TumbleLanding::ConsumeContact(AgainContext, Again.Data(), FLandRig::Contact(EGSTN_TumbleContactKind::Floor, 99, FVector::UpVector));
	TestTrue(TEXT("identical fragment after a replay"), Again.Fragment() == Fragment);

	// The hit's bNoTech goes straight to the impact
	FLandRig NoTech(true);
	NoTech.Press(95);
	FGSTN_ImpactProfile Profile;
	Profile.bNoTech = true;
	NoTech.Out.Core.Hit.ImpactProfileQ = Profile.Pack();
	FGSTN_CombatTickContext NoTechContext = NoTech.Make(100);
	TestFalse(TEXT("bNoTech: no tech"), FGSTN_TumbleLanding::ConsumeContact(NoTechContext, NoTech.Data(), FLandRig::Contact(EGSTN_TumbleContactKind::Floor, 99, FVector::UpVector)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceLandingTechFailureTest, "GSTNDefence.NP.Landing.TechFailureResolvesImpactFirst", TestFlags)
bool FGSTN_DefenceLandingTechFailureTest::RunTest(const FString& Parameters)
{
	// Pressed 39 frames before the contact: outside the 20 frame window, a miss
	FLandRig Land(true);
	Land.Press(60);
	FGSTN_CombatTickContext Context = Land.Make(100);
	const FGSTN_TumbleContactSyncState Wall = FLandRig::Contact(EGSTN_TumbleContactKind::Wall, 99, FVector(-1.0, 0.0, 0.0));
	TestFalse(TEXT("a miss returns the contact to the impact"), FGSTN_TumbleLanding::ConsumeContact(Context, Land.Data(), Wall));
	TestEqual(TEXT("the press counted: lockout from it"), Land.Fragment().TechLockoutEndFrame, 60 + Land.Rules->Tech.LockoutFrames);
	TestEqual(TEXT("still tumbling: the impact decides"), static_cast<int32>(Land.Out.Core.ActionState), static_cast<int32>(EGSTN_ActionState::Tumble));

	// The impact resolved first (a splat): the landing does nothing on top of it
	FGSTN_TumbleLanding::OnContactResolved(Context, Land.Data(), Wall, EGSTN_ImpactOutcome::Splat);
	TestEqual(TEXT("the impact's outcome stands: no knockdown"), Land.Fragment().KnockdownEndFrame, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("no tech"), Land.Fragment().TechEndFrame, static_cast<int32>(INDEX_NONE));

	// A missed floor contact with no impact outcome: the knockdown of the spec
	FLandRig Floor(true);
	Floor.Press(60);
	FGSTN_CombatTickContext FloorContext = Floor.Make(100);
	const FGSTN_TumbleContactSyncState Ground = FLandRig::Contact(EGSTN_TumbleContactKind::Floor, 99, FVector::UpVector);
	TestFalse(TEXT("floor miss"), FGSTN_TumbleLanding::ConsumeContact(FloorContext, Floor.Data(), Ground));
	FGSTN_TumbleLanding::OnContactResolved(FloorContext, Floor.Data(), Ground, EGSTN_ImpactOutcome::None);
	TestTrue(TEXT("downed after the miss"), Floor.Fragment().IsKnockedDown(100));

	// Mashing during the lockout does not extend it
	FLandRig Mash(true);
	Mash.Press(60);
	Mash.Press(70);
	Mash.Press(80);
	FGSTN_CombatTickContext MashContext = Mash.Make(90);
	FGSTN_TumbleLanding::AfterClocks(MashContext, Mash.Data());
	TestEqual(TEXT("presses in the lockout do not extend it"), Mash.Fragment().TechLockoutEndFrame, 60 + Mash.Rules->Tech.LockoutFrames);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceLandingNoTechSetTest, "GSTNDefence.NP.Landing.NoTechSetNoLockout", TestFlags)
bool FGSTN_DefenceLandingNoTechSetTest::RunTest(const FString& Parameters)
{
	FLandRig Land(false);
	Land.Press(95);
	FGSTN_CombatTickContext Air = Land.Make(97);
	FGSTN_TumbleLanding::AfterClocks(Air, Land.Data());
	TestEqual(TEXT("no tech set: the press starts no lockout"), Land.Fragment().TechLockoutEndFrame, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("and stamps no press"), Land.Fragment().LastTechPressFrame, static_cast<int32>(INDEX_NONE));
	FGSTN_CombatTickContext Context = Land.Make(100);
	TestFalse(TEXT("no tech: the contact goes to the impact"), FGSTN_TumbleLanding::ConsumeContact(Context, Land.Data(), FLandRig::Contact(EGSTN_TumbleContactKind::Floor, 99, FVector::UpVector)));
	TestFalse(TEXT("no lockout tag source"), Land.Fragment().IsTechLockedOut(100));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceDIRulesOverloadTest, "GSTNDefence.NP.DI.RulesOverloadApplied", TestFlags)
bool FGSTN_DefenceDIRulesOverloadTest::RunTest(const FString& Parameters)
{
	FLandRig Land(false);
	Land.Rules->DI.Params.MaxDIYawDegrees = 15.f;
	const FVector Launch(1000.0, 0.0, 600.0);
	const FVector Stick(0.0, 1.0, 0.0);
	FGSTN_CombatTickContext Context = Land.Make(48, FVector2D(1.0, 0.0));
	FVector Out = FVector::ZeroVector;
	TestTrue(TEXT("an entity with defence data takes the defence's DI"), FGSTN_DIWiring::OverrideDI(Context, Launch, Stick, FVector::UpVector, FVector::ForwardVector, Out));
	const FVector Expected = UGSTN_DIRulesMath::ApplyDI(Launch, Stick, FVector::UpVector, FVector::ForwardVector, Launch.Size(), FGameplayTagContainer(), Land.Rules->DI, false);
	TestTrue(TEXT("the rules' DI math is what applies"), Out.Equals(Expected, 1e-3));
	TestFalse(TEXT("and it bends the launch"), Out.Equals(Launch, 1.0));

	// The hit said bDisableDI: the launch is kept
	FGSTN_DefenceFragment Flags;
	Flags.HitFlags = GSTNDefenceHitFlags::DIDisabled;
	SetFragment(Land.Out.Fragments, Flags);
	FGSTN_DIWiring::OverrideDI(Context, Launch, Stick, FVector::UpVector, FVector::ForwardVector, Out);
	TestTrue(TEXT("bDisableDI: no DI"), Out.Equals(Launch, 1e-3));

	// An entity without defence data keeps the core's DI
	FGSTN_CombatSyncState In;
	FGSTN_CombatSyncState Plain;
	FGSTN_CombatTickContext None(48, In, Plain);
	None.Owner = MakeOwner();
	TestFalse(TEXT("no defence data: the core's DI"), FGSTN_DIWiring::OverrideDI(None, Launch, Stick, FVector::UpVector, FVector::ForwardVector, Out));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceSDIDriftSweptTest, "GSTNDefence.NP.SDI.DriftSweptByMover", TestFlags)
bool FGSTN_DefenceSDIDriftSweptTest::RunTest(const FString& Parameters)
{
	// The drift is the velocity of an OverrideAll freeze move: Mover sweeps it like any proposed move (a wall stops it)
	FGSTN_LayeredMove_Freeze Freeze;
	Freeze.Setup(FVector(300.0, 0.0, 0.0), 16.f, false);
	Freeze.DriftVelocity = FVector(0.0, 120.0, 0.0);
	FProposedMove Move;
	FMoverTickStartData StartData;
	FMoverTimeStep TimeStep;
	Freeze.GenerateMove(StartData, TimeStep, nullptr, nullptr, Move);
	TestEqual(TEXT("the move overrides everything"), static_cast<int32>(Move.MixMode), static_cast<int32>(EMoveMixMode::OverrideAll));
	TestTrue(TEXT("with the drift as its velocity"), Move.LinearVelocity.Equals(FVector(0.0, 120.0, 0.0)));

	// The drift survives the wire; a plain freeze stays a plain freeze
	FBitWriter Writer(1024, true);
	Freeze.NetSerialize(Writer);
	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	FGSTN_LayeredMove_Freeze Loaded;
	Loaded.NetSerialize(Reader);
	TestTrue(TEXT("the drift survives the wire"), Loaded.DriftVelocity.Equals(Freeze.DriftVelocity));
	FGSTN_LayeredMove_Freeze Plain;
	FProposedMove Still;
	Plain.GenerateMove(StartData, TimeStep, nullptr, nullptr, Still);
	TestTrue(TEXT("a plain freeze holds still"), Still.LinearVelocity.IsZero());

	// The drift effect queues the drift creator (above the hitstop freeze)
	const UGSTN_GE_SDIDrift* Effect = GetDefault<UGSTN_GE_SDIDrift>();
	const UApplyMoveGameplayEffectComponent* ApplyMove = Effect->FindComponent<UApplyMoveGameplayEffectComponent>();
	const UGSTN_FreezeMoveCreator* Creator = ApplyMove ? Cast<UGSTN_FreezeMoveCreator>(ApplyMove->MoveProcessor) : nullptr;
	TestTrue(TEXT("the drift effect uses the drift creator"), Creator && Creator->bDriftFromSetByCaller);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceSDIMaxDistanceTest, "GSTNDefence.NP.SDI.MaxDistancePerHit", TestFlags)
bool FGSTN_DefenceSDIMaxDistanceTest::RunTest(const FString& Parameters)
{
	FGSTN_SDIRules Rules;
	Rules.DriftSpeedCmPerSec = 200.f;
	Rules.MaxDistanceCmPerHit = 12.f;
	FGSTN_CombatSyncCore Core;
	Core.ActionState = EGSTN_ActionState::Hitstun;
	Core.HitstopStartFrame = 100;
	Core.HitstopEndFrame = 110;
	double Total = 0.0;
	int32 DriftFrames = 0;
	for (int32 Frame = 100; Frame < 112; ++Frame)
	{
		const FGSTN_SDIStep Step = FGSTN_SDIDrift::Step(Core, Frame, Rules, 0, FVector(1.0, 0.0, 0.0), FGameplayTagContainer());
		Total += Step.DistanceAdded;
		DriftFrames += Step.DriftVelocity.IsZero() ? 0 : 1;
	}
	// 200 cm/s is 3.2 cm a frame: three full frames and the remainder reach the 12 cm cap; the last window frame never drifts. The used distance is
	// stored rounded up to an eighth of a cm, so the drift may stop up to three eighths short of the cap, never past it
	TestTrue(TEXT("the total drift stops at the maximum, never past it"), Total <= 12.0 + 1e-6 && Total >= 12.0 - 3.0 / FGSTN_HitSyncState::SDIUnitsPerCm);
	TestEqual(TEXT("the distance used is in the core state, at the cap"), static_cast<int32>(Core.Hit.SDIDistanceUsed), static_cast<int32>(12.0 * FGSTN_HitSyncState::SDIUnitsPerCm));
	TestEqual(TEXT("frames that drifted"), DriftFrames, 4);
	TestFalse(TEXT("not on the window's last frame"), FGSTN_SDIDrift::IsDriftFrame(Core, 109));
	TestFalse(TEXT("not outside the window"), FGSTN_SDIDrift::IsDriftFrame(Core, 110));
	FGSTN_CombatSyncCore Attacker = Core;
	Attacker.ActionState = EGSTN_ActionState::InTimeline;
	TestFalse(TEXT("only a staggered victim drifts"), FGSTN_SDIDrift::IsDriftFrame(Attacker, 101));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceSDIThrowBlocksTest, "GSTNDefence.NP.SDI.ThrowBlocksSDI", TestFlags)
bool FGSTN_DefenceSDIThrowBlocksTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_DefenceRules> Rules(MakeRules());
	FGSTN_DefenceSnapshotExt Ext;
	Ext.RulesIndex = FGSTN_DefenceAttach::IndexOf(Rules.Get());
	FGSTN_HitVictimView::FSnapshotExtensionRef Ref;
	Ref.TypeId = GSTNFeatureIds::Defence;
	Ref.Data = &Ext;

	// A grab-like hit (the throw family) blocks the drift; a normal hit does not; the hit's bDisableDI is recorded beside it
	FGSTN_HitContextView Throw;
	Throw.Victim.SetSnapshotExtensions(&Ref, 1);
	Throw.Incoming.Hit.HitType = GSTNCombatHitTags::HitType_Grab.GetTag();
	TestTrue(TEXT("a throw blocks SDI"), (FGSTN_DIWiring::MakeHitFlags(Throw) & GSTNDefenceHitFlags::SDIBlocked) != 0);
	FGSTN_HitContextView Normal;
	Normal.Victim.SetSnapshotExtensions(&Ref, 1);
	Normal.Incoming.Hit.bDisableDI = true;
	const uint8 NormalFlags = FGSTN_DIWiring::MakeHitFlags(Normal);
	TestEqual(TEXT("a normal hit drifts"), NormalFlags & GSTNDefenceHitFlags::SDIBlocked, 0);
	TestTrue(TEXT("its bDisableDI is recorded"), (NormalFlags & GSTNDefenceHitFlags::DIDisabled) != 0);
	FGSTN_HitContextView Armored = Normal;
	Armored.HurtState = EGSTN_HurtState::Armor;
	TestTrue(TEXT("armor blocks SDI by default"), (FGSTN_DIWiring::MakeHitFlags(Armored) & GSTNDefenceHitFlags::SDIBlocked) != 0);

	// A blocked hit drifts nothing and uses no distance
	FGSTN_CombatSyncCore Core;
	Core.ActionState = EGSTN_ActionState::Hitstun;
	Core.HitstopStartFrame = 100;
	Core.HitstopEndFrame = 110;
	const FGSTN_SDIStep Step = FGSTN_SDIDrift::Step(Core, 101, Rules->SDI, GSTNDefenceHitFlags::SDIBlocked, FVector(1.0, 0.0, 0.0), FGameplayTagContainer());
	TestTrue(TEXT("no drift on a throw"), Step.DriftVelocity.IsZero() && Step.bBlocked);
	TestEqual(TEXT("no distance used"), static_cast<int32>(Core.Hit.SDIDistanceUsed), 0);
	FGSTN_DefenceAttach::Reset();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Automation tests of the defence plugin: the parry, one resolution for two sources (plan F4).
// Goldens are hand values; each test says how it was derived.

#include "Data/GSTN_DefenceRules.h"
#include "Data/GSTN_DefenceTracks.h"
#include "GSTN_DefenceTags.h"
#include "GSTN_DefenceTestUtils.h"
#include "Math/GSTN_DefenceMath.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A hit on the bubble of a victim that pressed at 100 and released at 150, at the given frame */
	FGSTN_ParryContext ParryTestShieldContext(int32 HitFrame, int32 Release = 150)
	{
		FGSTN_ParryContext Context;
		Context.HitFrame = HitFrame;
		Context.bHitOnBubble = true;
		Context.bShieldAvailable = true;
		Context.ShieldPressFrame = 100;
		Context.ShieldReleaseFrame = Release;
		return Context;
	}

	/** A hit on a body hurtbox of a victim whose move opened a parry window [10, 20) on profile 1 */
	FGSTN_ParryContext ParryTestMoveContext(int32 HitFrame)
	{
		FGSTN_ParryContext Context;
		Context.HitFrame = HitFrame;
		Context.MoveStartFrame = 10;
		Context.MoveEndFrame = 20;
		Context.MoveProfileIndex = 1;
		Context.bMoveCoversHurtbox = true;
		return Context;
	}

	/** Two profiles: 0 is the shield's, 1 is "Counter" with a longer stun */
	UGSTN_DefenceRules* ParryTestRules()
	{
		UGSTN_DefenceRules* Rules = GSTNDefenceTest::MakeRules();
		Rules->ParryProfiles[0].AttackerStunFrames = 20;
		FGSTN_ParryProfile Counter;
		Counter.Name = TEXT("Counter");
		Counter.AttackerStunFrames = 40;
		Rules->ParryProfiles.Add(Counter);
		Rules->Parry.Anchor = EGSTN_ParryAnchor::Release;
		Rules->Parry.WindowFrames = 5;
		Rules->Parry.MinHeldFramesBeforeRelease = 0;
		Rules->Parry.ProfileIndex = 0;
		return Rules;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceParryReleaseAnchorTest, "GSTNDefence.Core.Parry.ReleaseAnchorEdges", GSTNDefenceTest::Flags)
bool FGSTN_DefenceParryReleaseAnchorTest::RunTest(const FString& Parameters)
{
	// Release anchor (Ultimate): window [Release, Release + 5) = [150, 155)
	FGSTN_ParryRules Rules;
	Rules.Anchor = EGSTN_ParryAnchor::Release;
	Rules.WindowFrames = 5;
	Rules.MinHeldFramesBeforeRelease = 0;

	TestFalse(TEXT("One frame before the release"), UGSTN_DefenceMath::EvaluateParryWindow(100, 149, 150, Rules));
	TestTrue(TEXT("The release frame"), UGSTN_DefenceMath::EvaluateParryWindow(100, 150, 150, Rules));
	TestTrue(TEXT("The last frame of the window"), UGSTN_DefenceMath::EvaluateParryWindow(100, 154, 150, Rules));
	TestFalse(TEXT("One frame past the window"), UGSTN_DefenceMath::EvaluateParryWindow(100, 155, 150, Rules));
	TestFalse(TEXT("A press-anchored hit frame does not count: the press frame itself"), UGSTN_DefenceMath::EvaluateParryWindow(100, 100, 150, Rules));
	TestFalse(TEXT("A shield still held has no release window"), UGSTN_DefenceMath::EvaluateParryWindow(100, 150, INDEX_NONE, Rules));
	TestFalse(TEXT("A shield never raised has no window"), UGSTN_DefenceMath::EvaluateParryWindow(INDEX_NONE, 150, 150, Rules));

	Rules.WindowFrames = 0;
	TestFalse(TEXT("A window of 0 frames never opens"), UGSTN_DefenceMath::EvaluateParryWindow(100, 150, 150, Rules));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceParryPressAnchorTest, "GSTNDefence.Core.Parry.PressAnchorEdges", GSTNDefenceTest::Flags)
bool FGSTN_DefenceParryPressAnchorTest::RunTest(const FString& Parameters)
{
	// Press anchor (Brawl-style powershield): window [Press, Press + 5) = [100, 105), whatever the release
	FGSTN_ParryRules Rules;
	Rules.Anchor = EGSTN_ParryAnchor::Press;
	Rules.WindowFrames = 5;

	TestFalse(TEXT("One frame before the press"), UGSTN_DefenceMath::EvaluateParryWindow(100, 99, INDEX_NONE, Rules));
	TestTrue(TEXT("The press frame, shield still held"), UGSTN_DefenceMath::EvaluateParryWindow(100, 100, INDEX_NONE, Rules));
	TestTrue(TEXT("The last frame of the window"), UGSTN_DefenceMath::EvaluateParryWindow(100, 104, 130, Rules));
	TestFalse(TEXT("One frame past the window"), UGSTN_DefenceMath::EvaluateParryWindow(100, 105, 130, Rules));
	TestFalse(TEXT("The release does not open a second window"), UGSTN_DefenceMath::EvaluateParryWindow(100, 130, 130, Rules));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceParryMinHeldTest, "GSTNDefence.Core.Parry.MinHeldFrames", GSTNDefenceTest::Flags)
bool FGSTN_DefenceParryMinHeldTest::RunTest(const FString& Parameters)
{
	// The anti-tap knob: with the release anchor the shield must have been held 60 frames
	FGSTN_ParryRules Rules;
	Rules.Anchor = EGSTN_ParryAnchor::Release;
	Rules.WindowFrames = 5;
	Rules.MinHeldFramesBeforeRelease = 60;

	TestFalse(TEXT("Held 59 frames: a tap does not parry"), UGSTN_DefenceMath::EvaluateParryWindow(100, 159, 159, Rules));
	TestTrue(TEXT("Held exactly 60 frames parries"), UGSTN_DefenceMath::EvaluateParryWindow(100, 160, 160, Rules));
	TestTrue(TEXT("Held longer parries"), UGSTN_DefenceMath::EvaluateParryWindow(100, 202, 200, Rules));

	// The knob is about the release anchor only
	Rules.Anchor = EGSTN_ParryAnchor::Press;
	TestTrue(TEXT("The press anchor ignores it"), UGSTN_DefenceMath::EvaluateParryWindow(100, 101, 102, Rules));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceParryBothSourcesTest, "GSTNDefence.Core.Parry.ResolveBothSourcesEqualProfile", GSTNDefenceTest::Flags)
bool FGSTN_DefenceParryBothSourcesTest::RunTest(const FString& Parameters)
{
	// Both sources point at profile 1 ("Counter"): the decision differs only in the source, so one applier gives one outcome
	UGSTN_DefenceRules* Rules = ParryTestRules();
	Rules->Parry.ProfileIndex = 1;

	const FGSTN_ParryDecision FromShield = UGSTN_DefenceMath::ResolveParry(ParryTestShieldContext(151), Rules);
	const FGSTN_ParryDecision FromMove = UGSTN_DefenceMath::ResolveParry(ParryTestMoveContext(12), Rules);
	TestEqual(TEXT("The shield source"), static_cast<int32>(FromShield.Source), static_cast<int32>(EGSTN_ParrySource::Shield));
	TestEqual(TEXT("The move source"), static_cast<int32>(FromMove.Source), static_cast<int32>(EGSTN_ParrySource::Move));
	TestEqual(TEXT("The shield's profile"), FromShield.ProfileIndex, 1);
	TestEqual(TEXT("The move's profile"), FromMove.ProfileIndex, 1);
	TestEqual(TEXT("Same profile, same attacker stun"), Rules->ParryProfiles[FromShield.ProfileIndex].AttackerStunFrames, Rules->ParryProfiles[FromMove.ProfileIndex].AttackerStunFrames);
	TestTrue(TEXT("Both are parries"), FromShield.IsParry() && FromMove.IsParry());

	// No source: no parry, profile left at 0
	const FGSTN_ParryDecision None = UGSTN_DefenceMath::ResolveParry(FGSTN_ParryContext(), Rules);
	TestFalse(TEXT("An empty context is no parry"), None.IsParry());
	TestFalse(TEXT("No rules, no parry"), UGSTN_DefenceMath::ResolveParry(ParryTestShieldContext(151), nullptr).IsParry());

	// A profile index that does not exist is not a parry (never silently profile 0)
	Rules->Parry.ProfileIndex = 7;
	TestFalse(TEXT("The shield's profile does not exist"), UGSTN_DefenceMath::ResolveParry(ParryTestShieldContext(151), Rules).IsParry());

	// The attacker's stun by hit class: exact entry, deepest ancestor, default
	FGSTN_ParryProfile Profile;
	Profile.AttackerStunFrames = 20;
	const FGameplayTag Normal = FGameplayTag::RequestGameplayTag(TEXT("Combat.HitType.Normal"), false);
	const FGameplayTag Projectile = FGameplayTag::RequestGameplayTag(TEXT("Combat.HitType.Projectile"), false);
	const FGameplayTag HitTypeRoot = FGameplayTag::RequestGameplayTag(TEXT("Combat.HitType"), false);
	TestTrue(TEXT("The hit type tags exist"), Normal.IsValid() && Projectile.IsValid() && HitTypeRoot.IsValid());
	if (Normal.IsValid() && Projectile.IsValid() && HitTypeRoot.IsValid())
	{
		Profile.AttackerStunFramesByHitClass.Add(Projectile, 5);
		Profile.AttackerStunFramesByHitClass.Add(HitTypeRoot, 12);
		TestEqual(TEXT("The exact entry"), UGSTN_DefenceMath::AttackerStunFramesOf(Profile, Projectile), 5);
		TestEqual(TEXT("The ancestor entry for a type with none of its own"), UGSTN_DefenceMath::AttackerStunFramesOf(Profile, Normal), 12);
		TestEqual(TEXT("An unset hit type takes the default"), UGSTN_DefenceMath::AttackerStunFramesOf(Profile, FGameplayTag()), 20);
		TestEqual(TEXT("A type outside the map takes the default"), UGSTN_DefenceMath::AttackerStunFramesOf(Profile, GSTNDefenceTags::Event_Parried), 20);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceParryShieldFirstTest, "GSTNDefence.Core.Parry.ShieldSourceFirst", GSTNDefenceTest::Flags)
bool FGSTN_DefenceParryShieldFirstTest::RunTest(const FString& Parameters)
{
	UGSTN_DefenceRules* Rules = ParryTestRules();

	// Both stamps open at frame 152 (shield window [150, 155)) and the move window is [10, 20): move it to cover 152 too
	FGSTN_ParryContext Both = ParryTestShieldContext(152);
	Both.MoveStartFrame = 140;
	Both.MoveEndFrame = 160;
	Both.MoveProfileIndex = 1;
	Both.bMoveCoversHurtbox = true;
	const FGSTN_ParryDecision First = UGSTN_DefenceMath::ResolveParry(Both, Rules);
	TestEqual(TEXT("The shield wins when both would parry"), static_cast<int32>(First.Source), static_cast<int32>(EGSTN_ParrySource::Shield));
	TestEqual(TEXT("with the shield's profile"), First.ProfileIndex, 0);

	// The shield window closed, the move open: the move parries
	FGSTN_ParryContext MoveOnly = Both;
	MoveOnly.HitFrame = 157;
	const FGSTN_ParryDecision Second = UGSTN_DefenceMath::ResolveParry(MoveOnly, Rules);
	TestEqual(TEXT("The move parries once the shield window is over"), static_cast<int32>(Second.Source), static_cast<int32>(EGSTN_ParrySource::Move));
	TestEqual(TEXT("with the move's profile"), Second.ProfileIndex, 1);

	// The hit missed the bubble (a body hit): the shield source cannot parry it, the move still can
	FGSTN_ParryContext BodyHit = Both;
	BodyHit.bHitOnBubble = false;
	TestEqual(TEXT("A hit off the bubble falls through to the move"), static_cast<int32>(UGSTN_DefenceMath::ResolveParry(BodyHit, Rules).Source), static_cast<int32>(EGSTN_ParrySource::Move));
	BodyHit.bMoveCoversHurtbox = false;
	TestFalse(TEXT("and with no move cover it is no parry"), UGSTN_DefenceMath::ResolveParry(BodyHit, Rules).IsParry());

	// Where the mode has no shield the bubble does not exist: only the move can parry
	FGSTN_ParryContext NoShield = Both;
	NoShield.bShieldAvailable = false;
	TestEqual(TEXT("Without a shield the move parries"), static_cast<int32>(UGSTN_DefenceMath::ResolveParry(NoShield, Rules).Source), static_cast<int32>(EGSTN_ParrySource::Move));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceParryMoveEdgesTest, "GSTNDefence.Core.Parry.MoveWindowEdges", GSTNDefenceTest::Flags)
bool FGSTN_DefenceParryMoveEdgesTest::RunTest(const FString& Parameters)
{
	// Move window [10, 20): MoveParryStartFrame <= HitFrame < MoveParryEndFrame
	UGSTN_DefenceRules* Rules = ParryTestRules();
	TestFalse(TEXT("One frame before the window"), UGSTN_DefenceMath::ResolveParry(ParryTestMoveContext(9), Rules).IsParry());
	TestTrue(TEXT("The first frame"), UGSTN_DefenceMath::ResolveParry(ParryTestMoveContext(10), Rules).IsParry());
	TestTrue(TEXT("The last frame"), UGSTN_DefenceMath::ResolveParry(ParryTestMoveContext(19), Rules).IsParry());
	TestFalse(TEXT("The end frame is exclusive"), UGSTN_DefenceMath::ResolveParry(ParryTestMoveContext(20), Rules).IsParry());

	FGSTN_ParryContext NoWindow = ParryTestMoveContext(12);
	NoWindow.MoveStartFrame = INDEX_NONE;
	TestFalse(TEXT("A move with no window parries nothing"), UGSTN_DefenceMath::ResolveParry(NoWindow, Rules).IsParry());

	// The move source works with the shield disabled in the mode (parry is independent of the shield, m6 spec 1d)
	Rules->bShieldEnabled = false;
	TestTrue(TEXT("The move parries in a mode with no shield"), UGSTN_DefenceMath::ResolveParry(ParryTestMoveContext(12), Rules).IsParry());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceParryCloseTest, "GSTNDefence.Core.Parry.CloseOnParry", GSTNDefenceTest::Flags)
bool FGSTN_DefenceParryCloseTest::RunTest(const FString& Parameters)
{
	// bCloseOnParry: the window ends the frame AFTER the parried hit, so a second hit of the same frame is parried too
	TestEqual(TEXT("Closing at hit frame 12 ends the window at 13"), UGSTN_DefenceMath::MoveWindowEndAfterParry(true, 12, 20), 13);
	TestEqual(TEXT("Not closing leaves the end alone"), UGSTN_DefenceMath::MoveWindowEndAfterParry(false, 12, 20), 20);
	TestEqual(TEXT("A hit on the last frame cannot extend the window"), UGSTN_DefenceMath::MoveWindowEndAfterParry(true, 25, 20), 20);

	UGSTN_DefenceRules* Rules = ParryTestRules();
	FGSTN_ParryContext Same = ParryTestMoveContext(12);
	Same.MoveEndFrame = UGSTN_DefenceMath::MoveWindowEndAfterParry(true, 12, 20);
	TestTrue(TEXT("A second hit of the same frame is still parried"), UGSTN_DefenceMath::ResolveParry(Same, Rules).IsParry());
	Same.HitFrame = 13;
	TestFalse(TEXT("A hit the next frame is not"), UGSTN_DefenceMath::ResolveParry(Same, Rules).IsParry());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceParryHurtboxTest, "GSTNDefence.Core.Parry.HurtboxSelection", GSTNDefenceTest::Flags)
bool FGSTN_DefenceParryHurtboxTest::RunTest(const FString& Parameters)
{
	UGSTN_DefenceRules* Rules = ParryTestRules();

	// A counter that parries only the hits on its blade: the track's selector decides the context's cover flag
	FGSTN_ParryWindowTrack Track;
	Track.StartFrame = 10;
	Track.EndFrame = 20;
	Track.AffectedHurtboxes = { TEXT("Blade") };

	FGSTN_ParryContext OnBlade = ParryTestMoveContext(12);
	OnBlade.bMoveCoversHurtbox = Track.Affects(TEXT("Blade"), FGameplayTagContainer());
	FGSTN_ParryContext OnTorso = ParryTestMoveContext(12);
	OnTorso.bMoveCoversHurtbox = Track.Affects(TEXT("Torso"), FGameplayTagContainer());
	TestTrue(TEXT("A hit on the blade is parried"), UGSTN_DefenceMath::ResolveParry(OnBlade, Rules).IsParry());
	TestFalse(TEXT("A hit on the torso is not"), UGSTN_DefenceMath::ResolveParry(OnTorso, Rules).IsParry());

	// An empty selector covers any hurtbox
	FGSTN_ParryWindowTrack Any;
	FGSTN_ParryContext OnAnything = ParryTestMoveContext(12);
	OnAnything.bMoveCoversHurtbox = Any.Affects(TEXT("Torso"), FGameplayTagContainer());
	TestTrue(TEXT("An empty selector parries any hurtbox"), UGSTN_DefenceMath::ResolveParry(OnAnything, Rules).IsParry());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceParryNotParryableTest, "GSTNDefence.Core.Parry.NotParryableFlag", GSTNDefenceTest::Flags)
bool FGSTN_DefenceParryNotParryableTest::RunTest(const FString& Parameters)
{
	UGSTN_DefenceRules* Rules = ParryTestRules();

	// Each common condition vetoes both sources
	for (int32 Variant = 0; Variant < 4; ++Variant)
	{
		for (int32 SourceIndex = 0; SourceIndex < 2; ++SourceIndex)
		{
			FGSTN_ParryContext Context = SourceIndex == 0 ? ParryTestShieldContext(152) : ParryTestMoveContext(12);
			TestTrue(TEXT("Parryable before the veto"), UGSTN_DefenceMath::ResolveParry(Context, Rules).IsParry());
			switch (Variant)
			{
			case 0: Context.bHitParryable = false; break;
			case 1: Context.bHitImmune = true; break;
			case 2: Context.bVictimImmune = true; break;
			default: Context.bGrabLikeHit = true; break;
			}
			TestFalse(FString::Printf(TEXT("Veto %d on source %d"), Variant, SourceIndex), UGSTN_DefenceMath::ResolveParry(Context, Rules).IsParry());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceParryImmunityTagTest, "GSTNDefence.Core.Parry.ImmunityTagByName", GSTNDefenceTest::Flags)
bool FGSTN_DefenceParryImmunityTagTest::RunTest(const FString& Parameters)
{
	// The immunity is read by name, so a tag requested by name is the tag the plugin declares
	FGameplayTagContainer ByName;
	ByName.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Combat.Immunity.Parry"), false));
	TestTrue(TEXT("A container holding the tag by name is immune"), UGSTN_DefenceMath::HasParryImmunity(ByName));

	FGameplayTagContainer Native;
	Native.AddTag(GSTNDefenceTags::Immunity_Parry);
	TestTrue(TEXT("The native tag is the same"), UGSTN_DefenceMath::HasParryImmunity(Native));

	FGameplayTagContainer ShieldImmune;
	ShieldImmune.AddTag(GSTNDefenceTags::Immunity_Shield);
	TestFalse(TEXT("Immunity to the shield is not immunity to parry"), UGSTN_DefenceMath::HasParryImmunity(ShieldImmune));
	TestFalse(TEXT("An empty container is not immune"), UGSTN_DefenceMath::HasParryImmunity(FGameplayTagContainer()));

	// Grab, throw and pummel hit types are read by name too: the link plugin declares them and may not be loaded
	TestFalse(TEXT("An unset hit type is not grab-like"), UGSTN_DefenceMath::IsGrabLikeHitType(FGameplayTag()));
	const FGameplayTag Normal = FGameplayTag::RequestGameplayTag(TEXT("Combat.HitType.Normal"), false);
	TestFalse(TEXT("A normal hit is not grab-like"), UGSTN_DefenceMath::IsGrabLikeHitType(Normal));
	for (const TCHAR* Name : { TEXT("Combat.HitType.Grab"), TEXT("Combat.HitType.Throw"), TEXT("Combat.HitType.Pummel") })
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(Name), false);
		if (Tag.IsValid())
		{
			TestTrue(FString::Printf(TEXT("%s is grab-like"), Name), UGSTN_DefenceMath::IsGrabLikeHitType(Tag));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceParryProjectileTest, "GSTNDefence.Core.Parry.ProjectileFlag", GSTNDefenceTest::Flags)
bool FGSTN_DefenceParryProjectileTest::RunTest(const FString& Parameters)
{
	UGSTN_DefenceRules* Rules = ParryTestRules();
	Rules->ParryProfiles[0].bParryableProjectiles = false;
	Rules->ParryProfiles[1].bParryableProjectiles = true;

	// The shield's profile 0 refuses projectiles, the move's profile 1 accepts them
	FGSTN_ParryContext ShieldProjectile = ParryTestShieldContext(152);
	ShieldProjectile.bProjectile = true;
	TestFalse(TEXT("The shield profile refuses a projectile"), UGSTN_DefenceMath::ResolveParry(ShieldProjectile, Rules).IsParry());
	ShieldProjectile.bProjectile = false;
	TestTrue(TEXT("and parries the same hit as a melee hit"), UGSTN_DefenceMath::ResolveParry(ShieldProjectile, Rules).IsParry());

	FGSTN_ParryContext MoveProjectile = ParryTestMoveContext(12);
	MoveProjectile.bProjectile = true;
	TestTrue(TEXT("The move profile accepts a projectile"), UGSTN_DefenceMath::ResolveParry(MoveProjectile, Rules).IsParry());

	// A refused shield projectile falls through to an open move window with a profile that accepts it
	FGSTN_ParryContext Both = ParryTestShieldContext(152);
	Both.bProjectile = true;
	Both.MoveStartFrame = 140;
	Both.MoveEndFrame = 160;
	Both.MoveProfileIndex = 1;
	Both.bMoveCoversHurtbox = true;
	const FGSTN_ParryDecision Decision = UGSTN_DefenceMath::ResolveParry(Both, Rules);
	TestEqual(TEXT("The move parries the projectile the shield refused"), static_cast<int32>(Decision.Source), static_cast<int32>(EGSTN_ParrySource::Move));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

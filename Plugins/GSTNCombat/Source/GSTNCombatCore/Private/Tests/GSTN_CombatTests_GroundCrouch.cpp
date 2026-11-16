// Automation tests of the ground crouch rule (owner 2026-11-16): a pure function of the frame's facts, so every machine decides the same stance.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "GSTN_CombatCoreTags.h"
#include "Rules/GSTN_GroundCrouch.h"
#include "Timeline/GSTN_BodyCapsule.h"
#include "Timeline/GSTN_TimelineTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GroundCrouchHoldsTest, "GSTNCombat.Core.GroundCrouch.Holds", GSTN_CORE_TEST_FLAGS)
bool FGSTN_GroundCrouchHoldsTest::RunTest(const FString& Parameters)
{
	const FGameplayTagContainer NoTags;
	FGSTN_GroundCrouchKitExtension Crouch;
	// Args: grounded, held, in move, locked, was crouched
	TestTrue(TEXT("grounded with the button held"), Crouch.HoldsStance(true, true, false, false, false, NoTags));
	TestTrue(TEXT("the tag too"), Crouch.HoldsTag(true, true, false, false, false, NoTags));
	TestFalse(TEXT("in the air it is the air crouch"), Crouch.HoldsStance(false, true, false, false, true, NoTags));
	TestFalse(TEXT("released"), Crouch.HoldsStance(true, false, false, false, true, NoTags));
	TestFalse(TEXT("a hit reaction or a KO blocks it"), Crouch.HoldsStance(true, true, false, true, true, NoTags));

	// Moves: one started crouched keeps the capsule (default), the tag drops; a move never starts the crouch
	TestTrue(TEXT("crouch attack keeps the crouch capsule"), Crouch.HoldsStance(true, true, true, false, true, NoTags));
	TestFalse(TEXT("the move's clip owns the pose: no tag"), Crouch.HoldsTag(true, true, true, false, true, NoTags));
	TestFalse(TEXT("crouch pressed during a jab waits for its end"), Crouch.HoldsStance(true, true, true, false, false, NoTags));
	Crouch.bKeepTagDuringMoves = true;
	TestTrue(TEXT("tag kept through moves when asked"), Crouch.HoldsTag(true, true, true, false, true, NoTags));
	Crouch.bKeepStanceDuringMoves = false;
	TestFalse(TEXT("stance dropped in moves when asked"), Crouch.HoldsStance(true, true, true, false, true, NoTags));

	Crouch.bEnabled = false;
	TestFalse(TEXT("off in the kit"), Crouch.HoldsStance(true, true, false, false, false, NoTags));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GroundCrouchDataTest, "GSTNCombat.Core.GroundCrouch.Data", GSTN_CORE_TEST_FLAGS)
bool FGSTN_GroundCrouchDataTest::RunTest(const FString& Parameters)
{
	FGSTN_GroundCrouchKitExtension Crouch;
	TestEqual(TEXT("no button authored = the standard crouch button"), Crouch.ResolveHoldButton(), GSTNCombatButtonTags::Button_Crouch.GetTag());
	Crouch.HoldButton = GSTNCombatButtonTags::Button_Shield.GetTag();
	TestEqual(TEXT("an authored button wins"), Crouch.ResolveHoldButton(), GSTNCombatButtonTags::Button_Shield.GetTag());
	TestTrue(TEXT("crouch walk on by default (GASP sample)"), Crouch.bCrouchWalk);

	Crouch.BlockedTags.AddTag(GSTNCombatTags::State_Hitstop);
	FGameplayTagContainer Owner;
	Owner.AddTag(GSTNCombatTags::State_Busy);
	TestTrue(TEXT("unrelated owner tag"), Crouch.HoldsStance(true, true, false, false, false, Owner));
	Owner.AddTag(GSTNCombatTags::State_Hitstop);
	TestFalse(TEXT("blocked tag on the owner"), Crouch.HoldsStance(true, true, false, false, false, Owner));

	TestEqual(TEXT("the state tag keeps its name"), GSTNCombatTags::State_Crouch.GetTag().ToString(), FString(TEXT("Combat.State.Crouch")));

	// Default crouched body: one capsule, feet (-55 from the crouched centre) to about one metre
	TestEqual(TEXT("one default crouch hurtbox"), Crouch.CrouchHurtboxShapes.Num(), 1);
	const FGSTN_HitShape& Body = Crouch.CrouchHurtboxShapes[0].Shape;
	TestTrue(TEXT("bottom on the feet"), FMath::IsNearlyEqual(Body.Local.GetLocation().Z - Body.Extent.Z, -55.0, 0.01));
	FGSTN_MovementHurtboxExtension Kit;
	Kit.bOverrideDetail = true;
	const FGSTN_MovementHurtboxExtension Made = Crouch.MakeHurtbox(&Kit);
	TestTrue(TEXT("the kit's detail kept, the crouch shapes used"), Made.bOverrideDetail && Made.Shapes.Num() == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GroundCrouchCapsuleTest, "GSTNCombat.Core.GroundCrouch.Capsule", GSTN_CORE_TEST_FLAGS)
bool FGSTN_GroundCrouchCapsuleTest::RunTest(const FString& Parameters)
{
	const FGSTN_BodyCapsuleShape Standing = FGSTN_BodyCapsuleShape::Standing(90.f, 30.f);
	FGSTN_GroundCrouchKitExtension Crouch;
	const FGSTN_BodyCapsuleShape Shape = Crouch.MakeCapsule(Standing);
	TestTrue(TEXT("55 high, the pawn's radius, bottom on the feet"), FMath::IsNearlyEqual(Shape.HalfHeight, 55.f) && FMath::IsNearlyEqual(Shape.Radius, 30.f)
		&& FMath::IsNearlyEqual(static_cast<float>(Shape.Offset.Z), 55.f));
	TestTrue(TEXT("never taller than standing"), FMath::IsNearlyEqual(FGSTN_GroundCrouchKitExtension::MakeCapsule(120.f, 0.f, Standing).HalfHeight, 90.f));
	TestTrue(TEXT("never under the radius"), FMath::IsNearlyEqual(FGSTN_GroundCrouchKitExtension::MakeCapsule(10.f, 0.f, Standing).HalfHeight, 30.f));

	// No track: the crouch shape with the Body anchor (the mesh stays, the actor drops by 35)
	const FGSTN_BodyCapsuleTarget Target = FGSTN_BodyCapsule::ResolveTarget(nullptr, INDEX_NONE, Standing, &Shape);
	TestTrue(TEXT("crouch target, body anchor, no limit"), Target.Shape.NearlyEquals(Shape) && Target.Anchor == EGSTN_BodyCapsuleAnchor::Body && Target.MaxShiftPerFrame == 0.f);
	const FGSTN_BodyCapsuleStep Step = FGSTN_BodyCapsule::Step(Standing, Target);
	TestTrue(TEXT("actor drops by the offset change"), FMath::IsNearlyEqual(static_cast<float>(Step.ActorShiftLocal.Z), -35.f, 0.01f));
	TestTrue(TEXT("no idle shape = standing"), FGSTN_BodyCapsule::ResolveTarget(nullptr, INDEX_NONE, Standing, nullptr).Shape.NearlyEquals(Standing));
	return true;
}

#endif

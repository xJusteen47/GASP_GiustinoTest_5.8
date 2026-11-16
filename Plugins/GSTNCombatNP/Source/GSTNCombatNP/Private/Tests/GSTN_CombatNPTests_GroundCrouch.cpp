// Ground crouch, Network Prediction half (owner 2026-11-16): the stance rides in the body capsule modifier (Mover sync state), crosses the wire
// and reports Mover.IsCrouching, which GASP's animation and walking mode read.

#include "Misc/AutomationTest.h"
#include "Movement/GSTN_BodyCapsuleModifier.h"
#include "MoverTypes.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GroundCrouchModifierTest, "GSTNCombat.NP.GroundCrouch.Modifier",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_GroundCrouchModifierTest::RunTest(const FString& Parameters)
{
	FGSTN_BodyCapsuleModifier Standing;
	TestFalse(TEXT("standing: no crouch tag"), Standing.HasGameplayTag(Mover_IsCrouching, true));

	FGSTN_BodyCapsuleModifier Crouched;
	Crouched.bCrouched = true;
	Crouched.Applied = FGSTN_BodyCapsuleShape::Standing(55.f, 30.f);
	TestTrue(TEXT("crouched: Mover.IsCrouching"), Crouched.HasGameplayTag(Mover_IsCrouching, true));
	FGameplayTagContainer Tags;
	Crouched.GetGameplayTags(Tags);
	TestTrue(TEXT("listed in the tags too"), Tags.HasTagExact(Mover_IsCrouching));

	// The stance crosses the wire with the applied shape
	FBitWriter Writer(0, true);
	Crouched.NetSerialize(Writer);
	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	FGSTN_BodyCapsuleModifier Received;
	Received.NetSerialize(Reader);
	TestTrue(TEXT("crouch and shape received"), Received.bCrouched && Received.Applied.NearlyEquals(Crouched.Applied));

	TSharedPtr<FMovementModifierBase> Clone(Crouched.Clone());
	TestTrue(TEXT("a clone (rollback copy) keeps it"), static_cast<FGSTN_BodyCapsuleModifier*>(Clone.Get())->bCrouched);
	return true;
}

#endif

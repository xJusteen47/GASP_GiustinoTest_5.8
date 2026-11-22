// Automation tests of the outside-only extra hurtbox (M11 L7c, the backpack dome's shell): the one rule every consumer of the extras asks.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Hits/GSTN_HurtboxTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitsExtraOutsideOnlyTest, "GSTNCombat.Core.Hits.ExtraOutsideOnly", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HitsExtraOutsideOnlyTest::RunTest(const FString& Parameters)
{
	// A 400 cm shell centred 100 cm in front of an entity standing at (1000, 0, 0)
	FGSTN_ExtraHurtbox Shell;
	Shell.Shape.Type = EGSTN_ShapeType::Sphere;
	Shell.Shape.Extent = FVector(400.0);
	Shell.Shape.Local = FTransform(FVector(100.0, 0.0, 0.0));
	Shell.bOutsideOnly = true;
	const FTransform Entity(FVector(1000.0, 0.0, 0.0));

	TestTrue(TEXT("an attacker inside is left out"), Shell.IsSkippedFrom(FVector(1300.0, 0.0, 0.0), Entity));
	TestTrue(TEXT("the surface counts as inside"), Shell.IsSkippedFrom(FVector(1500.0, 0.0, 0.0), Entity));
	TestFalse(TEXT("an attacker outside hits the shell"), Shell.IsSkippedFrom(FVector(1501.0, 0.0, 0.0), Entity));
	TestFalse(TEXT("the centre is the shape's, not the entity's"), Shell.IsSkippedFrom(FVector(650.0, 0.0, 0.0), Entity));

	FGSTN_ExtraHurtbox Bubble = Shell;
	Bubble.bOutsideOnly = false;
	TestFalse(TEXT("an ordinary extra is never left out"), Bubble.IsSkippedFrom(FVector(1100.0, 0.0, 0.0), Entity));

	FGSTN_ExtraHurtbox Box = Shell;
	Box.Shape.Type = EGSTN_ShapeType::Box;
	TestFalse(TEXT("outside-only is a sphere rule: other shapes ignore it"), Box.IsSkippedFrom(FVector(1100.0, 0.0, 0.0), Entity));
	return true;
}

#endif

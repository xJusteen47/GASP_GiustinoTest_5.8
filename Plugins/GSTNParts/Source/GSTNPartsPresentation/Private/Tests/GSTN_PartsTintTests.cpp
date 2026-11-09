// Presentation tests (P10-min): the tint is a pure function of the part state, no rendering needed.

#include "GSTN_PartsTint.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNPartsPresTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsTintMathTest, "GSTNParts.Presentation.Tint.FromState", GSTNPartsPresTest::Flags)
bool FGSTN_PartsTintMathTest::RunTest(const FString& Parameters)
{
	FGSTN_PartsState State;
	State.PartDamage[1] = 51; // a fifth of the byte range (255)
	State.PartDamage[2] = 200;
	State.BrokenMask = 1u << 2; // broken wins over the stored damage
	TArray<float> Tint;
	UGSTN_PartsTint::ComputeTint(State, 4, Tint);
	TestEqual(TEXT("one value per part"), Tint.Num(), 4);
	TestEqual(TEXT("intact is 0"), Tint[0], 0.f);
	TestEqual(TEXT("damaged is the cap fraction"), Tint[1], 0.2f, 1e-4f);
	TestEqual(TEXT("broken is 1"), Tint[2], 1.f);
	TestEqual(TEXT("out of range is 0"), UGSTN_PartsTint::GetPartTint(State, 99), 0.f);
	UGSTN_PartsTint::ComputeTint(State, 100, Tint);
	TestEqual(TEXT("clamped to MaxParts"), Tint.Num(), GSTN_PartsLimits::MaxParts);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsTintComponentTest, "GSTNParts.Presentation.Tint.ComponentIdempotent", GSTNPartsPresTest::Flags)
bool FGSTN_PartsTintComponentTest::RunTest(const FString& Parameters)
{
	AActor* Actor = NewObject<AActor>(GetTransientPackage());
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor);
	Actor->AddOwnedComponent(Mesh);
	UGSTN_PartsTintComponent* Tint = NewObject<UGSTN_PartsTintComponent>(Actor);
	Actor->AddOwnedComponent(Tint);
	Tint->BaseIndex = 4;

	FGSTN_PartsState State;
	State.PartDamage[0] = 255;
	State.PartDamage[1] = 51;
	// Part 2 is intact (0): its slot is still created by the first write, then the value matches and later writes skip it
	TestEqual(TEXT("first apply writes the changed floats"), Tint->ApplyState(State, 3) > 0, true);
	const TArray<float>& Data = Mesh->GetCustomPrimitiveData().Data;
	if (!TestTrue(TEXT("data sized to the base index"), Data.Num() >= 7))
	{
		return false;
	}
	TestEqual(TEXT("part 0 at BaseIndex"), Data[4], 1.f);
	TestEqual(TEXT("part 1 at BaseIndex + 1"), Data[5], 0.2f, 1e-4f);
	TestEqual(TEXT("part 2 intact"), Data[6], 0.f);
	TestEqual(TEXT("same state again writes nothing"), Tint->ApplyState(State, 3), 0);
	State.PartDamage[1] = 0; // a rollback or a restore lowers the value
	TestTrue(TEXT("lower damage rewrites"), Tint->ApplyState(State, 3) > 0);
	TestEqual(TEXT("follows the state down"), Mesh->GetCustomPrimitiveData().Data[5], 0.f);
	return true;
}

#endif

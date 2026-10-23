// Automation tests of the stats movement modifier (combat-m3 section 5): the speeds of the modes follow the WalkSpeed stat, stop
// under Combat.Lock.Movement, and are always recomputed from the archetype. The substep integration (the modifier running inside
// Mover, rollback, two pawns) is checked by the PIE scenarios.

#include "AbilitySystemComponent.h"
#include "Attributes/GSTN_CombatAttributeSet.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "GSTN_CombatTags.h"
#include "Input/GSTN_CombatSettings.h"
#include "Misc/AutomationTest.h"
#include "Movement/GSTN_StatsMovementModifier.h"
#include "Simulation/GSTN_WeaponStatsSwap.h"
#include "MoverComponent.h"
#include "Tests/GSTN_StatsModifierTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** A transient world with an actor that has an ability system with the combat attribute set and a Mover component holding two modes (no simulation) */
	struct FStatsRig
	{
		FStatsRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNCombatNPStatsTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());

			AActor* Owner = World->SpawnActor<AActor>();
			ASC = NewObject<UAbilitySystemComponent>(Owner);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Owner, Owner);
			ASC->AddSet<UGSTN_CombatAttributeSet>();

			Mover = NewObject<UMoverComponent>(Owner);
			Walking = NewObject<UGSTN_TestGaitWalkingMode>(Mover);
			Flying = NewObject<UGSTN_TestPlainMode>(Mover);
			Mover->MovementModes.Add(TEXT("Walking"), Walking);
			Mover->MovementModes.Add(TEXT("Flying"), Flying);
		}

		~FStatsRig()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		/** One substep of the modifier on the current stats, without a simulation behind it */
		bool Step()
		{
			float Factor = 1.f;
			if (!FGSTN_StatsMovementModifier::ResolveSpeedFactor(*ASC, Factor))
			{
				return false;
			}
			FGSTN_StatsMovementModifier::ApplySpeedFactor(*Mover, Factor);
			return true;
		}

		void SetWalkSpeed(float Value) const
		{
			ASC->SetNumericAttributeBase(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute(), Value);
		}

		UWorld* World = nullptr;
		UAbilitySystemComponent* ASC = nullptr;
		UMoverComponent* Mover = nullptr;
		UGSTN_TestGaitWalkingMode* Walking = nullptr;
		UGSTN_TestPlainMode* Flying = nullptr;
	};

	/** Sets the data the modifier reads and restores it */
	struct FScopedStatsSettings
	{
		FScopedStatsSettings(float Reference, const TArray<FName>& Names)
		{
			UGSTN_CombatSettings* Settings = GetMutableDefault<UGSTN_CombatSettings>();
			SavedReference = Settings->StatsReferenceWalkSpeed;
			SavedNames = Settings->StatsSpeedProperties;
			Settings->StatsReferenceWalkSpeed = Reference;
			Settings->StatsSpeedProperties = Names;
		}

		~FScopedStatsSettings()
		{
			UGSTN_CombatSettings* Settings = GetMutableDefault<UGSTN_CombatSettings>();
			Settings->StatsReferenceWalkSpeed = SavedReference;
			Settings->StatsSpeedProperties = SavedNames;
		}

		float SavedReference = 0.f;
		TArray<FName> SavedNames;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StatsMaxSpeedFollowsAttributeTest, "GSTNCombat.NP.Stats.MaxSpeedFollowsAttribute", GSTN_NP_TEST_FLAGS)
bool FGSTN_StatsMaxSpeedFollowsAttributeTest::RunTest(const FString& Parameters)
{
	const FScopedStatsSettings Settings(600.f, { TEXT("WalkSpeed"), TEXT("RunSpeed"), TEXT("SprintSpeed") });
	FStatsRig Rig;
	Rig.SetWalkSpeed(600.f);
	TestTrue(TEXT("the modifier finds the combat attribute set"), Rig.Step());
	TestEqual(TEXT("at the reference speed the gait speeds are the authored ones"), Rig.Walking->RunSpeed, 375.f);

	Rig.SetWalkSpeed(1200.f);
	Rig.Step();
	TestEqual(TEXT("a doubled stat doubles a listed gait speed"), Rig.Walking->RunSpeed, 750.f);
	TestEqual(TEXT("and every other listed one"), Rig.Walking->SprintSpeed, 1170.f);
	TestEqual(TEXT("and the override the archetype sets"), Rig.Walking->MaxSpeedOverride, 750.f);
	TestEqual(TEXT("a mode whose archetype leaves the override at -1 keeps meaning 'use the shared settings'"), Rig.Flying->MaxSpeedOverride, -1.f);

	// The same stats written again, and a stat change after another, land on archetype * factor: nothing compounds
	Rig.Step();
	Rig.Step();
	TestEqual(TEXT("the same stat written again changes nothing"), Rig.Walking->RunSpeed, 750.f);
	Rig.SetWalkSpeed(300.f);
	Rig.Step();
	TestEqual(TEXT("a lower stat is relative to the archetype, not to the last write"), Rig.Walking->RunSpeed, 187.5f);
	TestEqual(TEXT("the override follows"), Rig.Walking->MaxSpeedOverride, 187.5f);

	// A rollback to a frame with the old stat: whatever the object holds, the next substep assigns the right value
	Rig.Walking->RunSpeed = 99999.f;
	Rig.SetWalkSpeed(600.f);
	Rig.Step();
	TestEqual(TEXT("a stale value is overwritten, not scaled"), Rig.Walking->RunSpeed, 375.f);

	// No names listed: only the plain path (the override) is scaled
	{
		const FScopedStatsSettings Plain(600.f, {});
		Rig.SetWalkSpeed(1200.f);
		Rig.Walking->RunSpeed = 375.f;
		Rig.Step();
		TestEqual(TEXT("an empty list leaves the gait variables alone"), Rig.Walking->RunSpeed, 375.f);
		TestEqual(TEXT("but the plain path still follows"), Rig.Walking->MaxSpeedOverride, 750.f);
	}

	// A character without the combat attribute set is left untouched
	{
		FStatsRig Bare;
		Bare.ASC->RemoveSpawnedAttribute(const_cast<UGSTN_CombatAttributeSet*>(Bare.ASC->GetSet<UGSTN_CombatAttributeSet>()));
		Bare.Walking->RunSpeed = 123.f;
		TestFalse(TEXT("no attribute set, no factor"), Bare.Step());
		TestEqual(TEXT("and nothing is written"), Bare.Walking->RunSpeed, 123.f);
	}
	return true;
}

namespace
{
	FGameplayModifierInfo WalkSpeedModifier(float Amount)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UGSTN_CombatAttributeSet::GetWalkSpeedAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Amount));
		return Modifier;
	}
}

UGSTN_TestWeaponStatsPlus::UGSTN_TestWeaponStatsPlus()
{
	Modifiers.Add(WalkSpeedModifier(300.f));
}

UGSTN_TestWeaponStatsMinus::UGSTN_TestWeaponStatsMinus()
{
	Modifiers.Add(WalkSpeedModifier(-300.f));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StatsSwapDeterministicTest, "GSTNCombat.NP.Stats.SwapDeterministic", GSTN_NP_TEST_FLAGS)
bool FGSTN_StatsSwapDeterministicTest::RunTest(const FString& Parameters)
{
	const FScopedStatsSettings Settings(600.f, { TEXT("RunSpeed") });
	FStatsRig Rig;
	Rig.SetWalkSpeed(600.f);

	TArray<FGSTN_WeaponStatsEntry> Weapons;
	Weapons.AddDefaulted_GetRef().Effect = UGSTN_TestWeaponStatsPlus::StaticClass();
	Weapons.AddDefaulted_GetRef().Effect = UGSTN_TestWeaponStatsMinus::StaticClass();
	Weapons.AddDefaulted();	// a support weapon without a stats effect inherits the kit's stats

	const auto Current = [&Rig]() { return Rig.ASC->GetNumericAttribute(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute()); };

	// The primary weapon is on from the start (the loadout applies it)
	TestTrue(TEXT("the primary's effect applies"), FGSTN_WeaponStatsSwap::Execute(*Rig.ASC, Weapons, INDEX_NONE, 0));
	TestEqual(TEXT("primary: +300"), Current(), 900.f);

	FGSTN_WeaponStatsSwap::Execute(*Rig.ASC, Weapons, 0, 1);
	TestEqual(TEXT("switched to the support: the primary's bonus is gone, the support's malus is on"), Current(), 300.f);

	// The same frame executed again, as a re-simulation does: the same result, nothing stacks
	FGSTN_WeaponStatsSwap::Execute(*Rig.ASC, Weapons, 0, 1);
	FGSTN_WeaponStatsSwap::Execute(*Rig.ASC, Weapons, 0, 1);
	TestEqual(TEXT("a replayed swap changes nothing"), Current(), 300.f);

	// A rolled back swap replayed from the state before it, then forward again
	FGSTN_WeaponStatsSwap::Execute(*Rig.ASC, Weapons, 1, 0);
	TestEqual(TEXT("back to the primary"), Current(), 900.f);
	FGSTN_WeaponStatsSwap::Execute(*Rig.ASC, Weapons, 0, 1);
	TestEqual(TEXT("and to the support again"), Current(), 300.f);

	// A support weapon with no effect leaves only the kit's stats
	FGSTN_WeaponStatsSwap::Execute(*Rig.ASC, Weapons, 1, 2);
	TestEqual(TEXT("no effect on the third weapon: the kit's 600"), Current(), 600.f);
	TestFalse(TEXT("the swap to it reports no effect applied"), FGSTN_WeaponStatsSwap::Execute(*Rig.ASC, Weapons, 2, 2));

	// A swap that names an old index the list does not have (a stale request) still applies the new weapon
	FGSTN_WeaponStatsSwap::Execute(*Rig.ASC, Weapons, 7, 0);
	TestEqual(TEXT("an unknown old index removes nothing and applies the new one"), Current(), 900.f);

	// The speeds follow the swap through the modifier
	Rig.Walking->RunSpeed = 0.f;
	Rig.Step();
	TestEqual(TEXT("the run speed follows the primary's +300 (x1.5)"), Rig.Walking->RunSpeed, 562.5f);
	FGSTN_WeaponStatsSwap::Execute(*Rig.ASC, Weapons, 0, 1);
	Rig.Step();
	TestEqual(TEXT("and the support's -300 (x0.5)"), Rig.Walking->RunSpeed, 187.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StatsLockMovementBrakesTest, "GSTNCombat.NP.Stats.LockMovementBrakes", GSTN_NP_TEST_FLAGS)
bool FGSTN_StatsLockMovementBrakesTest::RunTest(const FString& Parameters)
{
	const FScopedStatsSettings Settings(600.f, { TEXT("WalkSpeed"), TEXT("RunSpeed"), TEXT("SprintSpeed") });
	FStatsRig Rig;
	Rig.SetWalkSpeed(600.f);
	Rig.Step();
	TestEqual(TEXT("free: the authored speed"), Rig.Walking->RunSpeed, 375.f);

	Rig.ASC->AddLooseGameplayTag(GSTNCombatTags::Lock_Movement);
	Rig.Step();
	TestEqual(TEXT("locked: every listed speed is 0, so the mode brakes with its own deceleration"), Rig.Walking->RunSpeed, 0.f);
	TestEqual(TEXT("sprint too"), Rig.Walking->SprintSpeed, 0.f);
	TestEqual(TEXT("and the override"), Rig.Walking->MaxSpeedOverride, 0.f);

	Rig.SetWalkSpeed(1200.f);
	Rig.Step();
	TestEqual(TEXT("a stronger stat does not unlock"), Rig.Walking->RunSpeed, 0.f);

	Rig.ASC->RemoveLooseGameplayTag(GSTNCombatTags::Lock_Movement);
	Rig.Step();
	TestEqual(TEXT("unlocked: back to archetype * the current stat"), Rig.Walking->RunSpeed, 750.f);
	return true;
}

#endif

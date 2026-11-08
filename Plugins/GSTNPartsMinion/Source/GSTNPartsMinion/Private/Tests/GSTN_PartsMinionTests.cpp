// Automation tests of the parts bridge on minions (plan P16 sub-gate A): the hit flow writes the part row with no gameplay effect, a broken
// head keeps the minion fighting, a lethal part kills through the normal death path, a type without an extension pays nothing and a freed
// slot's row is never read for its next occupant. A transient game world with the activated minion subsystem (as the GSTNMinion.Runtime
// tests build it); rules, hurtboxes, the part set and the definitions are made in code (no content). Run headless with
// `Automation RunTests GSTNParts.Minion` (the plugin must be enabled).

#include "Data/GSTN_MinionDefinition.h"
#include "Data/GSTN_PartDefinition.h"
#include "Data/GSTN_PartHitData.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "Data/GSTN_PartsMinionExtension.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GSTN_PartsMinionSubsystem.h"
#include "Hits/GSTN_HitData.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Hits/GSTN_KnockbackModel.h"
#include "Misc/AutomationTest.h"
#include "Rules/GSTN_CombatRules.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "Simulation/GSTN_HitInbox.h"
#include "Spawn/GSTN_MinionSpawner.h"
#include "Sync/GSTN_SyncFragment.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_PARTS_MINION_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	constexpr uint8 TorsoBox = 0;
	constexpr uint8 HeadBox = 1;
	constexpr float MinionHealth = 40.f;

	/** A transient game world with an activated minion subsystem (every slot in Combat, no gravity) and the bridge bound to it */
	struct FPartsMinionRig
	{
		FPartsMinionRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNPartsMinionTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());

			Minions = World->GetSubsystem<UGSTN_MinionCombatSubsystem>();
			Bridge = World->GetSubsystem<UGSTN_PartsMinionSubsystem>();
			if (Minions)
			{
				Minions->ActivateForTests();
				Minions->SetForcedLODTier(static_cast<int32>(EGSTN_MinionLODTier::Combat));
				Minions->SetGravitySource([](const FVector&) { return FVector::ZeroVector; });
			}
		}

		~FPartsMinionRig()
		{
			if (Minions)
			{
				Minions->SetForcedLODTier(INDEX_NONE);
				Minions->SetGravitySource(nullptr);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		bool IsValid(FAutomationTestBase& Test) const
		{
			return Test.TestNotNull(TEXT("the minion subsystem exists"), Minions) && Test.TestNotNull(TEXT("the bridge subsystem exists"), Bridge)
				&& Test.TestTrue(TEXT("the bridge is bound"), Bridge->IsBound());
		}

		FGSTN_MinionHandle Spawn(UGSTN_MinionDefinition* Definition)
		{
			FGSTN_MinionSpawnParams Params;
			Params.bPlaySpawnTimeline = false;
			return Minions->EnqueueSpawn(Definition, FTransform::Identity, Params);
		}

		/** Posts a hit on a hurtbox: Damage, a fixed small knockback, penetration 5 (above hardness 0: the full band) */
		bool PostHit(const FGSTN_MinionHandle& Handle, uint8 HurtboxIndex, float Damage, int32 Frame)
		{
			FGSTN_HitInboxEntry Hit;
			Hit.Frame = Frame;
			Hit.AttackerId = 7;
			Hit.AttackerTeam = 255;
			Hit.Relationship = 255;
			Hit.Hit.Damage = Damage;
			Hit.Hit.FixedKnockback = 100.f;
			Hit.Hit.HitstopMode = EGSTN_HitstopMode::Override;
			Hit.Hit.HitstopFramesOverride = 2;
			FGSTN_PartHitData PartHit;
			PartHit.Penetration = 5;
			Hit.Hit.ExtensionData.Add(FInstancedStruct::Make(PartHit));
			Hit.HitLocation = FVector(Minions->GetStore().Position[Handle.Index]) + FVector(-60.0, 0.0, 90.0);
			Hit.HurtboxIndex = HurtboxIndex;
			// A distinct hitbox per post so the HitBy dedupe never drops a later hit of the same test
			Hit.AttackerHitboxIndex = static_cast<uint8>(Frame);
			return Minions->PostToEntityInbox(GSTNEntityKinds::MakeMinionEntityId(Handle.Index), Handle.Generation, Hit.ToInboxEntry());
		}

		EGSTN_MinionActionState StateOf(const FGSTN_MinionHandle& Handle) const
		{
			return static_cast<EGSTN_MinionActionState>(Minions->GetStore().ActionState[Handle.Index]);
		}

		UWorld* World = nullptr;
		UGSTN_MinionCombatSubsystem* Minions = nullptr;
		UGSTN_PartsMinionSubsystem* Bridge = nullptr;
	};

	/** The standard rules (ApplyDamage and HealthWeight kept, no PartResolve: the case the minion step compensates), every hit staggers */
	TStrongObjectPtr<UGSTN_CombatRules> MakeRules()
	{
		TStrongObjectPtr<UGSTN_CombatRules> Rules(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
		Rules->DamageMode = EGSTN_DamageMode::Percent;
		Rules->MaxHealth = 100.f;
		Rules->StaggerThreshold = 0.f;
		if (UGSTN_KnockbackModel* Model = Rules->KnockbackModel.Get())
		{
			Model->Tuning.PushBackThreshold = 300.f;
			Model->Tuning.LaunchThreshold = 1000.f;
			Model->Tuning.TumbleThreshold = 100000.f;
		}
		return Rules;
	}

	/** A torso box (index 0) and a head box (index 1) */
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> MakeHurtboxes()
	{
		TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
		for (const TCHAR* Name : { TEXT("TorsoBox"), TEXT("HeadBox") })
		{
			FGSTN_Hurtbox& Box = Set->Hurtboxes.AddDefaulted_GetRef();
			Box.Name = FName(Name);
			Box.Shape.Type = EGSTN_ShapeType::Capsule;
			Box.Shape.Extent = FVector(40.0, 40.0, 40.0);
			Box.Shape.Local = FTransform(FVector(0.0, 0.0, Box.Name == TEXT("HeadBox") ? 160.0 : 90.0));
		}
		return Set;
	}

	FGSTN_PartDefinition MakePart(FName Name, FName Hurtbox, float Cap)
	{
		FGSTN_PartDefinition Part;
		Part.Name = Name;
		Part.Hurtboxes = { Hurtbox };
		Part.Hardness = 0;
		Part.PartCap = Cap;
		Part.MainTransfer = 1.f;
		return Part;
	}

	/** Torso (0) and Head (1), hardness 0, cap 20: a 25 damage hit breaks either, a 10 damage hit only dents it */
	TStrongObjectPtr<UGSTN_PartSetDefinition> MakePartSet()
	{
		TStrongObjectPtr<UGSTN_PartSetDefinition> Set(NewObject<UGSTN_PartSetDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
		Set->Parts.Add(MakePart(TEXT("Torso"), TEXT("TorsoBox"), 20.f));
		Set->Parts.Add(MakePart(TEXT("Head"), TEXT("HeadBox"), 20.f));
		Set->bForMinions = true;
		Set->RebakeWeightCurve();
		return Set;
	}

	TStrongObjectPtr<UGSTN_MinionDefinition> MakeDefinition(UGSTN_HurtboxSetDefinition* Hurtboxes)
	{
		TStrongObjectPtr<UGSTN_MinionDefinition> Definition(NewObject<UGSTN_MinionDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
		Definition->Stats.MaxHealth = MinionHealth;
		Definition->Stats.Weight = 100.f;
		Definition->TeamId = 1;
		Definition->HurtboxSet = Hurtboxes;
		return Definition;
	}

	FGSTN_PartsMinionExtension MakeExtension(UGSTN_PartSetDefinition* Set, TArray<FName> LethalParts = {})
	{
		FGSTN_PartsMinionExtension Extension;
		Extension.PartSet = Set;
		Extension.LethalParts = MoveTemp(LethalParts);
		return Extension;
	}

	bool IsDyingOrDead(EGSTN_MinionActionState State)
	{
		return State == EGSTN_MinionActionState::Dying || State == EGSTN_MinionActionState::Dead;
	}

	/** Everything a test needs: rig, rules, hurtboxes, set, one part-bearing definition */
	struct FPartsMinionFixture
	{
		FPartsMinionFixture(TArray<FName> LethalParts = {})
		{
			Rules = MakeRules();
			Hurtboxes = MakeHurtboxes();
			PartSet = MakePartSet();
			PartDefinition = MakeDefinition(Hurtboxes.Get());
			if (Rig.Minions && Rig.Bridge)
			{
				Rig.Minions->SetCombatRules(Rules.Get());
				Rig.Bridge->RegisterDefinition(PartDefinition.Get(), MakeExtension(PartSet.Get(), MoveTemp(LethalParts)));
			}
		}

		FPartsMinionRig Rig;
		TStrongObjectPtr<UGSTN_CombatRules> Rules;
		TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Hurtboxes;
		TStrongObjectPtr<UGSTN_PartSetDefinition> PartSet;
		TStrongObjectPtr<UGSTN_MinionDefinition> PartDefinition;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsMinionStoreOutcomeTest, "GSTNParts.Minion.StoreOutcomeNoGE", GSTN_PARTS_MINION_TEST_FLAGS)
bool FGSTN_PartsMinionStoreOutcomeTest::RunTest(const FString& Parameters)
{
	FPartsMinionFixture Fixture;
	FPartsMinionRig& Rig = Fixture.Rig;
	if (!Rig.IsValid(*this))
	{
		return false;
	}
	const FGSTN_MinionHandle Handle = Rig.Spawn(Fixture.PartDefinition.Get());
	Rig.Minions->TickBatch(1);
	TestTrue(TEXT("the type has parts"), Rig.Bridge->HasParts(Handle));
	TestEqual(TEXT("the columns are sized to the store"), Rig.Bridge->GetColumnCapacity(), Rig.Minions->GetStore().Capacity());

	// A 10 damage hit on the torso: the part is dented, nothing breaks
	Rig.PostHit(Handle, TorsoBox, 10.f, 1);
	Rig.Minions->TickBatch(2);
	TestEqual(TEXT("the hit was applied"), Rig.Minions->GetLastBatchStats().HitsApplied, 1);
	const FGSTN_PartsMinionRow* Row = Rig.Bridge->GetRow(Handle);
	if (!TestNotNull(TEXT("a part-bearing minion has a row"), Row))
	{
		return false;
	}
	TestEqual(TEXT("nothing broke"), static_cast<int32>(Row->BrokenMask), 0);
	// 10 of a 20 cap, quantised to 255 at the cap: about 128
	TestTrue(TEXT("the torso's damage byte moved by about half"), Row->PartDamage[0] >= 120 && Row->PartDamage[0] <= 135);
	TestEqual(TEXT("the head is untouched"), static_cast<int32>(Row->PartDamage[1]), 0);
	// The store took the part's MainDelta once (the minion step took back ApplyDamage's copy): no GE, no double damage
	TestEqual(TEXT("health lost the hit once"), Rig.Minions->GetHealth(Handle), MinionHealth - 10.f);
	TestEqual(TEXT("no gameplay effect was produced"), Rig.Minions->GetLastBatchStats().IgnoredEffects, 0);
	TestEqual(TEXT("one part hit written"), Rig.Bridge->GetStats().PartHits, 1);

	// The second hit builds its view from the written row: 10 + 15 passes the cap of 20, the torso breaks
	Rig.PostHit(Handle, TorsoBox, 15.f, 2);
	Rig.Minions->TickBatch(3);
	TestEqual(TEXT("the torso broke on the second hit"), static_cast<int32>(Rig.Bridge->GetRow(Handle)->BrokenMask), 1);
	TestFalse(TEXT("not lethal by default"), IsDyingOrDead(Rig.StateOf(Handle)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsMinionHeadLossTest, "GSTNParts.Minion.HeadLossKeepsFighting", GSTN_PARTS_MINION_TEST_FLAGS)
bool FGSTN_PartsMinionHeadLossTest::RunTest(const FString& Parameters)
{
	// Default extension: LethalParts empty, the head is not lethal (Helldivers default)
	FPartsMinionFixture Fixture;
	FPartsMinionRig& Rig = Fixture.Rig;
	if (!Rig.IsValid(*this))
	{
		return false;
	}
	const FGSTN_MinionHandle Handle = Rig.Spawn(Fixture.PartDefinition.Get());
	Rig.Minions->TickBatch(1);
	TestEqual(TEXT("no lethal part"), static_cast<int32>(Rig.Bridge->GetLethalMask(Handle)), 0);

	Rig.PostHit(Handle, HeadBox, 25.f, 1);
	Rig.Minions->TickBatch(2);
	const FGSTN_PartsMinionRow* Row = Rig.Bridge->GetRow(Handle);
	if (!TestNotNull(TEXT("row"), Row))
	{
		return false;
	}
	TestEqual(TEXT("the head broke"), static_cast<int32>(Row->BrokenMask), 1 << 1);
	TestEqual(TEXT("health took the hit"), Rig.Minions->GetHealth(Handle), MinionHealth - 25.f);
	TestTrue(TEXT("still alive"), Rig.Minions->IsAlive(Handle));
	TestFalse(TEXT("not dying"), IsDyingOrDead(Rig.StateOf(Handle)));
	TestEqual(TEXT("no lethal kill"), Rig.Bridge->GetStats().LethalKills, 0);

	// It keeps going: frames pass, it is still a live part-bearing minion with its head gone
	for (int32 Frame = 3; Frame < 40; ++Frame)
	{
		Rig.Minions->TickBatch(Frame);
	}
	TestTrue(TEXT("alive frames later"), Rig.Minions->IsAlive(Handle));
	TestEqual(TEXT("the head stays broken"), static_cast<int32>(Rig.Bridge->GetRow(Handle)->BrokenMask), 1 << 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsMinionLethalPartTest, "GSTNParts.Minion.LethalPartKills", GSTN_PARTS_MINION_TEST_FLAGS)
bool FGSTN_PartsMinionLethalPartTest::RunTest(const FString& Parameters)
{
	FPartsMinionFixture Fixture(TArray<FName>{ FName(TEXT("Torso")) });
	FPartsMinionRig& Rig = Fixture.Rig;
	if (!Rig.IsValid(*this))
	{
		return false;
	}
	const FGSTN_MinionHandle Handle = Rig.Spawn(Fixture.PartDefinition.Get());
	Rig.Minions->TickBatch(1);
	TestEqual(TEXT("the torso is lethal"), static_cast<int32>(Rig.Bridge->GetLethalMask(Handle)), 1);

	// The head is not lethal even with a lethal torso
	Rig.PostHit(Handle, HeadBox, 25.f, 1);
	Rig.Minions->TickBatch(2);
	TestTrue(TEXT("head lost: alive"), Rig.Minions->IsAlive(Handle));

	// A fresh minion at full health: a 25 damage torso hit leaves 15 health, yet the broken torso kills through the normal death path
	const FGSTN_MinionHandle Second = Rig.Spawn(Fixture.PartDefinition.Get());
	Rig.Minions->TickBatch(3);
	Rig.PostHit(Second, TorsoBox, 25.f, 3);
	Rig.Minions->TickBatch(4);
	TestEqual(TEXT("the torso broke"), static_cast<int32>(Rig.Bridge->GetRow(Second)->BrokenMask & 1), 1);
	TestTrue(TEXT("the minion is dying"), IsDyingOrDead(Rig.StateOf(Second)));
	TestFalse(TEXT("it no longer counts as alive"), Rig.Minions->IsAlive(Second));
	TestEqual(TEXT("health set to 0 by the death path"), Rig.Minions->GetHealth(Second), 0.f);
	TestEqual(TEXT("one lethal kill"), Rig.Bridge->GetStats().LethalKills, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsMinionNoExtensionTest, "GSTNParts.Minion.NoExtensionNoCost", GSTN_PARTS_MINION_TEST_FLAGS)
bool FGSTN_PartsMinionNoExtensionTest::RunTest(const FString& Parameters)
{
	FPartsMinionFixture Fixture;
	FPartsMinionRig& Rig = Fixture.Rig;
	if (!Rig.IsValid(*this))
	{
		return false;
	}
	// A type nobody registered: same hurtboxes, no extension
	TStrongObjectPtr<UGSTN_MinionDefinition> Plain = MakeDefinition(Fixture.Hurtboxes.Get());
	const FGSTN_MinionHandle Handle = Rig.Spawn(Plain.Get());
	Rig.Minions->TickBatch(1);
	TestFalse(TEXT("no parts"), Rig.Bridge->HasParts(Handle));
	TestNull(TEXT("no row"), Rig.Bridge->GetRow(Handle));
	TestEqual(TEXT("no column was allocated for a world without part-bearing minions"), Rig.Bridge->GetColumnCapacity(), 0);

	FGSTN_HitVictimView View;
	TestTrue(TEXT("the view builds"), Rig.Minions->BuildVictimView(Handle, 2, View));
	TestNull(TEXT("the view carries no parts extension"), View.SnapshotExtension);

	// The M8 result, unchanged: the hit's full damage, no part write, no kill
	Rig.PostHit(Handle, HeadBox, 25.f, 1);
	Rig.Minions->TickBatch(2);
	TestEqual(TEXT("the hit's own damage"), Rig.Minions->GetHealth(Handle), MinionHealth - 25.f);
	TestEqual(TEXT("no part hit"), Rig.Bridge->GetStats().PartHits, 0);
	TestEqual(TEXT("no part activation"), Rig.Bridge->GetStats().PartActivations, 0);
	TestTrue(TEXT("alive"), Rig.Minions->IsAlive(Handle));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsMinionResetOnFreeTest, "GSTNParts.Minion.ResetOnFree", GSTN_PARTS_MINION_TEST_FLAGS)
bool FGSTN_PartsMinionResetOnFreeTest::RunTest(const FString& Parameters)
{
	FPartsMinionFixture Fixture;
	FPartsMinionRig& Rig = Fixture.Rig;
	if (!Rig.IsValid(*this))
	{
		return false;
	}
	const FGSTN_MinionHandle First = Rig.Spawn(Fixture.PartDefinition.Get());
	Rig.Minions->TickBatch(1);
	Rig.PostHit(First, HeadBox, 25.f, 1);
	Rig.Minions->TickBatch(2);
	TestEqual(TEXT("the head broke"), static_cast<int32>(Rig.Bridge->GetRow(First)->BrokenMask), 1 << 1);

	// Freed: the old handle reads nothing
	TestTrue(TEXT("despawned"), Rig.Minions->Despawn(First));
	TestFalse(TEXT("a freed minion has no parts"), Rig.Bridge->HasParts(First));
	TestNull(TEXT("and no row"), Rig.Bridge->GetRow(First));

	// The lowest free slot is reused: a new part-bearing occupant starts pristine
	const FGSTN_MinionHandle Second = Rig.Spawn(Fixture.PartDefinition.Get());
	Rig.Minions->TickBatch(3);
	TestEqual(TEXT("the slot is reused"), static_cast<int32>(Second.Index), static_cast<int32>(First.Index));
	const FGSTN_PartsMinionRow* Row = Rig.Bridge->GetRow(Second);
	if (TestNotNull(TEXT("the new occupant has a row"), Row))
	{
		bool bZero = Row->BrokenMask == 0;
		for (uint8 Damage : Row->PartDamage)
		{
			bZero &= Damage == 0;
		}
		TestTrue(TEXT("the row was reset"), bZero);
	}
	FGSTN_HitVictimView View;
	Rig.Minions->BuildVictimView(Second, 4, View);
	const FGSTN_PartsSnapshotExt* Ext = FGSTN_PartsSnapshotExt::FromView(View);
	TestTrue(TEXT("its view shows no broken part"), Ext && Ext->BrokenMask == 0);

	// Freed again and reused by a type without parts: the slot reads as no parts
	TestTrue(TEXT("despawned again"), Rig.Minions->Despawn(Second));
	TStrongObjectPtr<UGSTN_MinionDefinition> Plain = MakeDefinition(Fixture.Hurtboxes.Get());
	const FGSTN_MinionHandle Third = Rig.Spawn(Plain.Get());
	Rig.Minions->TickBatch(4);
	TestEqual(TEXT("same slot again"), static_cast<int32>(Third.Index), static_cast<int32>(First.Index));
	TestFalse(TEXT("a plain occupant has no parts"), Rig.Bridge->HasParts(Third));
	FGSTN_HitVictimView PlainView;
	Rig.Minions->BuildVictimView(Third, 5, PlainView);
	TestNull(TEXT("and its view no extension"), PlainView.SnapshotExtension);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

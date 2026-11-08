// Automation tests of the parts bridge on minions (plan P16 sub-gates A and B; B: the ExtByte only on part-bearing types and only on a
// mask change, the promotion copy into the fighter's parts fragment). Sub-gate A: the hit flow writes the part row with no gameplay effect, a broken
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
#include "GameFramework/Actor.h"
#include "GSTN_PartsMinionSubsystem.h"
#include "GSTN_HitStep_PartResolveMinion.h"
#include "Hits/GSTN_HitData.h"
#include "Hits/GSTN_HitRunner.h"
#include "Pipeline/GSTN_PartHitOutcome.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Hits/GSTN_KnockbackModel.h"
#include "Misc/AutomationTest.h"
#include "Rules/GSTN_CombatRules.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "Sim/GSTN_PartsAttach.h"
#include "Simulation/GSTN_HitInbox.h"
#include "Spawn/GSTN_MinionSpawner.h"
#include "State/GSTN_PartsFragment.h"
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

// ---- Sub-gate B: the wire byte and the promotion copy ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsMinionExtByteTypesTest, "GSTNParts.Minion.ExtByteOnlyOnTypesWithParts", GSTN_PARTS_MINION_TEST_FLAGS)
bool FGSTN_PartsMinionExtByteTypesTest::RunTest(const FString& Parameters)
{
	FPartsMinionFixture Fixture;
	FPartsMinionRig& Rig = Fixture.Rig;
	if (!Rig.IsValid(*this))
	{
		return false;
	}
	TStrongObjectPtr<UGSTN_MinionDefinition> Plain = MakeDefinition(Fixture.Hurtboxes.Get());
	TestFalse(TEXT("nothing is flagged before the type registers"), Fixture.PartDefinition->bHasNetExt);

	// The first spawn registers the type: the bridge flags it there, before the type table and the codec are built
	const FGSTN_MinionHandle PartHandle = Rig.Spawn(Fixture.PartDefinition.Get());
	const FGSTN_MinionHandle PlainHandle = Rig.Spawn(Plain.Get());
	Rig.Minions->TickBatch(1);
	TestTrue(TEXT("a part-bearing type carries the extension byte"), Fixture.PartDefinition->bHasNetExt);
	TestTrue(TEXT("its wire format has the byte"), Fixture.PartDefinition->MakeNetFormat().bTypeHasExt);
	TestFalse(TEXT("a type without parts keeps 0 extension bits"), Plain->bHasNetExt);
	TestFalse(TEXT("its wire format has no byte"), Plain->MakeNetFormat().bTypeHasExt);
	TestEqual(TEXT("one type flagged"), Rig.Bridge->GetStats().NetExtFlagged, 1);

	// A hit on the plain minion never writes a byte
	Rig.PostHit(PlainHandle, HeadBox, 25.f, 1);
	Rig.Minions->TickBatch(2);
	TestEqual(TEXT("the plain minion's byte stays 0"), static_cast<int32>(Rig.Minions->GetExtByte(PlainHandle)), 0);
	TestEqual(TEXT("no byte write"), Rig.Bridge->GetStats().ExtByteWrites, 0);

	// The tag path (settings) flags at registration too
	const FGameplayTag GruntTag = FGameplayTag::RequestGameplayTag(TEXT("Minion.Type.Grunt"), false);
	if (GruntTag.IsValid())
	{
		TStrongObjectPtr<UGSTN_MinionDefinition> Tagged = MakeDefinition(Fixture.Hurtboxes.Get());
		Tagged->TypeTag = GruntTag;
		Rig.Bridge->RegisterTypeTag(GruntTag, MakeExtension(Fixture.PartSet.Get()));
		Rig.Spawn(Tagged.Get());
		Rig.Minions->TickBatch(3);
		TestTrue(TEXT("a type tagged in the settings carries the byte"), Tagged->bHasNetExt);
	}
	else
	{
		AddInfo(TEXT("Minion.Type.Grunt is not a tag here: the tag path is not checked"));
	}

	// A definition given parts after its first spawn keeps its wire format (flipping it then would desync the codec)
	AddExpectedMessagePlain(TEXT("after its first spawn"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	Rig.Bridge->RegisterDefinition(Plain.Get(), MakeExtension(Fixture.PartSet.Get()));
	Rig.Spawn(Plain.Get());
	Rig.Minions->TickBatch(4);
	TestFalse(TEXT("a late registration does not change the wire format"), Plain->bHasNetExt);
	TestTrue(TEXT("the part-bearing minion still has its row"), Rig.Bridge->HasParts(PartHandle));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsMinionDirtyOnMaskChangeTest, "GSTNParts.Minion.DirtyOnlyOnMaskChange", GSTN_PARTS_MINION_TEST_FLAGS)
bool FGSTN_PartsMinionDirtyOnMaskChangeTest::RunTest(const FString& Parameters)
{
	FPartsMinionFixture Fixture;
	FPartsMinionRig& Rig = Fixture.Rig;
	if (!Rig.IsValid(*this))
	{
		return false;
	}
	const FGSTN_MinionHandle Handle = Rig.Spawn(Fixture.PartDefinition.Get());
	Rig.Minions->TickBatch(1);

	// A dent: the row's damage byte moves, the wire byte does not
	Rig.PostHit(Handle, TorsoBox, 10.f, 1);
	Rig.Minions->TickBatch(2);
	TestTrue(TEXT("the torso took damage"), Rig.Bridge->GetRow(Handle)->PartDamage[0] > 0);
	TestEqual(TEXT("a dent writes no byte"), Rig.Bridge->GetStats().ExtByteWrites, 0);
	TestEqual(TEXT("the byte is still 0"), static_cast<int32>(Rig.Minions->GetExtByte(Handle)), 0);

	// The torso breaks: one write, the byte is the mask
	Rig.PostHit(Handle, TorsoBox, 15.f, 2);
	Rig.Minions->TickBatch(3);
	TestEqual(TEXT("a break writes once"), Rig.Bridge->GetStats().ExtByteWrites, 1);
	TestEqual(TEXT("the byte is the broken mask"), static_cast<int32>(Rig.Minions->GetExtByte(Handle)), 1);
	TestTrue(TEXT("the break frame is recorded"), Rig.Bridge->GetLastBreakFrame(Handle) >= 2);

	// Another hit on the broken torso: the mask is the same, no write
	Rig.PostHit(Handle, TorsoBox, 5.f, 3);
	Rig.Minions->TickBatch(4);
	TestEqual(TEXT("a hit on a broken part writes nothing"), Rig.Bridge->GetStats().ExtByteWrites, 1);
	TestEqual(TEXT("the byte is unchanged"), static_cast<int32>(Rig.Minions->GetExtByte(Handle)), 1);

	// The head breaks: a second write with both bits
	Rig.PostHit(Handle, HeadBox, 25.f, 4);
	Rig.Minions->TickBatch(5);
	TestEqual(TEXT("a new break writes again"), Rig.Bridge->GetStats().ExtByteWrites, 2);
	TestEqual(TEXT("the byte holds both parts"), static_cast<int32>(Rig.Minions->GetExtByte(Handle)), 3);
	TestTrue(TEXT("every damaging hit went through the row"), Rig.Bridge->GetStats().PartHits >= 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsMinionPromotionTest, "GSTNParts.Minion.PromotionCopiesToFragment", GSTN_PARTS_MINION_TEST_FLAGS)
bool FGSTN_PartsMinionPromotionTest::RunTest(const FString& Parameters)
{
	FPartsMinionFixture Fixture;
	FPartsMinionRig& Rig = Fixture.Rig;
	if (!Rig.IsValid(*this))
	{
		return false;
	}
	const FGSTN_MinionHandle Handle = Rig.Spawn(Fixture.PartDefinition.Get());
	Rig.Minions->TickBatch(1);
	Rig.PostHit(Handle, HeadBox, 25.f, 1);
	Rig.Minions->TickBatch(2);
	Rig.PostHit(Handle, TorsoBox, 10.f, 2);
	Rig.Minions->TickBatch(3);
	const FGSTN_PartsMinionRow Row = *Rig.Bridge->GetRow(Handle);
	TestEqual(TEXT("the head broke as a minion"), static_cast<int32>(Row.BrokenMask), 1 << 1);
	TestTrue(TEXT("the torso is dented"), Row.PartDamage[0] > 0);

	FGSTN_MinionPromotionParams Params;
	Params.FighterClassOverride = AActor::StaticClass();
	AActor* Fighter = Rig.Minions->PromoteToFighter(Handle, Params);
	if (!TestNotNull(TEXT("the minion was promoted"), Fighter))
	{
		return false;
	}
	TestEqual(TEXT("one seed"), Rig.Bridge->GetStats().PromotionSeeds, 1);
	TestTrue(TEXT("the fighter has a pending seed"), FGSTN_PartsAttach::HasPendingSeed(Fighter));
	TestFalse(TEXT("the minion is gone"), Rig.Bridge->HasParts(Handle));

	// The fighter's set lists the same parts in another order: the copy goes by name (Head is index 0 here, 1 on the minion)
	TStrongObjectPtr<UGSTN_PartSetDefinition> FighterSet(NewObject<UGSTN_PartSetDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
	FighterSet->Parts.Add(MakePart(TEXT("Head"), TEXT("HeadBox"), 20.f));
	FighterSet->Parts.Add(MakePart(TEXT("Torso"), TEXT("TorsoBox"), 20.f));
	FighterSet->RebakeWeightCurve();

	// What the parts tick extension does at the fighter's next AfterClocks
	FGSTN_SyncFragmentArray Fragments;
	TestTrue(TEXT("the seed is consumed"), FGSTN_PartsAttach::ConsumePendingSeed(Fighter, *FighterSet, Fragments));
	const FGSTN_PartsFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Fragments);
	if (TestNotNull(TEXT("the fighter has a parts fragment"), Fragment))
	{
		TestEqual(TEXT("the fighter's head is broken"), static_cast<int32>(Fragment->BrokenMask), 1 << 0);
		TestEqual(TEXT("the head damage came along"), static_cast<int32>(Fragment->PartDamage[0]), static_cast<int32>(Row.PartDamage[1]));
		TestEqual(TEXT("the torso dent came along"), static_cast<int32>(Fragment->PartDamage[1]), static_cast<int32>(Row.PartDamage[0]));
		TestEqual(TEXT("the part count is the fighter set's"), static_cast<int32>(Fragment->PartCount), 2);
		TestEqual(TEXT("no break event serial for a copy"), static_cast<int32>(Fragment->BreakSerial), 0);
	}
	TestFalse(TEXT("the seed is gone"), FGSTN_PartsAttach::HasPendingSeed(Fighter));
	TestFalse(TEXT("a second consume does nothing"), FGSTN_PartsAttach::ConsumePendingSeed(Fighter, *FighterSet, Fragments));

	// A pristine minion leaves no seed
	const FGSTN_MinionHandle Pristine = Rig.Spawn(Fixture.PartDefinition.Get());
	Rig.Minions->TickBatch(4);
	AActor* Clean = Rig.Minions->PromoteToFighter(Pristine, Params);
	TestNotNull(TEXT("the pristine minion was promoted"), Clean);
	TestFalse(TEXT("no seed for a pristine minion"), Clean && FGSTN_PartsAttach::HasPendingSeed(Clean));
	TestEqual(TEXT("still one seed"), Rig.Bridge->GetStats().PromotionSeeds, 1);

	Fighter->Destroy();
	if (Clean)
	{
		Clean->Destroy();
	}
	return true;
}

namespace
{
	const FGSTN_PartHitOutcome* FindPartOutcome(const FGSTN_StepResult& Result)
	{
		for (const FInstancedStruct& Entry : Result.OutcomeExtensions)
		{
			if (const FGSTN_PartHitOutcome* Found = Entry.GetPtr<FGSTN_PartHitOutcome>())
			{
				return Found;
			}
		}
		return nullptr;
	}
}

// P16-C: the cached step path (per (set, rules) cache, no scratch HealthWeight result, no request) gives the same result as the first path on a sample of
// rows, hurtboxes and damages, for rules with ApplyDamage + HealthWeight (the usual case), with neither, and for a ricochet. Requests are the one
// documented difference (the minion batch never reads them)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsMinionCachedStepTest, "GSTNParts.Minion.CachedStepSameOutput", GSTN_PARTS_MINION_TEST_FLAGS)
bool FGSTN_PartsMinionCachedStepTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_PartSetDefinition> Set = MakePartSet();
	// A third, hard part (hardness above the weapon's penetration: a ricochet) on a box of its own
	Set->Parts.Add(MakePart(TEXT("Plate"), TEXT("PlateBox"), 20.f));
	Set->Parts.Last().Hardness = 6;
	Set->Parts.Last().Reaction.JuggleFactor = 0.5f;
	Set->Parts[1].LostMassFraction = 0.2f;
	Set->RebakeWeightCurve();
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Hurtboxes = MakeHurtboxes();
	Hurtboxes->Hurtboxes.AddDefaulted_GetRef().Name = FName(TEXT("PlateBox"));
	// P16-D: a screw (index 3), and a cover (index 4) held by it with two hurtboxes, the second with its own hardness (the per-hurtbox
	// override and the held-by pass go through the cached table too). An unmapped box (index 7) is the body
	Set->Parts.Add(MakePart(TEXT("Screw"), TEXT("ScrewBox"), 20.f));
	Set->Parts.Last().Screw.bEnabled = true;
	Set->Parts.Last().Screw.LoosenSense = 1;
	Set->Parts.Last().Screw.TurnsToRelease = 2;
	Set->Parts.Add(MakePart(TEXT("Cover"), TEXT("CoverBox"), 10.f));
	Set->Parts.Last().Hurtboxes.Add(FName(TEXT("CoverBox2")));
	Set->Parts.Last().Hardness = 1;
	Set->Parts.Last().HurtboxHardnessOverride.Add(FName(TEXT("CoverBox2")), 4);
	Set->Parts.Last().HeldBy = { FName(TEXT("Screw")) };
	Set->Parts.Last().LostMassFraction = 0.1f;
	Set->RebakeWeightCurve();
	for (const TCHAR* Name : { TEXT("ScrewBox"), TEXT("CoverBox"), TEXT("CoverBox2"), TEXT("BodyBox") })
	{
		Hurtboxes->Hurtboxes.AddDefaulted_GetRef().Name = FName(Name);
	}

	TStrongObjectPtr<UGSTN_CombatRules> Standard = MakeRules();
	TStrongObjectPtr<UGSTN_CombatRules> Bare(NewObject<UGSTN_CombatRules>(GetTransientPackage(), NAME_None, RF_Transient));
	Bare->ResolveHitPipeline();
	const TStrongObjectPtr<UGSTN_CombatRules> RulesList[2] = { Standard, Bare };

	struct FRow { uint8 Mask; uint8 Damage[GSTN_PartsLimits::MaxMinionParts]; };
	const FRow Rows[5] = { { 0, {} }, { 0, { 120, 60, 0, 0, 0, 0, 0, 0 } }, { 0b10, { 0, 255, 90, 0, 0, 0, 0, 0 } },
		{ 0, { 0, 0, 0, 200, 250, 0, 0, 0 } }, { 0b1000, { 0, 0, 0, 255, 250, 0, 0, 0 } } };
	const int32 Penetrations[3] = { 2, 5, 20 };
	const int32 TurnSenses[2] = { 0, 1 };
	const float Damages[3] = { 0.f, 10.f, 25.f };

	const bool bWasEnabled = UGSTN_HitStep_PartResolveMinion::IsStepCacheEnabled();
	// This test reads the outcome from the result's array: no scratch users meanwhile (a bridge of a live world may have registered one)
	const int32 ScratchUsers = UGSTN_HitStep_PartResolveMinion::GetOutcomeScratchUsers();
	UGSTN_HitStep_PartResolveMinion::AddOutcomeScratchUser(-ScratchUsers);
	UGSTN_HitStep_PartResolveMinion::InvalidateStepCache();
	const UGSTN_HitStep_PartResolveMinion* Step = GetDefault<UGSTN_HitStep_PartResolveMinion>();
	int32 Compared = 0;
	for (int32 RulesIndex = 0; RulesIndex < 2; ++RulesIndex)
	{
		for (const FRow& Row : Rows)
		{
			const FGSTN_PartsSnapshotExt Ext = FGSTN_PartsSnapshotExt::FromMinionStore(Set.Get(), Row.Mask, Row.Damage);
			for (int32 Box = 0; Box < Hurtboxes->Hurtboxes.Num(); ++Box)
			{
				for (const float Damage : Damages)
				for (const int32 Penetration : Penetrations)
				for (const int32 TurnSense : TurnSenses)
				{
					FGSTN_StepResult Results[2];
					EGSTN_StepResult Verdicts[2] = { EGSTN_StepResult::Continue, EGSTN_StepResult::Continue };
					for (int32 Cached = 0; Cached < 2; ++Cached)
					{
						FGSTN_HitResolutionContext Context;
						Context.Incoming.Hit.Damage = Damage;
						Context.Incoming.Hit.BaseKnockback = 300.f;
						FGSTN_PartHitData PartHit;
						PartHit.Penetration = static_cast<uint8>(Penetration);
						Context.Incoming.Hit.ExtensionData.Add(FInstancedStruct::Make(PartHit));
						Context.Incoming.Hit.TurnSense = TurnSense;
						Context.Incoming.Hit.TurnSteps = 1;
						Context.Victim.Weight = 100.f;
						Context.Victim.SnapshotExtension = &Ext;
						Context.HurtboxSet = Hurtboxes.Get();
						Context.HurtboxIndex = static_cast<uint8>(Box);
						FGSTN_HitContextView View;
						FGSTN_HitPipelineRunner::MakeView(RulesList[RulesIndex].Get(), Context, View);
						UGSTN_HitStep_PartResolveMinion::SetStepCacheEnabled(Cached != 0);
						Verdicts[Cached] = Step->Resolve(View, Results[Cached]);
					}
					const FString Label = FString::Printf(TEXT("rules %d mask %d box %d damage %.0f pen %d turn %d"), RulesIndex, Row.Mask, Box, Damage, Penetration, TurnSense);
					TestEqual(*(Label + TEXT(": verdict")), static_cast<int32>(Verdicts[1]), static_cast<int32>(Verdicts[0]));
					TestEqual(*(Label + TEXT(": damage add")), Results[1].DamageAdd, Results[0].DamageAdd);
					TestEqual(*(Label + TEXT(": weight scale")), Results[1].WeightScale, Results[0].WeightScale);
					TestEqual(*(Label + TEXT(": knockback scale")), Results[1].KnockbackScale, Results[0].KnockbackScale);
					TestEqual(*(Label + TEXT(": hitstop scale")), Results[1].HitstopScale, Results[0].HitstopScale);
					TestEqual(*(Label + TEXT(": stagger scale")), Results[1].StaggerScale, Results[0].StaggerScale);
					TestEqual(*(Label + TEXT(": stagger threshold")), Results[1].StaggerThreshold.Value, Results[0].StaggerThreshold.Value);
					TestEqual(*(Label + TEXT(": max reaction")), static_cast<int32>(Results[1].MaxReaction), static_cast<int32>(Results[0].MaxReaction));
					TestEqual(*(Label + TEXT(": effects")), Results[1].Effects.Num(), Results[0].Effects.Num());
					const FGSTN_PartHitOutcome* A = FindPartOutcome(Results[0]);
					const FGSTN_PartHitOutcome* B = FindPartOutcome(Results[1]);
					if (A && B)
					{
						TestEqual(*(Label + TEXT(": mask")), B->ResultBrokenMask, A->ResultBrokenMask);
						TestTrue(*(Label + TEXT(": damage bytes")), FMemory::Memcmp(A->ResultPartDamage, B->ResultPartDamage, sizeof(A->ResultPartDamage)) == 0);
						TestEqual(*(Label + TEXT(": main delta")), B->MainDelta, A->MainDelta);
						TestEqual(*(Label + TEXT(": flags")), B->Flags, A->Flags);
						TestEqual(*(Label + TEXT(": part index")), B->PartIndex, A->PartIndex);
					}
					else
					{
						TestEqual(*(Label + TEXT(": both or neither carry an outcome")), B == nullptr, A == nullptr);
					}
					++Compared;
				}
			}
		}
	}
	UGSTN_HitStep_PartResolveMinion::SetStepCacheEnabled(bWasEnabled);
	UGSTN_HitStep_PartResolveMinion::AddOutcomeScratchUser(ScratchUsers);
	UGSTN_HitStep_PartResolveMinion::InvalidateStepCache();
	TestTrue(TEXT("a sample was compared"), Compared > 0);
	return true;
}

namespace
{
	/** One damaging hit of the given part set on hurtbox Box through the step, cached or not; the outcome is read from the array or the scratch */
	struct FStepRun
	{
		EGSTN_StepResult Verdict = EGSTN_StepResult::Continue;
		FGSTN_StepResult Result;
		bool bHasOutcome = false;
		FGSTN_PartHitOutcome Outcome;
	};

	FStepRun RunStep(const UGSTN_PartSetDefinition* Set, const UGSTN_CombatRules* Rules, const UGSTN_HurtboxSetDefinition* Hurtboxes, int32 Box, uint8 Mask,
		const uint8* Damage, float HitDamage, int32 Penetration, bool bCached)
	{
		FStepRun Run;
		const FGSTN_PartsSnapshotExt Ext = FGSTN_PartsSnapshotExt::FromMinionStore(Set, Mask, Damage);
		FGSTN_HitResolutionContext Context;
		Context.Incoming.Hit.Damage = HitDamage;
		Context.Incoming.Hit.BaseKnockback = 300.f;
		FGSTN_PartHitData PartHit;
		PartHit.Penetration = static_cast<uint8>(Penetration);
		Context.Incoming.Hit.ExtensionData.Add(FInstancedStruct::Make(PartHit));
		Context.Victim.Weight = 100.f;
		Context.Victim.SnapshotExtension = &Ext;
		Context.HurtboxSet = Hurtboxes;
		Context.HurtboxIndex = static_cast<uint8>(Box);
		FGSTN_HitContextView View;
		FGSTN_HitPipelineRunner::MakeView(Rules, Context, View);
		UGSTN_HitStep_PartResolveMinion::SetStepCacheEnabled(bCached);
		Run.Verdict = GetDefault<UGSTN_HitStep_PartResolveMinion>()->Resolve(View, Run.Result);
		if (const FGSTN_PartHitOutcome* FromArray = FindPartOutcome(Run.Result))
		{
			Run.bHasOutcome = true;
			Run.Outcome = *FromArray;
		}
		else if (const FGSTN_PartHitOutcome* FromScratch = UGSTN_HitStep_PartResolveMinion::ConsumeScratchOutcome())
		{
			Run.bHasOutcome = true;
			Run.Outcome = *FromScratch;
		}
		return Run;
	}

	void CompareRuns(FAutomationTestBase& Test, const FString& Label, const FStepRun& A, const FStepRun& B)
	{
		Test.TestEqual(*(Label + TEXT(": verdict")), static_cast<int32>(B.Verdict), static_cast<int32>(A.Verdict));
		Test.TestEqual(*(Label + TEXT(": damage add")), B.Result.DamageAdd, A.Result.DamageAdd);
		Test.TestEqual(*(Label + TEXT(": weight scale")), B.Result.WeightScale, A.Result.WeightScale);
		Test.TestEqual(*(Label + TEXT(": has outcome")), B.bHasOutcome, A.bHasOutcome);
		if (A.bHasOutcome && B.bHasOutcome)
		{
			Test.TestEqual(*(Label + TEXT(": mask")), B.Outcome.ResultBrokenMask, A.Outcome.ResultBrokenMask);
			Test.TestTrue(*(Label + TEXT(": damage bytes")), FMemory::Memcmp(A.Outcome.ResultPartDamage, B.Outcome.ResultPartDamage, sizeof(A.Outcome.ResultPartDamage)) == 0);
			Test.TestEqual(*(Label + TEXT(": main delta")), B.Outcome.MainDelta, A.Outcome.MainDelta);
			Test.TestEqual(*(Label + TEXT(": flags")), B.Outcome.Flags, A.Outcome.Flags);
			Test.TestEqual(*(Label + TEXT(": part index")), B.Outcome.PartIndex, A.Outcome.PartIndex);
			Test.TestEqual(*(Label + TEXT(": band")), B.Outcome.BandIndex, A.Outcome.BandIndex);
		}
	}
}

// P16-E: the cache is per (part set, rules): two different minion types (two part sets, one rules asset and one hurtbox set) alternated hit by hit
// give the uncached result each time, so a type never reads the other's cached bands, parts or weight factors
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsMinionCachedTwoTypesTest, "GSTNParts.Minion.CachedStepTwoTypesBackToBack", GSTN_PARTS_MINION_TEST_FLAGS)
bool FGSTN_PartsMinionCachedTwoTypesTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_PartSetDefinition> SetA = MakePartSet();
	TStrongObjectPtr<UGSTN_PartSetDefinition> SetB = MakePartSet();
	SetB->Parts[0].Hardness = 4;
	SetB->Parts[1].LostMassFraction = 0.3f;
	SetB->Parts[1].Reaction.JuggleFactor = 0.5f;
	SetB->RebakeWeightCurve();
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Hurtboxes = MakeHurtboxes();
	TStrongObjectPtr<UGSTN_CombatRules> Rules = MakeRules();

	const bool bWasEnabled = UGSTN_HitStep_PartResolveMinion::IsStepCacheEnabled();
	const int32 ScratchUsers = UGSTN_HitStep_PartResolveMinion::GetOutcomeScratchUsers();
	UGSTN_HitStep_PartResolveMinion::AddOutcomeScratchUser(-ScratchUsers);
	UGSTN_HitStep_PartResolveMinion::InvalidateStepCache();

	const UGSTN_PartSetDefinition* Sets[2] = { SetA.Get(), SetB.Get() };
	const uint8 Damage[GSTN_PartsLimits::MaxMinionParts] = { 30, 20, 0, 0, 0, 0, 0, 0 };
	int32 Compared = 0;
	for (int32 Round = 0; Round < 3; ++Round)
	{
		for (int32 Which = 0; Which < 2; ++Which)
		{
			for (int32 Box = 0; Box < 2; ++Box)
			{
				for (const int32 Penetration : { 2, 5 })
				{
					const FStepRun Cached = RunStep(Sets[Which], Rules.Get(), Hurtboxes.Get(), Box, 0, Damage, 15.f, Penetration, true);
					const FStepRun Reference = RunStep(Sets[Which], Rules.Get(), Hurtboxes.Get(), Box, 0, Damage, 15.f, Penetration, false);
					CompareRuns(*this, FString::Printf(TEXT("round %d type %d box %d pen %d"), Round, Which, Box, Penetration), Reference, Cached);
					++Compared;
				}
			}
		}
	}
	UGSTN_HitStep_PartResolveMinion::SetStepCacheEnabled(bWasEnabled);
	UGSTN_HitStep_PartResolveMinion::AddOutcomeScratchUser(ScratchUsers);
	UGSTN_HitStep_PartResolveMinion::InvalidateStepCache();
	TestTrue(TEXT("a sample was compared"), Compared > 0);
	return true;
}

// P16-E: with a scratch user registered the cached step leaves OutcomeExtensions empty and the scratch carries the same outcome the array path
// gives; the scratch is taken once (a second take is null, so a later hit never reads it)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsMinionScratchOutcomeTest, "GSTNParts.Minion.ScratchOutcomeSameAsArray", GSTN_PARTS_MINION_TEST_FLAGS)
bool FGSTN_PartsMinionScratchOutcomeTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_PartSetDefinition> Set = MakePartSet();
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Hurtboxes = MakeHurtboxes();
	TStrongObjectPtr<UGSTN_CombatRules> Rules = MakeRules();

	const bool bWasEnabled = UGSTN_HitStep_PartResolveMinion::IsStepCacheEnabled();
	const int32 ScratchUsers = UGSTN_HitStep_PartResolveMinion::GetOutcomeScratchUsers();
	UGSTN_HitStep_PartResolveMinion::AddOutcomeScratchUser(-ScratchUsers);
	UGSTN_HitStep_PartResolveMinion::InvalidateStepCache();

	const uint8 Damage[GSTN_PartsLimits::MaxMinionParts] = { 100, 60, 0, 0, 0, 0, 0, 0 };
	for (int32 Box = 0; Box < 2; ++Box)
	{
		const FStepRun Array = RunStep(Set.Get(), Rules.Get(), Hurtboxes.Get(), Box, 0, Damage, 25.f, 5, true);
		TestTrue(TEXT("without a scratch user the outcome is in the array"), FindPartOutcome(Array.Result) != nullptr);

		UGSTN_HitStep_PartResolveMinion::AddOutcomeScratchUser(1);
		const FStepRun Scratch = RunStep(Set.Get(), Rules.Get(), Hurtboxes.Get(), Box, 0, Damage, 25.f, 5, true);
		UGSTN_HitStep_PartResolveMinion::AddOutcomeScratchUser(-1);
		TestTrue(TEXT("with a scratch user the array carries no outcome"), FindPartOutcome(Scratch.Result) == nullptr);
		TestTrue(TEXT("the scratch carried it"), Scratch.bHasOutcome);
		TestTrue(TEXT("RunStep took it, a second take is empty"), UGSTN_HitStep_PartResolveMinion::ConsumeScratchOutcome() == nullptr);
		CompareRuns(*this, FString::Printf(TEXT("box %d"), Box), Array, Scratch);
	}
	UGSTN_HitStep_PartResolveMinion::SetStepCacheEnabled(bWasEnabled);
	UGSTN_HitStep_PartResolveMinion::AddOutcomeScratchUser(ScratchUsers);
	UGSTN_HitStep_PartResolveMinion::InvalidateStepCache();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

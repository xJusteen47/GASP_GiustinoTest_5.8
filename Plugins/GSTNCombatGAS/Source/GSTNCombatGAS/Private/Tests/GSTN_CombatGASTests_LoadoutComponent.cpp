// Automation tests of the loadout component and subsystem: the server apply path on a transient ability system (no PIE, no
// network, no Asset Manager), the rules refusing a request, and the save file with its unknown-id handling.

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbility_Montage.h"
#include "Attributes/GSTN_CombatAttributeSet.h"
#include "Attributes/GSTN_MeterAttributeSet.h"
#include "Effects/GSTN_GE_WeaponStats.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GSTN_CombatTags.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_StandardSlots.h"
#include "Kits/GSTN_AbilityBinding.h"
#include "Kits/GSTN_CombatAssetTypes.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "Loadout/GSTN_LoadoutSaveGame.h"
#include "Loadout/GSTN_LoadoutSubsystem.h"
#include "Tests/GSTN_GASTestTypes.h"
#include "Tests/GSTN_LoadoutTestTypes.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags LoadoutComponentTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	FPrimaryAssetId KitId(const TCHAR* Name) { return FPrimaryAssetId(GSTNCombatAssetTypes::PrimaryKit(), FName(Name)); }
	FPrimaryAssetId WeaponId(const TCHAR* Name) { return FPrimaryAssetId(GSTNCombatAssetTypes::Weapon(), FName(Name)); }
	FPrimaryAssetId SuperId(const TCHAR* Name) { return FPrimaryAssetId(GSTNCombatAssetTypes::Super(), FName(Name)); }
	FPrimaryAssetId SkinId(const TCHAR* Name) { return FPrimaryAssetId(GSTNCombatAssetTypes::Skin(), FName(Name)); }

	TSoftClassPtr<UGameplayAbility> AbilityA() { return TSoftClassPtr<UGameplayAbility>(UGSTN_TestGrantAbilityA::StaticClass()); }
	TSoftClassPtr<UGameplayAbility> AbilityB() { return TSoftClassPtr<UGameplayAbility>(UGSTN_TestGrantAbilityB::StaticClass()); }
	TSoftClassPtr<UGameplayAbility> AbilityC() { return TSoftClassPtr<UGameplayAbility>(UGSTN_TestGrantAbilityC::StaticClass()); }

	FGSTN_AbilityBinding MakeBinding(const FGameplayTag& Command, const TSoftClassPtr<UGameplayAbility>& Ability)
	{
		FGSTN_AbilityBinding Binding;
		Binding.Command = Command;
		Binding.Ability = Ability;
		return Binding;
	}

	bool AnyContains(const TArray<FString>& Messages, const TCHAR* Needle)
	{
		return Messages.ContainsByPredicate([Needle](const FString& Message) { return Message.Contains(Needle); });
	}

	/** A loadout that exercises every part the persistence has to keep */
	FGSTN_LoadoutData MakeRichLoadout()
	{
		FGSTN_LoadoutData Data;
		Data.PrimaryKit = KitId(TEXT("KitA"));
		Data.AdditionalWeapons = { WeaponId(TEXT("W1")), WeaponId(TEXT("W2")), WeaponId(TEXT("W3")) };
		Data.Supers = { SuperId(TEXT("S1")), FPrimaryAssetId(), SuperId(TEXT("S3")) };
		Data.Skin = SkinId(TEXT("Skin1"));

		FGSTN_SlotCustomisation Customisation;
		Customisation.WeaponIndex = 3;
		Customisation.Command = GSTNCommandTags::Attack_Neutral.GetTag();
		Customisation.AlternativeAbility = TSoftClassPtr<UObject>(UGameplayAbility_Montage::StaticClass());
		Data.SlotCustomisations.Add(Customisation);

		FGSTN_LoadoutValue Value;
		Value.Value = 0.5f;
		Data.DesignerValues.Add(GSTNCombatTags::State_Busy.GetTag(), Value);
		return Data;
	}

	/**
	 * A transient world with an ability system, in-memory kits and a weapon, and a loadout component that resolves their ids.
	 * Declaration order is destruction order backwards: the objects go before the world.
	 */
	struct FLoadoutFixture
	{
		GSTNCombatGASTest::FRig Rig;
		TStrongObjectPtr<UGSTN_InputSlotSet> Base;
		TStrongObjectPtr<UGSTN_WeaponDefinition> Weapon;
		TStrongObjectPtr<UGSTN_PrimaryKitDefinition> KitA;
		TStrongObjectPtr<UGSTN_PrimaryKitDefinition> KitB;
		TStrongObjectPtr<UGSTN_TestLoadoutComponent> Component;
		TStrongObjectPtr<UGSTN_TestLoadoutListener> Listener;

		FLoadoutFixture()
		{
			Base.Reset(NewObject<UGSTN_InputSlotSet>(GetTransientPackage()));
			for (const FGSTN_InputSlotSpec& Spec : UGSTN_StandardSlots::Build())
			{
				Base->Slots.AddDefaulted_GetRef().Inline = Spec;
			}

			// The primary weapon: a stats effect (+100 walk speed) and one passive
			Weapon.Reset(NewObject<UGSTN_WeaponDefinition>(GetTransientPackage()));
			Weapon->StatsEffect = TSoftClassPtr<UGSTN_GE_WeaponStats>(UGSTN_TestWeaponStatsA::StaticClass());
			Weapon->StatModifiers = { { GSTNCombatGASTest::Stat_TestSpeed, 100.f } };
			Weapon->PassivesWhileActive = { AbilityC() };

			// Kit A: one command bound to ability A, ability B triggered, walk speed 700, the weapon above
			KitA.Reset(NewObject<UGSTN_PrimaryKitDefinition>(GetTransientPackage()));
			KitA->CommandSet.BaseSet = TSoftObjectPtr<UGSTN_InputSlotSet>(Base.Get());
			KitA->KitBindings = { MakeBinding(GSTNCommandTags::Strong_Neutral.GetTag(), AbilityA()) };
			KitA->TriggeredAbilities = { AbilityB() };
			KitA->Stats.Values = { { GSTNCombatGASTest::Stat_TestSpeed, 700.f }, { GSTNCombatGASTest::Stat_TestMeterMax, 250.f } };
			KitA->InitStatsEffect = TSoftClassPtr<UGameplayEffect>(UGSTN_TestKitInitEffect::StaticClass());
			KitA->PrimaryWeapon = TSoftObjectPtr<UGSTN_WeaponDefinition>(Weapon.Get());

			// Kit B: nothing bound, ability B triggered, walk speed 500, no weapon
			KitB.Reset(NewObject<UGSTN_PrimaryKitDefinition>(GetTransientPackage()));
			KitB->CommandSet.BaseSet = TSoftObjectPtr<UGSTN_InputSlotSet>(Base.Get());
			KitB->TriggeredAbilities = { AbilityB() };
			KitB->Stats.Values = { { GSTNCombatGASTest::Stat_TestSpeed, 500.f }, { GSTNCombatGASTest::Stat_TestMeterMax, 250.f } };
			KitB->InitStatsEffect = TSoftClassPtr<UGameplayEffect>(UGSTN_TestKitInitEffect::StaticClass());

			Component.Reset(MakeComponent(Rig.ASC));
			Listener.Reset(NewObject<UGSTN_TestLoadoutListener>(GetTransientPackage()));
			Component->OnLoadoutApplied.AddDynamic(Listener.Get(), &UGSTN_TestLoadoutListener::OnApplied);
		}

		/** Another loadout component on the same owner, answering the same ids. Target null = the pawn is not there yet */
		UGSTN_TestLoadoutComponent* MakeComponent(UAbilitySystemComponent* Target) const
		{
			UGSTN_TestLoadoutComponent* Made = NewObject<UGSTN_TestLoadoutComponent>(Rig.ASC->GetOwner());
			Made->RegisterComponent();
			Made->Target = Target;
			Made->Assets.Add(KitId(TEXT("KitA")), KitA.Get());
			Made->Assets.Add(KitId(TEXT("KitB")), KitB.Get());
			Made->Assets.Add(WeaponId(TEXT("W1")), Weapon.Get());
			return Made;
		}

		FGSTN_LoadoutData RequestKit(const TCHAR* Name) const
		{
			FGSTN_LoadoutData Request;
			Request.PrimaryKit = KitId(Name);
			return Request;
		}

		bool HasAbility(TSubclassOf<UGameplayAbility> Class) const
		{
			return Rig.ASC->FindAbilitySpecFromClass(Class) != nullptr;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LoadoutSaveRoundTripTest, "GSTNCombat.GAS.Loadout.SaveRoundTrip", LoadoutComponentTestFlags)
bool FGSTN_LoadoutSaveRoundTripTest::RunTest(const FString& Parameters)
{
	// The subsystem lives within a local player; one that was never initialised is enough, its storage is replaced
	TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
	TStrongObjectPtr<UGSTN_TestLoadoutSubsystem> First(NewObject<UGSTN_TestLoadoutSubsystem>(Player.Get()));

	const FGSTN_LoadoutData Rich = MakeRichLoadout();
	FGSTN_LoadoutData Plain;
	Plain.PrimaryKit = KitId(TEXT("KitB"));

	TestFalse(TEXT("a preset needs a name"), First->SaveLoadout(FString(), Rich));
	TestTrue(TEXT("a preset is saved"), First->SaveLoadout(TEXT("Rich"), Rich));
	TestTrue(TEXT("a second preset is saved"), First->SaveLoadout(TEXT("Plain"), Plain));
	TestTrue(TEXT("the bytes reached the storage"), First->Storage.Num() > 0);

	// A fresh subsystem over the same bytes is a new session
	TStrongObjectPtr<UGSTN_TestLoadoutSubsystem> Second(NewObject<UGSTN_TestLoadoutSubsystem>(Player.Get()));
	Second->Storage = First->Storage;

	const TArray<FString> Names = Second->ListLoadouts();
	if (TestEqual(TEXT("both presets are listed"), Names.Num(), 2))
	{
		TestEqual(TEXT("sorted, first"), Names[0], FString(TEXT("Plain")));
		TestEqual(TEXT("sorted, second"), Names[1], FString(TEXT("Rich")));
	}

	FGSTN_LoadoutData Loaded;
	TestTrue(TEXT("the rich preset loads"), Second->LoadLoadout(TEXT("Rich"), Loaded));
	TestTrue(TEXT("and equals what was saved: ids, customisation, designer value, skin"), Loaded == Rich);
	TestTrue(TEXT("the plain preset loads"), Second->LoadLoadout(TEXT("Plain"), Loaded));
	TestTrue(TEXT("and equals what was saved"), Loaded == Plain);

	FGSTN_LoadoutData Untouched = Rich;
	TestFalse(TEXT("an unknown preset does not load"), Second->LoadLoadout(TEXT("Nope"), Untouched));
	TestTrue(TEXT("and leaves the output alone"), Untouched == Rich);

	// Replace, then delete
	TestTrue(TEXT("saving under an existing name replaces it"), Second->SaveLoadout(TEXT("Plain"), Rich));
	Second->LoadLoadout(TEXT("Plain"), Loaded);
	TestTrue(TEXT("the replacement is what loads"), Loaded == Rich);
	TestTrue(TEXT("a preset is deleted"), Second->DeleteLoadout(TEXT("Plain")));
	TestFalse(TEXT("and is gone"), Second->HasLoadout(TEXT("Plain")));
	TestFalse(TEXT("deleting it again does nothing"), Second->DeleteLoadout(TEXT("Plain")));

	// The save game object alone round-trips through bytes
	TStrongObjectPtr<UGSTN_LoadoutSaveGame> Save(NewObject<UGSTN_LoadoutSaveGame>(GetTransientPackage()));
	Save->Loadouts.Add(TEXT("X"), Rich);
	TArray<uint8> Bytes;
	TestTrue(TEXT("the save game serializes"), Save->ToBytes(Bytes));
	UGSTN_LoadoutSaveGame* Read = UGSTN_LoadoutSaveGame::FromBytes(Bytes);
	if (TestNotNull(TEXT("and reads back"), Read))
	{
		const FGSTN_LoadoutData* X = Read->Loadouts.Find(TEXT("X"));
		TestTrue(TEXT("with the same loadout"), X && *X == Rich);
	}
	TestNull(TEXT("no bytes read nothing"), UGSTN_LoadoutSaveGame::FromBytes(TArray<uint8>()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LoadoutUnknownIdDroppedTest, "GSTNCombat.GAS.Loadout.UnknownIdDropped", LoadoutComponentTestFlags)
bool FGSTN_LoadoutUnknownIdDroppedTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
	TStrongObjectPtr<UGSTN_TestLoadoutSubsystem> Subsystem(NewObject<UGSTN_TestLoadoutSubsystem>(Player.Get()));

	// Weapons W1..W3 are loadout weapons 1..3. Customisations: on W3, on W2 (to be dropped) and on the kit's own weapon
	FGSTN_LoadoutData Data = MakeRichLoadout();
	FGSTN_SlotCustomisation OnW2 = Data.SlotCustomisations[0];
	OnW2.WeaponIndex = 2;
	FGSTN_SlotCustomisation OnPrimary = Data.SlotCustomisations[0];
	OnPrimary.WeaponIndex = 0;
	Data.SlotCustomisations.Add(OnW2);
	Data.SlotCustomisations.Add(OnPrimary);
	Subsystem->SaveLoadout(TEXT("Old"), Data);

	// Content moved on: W2, super S1 and the skin no longer exist
	Subsystem->bEverythingKnown = false;
	Subsystem->Known = { KitId(TEXT("KitA")), WeaponId(TEXT("W1")), WeaponId(TEXT("W3")), SuperId(TEXT("S3")) };

	FGSTN_LoadoutData Loaded;
	TestTrue(TEXT("the preset still loads"), Subsystem->LoadLoadout(TEXT("Old"), Loaded));
	TestTrue(TEXT("the kit is kept"), Loaded.PrimaryKit == KitId(TEXT("KitA")));
	if (TestEqual(TEXT("the unknown weapon left the list"), Loaded.AdditionalWeapons.Num(), 2))
	{
		TestTrue(TEXT("W1 kept its place"), Loaded.AdditionalWeapons[0] == WeaponId(TEXT("W1")));
		TestTrue(TEXT("W3 moved up"), Loaded.AdditionalWeapons[1] == WeaponId(TEXT("W3")));
	}
	if (TestEqual(TEXT("super slots keep their positions"), Loaded.Supers.Num(), 3))
	{
		TestFalse(TEXT("the unknown super left its slot empty"), Loaded.Supers[0].IsValid());
		TestFalse(TEXT("the slot that was empty still is"), Loaded.Supers[1].IsValid());
		TestTrue(TEXT("the known super stayed"), Loaded.Supers[2] == SuperId(TEXT("S3")));
	}
	TestFalse(TEXT("the unknown skin was cleared"), Loaded.Skin.IsValid());
	if (TestEqual(TEXT("the customisation of the dropped weapon went with it"), Loaded.SlotCustomisations.Num(), 2))
	{
		// Order of the list is kept: W3's (was weapon 3, now 2) then the primary's
		TestEqual(TEXT("W3's customisation follows its weapon to index 2"), Loaded.SlotCustomisations[0].WeaponIndex, 2);
		TestEqual(TEXT("the primary weapon's is untouched"), Loaded.SlotCustomisations[1].WeaponIndex, 0);
	}
	TestEqual(TEXT("the designer values are untouched"), Loaded.DesignerValues.Num(), 1);

	// The file keeps what was saved: when the content comes back, so does the preset
	Subsystem->bEverythingKnown = true;
	FGSTN_LoadoutData Restored;
	Subsystem->LoadLoadout(TEXT("Old"), Restored);
	TestTrue(TEXT("a content that returns restores the saved preset"), Restored == Data);

	// The static does the same on a value, and counts: W2, S1, skin and the customisation of W2
	FGSTN_LoadoutData Copy = Data;
	const int32 Dropped = UGSTN_LoadoutSaveGame::DropUnknownAssets(Copy, [](const FPrimaryAssetId& Id) { return Id == KitId(TEXT("KitA")) || Id == WeaponId(TEXT("W1")) || Id == WeaponId(TEXT("W3")) || Id == SuperId(TEXT("S3")); }, TEXT("Direct"));
	TestEqual(TEXT("four things were dropped"), Dropped, 4);
	TestTrue(TEXT("the same result as through the subsystem"), Copy == Loaded);

	// An unknown kit clears the kit: the server's rules refuse such a loadout, which is the right outcome
	Subsystem->bEverythingKnown = false;
	Subsystem->Known = { WeaponId(TEXT("W1")) };
	Subsystem->LoadLoadout(TEXT("Old"), Loaded);
	TestFalse(TEXT("an unknown kit is dropped"), Loaded.PrimaryKit.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LoadoutRulesRejectInvalidTest, "GSTNCombat.GAS.Loadout.RulesRejectInvalid", LoadoutComponentTestFlags)
bool FGSTN_LoadoutRulesRejectInvalidTest::RunTest(const FString& Parameters)
{
	FLoadoutFixture Fixture;
	UGSTN_TestLoadoutComponent& Component = *Fixture.Component;
	TArray<FString> Errors;

	// No kit at all
	TestFalse(TEXT("- an empty loadout is rejected"), Component.AcceptLoadout(FGSTN_LoadoutData(), Errors));
	TestTrue(TEXT("by the kit rule"), AnyContains(Errors, TEXT("names no kit")));

	// A kit id the content source does not know
	TestFalse(TEXT("- an unknown kit is rejected"), Component.AcceptLoadout(Fixture.RequestKit(TEXT("Missing")), Errors));
	TestTrue(TEXT("by the kit rule"), AnyContains(Errors, TEXT("does not exist")));

	// An additional weapon that does not exist
	FGSTN_LoadoutData NoWeapon = Fixture.RequestKit(TEXT("KitA"));
	NoWeapon.AdditionalWeapons = { WeaponId(TEXT("Missing")) };
	TestFalse(TEXT("- an unknown additional weapon is rejected"), Component.AcceptLoadout(NoWeapon, Errors));
	TestTrue(TEXT("by the weapon rule"), AnyContains(Errors, TEXT("does not exist")));

	// A weapon that exists, but the kit's query (empty) allows none
	FGSTN_LoadoutData NotAllowed = Fixture.RequestKit(TEXT("KitA"));
	NotAllowed.AdditionalWeapons = { WeaponId(TEXT("W1")) };
	TestFalse(TEXT("- a weapon the kit does not allow is rejected"), Component.AcceptLoadout(NotAllowed, Errors));
	TestTrue(TEXT("by the weapon rule"), AnyContains(Errors, TEXT("not one the kit may carry")));

	// A super in a slot the kit does not have
	FGSTN_LoadoutData NoSlot = Fixture.RequestKit(TEXT("KitA"));
	NoSlot.Supers = { SuperId(TEXT("S1")) };
	TestFalse(TEXT("- a super beyond the kit's slots is rejected"), Component.AcceptLoadout(NoSlot, Errors));
	TestTrue(TEXT("by the super rule"), AnyContains(Errors, TEXT("super slots")));

	// A refusal changes nothing, anywhere
	TestFalse(TEXT("no loadout was applied"), Component.HasLoadout());
	TestEqual(TEXT("the revision did not move"), Component.GetRevision(), 0);
	TestEqual(TEXT("nobody was told"), Fixture.Listener->Count, 0);
	TestEqual(TEXT("no ability was granted"), Fixture.Rig.ASC->GetActivatableAbilities().Num(), 0);
	TestEqual(TEXT("and no table was compiled"), Component.GetCompiledTable().Entries.Num(), 0);

	// The same component still accepts a valid request afterwards
	TestTrue(TEXT("+ a valid loadout is accepted after the refusals"), Component.AcceptLoadout(Fixture.RequestKit(TEXT("KitA")), Errors));
	TestEqual(TEXT("with no findings"), Errors.Num(), 0);
	TestEqual(TEXT("revision 1"), Component.GetRevision(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LoadoutApplyOnHostWithoutOnRepTest, "GSTNCombat.GAS.Loadout.ApplyOnHostWithoutOnRep", LoadoutComponentTestFlags)
bool FGSTN_LoadoutApplyOnHostWithoutOnRepTest::RunTest(const FString& Parameters)
{
	FLoadoutFixture Fixture;
	UGSTN_TestLoadoutComponent& Component = *Fixture.Component;
	UAbilitySystemComponent& ASC = *Fixture.Rig.ASC;
	const FGameplayAttribute WalkSpeed = UGSTN_CombatAttributeSet::GetWalkSpeedAttribute();

	// Nothing here calls OnRep_Applied: the host never receives the replication of what it sets itself
	TArray<FString> Errors;
	const FGSTN_LoadoutData RequestA = Fixture.RequestKit(TEXT("KitA"));
	TestTrue(TEXT("the server accepts the loadout"), Component.AcceptLoadout(RequestA, Errors));
	TestEqual(TEXT("with no findings"), Errors.Num(), 0);
	TestTrue(TEXT("it is the applied loadout"), Component.GetApplied() == RequestA);
	TestEqual(TEXT("the revision counted it"), Component.GetRevision(), 1);
	TestEqual(TEXT("the applied broadcast came once, from the accept path"), Fixture.Listener->Count, 1);
	TestEqual(TEXT("with the revision"), Fixture.Listener->LastRevision, 1);
	TestFalse(TEXT("the grant did not wait for a pawn"), Component.IsGrantPending());

	// The assets were resolved once and the table compiled on this machine
	TestTrue(TEXT("the kit is resolved"), Component.GetResolvedKit() == Fixture.KitA.Get());
	TestTrue(TEXT("and the primary weapon is weapon 0"), Component.GetResolvedLoadout().Weapons.Num() == 1 && Component.GetResolvedLoadout().Weapons[0] == Fixture.Weapon.Get());
	TestEqual(TEXT("the table carries the revision"), Component.GetCompiledTable().Revision, 1);
	TestEqual(TEXT("and lists the bound and the triggered ability for granting"), Component.GetCompiledTable().GrantedAbilities.Num(), 2);

	// Grants visible on the ability system, outside any simulation
	TestTrue(TEXT("the bound ability is granted"), Fixture.HasAbility(UGSTN_TestGrantAbilityA::StaticClass()));
	TestTrue(TEXT("the triggered ability is granted"), Fixture.HasAbility(UGSTN_TestGrantAbilityB::StaticClass()));
	TestTrue(TEXT("the primary weapon's passive is granted"), Fixture.HasAbility(UGSTN_TestGrantAbilityC::StaticClass()));
	TestEqual(TEXT("the kit's init effect set the base speed"), ASC.GetNumericAttributeBase(WalkSpeed), 700.f);
	TestEqual(TEXT("and the meter maximum"), ASC.GetNumericAttributeBase(UGSTN_MeterAttributeSet::GetExMeterMaxAttribute()), 250.f);
	TestTrue(TEXT("the primary weapon's stats effect is active"), UGSTN_GE_WeaponStats::IsApplied(ASC, UGSTN_TestWeaponStatsA::StaticClass()));
	TestEqual(TEXT("and adds its speed on top"), ASC.GetNumericAttribute(WalkSpeed), 800.f);

	// Asking for the same loadout again is a no-op
	TestTrue(TEXT("the same request is accepted"), Component.AcceptLoadout(RequestA, Errors));
	TestEqual(TEXT("without a new revision"), Component.GetRevision(), 1);
	TestEqual(TEXT("or a new broadcast"), Fixture.Listener->Count, 1);

	// A different loadout replaces the grants: kit A's ability, the passive and the weapon's effect go, B stays
	const FGSTN_LoadoutData RequestB = Fixture.RequestKit(TEXT("KitB"));
	TestTrue(TEXT("a second loadout is accepted"), Component.AcceptLoadout(RequestB, Errors));
	TestEqual(TEXT("revision 2"), Component.GetRevision(), 2);
	TestEqual(TEXT("and a second broadcast"), Fixture.Listener->Count, 2);
	TestFalse(TEXT("kit A's ability was removed"), Fixture.HasAbility(UGSTN_TestGrantAbilityA::StaticClass()));
	TestFalse(TEXT("the old weapon's passive was removed"), Fixture.HasAbility(UGSTN_TestGrantAbilityC::StaticClass()));
	TestTrue(TEXT("the ability both kits trigger is there"), Fixture.HasAbility(UGSTN_TestGrantAbilityB::StaticClass()));
	TestFalse(TEXT("the old weapon's stats effect was removed"), UGSTN_GE_WeaponStats::IsApplied(ASC, UGSTN_TestWeaponStatsA::StaticClass()));
	TestEqual(TEXT("the new kit's stats are in"), ASC.GetNumericAttribute(WalkSpeed), 500.f);
	TestEqual(TEXT("the table is the new one"), Component.GetCompiledTable().Revision, 2);

	// A pawn that is not there yet: the grant waits, and a later apply delivers it
	TStrongObjectPtr<UGSTN_TestLoadoutComponent> Late(Fixture.MakeComponent(nullptr));
	TestFalse(TEXT("kit A's ability is not there before"), Fixture.HasAbility(UGSTN_TestGrantAbilityA::StaticClass()));
	TestTrue(TEXT("a loadout is accepted without a pawn"), Late->AcceptLoadout(RequestA, Errors));
	TestTrue(TEXT("the grant waits"), Late->IsGrantPending());
	TestFalse(TEXT("nothing was granted"), Fixture.HasAbility(UGSTN_TestGrantAbilityA::StaticClass()));
	TestEqual(TEXT("the table is compiled anyway (every machine needs it)"), Late->GetCompiledTable().Revision, 1);

	Late->Target = &ASC;
	Late->ApplyAppliedLoadout();
	TestFalse(TEXT("the pawn arrived"), Late->IsGrantPending());
	TestTrue(TEXT("and the abilities are granted"), Fixture.HasAbility(UGSTN_TestGrantAbilityA::StaticClass()));
	TestTrue(TEXT("with the weapon's passive"), Fixture.HasAbility(UGSTN_TestGrantAbilityC::StaticClass()));
	TestEqual(TEXT("under the same revision"), Late->GetRevision(), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Automation tests of the defence GAS half (plan F6): the attribute mirror and its clamps, the loadout contribution (the shield binding
// only where the shield is available, the get-up binding, the Forced-started classes), the decisions the abilities make, the restore
// request. On a stock ability system component in a transient world: no Network Prediction, no Mover, no asset loaded from disk.

#include "AbilitySystemComponent.h"
#include "Abilities/GSTN_ShieldAbilityData.h"
#include "Attributes/GSTN_DefenceAttributeSet.h"
#include "Data/GSTN_DefenceKitExtension.h"
#include "Data/GSTN_DefenceRules.h"
#include "Data/GSTN_DefenceTracks.h"
#include "Effects/GSTN_GE_ShieldBreak.h"
#include "Effects/GSTN_GE_ShieldDamage.h"
#include "Effects/GSTN_GE_ShieldRestore.h"
#include "GSTN_DefenceLibrary.h"
#include "GSTN_DefenceTags.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_InputTags.h"
#include "Input/GSTN_StandardSlots.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Math/GSTN_DefenceViews.h"
#include "Misc/AutomationTest.h"
#include "Tests/GSTN_DefenceGASTestTypes.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using namespace GSTNDefenceGASTest;

	/** A kit in memory with the standard slots as its command set and one defence extension. Destroyed after the world (declaration order) */
	struct FKitFixture
	{
		TStrongObjectPtr<UGSTN_InputSlotSet> Base;
		TStrongObjectPtr<UGSTN_DefenceRules> Rules;
		TStrongObjectPtr<UGSTN_PrimaryKitDefinition> Kit;

		explicit FKitFixture(bool bWithExtension = true)
		{
			Base.Reset(NewObject<UGSTN_InputSlotSet>(GetTransientPackage()));
			for (const FGSTN_InputSlotSpec& Spec : UGSTN_StandardSlots::Build())
			{
				Base->Slots.AddDefaulted_GetRef().Inline = Spec;
			}
			Rules.Reset(NewObject<UGSTN_DefenceRules>(GetTransientPackage()));
			Rules->Shield.MaxShield = 40.f;
			Rules->Shield.DrainPerFrame = 0.25f;
			Rules->Shield.RegenPerFrame = 0.125f;

			Kit.Reset(NewObject<UGSTN_PrimaryKitDefinition>(GetTransientPackage()));
			Kit->CommandSet.BaseSet = TSoftObjectPtr<UGSTN_InputSlotSet>(Base.Get());
			if (bWithExtension)
			{
				FGSTN_DefenceKitExtension Extension;
				Extension.Rules = TSoftObjectPtr<UGSTN_DefenceRules>(Rules.Get());
				Extension.ShieldAbility = TSoftClassPtr<UObject>(UGSTN_TestDefenceShieldAbility::StaticClass());
				Kit->Extensions.ExtensionData.Add(FInstancedStruct::Make(Extension));
			}
		}

		/** The extension entry, to edit it between builds */
		FGSTN_DefenceKitExtension& Extension() { return Kit->Extensions.ExtensionData[0].GetMutable<FGSTN_DefenceKitExtension>(); }

		FGSTN_ResolvedLoadout Resolved() const
		{
			FGSTN_ResolvedLoadout Out;
			Out.Kit = Kit.Get();
			return Out;
		}

		/** The compile input of the kit, through the assembly (so the registered extension runs) */
		FGSTN_CompileInput Build() const
		{
			FGSTN_CompileInput Input;
			FGSTN_LoadoutAssembly::BuildCompileInput(Resolved(), 1, Input);
			return Input;
		}
	};

	const FGSTN_BindingInput* FindBinding(const FGSTN_CompileInput& Input, const FGameplayTag& Command)
	{
		return Input.Kit.Bindings.FindByPredicate([&Command](const FGSTN_BindingInput& Binding) { return Binding.Command == Command; });
	}

	int32 CountBindings(const FGSTN_CompileInput& Input, const FGameplayTag& Command)
	{
		int32 Count = 0;
		for (const FGSTN_BindingInput& Binding : Input.Kit.Bindings)
		{
			Count += Binding.Command == Command ? 1 : 0;
		}
		return Count;
	}

	bool HasTriggered(const FGSTN_CompileInput& Input, const UClass* Class)
	{
		return Input.Kit.TriggeredAbilities.Contains(FSoftObjectPath(Class));
	}

	UGSTN_DefenceAttributeSet* AddSet(UAbilitySystemComponent& ASC)
	{
		UGSTN_DefenceAttributeSet* Set = NewObject<UGSTN_DefenceAttributeSet>(ASC.GetOwner());
		ASC.AddAttributeSetSubobject(Set);
		return Set;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceGASAttributeClampTest, "GSTNDefence.GAS.AttributeClamp", Flags)
bool FGSTN_DefenceGASAttributeClampTest::RunTest(const FString& Parameters)
{
	FRig Rig;
	UAbilitySystemComponent& ASC = *Rig.ASC;
	UGSTN_DefenceAttributeSet* Set = AddSet(ASC);
	const FGameplayAttribute Health = UGSTN_DefenceAttributeSet::GetShieldHealthAttribute();
	const FGameplayAttribute Max = UGSTN_DefenceAttributeSet::GetMaxShieldHealthAttribute();

	// The data defaults of the rules struct, so a set never initialised from rules is a full default shield
	const FGSTN_ShieldRules Defaults;
	TestEqual(TEXT("the default maximum is the rules' default"), Set->GetMaxShieldHealth(), Defaults.MaxShield);
	TestEqual(TEXT("and the health starts full"), Set->GetShieldHealth(), Defaults.MaxShield);
	TestEqual(TEXT("the multiplier is 1"), Set->GetIncomingShieldDamageMultiplier(), 1.f);
	TestEqual(TEXT("the grab resistance is 1"), Set->GetGrabResistance(), 1.f);

	// [0, Max] on the base value and on the current value
	ASC.SetNumericAttributeBase(Health, 80.f);
	TestEqual(TEXT("the health is capped at the maximum"), ASC.GetNumericAttributeBase(Health), Defaults.MaxShield);
	ASC.SetNumericAttributeBase(Health, -5.f);
	TestEqual(TEXT("and floored at zero"), ASC.GetNumericAttributeBase(Health), 0.f);
	ASC.SetNumericAttributeBase(Health, 45.f);
	TestEqual(TEXT("a value in range is kept"), Set->GetShieldHealth(), 45.f);

	// A maximum that drops pulls the health down with it
	ASC.SetNumericAttributeBase(Max, 30.f);
	TestEqual(TEXT("the maximum changed"), Set->GetMaxShieldHealth(), 30.f);
	TestEqual(TEXT("the health followed the lower maximum"), Set->GetShieldHealth(), 30.f);
	ASC.SetNumericAttributeBase(Max, 60.f);
	TestEqual(TEXT("a higher maximum leaves the health alone"), Set->GetShieldHealth(), 30.f);

	// Rates and scales are never negative
	ASC.SetNumericAttributeBase(UGSTN_DefenceAttributeSet::GetShieldDrainRateAttribute(), -1.f);
	TestEqual(TEXT("the drain rate is floored at zero"), Set->GetShieldDrainRate(), 0.f);
	ASC.SetNumericAttributeBase(UGSTN_DefenceAttributeSet::GetGrabResistanceAttribute(), -2.f);
	TestEqual(TEXT("the grab resistance too"), Set->GetGrabResistance(), 0.f);

	// InitFromRules writes the rules' numbers, maximum first; the rates read back as the model's view
	FGSTN_ShieldRules Rules;
	Rules.MaxShield = 70.f;
	Rules.DrainPerFrame = 0.5f;
	Rules.RegenPerFrame = 0.25f;
	Rules.RegenDelayFrames = 12;
	Set->InitFromRules(Rules);
	TestEqual(TEXT("init: maximum"), Set->GetMaxShieldHealth(), 70.f);
	TestEqual(TEXT("init: full"), Set->GetShieldHealth(), 70.f);
	const FGSTN_ShieldRates Rates = Set->ReadShieldRates(Rules.RegenDelayFrames);
	TestEqual(TEXT("rates: drain"), Rates.Drain, 0.5);
	TestEqual(TEXT("rates: regen"), Rates.Regen, 0.25);
	TestEqual(TEXT("rates: max"), Rates.Max, 70.0);
	TestEqual(TEXT("rates: delay"), Rates.RegenDelayFrames, 12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceGASShieldBindingOmittedTest, "GSTNDefence.GAS.ShieldBindingOmittedWhenUnavailable", Flags)
bool FGSTN_DefenceGASShieldBindingOmittedTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("the plugin's loadout extension is registered"), FGSTN_LoadoutExtensionRegistry::IsRegistered(UGSTN_DefenceLibrary::GetLoadoutExtensionId()));

	FRig Rig;
	UAbilitySystemComponent& ASC = *Rig.ASC;
	FKitFixture Fixture;
	const FGameplayTag ShieldCommand = GSTNDefenceTags::Command_Defence_Shield.GetTag();

	// Available: mode enabled (the rules default) and the kit names a shield ability
	TestTrue(TEXT("the shield is available for the kit"), UGSTN_DefenceLibrary::IsShieldAvailableForKit(Fixture.Kit.Get(), nullptr));
	FGSTN_CompileInput Input = Fixture.Build();
	TestEqual(TEXT("one shield binding"), CountBindings(Input, ShieldCommand), 1);
	if (const FGSTN_BindingInput* Binding = FindBinding(Input, ShieldCommand))
	{
		TestTrue(TEXT("to the kit's shield ability"), Binding->Ability == FSoftObjectPath(UGSTN_TestDefenceShieldAbility::StaticClass()));
		TestTrue(TEXT("grounded only"), Binding->Requirements.RequireTags.HasTagExact(GSTNInputTags::Context_Grounded.GetTag()));
		TestTrue(TEXT("with its own slot (the command set has none)"), Binding->bHasSlotOverride);
		TestTrue(TEXT("the slot is the shield command"), Binding->SlotOverride.Command == ShieldCommand);
		FGameplayTagContainer Buttons;
		for (const FGSTN_InputStep& Step : Binding->SlotOverride.Pattern.Steps)
		{
			for (const TInstancedStruct<FGSTN_InputCondition>& Condition : Step.AllOf)
			{
				Condition.Get().GetReferencedButtons(Buttons);
			}
		}
		TestEqual(TEXT("one step"), Binding->SlotOverride.Pattern.Steps.Num(), 1);
		TestTrue(TEXT("anchored on the shield button"), Buttons.HasTagExact(GSTNCombatButtonTags::Button_Shield.GetTag()) && Buttons.Num() == 1);
		TestTrue(TEXT("the pattern is structurally valid"), Binding->SlotOverride.Pattern.IsStructurallyValid());
	}

	// The compiler accepts the override: the table has a row for the shield command with a slot of its own
	FGSTN_CompileReport Report;
	const FGSTN_CompiledBindingTable Table = FGSTN_BindingTableCompiler::Compile(Input, UGSTN_InputSettings::Get(), &Report);
	TestTrue(TEXT("the table compiles without error"), Report.Errors.IsEmpty());
	const FGSTN_CompiledEntry* Row = Table.Entries.FindByPredicate([&](const FGSTN_CompiledEntry& Entry) { return Entry.Command == ShieldCommand; });
	TestNotNull(TEXT("a row for the shield command"), Row);
	if (Row)
	{
		TestTrue(TEXT("with a slot"), Table.Slots.IsValidIndex(Row->SlotIndex) && Table.Slots[Row->SlotIndex].Command == ShieldCommand);
	}
	TestTrue(TEXT("the shield ability is granted by the table"), Table.GrantedAbilities.Contains(FSoftObjectPath(UGSTN_TestDefenceShieldAbility::StaticClass())));

	// Granted: the attribute set, initialised from the kit's rules
	FGSTN_LoadoutExtensionContext Context;
	const FGSTN_ResolvedLoadout Resolved = Fixture.Resolved();
	Context.Resolved = &Resolved;
	UGSTN_DefenceLibrary::GrantDefence(Context, ASC);
	const UGSTN_DefenceAttributeSet* Set = UGSTN_DefenceLibrary::FindDefenceAttributeSet(&ASC);
	TestNotNull(TEXT("the attribute set is granted"), Set);
	if (Set)
	{
		TestEqual(TEXT("from the kit's rules: maximum"), Set->GetMaxShieldHealth(), 40.f);
		TestEqual(TEXT("from the kit's rules: drain"), Set->GetShieldDrainRate(), 0.25f);
	}
	UGSTN_DefenceLibrary::GrantDefence(Context, ASC);
	TestEqual(TEXT("granting twice adds no second set"), ASC.GetSpawnedAttributes().Num(), 1);

	// The mode disables the shield: no binding, the set is revoked, the shield slot stays free
	Fixture.Rules->bShieldEnabled = false;
	TestFalse(TEXT("the shield is not available in this mode"), UGSTN_DefenceLibrary::IsShieldAvailableForKit(Fixture.Kit.Get(), nullptr));
	Input = Fixture.Build();
	TestEqual(TEXT("no shield binding where the mode disables it"), CountBindings(Input, ShieldCommand), 0);
	TestFalse(TEXT("and no shield ability granted"), Input.Kit.TriggeredAbilities.Contains(FSoftObjectPath(UGSTN_TestDefenceShieldAbility::StaticClass())));
	UGSTN_DefenceLibrary::GrantDefence(Context, ASC);
	TestNull(TEXT("the attribute set is gone"), UGSTN_DefenceLibrary::FindDefenceAttributeSet(&ASC));
	TestEqual(TEXT("nothing of the shield on the ability system"), ASC.GetSpawnedAttributes().Num(), 0);

	// The mode enables it but the kit has no shield ability: the same
	Fixture.Rules->bShieldEnabled = true;
	Fixture.Extension().ShieldAbility = nullptr;
	TestFalse(TEXT("a kit without a shield has none"), UGSTN_DefenceLibrary::IsShieldAvailableForKit(Fixture.Kit.Get(), nullptr));
	Input = Fixture.Build();
	TestEqual(TEXT("no shield binding for a kit without a shield"), CountBindings(Input, ShieldCommand), 0);
	UGSTN_DefenceLibrary::GrantDefence(Context, ASC);
	TestNull(TEXT("no attribute set for a kit without a shield"), UGSTN_DefenceLibrary::FindDefenceAttributeSet(&ASC));

	// A kit without the extension at all adds nothing
	const FKitFixture Plain(false);
	TestFalse(TEXT("no extension, no shield"), UGSTN_DefenceLibrary::IsShieldAvailableForKit(Plain.Kit.Get(), nullptr));
	Input = Plain.Build();
	TestEqual(TEXT("no extension, no binding"), CountBindings(Input, ShieldCommand), 0);
	TestTrue(TEXT("no extension, nothing granted"), Input.Kit.TriggeredAbilities.IsEmpty());

	UGSTN_DefenceLibrary::RevokeDefence(ASC);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceGASShieldAbilityRefusesTest, "GSTNDefence.GAS.ShieldAbilityRefusesWhenUnavailable", Flags)
bool FGSTN_DefenceGASShieldAbilityRefusesTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_DefenceRules> Rules(NewObject<UGSTN_DefenceRules>(GetTransientPackage()));
	EGSTN_ShieldRefusal Reason = EGSTN_ShieldRefusal::None;

	// The safeguard: where the shield is not available the ability refuses even if the command were somehow built
	TestFalse(TEXT("no rules: refused"), UGSTN_DefenceLibrary::CanExecuteShield(nullptr, true, 50.0, false, true, Reason));
	TestTrue(TEXT("as unavailable"), Reason == EGSTN_ShieldRefusal::Unavailable);
	TestFalse(TEXT("a kit without a shield: refused"), UGSTN_DefenceLibrary::CanExecuteShield(Rules.Get(), false, 50.0, false, true, Reason));
	TestTrue(TEXT("as unavailable"), Reason == EGSTN_ShieldRefusal::Unavailable);
	Rules->bShieldEnabled = false;
	TestFalse(TEXT("a mode without a shield: refused"), UGSTN_DefenceLibrary::CanExecuteShield(Rules.Get(), true, 50.0, false, true, Reason));
	TestTrue(TEXT("as unavailable"), Reason == EGSTN_ShieldRefusal::Unavailable);
	Rules->bShieldEnabled = true;

	// The command rules of spec 5.3: value above zero, not breaking, grounded
	TestFalse(TEXT("an empty shield: refused"), UGSTN_DefenceLibrary::CanExecuteShield(Rules.Get(), true, 0.0, false, true, Reason));
	TestTrue(TEXT("as empty"), Reason == EGSTN_ShieldRefusal::Empty);
	TestFalse(TEXT("a breaking shield: refused"), UGSTN_DefenceLibrary::CanExecuteShield(Rules.Get(), true, 10.0, true, true, Reason));
	TestTrue(TEXT("as breaking"), Reason == EGSTN_ShieldRefusal::Breaking);
	TestFalse(TEXT("in the air: refused"), UGSTN_DefenceLibrary::CanExecuteShield(Rules.Get(), true, 10.0, false, false, Reason));
	TestTrue(TEXT("as not grounded"), Reason == EGSTN_ShieldRefusal::NotGrounded);
	TestTrue(TEXT("grounded with a shield: allowed"), UGSTN_DefenceLibrary::CanExecuteShield(Rules.Get(), true, 0.01, false, true, Reason));
	TestTrue(TEXT("no reason"), Reason == EGSTN_ShieldRefusal::None);

	// The timeline the ability runs: startup, one hold window on the shield button that ends at zero value, release; options over the hold
	FGSTN_ShieldAbilitySpec Spec;
	TestTrue(TEXT("the hold button defaults to the shield button"), Spec.HoldButton == GSTNCombatButtonTags::Button_Shield.GetTag());
	FGSTN_OutOfShieldOption Jump;
	Jump.IntoCommands = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNCommandTags::Movement_Jump.GetTag()));
	Spec.OutOfShieldOptions.Add(Jump);
	Spec.OutOfShieldOptions.Add(Jump);
	Rules->Shield.StartupFrames = 2;
	Rules->Shield.ReleaseFrames = 6;
	const FGSTN_CombatTimelineData Timeline = Spec.BuildTimeline(Rules->Shield);
	TestEqual(TEXT("startup + hold + release"), Timeline.TotalFrames, 9);
	TestEqual(TEXT("actionable at the end"), Timeline.FirstActionableFrame, 9);
	TestEqual(TEXT("no override window"), Timeline.OverrideFrames, 0);
	TestEqual(TEXT("the hold window and one cancel window per option"), Timeline.Tracks.Num(), 3);
	const FGSTN_HoldWindowTrack* Hold = Timeline.Tracks.Num() > 0 ? Timeline.Tracks[0].GetPtr<FGSTN_HoldWindowTrack>() : nullptr;
	TestNotNull(TEXT("the first track is the hold window"), Hold);
	if (Hold)
	{
		TestEqual(TEXT("from the end of startup"), Hold->StartFrame, 2);
		TestEqual(TEXT("one frame long"), Hold->EndFrame, 3);
		TestTrue(TEXT("on the shield button"), Hold->HoldButton == GSTNCombatButtonTags::Button_Shield.GetTag());
		TestTrue(TEXT("ends when the value decays to zero"), Hold->bEndOnZeroShield);
		TArray<FString> Errors;
		Hold->Validate(TEXT("hold"), Errors);
		TestTrue(TEXT("and validates"), Errors.IsEmpty());
	}
	for (int32 Index = 1; Index < Timeline.Tracks.Num(); ++Index)
	{
		const FGSTN_TimelineTrack_CancelWindow* Window = Timeline.Tracks[Index].GetPtr<FGSTN_TimelineTrack_CancelWindow>();
		TestNotNull(TEXT("an out-of-shield option is a cancel window"), Window);
		if (Window)
		{
			TestTrue(TEXT("over the hold window"), Window->StartFrame == 2 && Window->EndFrame == 3);
			TestTrue(TEXT("into the option's commands"), Window->IntoCommands.Matches(FGameplayTagContainer(GSTNCommandTags::Movement_Jump.GetTag())));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceGASTechSetPerKitTest, "GSTNDefence.GAS.TechSetPerKit", Flags)
bool FGSTN_DefenceGASTechSetPerKitTest::RunTest(const FString& Parameters)
{
	// Kit A techs in place with A and rolls left with B; kit B only techs in place, with B; kit C names a class that is not an ability
	FKitFixture A;
	A.Extension().Tech.Abilities.Add(EGSTN_TechType::InPlace, TSoftClassPtr<UObject>(UGSTN_TestDefenceTechA::StaticClass()));
	A.Extension().Tech.Abilities.Add(EGSTN_TechType::RollLeft, TSoftClassPtr<UObject>(UGSTN_TestDefenceTechB::StaticClass()));
	FKitFixture B;
	B.Extension().Tech.Abilities.Add(EGSTN_TechType::InPlace, TSoftClassPtr<UObject>(UGSTN_TestDefenceTechB::StaticClass()));
	FKitFixture C;
	C.Extension().Tech.Abilities.Add(EGSTN_TechType::WallTech, TSoftClassPtr<UObject>(UGSTN_TestDefenceNotAnAbility::StaticClass()));
	FKitFixture None;

	TMap<EGSTN_TechType, TSubclassOf<UGameplayAbility>> Resolved;
	UGSTN_DefenceLibrary::ResolveTechSet(A.Extension().Tech, Resolved);
	TestEqual(TEXT("kit A: two tech kinds"), Resolved.Num(), 2);
	TestTrue(TEXT("kit A: in place is A"), Resolved.FindRef(EGSTN_TechType::InPlace) == UGSTN_TestDefenceTechA::StaticClass());
	TestTrue(TEXT("kit A: roll left is B"), Resolved.FindRef(EGSTN_TechType::RollLeft) == UGSTN_TestDefenceTechB::StaticClass());

	UGSTN_DefenceLibrary::ResolveTechSet(B.Extension().Tech, Resolved);
	TestEqual(TEXT("kit B: one tech kind"), Resolved.Num(), 1);
	TestTrue(TEXT("kit B: in place is B"), Resolved.FindRef(EGSTN_TechType::InPlace) == UGSTN_TestDefenceTechB::StaticClass());
	TestFalse(TEXT("kit B: no roll"), Resolved.Contains(EGSTN_TechType::RollLeft));

	AddExpectedMessage(TEXT("is not a gameplay ability class"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	UGSTN_DefenceLibrary::ResolveTechSet(C.Extension().Tech, Resolved);
	TestTrue(TEXT("kit C: a class that is not an ability is dropped"), Resolved.IsEmpty());

	UGSTN_DefenceLibrary::ResolveTechSet(None.Extension().Tech, Resolved);
	TestTrue(TEXT("the standard set has no tech"), Resolved.IsEmpty() && None.Extension().Tech.IsEmpty());

	// The loadout grants the kit's own tech classes (Forced starts need them granted and in the registry), nothing of another kit's
	const FGSTN_CompileInput InputA = A.Build();
	TestTrue(TEXT("kit A grants tech A"), HasTriggered(InputA, UGSTN_TestDefenceTechA::StaticClass()));
	TestTrue(TEXT("kit A grants tech B"), HasTriggered(InputA, UGSTN_TestDefenceTechB::StaticClass()));
	const FGSTN_CompileInput InputB = B.Build();
	TestFalse(TEXT("kit B does not grant tech A"), HasTriggered(InputB, UGSTN_TestDefenceTechA::StaticClass()));
	TestTrue(TEXT("kit B grants tech B"), HasTriggered(InputB, UGSTN_TestDefenceTechB::StaticClass()));
	const FGSTN_CompileInput InputNone = None.Build();
	TestTrue(TEXT("no tech, nothing granted for it"), InputNone.Kit.TriggeredAbilities.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceGASGetupNeutralTest, "GSTNDefence.GAS.GetupNeutralRequiresKnockdown", Flags)
bool FGSTN_DefenceGASGetupNeutralTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Knockdown = GSTNDefenceTags::State_Knockdown.GetTag();
	const FGameplayTag Jump = GSTNCommandTags::Movement_Jump.GetTag();

	// The requirement the ability and its binding share: downed, and only downed
	const FGSTN_TagRequirements Requirements = UGSTN_DefenceLibrary::GetupNeutralRequirements();
	TestTrue(TEXT("requires the downed state"), Requirements.RequireTags.HasTagExact(Knockdown));
	TestEqual(TEXT("and nothing else"), Requirements.RequireTags.Num(), 1);
	TestTrue(TEXT("ignores nothing"), Requirements.IgnoreTags.IsEmpty());
	TestFalse(TEXT("standing: not satisfied"), Requirements.IsSatisfiedBy(FGameplayTagContainer(GSTNInputTags::Context_Grounded.GetTag())));
	FGameplayTagContainer Downed;
	Downed.AddTag(Knockdown);
	Downed.AddTag(GSTNInputTags::Context_Grounded.GetTag());
	TestTrue(TEXT("downed: satisfied"), Requirements.IsSatisfiedBy(Downed));

	// The binding: the jump command, the kit's own jump slot (no override), the ability of the kit's get-up set
	FKitFixture Fixture;
	Fixture.Extension().GetupSet.Neutral = TSoftClassPtr<UObject>(UGSTN_TestDefenceGetupNeutral::StaticClass());
	Fixture.Extension().GetupSet.Roll = TSoftClassPtr<UObject>(UGSTN_TestDefenceTechA::StaticClass());
	Fixture.Extension().GetupSet.GroundBounce = TSoftClassPtr<UObject>(UGSTN_TestDefenceTechB::StaticClass());
	const FGSTN_CompileInput Input = Fixture.Build();
	TestEqual(TEXT("one jump binding from the extension"), CountBindings(Input, Jump), 1);
	if (const FGSTN_BindingInput* Binding = FindBinding(Input, Jump))
	{
		TestTrue(TEXT("to the neutral get-up"), Binding->Ability == FSoftObjectPath(UGSTN_TestDefenceGetupNeutral::StaticClass()));
		TestTrue(TEXT("requiring the downed state"), Binding->Requirements.RequireTags.HasTagExact(Knockdown));
		TestEqual(TEXT("exactly as the shared requirement"), Binding->Requirements.RequireTags.Num(), 1);
		TestFalse(TEXT("on the kit's jump slot"), Binding->bHasSlotOverride);
	}
	// The roll and the bounce are started Forced by the model: granted, never bound to a command
	TestTrue(TEXT("the roll is granted"), HasTriggered(Input, UGSTN_TestDefenceTechA::StaticClass()));
	TestTrue(TEXT("the bounce is granted"), HasTriggered(Input, UGSTN_TestDefenceTechB::StaticClass()));
	TestFalse(TEXT("the neutral get-up is bound, not triggered"), HasTriggered(Input, UGSTN_TestDefenceGetupNeutral::StaticClass()));
	for (const FGSTN_BindingInput& Binding : Input.Kit.Bindings)
	{
		TestFalse(TEXT("no binding to the roll or the bounce"), Binding.Ability == FSoftObjectPath(UGSTN_TestDefenceTechA::StaticClass()) || Binding.Ability == FSoftObjectPath(UGSTN_TestDefenceTechB::StaticClass()));
	}

	// Without a neutral get-up there is no jump binding
	Fixture.Extension().GetupSet.Neutral = nullptr;
	TestEqual(TEXT("no neutral get-up, no jump binding"), CountBindings(Fixture.Build(), Jump), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceGASShieldRestoreRequestTest, "GSTNDefence.GAS.ShieldRestoreSeenAsRequest", Flags)
bool FGSTN_DefenceGASShieldRestoreRequestTest::RunTest(const FString& Parameters)
{
	FRig Rig;
	UAbilitySystemComponent& ASC = *Rig.ASC;
	const FGameplayAttribute Health = UGSTN_DefenceAttributeSet::GetShieldHealthAttribute();

	// The native defaults: instant effects, the mirror effect carrying the damage key, the restore its name key
	TestTrue(TEXT("the mirror effect is instant"), GetDefault<UGSTN_GE_ShieldDamage>()->DurationPolicy == EGameplayEffectDurationType::Instant);
	TestTrue(TEXT("the restore effect is instant"), GetDefault<UGSTN_GE_ShieldRestore>()->DurationPolicy == EGameplayEffectDurationType::Instant);
	TestTrue(TEXT("the break effect is instant and empty"), GetDefault<UGSTN_GE_ShieldBreak>()->DurationPolicy == EGameplayEffectDurationType::Instant && GetDefault<UGSTN_GE_ShieldBreak>()->Modifiers.IsEmpty());
	TestFalse(TEXT("the restore has a key"), UGSTN_GE_ShieldRestore::GetAmountKey().IsNone());

	// Without the set nothing can be mirrored or asked
	TestFalse(TEXT("no set: the mirror is not written"), UGSTN_GE_ShieldDamage::ApplyMirror(ASC, nullptr, 20.f));
	TestEqual(TEXT("no set: no request"), UGSTN_GE_ShieldRestore::RequestedDelta(ASC, 0.f), 0.f);

	UGSTN_DefenceAttributeSet* Set = AddSet(ASC);
	FGSTN_ShieldRules Rules;
	Rules.MaxShield = 50.f;
	Set->InitFromRules(Rules);

	// The model mirrors: whatever it writes, the mirror never looks like a request to it
	TestTrue(TEXT("the mirror is written"), UGSTN_GE_ShieldDamage::ApplyMirror(ASC, nullptr, 20.f));
	TestEqual(TEXT("to the value"), ASC.GetNumericAttributeBase(Health), 20.f);
	float LastMirrored = 20.f;
	TestEqual(TEXT("a mirrored value is no request"), UGSTN_GE_ShieldRestore::RequestedDelta(ASC, LastMirrored), 0.f);
	TestTrue(TEXT("a blocked hit's loss is applied"), UGSTN_GE_ShieldDamage::Apply(ASC, nullptr, 5.f));
	TestEqual(TEXT("and subtracts"), ASC.GetNumericAttributeBase(Health), 15.f);
	LastMirrored = 15.f;
	TestTrue(TEXT("regeneration mirrored upward"), UGSTN_GE_ShieldDamage::ApplyMirror(ASC, nullptr, 18.f));
	TestEqual(TEXT("a negative loss raises the mirror"), ASC.GetNumericAttributeBase(Health), 18.f);
	LastMirrored = 18.f;
	TestEqual(TEXT("still no request"), UGSTN_GE_ShieldRestore::RequestedDelta(ASC, LastMirrored), 0.f);

	// A pickup restores: the model sees the difference against what it last mirrored
	TestTrue(TEXT("a restore is applied"), UGSTN_GE_ShieldRestore::Apply(ASC, nullptr, 10.f, nullptr));
	TestEqual(TEXT("the mirror moved"), ASC.GetNumericAttributeBase(Health), 28.f);
	TestEqual(TEXT("seen as a request of the amount"), UGSTN_GE_ShieldRestore::RequestedDelta(ASC, LastMirrored), 10.f);

	// The model honours it and mirrors the result: the request is consumed
	TestTrue(TEXT("the model mirrors the restored value"), UGSTN_GE_ShieldDamage::ApplyMirror(ASC, nullptr, 28.f));
	LastMirrored = 28.f;
	TestEqual(TEXT("consumed"), UGSTN_GE_ShieldRestore::RequestedDelta(ASC, LastMirrored), 0.f);

	// An over-restore is clamped by the attribute, so the request is at most what fits
	TestTrue(TEXT("an over-restore is applied"), UGSTN_GE_ShieldRestore::Apply(ASC, nullptr, 100.f, nullptr));
	TestEqual(TEXT("clamped at the maximum"), ASC.GetNumericAttributeBase(Health), 50.f);
	TestEqual(TEXT("the request is what fits"), UGSTN_GE_ShieldRestore::RequestedDelta(ASC, LastMirrored), 22.f);

	// A loss something else applied is a negative request the model honours too
	LastMirrored = 50.f;
	TestTrue(TEXT("a hand-applied loss"), UGSTN_GE_ShieldDamage::Apply(ASC, nullptr, 7.f));
	TestEqual(TEXT("seen as a negative request"), UGSTN_GE_ShieldRestore::RequestedDelta(ASC, LastMirrored), -7.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

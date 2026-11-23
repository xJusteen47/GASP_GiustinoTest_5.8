// Tests of the anim set resolution (anim-layers.md section 4): GSTNAnimation.Core.*
//
// Pure: transient sets and collections, no world, no mesh. Slot tags only need to be valid, so the native roots stand in for two
// feature slots; the cosmetic traits are the example leaves of Config/Tags/GSTNAnimation.ini.

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimInstance.h"
#include "GameplayTagsManager.h"
#include "GSTN_AnimSetUtils.h"
#include "GSTN_AnimTags.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace GSTNAnimSetTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	static UGSTN_AnimSetDefinition* MakeSet(FGameplayTag Slot, UClass* LayerClass = nullptr)
	{
		UGSTN_AnimSetDefinition* Set = NewObject<UGSTN_AnimSetDefinition>(GetTransientPackage());
		Set->SetTag = Slot;
		Set->LayerClass = TSoftClassPtr<UAnimInstance>(LayerClass);
		return Set;
	}

	static FGSTN_AnimSetRule MakeRule(UGSTN_AnimSetDefinition* Set, const FGameplayTagQuery& When = FGameplayTagQuery(), int32 Priority = 0)
	{
		FGSTN_AnimSetRule Rule;
		Rule.Set = Set;
		Rule.When = When;
		Rule.Priority = Priority;
		return Rule;
	}

	static FGSTN_AnimSetSource MakeSource(int32 Priority, TArray<FGSTN_AnimSetRule> Rules)
	{
		UGSTN_AnimSetCollection* Collection = NewObject<UGSTN_AnimSetCollection>(GetTransientPackage());
		Collection->Rules = MoveTemp(Rules);
		FGSTN_AnimSetSource Source;
		Source.SourcePriority = Priority;
		Source.Collection = Collection;
		return Source;
	}

	static FGameplayTag Trait(const TCHAR* Name)
	{
		return UGameplayTagsManager::Get().RequestGameplayTag(FName(Name), /*ErrorIfNotFound*/ false);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AnimSetPriorityTest, "GSTNAnimation.Core.ResolveWinnersPriority", GSTNAnimSetTest::TestFlags)
bool FGSTN_AnimSetPriorityTest::RunTest(const FString& Parameters)
{
	using namespace GSTNAnimSetTest;
	const FGameplayTag SlotA = GSTNAnimTags::Root_Set;
	const FGameplayTag SlotB = GSTNAnimTags::Root_Trait;

	UGSTN_AnimSetDefinition* CharacterA = MakeSet(SlotA);
	UGSTN_AnimSetDefinition* CharacterB = MakeSet(SlotB);
	UGSTN_AnimSetDefinition* SkinA = MakeSet(SlotA);
	UGSTN_AnimSetDefinition* KitA = MakeSet(SlotA);

	TArray<FGSTN_AnimSetSource> Sources;
	Sources.Add(MakeSource(GSTNAnimSet::PriorityCharacter, { MakeRule(CharacterA), MakeRule(CharacterB) }));
	Sources.Add(MakeSource(GSTNAnimSet::PrioritySkin, { MakeRule(SkinA) }));
	Sources.Add(MakeSource(GSTNAnimSet::PriorityKit, { MakeRule(KitA, FGameplayTagQuery(), 1000) }));

	TMap<FGameplayTag, UGSTN_AnimSetDefinition*> Winners = UGSTN_AnimSetUtils::ResolveWinners(Sources, FGameplayTagContainer());
	TestTrue(TEXT("Skin source wins its slot, whatever the kit rule priority"), Winners.FindRef(SlotA) == SkinA);
	TestTrue(TEXT("A slot only the character fills keeps the character's set"), Winners.FindRef(SlotB) == CharacterB);

	// Within one source the rule priority decides; on a tie the first rule wins
	UGSTN_AnimSetDefinition* First = MakeSet(SlotA);
	UGSTN_AnimSetDefinition* Second = MakeSet(SlotA);
	UGSTN_AnimSetDefinition* High = MakeSet(SlotA);
	TArray<FGSTN_AnimSetSource> Tie = { MakeSource(0, { MakeRule(First), MakeRule(Second) }) };
	TestTrue(TEXT("Tie: first rule wins"), UGSTN_AnimSetUtils::ResolveWinners(Tie, FGameplayTagContainer()).FindRef(SlotA) == First);
	TArray<FGSTN_AnimSetSource> Ranked = { MakeSource(0, { MakeRule(First), MakeRule(High, FGameplayTagQuery(), 5) }) };
	TestTrue(TEXT("Higher rule priority wins"), UGSTN_AnimSetUtils::ResolveWinners(Ranked, FGameplayTagContainer()).FindRef(SlotA) == High);

	// A rule whose set has no slot tag never competes
	TArray<FGSTN_AnimSetSource> NoTag = { MakeSource(0, { MakeRule(MakeSet(FGameplayTag())) }) };
	TestEqual(TEXT("Untagged set ignored"), UGSTN_AnimSetUtils::ResolveWinners(NoTag, FGameplayTagContainer()).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AnimSetTraitTest, "GSTNAnimation.Core.ResolveByCosmeticTraits", GSTNAnimSetTest::TestFlags)
bool FGSTN_AnimSetTraitTest::RunTest(const FString& Parameters)
{
	using namespace GSTNAnimSetTest;
	const FGameplayTag Heavy = Trait(TEXT("Anim.Trait.Build.Heavy"));
	const FGameplayTag Female = Trait(TEXT("Anim.Trait.Body.Female"));
	if (!Heavy.IsValid() || !Female.IsValid())
	{
		AddError(TEXT("The example Anim.Trait leaves of Config/Tags/GSTNAnimation.ini are not registered"));
		return false;
	}

	const FGameplayTag Slot = GSTNAnimTags::Root_Set;
	UGSTN_AnimSetDefinition* Base = MakeSet(Slot);
	UGSTN_AnimSetDefinition* HeavyVariant = MakeSet(Slot);
	UGSTN_AnimSetDefinition* HeavyFemaleVariant = MakeSet(Slot);

	// The skin's collection: heavy variant, and a more specific one for heavy female bodies
	TArray<FGSTN_AnimSetSource> Sources;
	Sources.Add(MakeSource(GSTNAnimSet::PriorityCharacter, { MakeRule(Base) }));
	Sources.Add(MakeSource(GSTNAnimSet::PrioritySkin, {
		MakeRule(HeavyVariant, FGameplayTagQuery::MakeQuery_MatchTag(Heavy)),
		MakeRule(HeavyFemaleVariant, FGameplayTagQuery::MakeQuery_MatchAllTags(FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{ Heavy, Female })), 1) }));

	TestTrue(TEXT("No trait: base"), UGSTN_AnimSetUtils::ResolveWinners(Sources, FGameplayTagContainer()).FindRef(Slot) == Base);
	TestTrue(TEXT("Heavy: heavy variant"), UGSTN_AnimSetUtils::ResolveWinners(Sources, FGameplayTagContainer(Heavy)).FindRef(Slot) == HeavyVariant);

	FGameplayTagContainer HeavyFemale(Heavy);
	HeavyFemale.AddTag(Female);
	TestTrue(TEXT("Heavy + female: the more specific variant"), UGSTN_AnimSetUtils::ResolveWinners(Sources, HeavyFemale).FindRef(Slot) == HeavyFemaleVariant);
	TestTrue(TEXT("Female only: base"), UGSTN_AnimSetUtils::ResolveWinners(Sources, FGameplayTagContainer(Female)).FindRef(Slot) == Base);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AnimSetLayerFallbackTest, "GSTNAnimation.Core.ResolveLayerClassesFallback", GSTNAnimSetTest::TestFlags)
bool FGSTN_AnimSetLayerFallbackTest::RunTest(const FString& Parameters)
{
	using namespace GSTNAnimSetTest;
	const FGameplayTag Slot = GSTNAnimTags::Root_Set;

	// UAnimInstance itself stands in for a layer class: only the class identity matters here
	UGSTN_AnimSetDefinition* WithLayer = MakeSet(Slot, UAnimInstance::StaticClass());
	UGSTN_AnimSetDefinition* MontagesOnly = MakeSet(Slot);

	TArray<FGSTN_AnimSetSource> Sources;
	Sources.Add(MakeSource(GSTNAnimSet::PriorityCharacter, { MakeRule(WithLayer) }));
	Sources.Add(MakeSource(GSTNAnimSet::PrioritySkin, { MakeRule(MontagesOnly) }));

	TestTrue(TEXT("The montage-only skin set wins the slot"), UGSTN_AnimSetUtils::ResolveWinners(Sources, FGameplayTagContainer()).FindRef(Slot) == MontagesOnly);
	const TMap<FGameplayTag, TSoftClassPtr<UAnimInstance>> Layers = UGSTN_AnimSetUtils::ResolveLayerClasses(Sources, FGameplayTagContainer());
	TestTrue(TEXT("...but the character's layer class stays linked"), Layers.Contains(Slot) && Layers[Slot].Get() == UAnimInstance::StaticClass());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

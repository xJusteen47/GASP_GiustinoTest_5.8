// Automation tests of two fixes of the M3 review: a refused commit ends the ability (no free timeline) and the preloaded montage
// of a timeline ability stays in memory (a montage collected on one machine only would desynchronise the played montages).

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Tests/GSTN_ReviewTestTypes.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** A transient world with an actor that has a plain ability system */
	struct FReviewRig
	{
		FReviewRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNCombatNPReviewTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());

			AActor* Owner = World->SpawnActor<AActor>();
			ASC = NewObject<UAbilitySystemComponent>(Owner);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Owner, Owner);
		}

		~FReviewRig()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		/** Grants and activates the ability; returns its spec (null when it did not activate) */
		const FGameplayAbilitySpec* Activate(TSubclassOf<UGameplayAbility> Class)
		{
			const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(Class, 1));
			ASC->TryActivateAbility(Handle);
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
			return Spec && Spec->IsActive() ? Spec : nullptr;
		}

		UWorld* World = nullptr;
		UAbilitySystemComponent* ASC = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPReviewCommitRefusedTest, "GSTNCombat.NP.Review.CommitRefusedCancels", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPReviewCommitRefusedTest::RunTest(const FString& Parameters)
{
	FReviewRig Rig;

	const FGameplayAbilitySpec* Paid = Rig.Activate(UGSTN_TestCommitAbility::StaticClass());
	if (!TestNotNull(TEXT("the plain ability activates"), Paid))
	{
		return false;
	}
	TestTrue(TEXT("a commit that goes through reports it"), UGSTN_GA_BaseCombat::CommitOrCancel(*Rig.ASC, *Paid));
	TestTrue(TEXT("and leaves the ability running"), Paid->IsActive());

	const FGameplayAbilitySpec* Refused = Rig.Activate(UGSTN_TestRefusedCommitAbility::StaticClass());
	if (!TestNotNull(TEXT("the ability that will be refused activates"), Refused))
	{
		return false;
	}
	const FGameplayAbilitySpecHandle RefusedHandle = Refused->Handle;
	TestFalse(TEXT("a refused commit reports it"), UGSTN_GA_BaseCombat::CommitOrCancel(*Rig.ASC, *Refused));
	const FGameplayAbilitySpec* After = Rig.ASC->FindAbilitySpecFromHandle(RefusedHandle);
	TestTrue(TEXT("and ends the ability, so the timeline cannot run for free"), After && !After->IsActive());

	// An ability that is not running has nothing to commit
	TestFalse(TEXT("an ended ability is not committed again"), After && UGSTN_GA_BaseCombat::CommitOrCancel(*Rig.ASC, *After));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPReviewMontageRetainedTest, "GSTNCombat.NP.Review.MontageStaysLoaded", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPReviewMontageRetainedTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_TestMontageTimelineAbility> Ability(NewObject<UGSTN_TestMontageTimelineAbility>(GetTransientPackage()));
	{
		UAnimMontage* Montage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("GSTNReviewTestMontage"));
		Ability->Timeline.Montage = TSoftObjectPtr<UAnimMontage>(Montage);

		// The owner-kept list holds the montage the call loaded
		TArray<TObjectPtr<UObject>> Kept;
		Ability->LoadTimelineContent(Kept);
		TestTrue(TEXT("the owner's list receives the montage"), Kept.Contains(Montage));

		// The ability's own preload keeps it too, with nothing else referencing the montage once this scope ends
		Ability->PreloadTimelineContent();
	}

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestNotNull(TEXT("the montage survives garbage collection through the ability"), Ability->GetTimelineData().Montage.Get());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Shared fixtures of the defence NP automation tests (M6 F7). Private: nothing outside the tests includes this.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Data/GSTN_DefenceRules.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Sim/GSTN_DefenceAttach.h"
#include "Sim/GSTN_DefenceFragment.h"
#include "Sync/GSTN_SyncFragment.h"
#include "UObject/StrongObjectPtr.h"
#include "World/GSTN_StepSnapshot.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNDefenceNPTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** The short test numbers of m6 spec 11: shield 100, drain 0.4 a frame, a five frame release-anchored perfect shield, one startup frame, six release frames */
	inline UGSTN_DefenceRules* MakeRules(bool bShieldEnabled = true)
	{
		UGSTN_DefenceRules* Rules = NewObject<UGSTN_DefenceRules>(GetTransientPackage());
		Rules->bShieldEnabled = bShieldEnabled;
		Rules->Shield.MaxShield = 100.f;
		Rules->Shield.DrainPerFrame = 0.4f;
		Rules->Shield.RegenPerFrame = 0.2f;
		Rules->Shield.RegenDelayFrames = 10;
		Rules->Shield.StartupFrames = 1;
		Rules->Shield.ReleaseFrames = 6;
		Rules->Shield.RadiusCm = 80.f;
		Rules->Shield.CentreHeightCm = 90.f;
		Rules->Parry.WindowFrames = 5;
		if (Rules->ParryProfiles.Num() < 2)
		{
			FGSTN_ParryProfile Counter;
			Counter.Name = TEXT("Counter");
			Counter.HitstopFrames = 8;
			Counter.AttackerStunFrames = 20;
			Rules->ParryProfiles.Add(Counter);
		}
		return Rules;
	}

	/** An actor that is not in any world: enough to be the key of an attachment */
	inline AActor* MakeOwner()
	{
		return NewObject<AActor>(GetTransientPackage());
	}

	/** A fragment raised at PressFrame with the full value stamped there (what the tick extension writes on the hook's first stamp) */
	inline FGSTN_DefenceFragment MakeRaised(int32 PressFrame, double Value = 100.0)
	{
		FGSTN_DefenceFragment Fragment;
		Fragment.ShieldValueQ = FGSTN_DefenceFragment::QuantizeValue(Value);
		Fragment.ShieldStampFrame = PressFrame;
		Fragment.ShieldFlags = GSTNShieldFlags::Raised;
		Fragment.ShieldPressFrame = PressFrame;
		Fragment.ShieldReleaseFrame = INDEX_NONE;
		return Fragment;
	}

	/** The fragment of the array, or the default fragment when it has none */
	inline FGSTN_DefenceFragment ReadFragment(const FGSTN_SyncFragmentArray& Fragments)
	{
		const FGSTN_DefenceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Fragments);
		return Fragment ? *Fragment : FGSTN_DefenceFragment();
	}

	inline void SetFragment(FGSTN_SyncFragmentArray& Fragments, const FGSTN_DefenceFragment& Fragment)
	{
		FGSTN_SyncFragmentList::FindOrAdd<FGSTN_DefenceFragment>(Fragments) = Fragment;
	}

	/** A transient game world with one actor owning a stock ability system component: loose tags need no more */
	struct FRig
	{
		FRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNDefenceNPTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Owner = World->SpawnActor<AActor>();
			ASC = NewObject<UAbilitySystemComponent>(Owner);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Owner, Owner);
		}

		~FRig()
		{
			FGSTN_DefenceAttach::Reset();
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		UWorld* World = nullptr;
		AActor* Owner = nullptr;
		UAbilitySystemComponent* ASC = nullptr;
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS

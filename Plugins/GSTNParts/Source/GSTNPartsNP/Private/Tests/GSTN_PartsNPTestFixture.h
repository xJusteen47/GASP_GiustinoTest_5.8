// Shared fixtures of the parts NP automation tests (P7). Private: nothing outside the tests includes this.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Data/GSTN_PartBreakEffects.h"
#include "Data/GSTN_PartDefinition.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Misc/AutomationTest.h"
#include "Pipeline/GSTN_PartsSnapshotExt.h"
#include "Sim/GSTN_PartsAttach.h"
#include "State/GSTN_PartsFragment.h"
#include "Sync/GSTN_SyncFragment.h"
#include "UObject/StrongObjectPtr.h"
#include "World/GSTN_StepSnapshot.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNPartsNPTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Part indices of the fixture set */
	constexpr int32 Torso = 0;
	constexpr int32 Arm = 1;
	constexpr int32 Hand = 2;
	constexpr int32 Head = 3;

	/** The hit group the broken arm stops */
	constexpr uint8 ArmHitGroup = 2;

	/**
	 * A small enemy: torso, an arm whose break stops hit group 2 and hides its hurtbox, a hand (child of the arm, which Keep policy), a
	 * weak point head. The grab gate opens after one broken part (the whole entity is grab locked). Hurtboxes are named after the parts, one box per part, plus a body box.
	 */
	struct FPartsFixture
	{
		TStrongObjectPtr<UGSTN_PartSetDefinition> Set;
		TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Hurtboxes;

		FPartsFixture()
		{
			Set.Reset(NewObject<UGSTN_PartSetDefinition>(GetTransientPackage()));
			Set->Parts.Add(MakePart("Torso", "TorsoBox", 0, 100.f));
			Set->Parts.Add(MakePart("Arm", "ArmBox", 4, 60.f));
			Set->Parts.Add(MakePart("Hand", "HandBox", 2, 30.f, Arm));
			Set->Parts.Add(MakePart("Head", "HeadBox", 1, 40.f));
			Set->Parts[Head].bWeakPoint = true;

			Set->Parts[Arm].BrokenPolicy = EGSTN_BrokenHurtboxPolicy::Disable;
			FGSTN_PartBreakEffect_DisableHitGroups Groups;
			Groups.GroupMask = static_cast<uint8>(1u << ArmHitGroup);
			Set->Parts[Arm].OnBreak.Add(FInstancedStruct::Make(Groups));

			Set->GrabGate.MinBrokenParts = 1;
			Set->RebakeWeightCurve();

			Hurtboxes.Reset(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage()));
			for (const TCHAR* Name : { TEXT("TorsoBox"), TEXT("ArmBox"), TEXT("HandBox"), TEXT("HeadBox"), TEXT("BodyBox") })
			{
				FGSTN_Hurtbox& Box = Hurtboxes->Hurtboxes.AddDefaulted_GetRef();
				Box.Name = FName(Name);
			}
		}

		static FGSTN_PartDefinition MakePart(FName Name, FName Hurtbox, int32 Hardness, float Cap, int32 ParentIndex = INDEX_NONE)
		{
			FGSTN_PartDefinition Part;
			Part.Name = Name;
			Part.Hurtboxes.Add(Hurtbox);
			Part.Hardness = Hardness;
			Part.PartCap = Cap;
			Part.ParentIndex = ParentIndex;
			return Part;
		}

		int32 Num() const { return Set->GetParts().Num(); }

		/** A snapshot entry with the hurtbox set and one normal state per box */
		FGSTN_SnapshotEntry MakeEntry(int32 EntityId = 2) const
		{
			FGSTN_SnapshotEntry Entry;
			Entry.EntityId = EntityId;
			Entry.Hurtboxes = Hurtboxes.Get();
			Entry.HurtStates.SetNum(Hurtboxes->Hurtboxes.Num());
			return Entry;
		}
	};

	/** A parts fragment array holding the state, as the combat sync state keeps it */
	inline void SetFragment(FGSTN_SyncFragmentArray& Fragments, const FGSTN_PartsState& State, int32 PartCount)
	{
		FGSTN_SyncFragmentList::FindOrAdd<FGSTN_PartsFragment>(Fragments).SetState(State, PartCount);
	}

	inline FGSTN_PartsState MakeState(uint16 Mask, uint8 Serial = 0, std::initializer_list<TPair<int32, uint8>> Damage = {})
	{
		FGSTN_PartsState State;
		State.BrokenMask = Mask;
		State.BreakSerial = Serial;
		for (const TPair<int32, uint8>& Entry : Damage)
		{
			State.PartDamage[Entry.Key] = Entry.Value;
		}
		return State;
	}

	/** The state of the fragment array, or the empty state when it has none */
	inline FGSTN_PartsState ReadState(const FGSTN_SyncFragmentArray& Fragments)
	{
		const FGSTN_PartsFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Fragments);
		return Fragment ? Fragment->ToState() : FGSTN_PartsState();
	}

	/** An actor that is not in any world: enough to be the key of an attachment */
	inline AActor* MakeOwner()
	{
		return NewObject<AActor>(GetTransientPackage());
	}

	/** A transient game world with one actor owning a stock ability system component: loose tags need no more */
	struct FRig
	{
		FRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNPartsNPTestWorld"));
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
			FGSTN_PartsAttach::Reset();
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

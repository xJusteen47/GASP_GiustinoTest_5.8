// Defence, Network Prediction half.

#include "GSTN_DefenceContentLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/GSTN_DefenceAttributeSet.h"
#include "Data/GSTN_DefenceKitExtension.h"
#include "Data/GSTN_DefenceRules.h"
#include "GameFramework/Actor.h"
#include "GSTN_DefenceLibrary.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Sim/GSTN_DefenceAttach.h"
#include "Sim/GSTN_DefenceFragment.h"
#include "Simulation/GSTN_CombatSimComponent.h"

bool UGSTN_DefenceContentLibrary::SetKitDefenceExtension(UGSTN_PrimaryKitDefinition* Kit, UGSTN_DefenceRules* Rules, TSubclassOf<UObject> ShieldAbility)
{
	if (!Kit)
	{
		return false;
	}
	TArray<FInstancedStruct>& Data = Kit->Extensions.ExtensionData;
	const int32 Existing = Data.IndexOfByPredicate([](const FInstancedStruct& Entry) { return Entry.GetPtr<FGSTN_DefenceKitExtension>() != nullptr; });
	FGSTN_DefenceKitExtension Extension;
	if (Existing != INDEX_NONE)
	{
		Extension = *Data[Existing].GetPtr<FGSTN_DefenceKitExtension>();
	}
	Extension.Rules = TSoftObjectPtr<UGSTN_DefenceRules>(Rules);
	Extension.ShieldAbility = ShieldAbility ? TSoftClassPtr<UObject>(ShieldAbility.Get()) : TSoftClassPtr<UObject>();
	if (Existing != INDEX_NONE)
	{
		Data[Existing] = FInstancedStruct::Make(Extension);
	}
	else
	{
		Data.Add(FInstancedStruct::Make(Extension));
	}
	Kit->MarkPackageDirty();
	return true;
}

bool UGSTN_DefenceContentLibrary::ClearKitDefenceExtension(UGSTN_PrimaryKitDefinition* Kit)
{
	if (!Kit)
	{
		return false;
	}
	const int32 Removed = Kit->Extensions.ExtensionData.RemoveAll([](const FInstancedStruct& Entry) { return Entry.GetPtr<FGSTN_DefenceKitExtension>() != nullptr; });
	if (Removed > 0)
	{
		Kit->MarkPackageDirty();
	}
	return Removed > 0;
}

bool UGSTN_DefenceContentLibrary::AttachDefenceToActor(AActor* Actor, UGSTN_DefenceRules* Rules, bool bKitHasShield)
{
	if (!Actor)
	{
		return false;
	}
	const UGSTN_CombatSimComponent* Combat = Actor->FindComponentByClass<UGSTN_CombatSimComponent>();
	return FGSTN_DefenceAttach::Apply(Actor, Rules, bKitHasShield, FGSTN_DefenceAttach::MakeBubbleIgnoreHitTypes(Combat ? Combat->GetCombatRules() : nullptr));
}

FString UGSTN_DefenceContentLibrary::GetDefenceStateString(const AActor* Actor)
{
	const UGSTN_CombatSimComponent* Combat = Actor ? Actor->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	const FGSTN_CombatSyncState* Sync = Combat ? Combat->GetSyncState() : nullptr;
	if (!Sync)
	{
		return TEXT("no combat");
	}
	const FGSTN_DefenceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Sync->Fragments);
	if (!Fragment)
	{
		return TEXT("none");
	}
	const FGSTN_DefenceFragment& F = *Fragment;
	return FString::Printf(TEXT("value=%d.%02d@%d flags=%d press=%d release=%d stun=%d recovery=%d break=%d parry=[%d,%d)p%d last=%d punish=%d push=%d"),
		F.ShieldValueQ / 4, (F.ShieldValueQ % 4) * 25, F.ShieldStampFrame, static_cast<int32>(F.ShieldFlags), F.ShieldPressFrame, F.ShieldReleaseFrame,
		F.ShieldStunEndFrame, F.ParryRecoveryEndFrame, F.BreakEndFrame, F.MoveParryStartFrame, F.MoveParryEndFrame, static_cast<int32>(F.MoveParryProfile),
		F.LastParryFrame, F.PunishEndFrame, F.PushbackStartFrame);
}

float UGSTN_DefenceContentLibrary::GetShieldMirror(const AActor* Actor)
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	const UGSTN_DefenceAttributeSet* Set = UGSTN_DefenceLibrary::FindDefenceAttributeSet(ASC);
	return Set ? Set->GetShieldHealth() : -1.f;
}

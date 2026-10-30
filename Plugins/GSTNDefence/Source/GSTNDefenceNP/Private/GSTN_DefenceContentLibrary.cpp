// Defence, Network Prediction half.

#include "GSTN_DefenceContentLibrary.h"
#include "Hits/GSTN_HitSteps.h"
#include "Pipeline/GSTN_HitStep_Parry.h"
#include "Pipeline/GSTN_HitStep_Shield.h"
#include "Rules/GSTN_CombatRules.h"
#include "Events/GSTN_SimEventSubsystem.h"
#include "GSTN_DefenceTags.h"
#include "Engine/World.h"

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
#include "Abilities/GSTN_DefenceAbilities.h"

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

bool UGSTN_DefenceContentLibrary::SetKitDefenceMoves(UGSTN_PrimaryKitDefinition* Kit, bool bLanding, bool bTechSet)
{
	if (!Kit)
	{
		return false;
	}
	TArray<FInstancedStruct>& Data = Kit->Extensions.ExtensionData;
	int32 Existing = Data.IndexOfByPredicate([](const FInstancedStruct& Entry) { return Entry.GetPtr<FGSTN_DefenceKitExtension>() != nullptr; });
	if (Existing == INDEX_NONE)
	{
		Existing = Data.Add(FInstancedStruct::Make(FGSTN_DefenceKitExtension()));
	}
	FGSTN_DefenceKitExtension Extension = *Data[Existing].GetPtr<FGSTN_DefenceKitExtension>();
	Extension.GetupSet = FGSTN_GetupSet();
	if (bLanding)
	{
		Extension.GetupSet.GroundBounce = UGSTN_GA_GroundBounce::StaticClass();
		Extension.GetupSet.Roll = UGSTN_GA_GetupRoll::StaticClass();
		Extension.GetupSet.Neutral = UGSTN_GA_GetupNeutral::StaticClass();
	}
	Extension.Tech = FGSTN_TechSet();
	if (bTechSet)
	{
		for (const EGSTN_TechType Type : { EGSTN_TechType::InPlace, EGSTN_TechType::RollLeft, EGSTN_TechType::RollRight, EGSTN_TechType::WallTech, EGSTN_TechType::WallJumpTech })
		{
			Extension.Tech.Abilities.Add(Type, UGSTN_GA_Tech::StaticClass());
		}
	}
	Data[Existing] = FInstancedStruct::Make(Extension);
	Kit->MarkPackageDirty();
	return true;
}

float UGSTN_DefenceContentLibrary::GetSDIDistanceUsed(const AActor* Actor)
{
	const UGSTN_CombatSimComponent* Combat = Actor ? Actor->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	const FGSTN_CombatSyncState* Sync = Combat ? Combat->GetSyncState() : nullptr;
	return Sync ? static_cast<float>(Sync->Core.Hit.SDIDistanceUsed / FGSTN_HitSyncState::SDIUnitsPerCm) : -1.f;
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
	return FString::Printf(TEXT("value=%d.%02d@%d flags=%d press=%d release=%d stun=%d recovery=%d break=%d parry=[%d,%d)p%d last=%d punish=%d push=%d techlock=%d techpress=%d techend=%d techtype=%d down=%d hitflags=%d"),
		F.ShieldValueQ / 4, (F.ShieldValueQ % 4) * 25, F.ShieldStampFrame, static_cast<int32>(F.ShieldFlags), F.ShieldPressFrame, F.ShieldReleaseFrame,
		F.ShieldStunEndFrame, F.ParryRecoveryEndFrame, F.BreakEndFrame, F.MoveParryStartFrame, F.MoveParryEndFrame, static_cast<int32>(F.MoveParryProfile),
		F.LastParryFrame, F.PunishEndFrame, F.PushbackStartFrame, F.TechLockoutEndFrame, F.LastTechPressFrame, F.TechEndFrame, static_cast<int32>(F.TechType),
		F.KnockdownEndFrame, static_cast<int32>(F.HitFlags));
}

float UGSTN_DefenceContentLibrary::GetShieldMirror(const AActor* Actor)
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	const UGSTN_DefenceAttributeSet* Set = UGSTN_DefenceLibrary::FindDefenceAttributeSet(ASC);
	return Set ? Set->GetShieldHealth() : -1.f;
}

namespace
{
	bool IsDefenceStepOf(const TSoftClassPtr<UGSTN_HitResolutionStep>& Step, const UClass* Class)
	{
		return Step.ToSoftObjectPath() == FSoftObjectPath(Class);
	}
}

bool UGSTN_DefenceContentLibrary::AddDefenceStepsToRules(UGSTN_CombatRules* Rules)
{
	if (!Rules)
	{
		return false;
	}
	TArray<TSoftClassPtr<UGSTN_HitResolutionStep>>& Pipeline = Rules->HitPipeline;
	const bool bHasParry = Pipeline.ContainsByPredicate([](const TSoftClassPtr<UGSTN_HitResolutionStep>& Step) { return IsDefenceStepOf(Step, UGSTN_HitStep_Parry::StaticClass()); });
	const bool bHasShield = Pipeline.ContainsByPredicate([](const TSoftClassPtr<UGSTN_HitResolutionStep>& Step) { return IsDefenceStepOf(Step, UGSTN_HitStep_Shield::StaticClass()); });
	if (bHasParry && bHasShield)
	{
		return true;
	}
	RemoveDefenceStepsFromRules(Rules);
	int32 At = 0;
	for (int32 Index = 0; Index < Pipeline.Num(); ++Index)
	{
		if (IsDefenceStepOf(Pipeline[Index], UGSTN_HitStep_HurtboxReaction::StaticClass()))
		{
			At = Index + 1;
			break;
		}
		if (IsDefenceStepOf(Pipeline[Index], UGSTN_HitStep_HurtState::StaticClass()))
		{
			At = Index + 1;
		}
	}
	Pipeline.Insert(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_Shield::StaticClass()), At);
	Pipeline.Insert(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_Parry::StaticClass()), At);
	Rules->ResolveHitPipeline();
	Rules->MarkPackageDirty();
	return true;
}

bool UGSTN_DefenceContentLibrary::RemoveDefenceStepsFromRules(UGSTN_CombatRules* Rules)
{
	if (!Rules)
	{
		return false;
	}
	const int32 Removed = Rules->HitPipeline.RemoveAll([](const TSoftClassPtr<UGSTN_HitResolutionStep>& Step)
	{
		return IsDefenceStepOf(Step, UGSTN_HitStep_Parry::StaticClass()) || IsDefenceStepOf(Step, UGSTN_HitStep_Shield::StaticClass());
	});
	if (Removed > 0)
	{
		Rules->ResolveHitPipeline();
		Rules->MarkPackageDirty();
	}
	return Removed > 0;
}

namespace
{
	/** The defence event rows of the worlds that asked for a log: debug only, outside the simulation */
	struct FDefenceEventLog
	{
		TArray<FString> Rows;
		FGSTN_SimEventListenerHandle Handle;
	};

	TMap<TWeakObjectPtr<UWorld>, FDefenceEventLog>& GetDefenceLogs()
	{
		static TMap<TWeakObjectPtr<UWorld>, FDefenceEventLog> Logs;
		return Logs;
	}
}

void UGSTN_DefenceContentLibrary::StartDefenceEventLog(AActor* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(WorldContext);
	if (!World || !Events)
	{
		return;
	}
	StopDefenceEventLog(WorldContext);
	const TWeakObjectPtr<UWorld> Key(World);
	FDefenceEventLog& Log = GetDefenceLogs().Add(Key);
	FGameplayTagContainer Types;
	Types.AddTag(GSTNDefenceTags::Event_Parried);
	Types.AddTag(GSTNDefenceTags::Event_Blocked);
	Types.AddTag(GSTNDefenceTags::Event_ShieldBroken);
	Types.AddTag(GSTNDefenceTags::Event_Teched);
	Types.AddTag(GSTNDefenceTags::Event_KnockedDown);
	Log.Handle = Events->ListenNative(FGameplayTagQuery::MakeQuery_MatchAnyTags(Types), [Key](const FGSTN_SimEvent& Event)
	{
		if (FDefenceEventLog* Found = GetDefenceLogs().Find(Key))
		{
			Found->Rows.Add(FString::Printf(TEXT("%s:%d:%d:%s"), *Event.EventType.GetTagLeafName().ToString(), Event.Frame, FMath::RoundToInt(Event.Magnitude),
				Event.Target.IsValid() ? *Event.Target->GetName() : TEXT("none")));
		}
	});
}

FString UGSTN_DefenceContentLibrary::GetDefenceEventLog(AActor* WorldContext)
{
	const FDefenceEventLog* Log = WorldContext ? GetDefenceLogs().Find(WorldContext->GetWorld()) : nullptr;
	return Log ? FString::Join(Log->Rows, TEXT(";")) : FString();
}

void UGSTN_DefenceContentLibrary::StopDefenceEventLog(AActor* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (FDefenceEventLog* Log = World ? GetDefenceLogs().Find(World) : nullptr)
	{
		if (UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(WorldContext))
		{
			Events->StopListening(Log->Handle);
		}
		GetDefenceLogs().Remove(World);
	}
}

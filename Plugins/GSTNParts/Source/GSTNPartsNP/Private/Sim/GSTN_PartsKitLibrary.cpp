// Destructible parts, Network Prediction half.

#include "Sim/GSTN_PartsKitLibrary.h"

#include "Events/GSTN_PartEvents.h"
#include "Events/GSTN_SimEventSubsystem.h"
#include "GSTN_PartsTags.h"

#include "Data/GSTN_PartSetDefinition.h"
#include "Data/GSTN_PartsKitExtension.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Sim/GSTN_PartsAttach.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "State/GSTN_PartsFragment.h"

namespace
{
	/** The part event rows of the worlds that asked for a log: debug only, outside the simulation */
	struct FPartEventLog
	{
		TArray<FString> Rows;
		FGSTN_SimEventListenerHandle Handle;
	};

	TMap<TWeakObjectPtr<UWorld>, FPartEventLog>& GetLogs()
	{
		static TMap<TWeakObjectPtr<UWorld>, FPartEventLog> Logs;
		return Logs;
	}
}

void UGSTN_PartsKitLibrary::StartPartEventLog(AActor* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(WorldContext);
	if (!World || !Events)
	{
		return;
	}
	StopPartEventLog(WorldContext);
	const TWeakObjectPtr<UWorld> Key(World);
	FPartEventLog& Log = GetLogs().Add(Key);
	FGameplayTagContainer Types;
	Types.AddTag(GSTNPartsTags::Event_Part_Damaged);
	Types.AddTag(GSTNPartsTags::Event_Part_WeakPointHit);
	Types.AddTag(GSTNPartsTags::Event_Part_Ricochet);
	Types.AddTag(GSTNPartsTags::Event_Part_Broken);
	Types.AddTag(GSTNPartsTags::Event_Part_AllBroken);
	Types.AddTag(GSTNPartsTags::Event_Part_Restored);
	Log.Handle = Events->ListenNative(FGameplayTagQuery::MakeQuery_MatchAnyTags(Types), [Key](const FGSTN_SimEvent& Event)
	{
		if (FPartEventLog* Found = GetLogs().Find(Key))
		{
			const FGSTN_PartEventPayload Payload = FGSTN_PartEvents::GetPayload(Event);
			Found->Rows.Add(FString::Printf(TEXT("%s:%d:%d:%d:%s"), *Event.EventType.GetTagLeafName().ToString(), Event.Frame, Payload.PartIndex, Payload.BreakSerial,
				Event.Target.IsValid() ? *Event.Target->GetName() : TEXT("none")));
		}
	});
}

FString UGSTN_PartsKitLibrary::GetPartEventLog(AActor* WorldContext)
{
	const FPartEventLog* Log = WorldContext ? GetLogs().Find(WorldContext->GetWorld()) : nullptr;
	return Log ? FString::Join(Log->Rows, TEXT(";")) : FString();
}

void UGSTN_PartsKitLibrary::StopPartEventLog(AActor* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (FPartEventLog* Log = World ? GetLogs().Find(World) : nullptr)
	{
		if (UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(WorldContext))
		{
			Events->StopListening(Log->Handle);
		}
		GetLogs().Remove(World);
	}
}

bool UGSTN_PartsKitLibrary::SetKitPartSet(UGSTN_PrimaryKitDefinition* Kit, UGSTN_PartSetDefinition* Set)
{
	if (!Kit)
	{
		return false;
	}
	TArray<FInstancedStruct>& Data = Kit->Extensions.ExtensionData;
	const int32 Existing = Data.IndexOfByPredicate([](const FInstancedStruct& Entry) { return Entry.GetPtr<FGSTN_PartsKitExtension>() != nullptr; });
	if (!Set)
	{
		if (Existing != INDEX_NONE)
		{
			Data.RemoveAt(Existing);
			Kit->MarkPackageDirty();
		}
		return true;
	}
	FGSTN_PartsKitExtension Extension;
	Extension.PartSet = Set;
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

bool UGSTN_PartsKitLibrary::AttachPartSetToActor(AActor* Actor, UGSTN_PartSetDefinition* Set)
{
	if (!Actor)
	{
		return false;
	}
	return FGSTN_PartsAttach::ApplyPartSet(Actor, Set, UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor));
}

FString UGSTN_PartsKitLibrary::GetPartsStateString(const AActor* Actor)
{
	const UGSTN_CombatSimComponent* Combat = Actor ? Actor->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	const FGSTN_CombatSyncState* Sync = Combat ? Combat->GetSyncState() : nullptr;
	if (!Sync)
	{
		return TEXT("no combat");
	}
	const FGSTN_PartsFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Sync->Fragments);
	if (!Fragment)
	{
		return TEXT("none");
	}
	FString Damage;
	for (int32 Index = 0; Index < GSTN_PartsLimits::MaxParts; ++Index)
	{
		if (Index >= Fragment->PartCount && Fragment->PartDamage[Index] == 0)
		{
			break;
		}
		Damage += FString::Printf(TEXT("%s%d"), Index ? TEXT(",") : TEXT(""), Fragment->PartDamage[Index]);
	}
	return FString::Printf(TEXT("mask=0x%04x serial=%d dmg=[%s]"), Fragment->BrokenMask, Fragment->BreakSerial, *Damage);
}

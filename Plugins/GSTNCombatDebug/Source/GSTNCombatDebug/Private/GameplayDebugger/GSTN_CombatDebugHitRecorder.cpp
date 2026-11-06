#include "GameplayDebugger/GSTN_CombatDebugHitRecorder.h"

#include "Engine/World.h"
#include "GSTN_CombatTags.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarCombatDebugHitRing(TEXT("gstn.Combat.Debug.HitRing"), 1,
		TEXT("Record the last 8 hits dealt and received per pawn for the combat Gameplay Debugger category (authority only). 0 = off, 1 = on."), ECVF_Default);
}

UGSTN_CombatDebugHitRecorder* UGSTN_CombatDebugHitRecorder::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UGSTN_CombatDebugHitRecorder>() : nullptr;
}

bool UGSTN_CombatDebugHitRecorder::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_CombatDebugHitRecorder::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(&InWorld);
	if (!Events)
	{
		return;
	}
	FGameplayTagContainer Types;
	Types.AddTag(GSTNCombatTags::Event_HitLanded);
	Types.AddTag(GSTNCombatTags::Event_HitReceived);
	TWeakObjectPtr<UGSTN_CombatDebugHitRecorder> WeakThis(this);
	Listener = Events->ListenNative(FGameplayTagQuery::MakeQuery_MatchAnyTags(Types), [WeakThis](const FGSTN_SimEvent& Event)
	{
		if (UGSTN_CombatDebugHitRecorder* Self = WeakThis.Get(); Self && CVarCombatDebugHitRing.GetValueOnGameThread() != 0)
		{
			Self->Record(Event);
		}
	});
}

void UGSTN_CombatDebugHitRecorder::Deinitialize()
{
	if (Listener.IsValid())
	{
		if (UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(this))
		{
			Events->StopListening(Listener);
		}
		Listener = FGSTN_SimEventListenerHandle();
	}
	Rings.Reset();
	Super::Deinitialize();
}

void UGSTN_CombatDebugHitRecorder::Record(const FGSTN_SimEvent& Event)
{
	// HitLanded goes to the attacker (and to a credited partner, whose copy names it as instigator), HitReceived to the victim
	const bool bDealt = Event.EventType == GSTNCombatTags::Event_HitLanded;
	if (!bDealt && Event.EventType != GSTNCombatTags::Event_HitReceived)
	{
		return;
	}
	const AActor* Owner = bDealt ? Event.Instigator.Get() : Event.Target.Get();
	if (!Owner)
	{
		return;
	}
	Rings.FindOrAdd(Owner).Add(FGSTN_CombatHitRecord::FromEvent(Event, bDealt));
}

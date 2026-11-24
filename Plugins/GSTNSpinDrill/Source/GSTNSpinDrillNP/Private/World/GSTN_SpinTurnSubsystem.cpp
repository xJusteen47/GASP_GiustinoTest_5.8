// Turns the world's IGSTN_SpinTurnable actors from the authoritative spin and drill events.

#include "World/GSTN_SpinTurnSubsystem.h"

#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Events/GSTN_SimEvent.h"
#include "GameFramework/Actor.h"
#include "GSTN_SpinDrillTags.h"

UGSTN_SpinTurnSubsystem* UGSTN_SpinTurnSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UGSTN_SpinTurnSubsystem>() : nullptr;
}

bool UGSTN_SpinTurnSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_SpinTurnSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Only the authority's outbox fills this stream, so a client's subsystem never hears anything: no authority test needed here
	if (UGSTN_SimEventSubsystem* Events = Collection.InitializeDependency<UGSTN_SimEventSubsystem>())
	{
		FGameplayTagContainer Types;
		Types.AddTag(GSTNSpinDrillTags::Event_Spin_Started);
		Types.AddTag(GSTNSpinDrillTags::Event_Drill_GroundLag);
		TWeakObjectPtr<UGSTN_SpinTurnSubsystem> WeakThis(this);
		EventListener = Events->ListenNative(FGameplayTagQuery::MakeQuery_MatchAnyTags(Types), [WeakThis](const FGSTN_SimEvent& Event)
		{
			if (UGSTN_SpinTurnSubsystem* This = WeakThis.Get())
			{
				This->HandleSimEvent(Event);
			}
		});
	}
}

void UGSTN_SpinTurnSubsystem::Deinitialize()
{
	if (EventListener.IsValid())
	{
		if (UGSTN_SimEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UGSTN_SimEventSubsystem>() : nullptr)
		{
			Events->StopListening(EventListener);
		}
		EventListener = FGSTN_SimEventListenerHandle();
	}
	Super::Deinitialize();
}

void UGSTN_SpinTurnSubsystem::HandleSimEvent(const FGSTN_SimEvent& Event)
{
	// The query matches the type, but the dispatcher may hand any event to a listener with an empty query: check again
	const bool bSpin = Event.EventType.MatchesTagExact(GSTNSpinDrillTags::Event_Spin_Started);
	const bool bDrill = Event.EventType.MatchesTagExact(GSTNSpinDrillTags::Event_Drill_GroundLag);
	if (!bSpin && !bDrill)
	{
		return;
	}
	const int32 Sense = Event.Tags.HasTagExact(GSTNSpinDrillTags::State_Spinning_CounterClockwise) ? -1 : 1;
	TurnInReach(Event.Instigator.Get(), Sense, bSpin ? EGSTN_SpinTurnSource::Spin : EGSTN_SpinTurnSource::Drill);
}

int32 UGSTN_SpinTurnSubsystem::TurnInReach(AActor* Turner, int32 Sense, EGSTN_SpinTurnSource Source)
{
	UWorld* World = GetWorld();
	if (!World || !Turner || !Turner->HasAuthority())
	{
		return 0;
	}
	// The spin reaches around the body's centre; the drill only what is under the feet (the dive ended on it)
	FVector Centre = Turner->GetActorLocation();
	float Reach = SpinReachCm;
	if (Source == EGSTN_SpinTurnSource::Drill)
	{
		float Radius = 0.f;
		float HalfHeight = 0.f;
		Turner->GetSimpleCollisionCylinder(Radius, HalfHeight);
		Centre -= Turner->GetActorUpVector() * HalfHeight;
		Reach = DrillReachCm;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTN_SpinTurn), false, Turner);
	World->OverlapMultiByObjectType(Overlaps, Centre, FQuat::Identity, FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
		FCollisionShape::MakeSphere(Reach), Params);

	TSet<AActor*> Seen;
	int32 Turned = 0;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!Actor || Seen.Contains(Actor) || !Actor->GetClass()->ImplementsInterface(UGSTN_SpinTurnable::StaticClass()))
		{
			continue;
		}
		Seen.Add(Actor);
		if (!IGSTN_SpinTurnable::Execute_CanBeSpinTurned(Actor, Turner))
		{
			continue;
		}
		IGSTN_SpinTurnable::Execute_OnSpinTurned(Actor, Turner, Sense, Source);
		OnTurned.Broadcast(Actor, Turner, Sense, Source);
		++Turned;
	}
	return Turned;
}

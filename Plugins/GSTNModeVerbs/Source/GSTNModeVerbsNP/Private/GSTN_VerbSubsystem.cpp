// Mode verbs, Network Prediction half: the host's validation and the once-only republisher (game-modes 2.4, spike G5).

#include "GSTN_VerbSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GSTN_InteractSubsystem.h"
#include "GSTN_ModeVerbTags.h"
#include "GSTN_RequestableComponent.h"
#include "NetworkPredictionWorldManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNModeVerbs, Log, All);

UGSTN_VerbSubsystem* UGSTN_VerbSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UGSTN_VerbSubsystem>() : nullptr;
}

bool UGSTN_VerbSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UGSTN_VerbSubsystem::HasAuthority() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

void UGSTN_VerbSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UNetworkPredictionWorldManager* Manager = Collection.InitializeDependency<UNetworkPredictionWorldManager>())
	{
		PostFixedTickHandle = Manager->OnPostFixedTick.AddUObject(this, &UGSTN_VerbSubsystem::HandlePostFixedTick);
	}

	// Seam D-1: the host publishes a timeline key's outbox event once, so this listener never sees a client's prediction
	if (UGSTN_SimEventSubsystem* Events = Collection.InitializeDependency<UGSTN_SimEventSubsystem>())
	{
		FGameplayTagContainer Raw;
		Raw.AddTag(GSTNModeVerbTags::Message_Mode_VerbConfirmed);
		TWeakObjectPtr<UGSTN_VerbSubsystem> WeakThis(this);
		RawListener = Events->ListenNative(FGameplayTagQuery::MakeQuery_MatchAnyTags(Raw), [WeakThis](const FGSTN_SimEvent& Event)
		{
			if (UGSTN_VerbSubsystem* This = WeakThis.Get())
			{
				This->HandleRawConfirmed(Event);
			}
		});
	}
}

void UGSTN_VerbSubsystem::Deinitialize()
{
	UWorld* World = GetWorld();
	if (PostFixedTickHandle.IsValid())
	{
		if (UNetworkPredictionWorldManager* Manager = World ? World->GetSubsystem<UNetworkPredictionWorldManager>() : nullptr)
		{
			Manager->OnPostFixedTick.Remove(PostFixedTickHandle);
		}
		PostFixedTickHandle.Reset();
	}
	if (RawListener.IsValid())
	{
		if (UGSTN_SimEventSubsystem* Events = World ? World->GetSubsystem<UGSTN_SimEventSubsystem>() : nullptr)
		{
			Events->StopListening(RawListener);
		}
		RawListener = FGSTN_SimEventListenerHandle();
	}
	Queue.Reset();
	OnModeVerb.Clear();
	OnModeVerbRefused.Clear();
	Super::Deinitialize();
}

void UGSTN_VerbSubsystem::HandlePostFixedTick(int32 ServerFrame, int32 EndTimeMS, bool bResimulating)
{
	LastFrame = ServerFrame;
	// The authority publishes outside a re-simulation; everyone else throws the predictions away. Nothing else decides "once" here
	Queue.Flush(HasAuthority(), bResimulating, ServerFrame, [this](const FGSTN_VerbConfirmRequest& Request) { ProcessConfirmed(Request); });
}

void UGSTN_VerbSubsystem::HandleRawConfirmed(const FGSTN_SimEvent& Event)
{
	if (!HasAuthority())
	{
		return;
	}
	FGSTN_VerbConfirmRequest Request;
	Request.Payload = FGSTN_ModeVerbPayload::FromEvent(Event);
	if (!Request.Payload.VerbTag.IsValid())
	{
		UE_LOG(LogGSTNModeVerbs, Warning, TEXT("A %s event carries no Ability.Mode.* tag and was ignored"), *GSTNModeVerbTags::Message_Mode_VerbConfirmed.GetTag().ToString());
		return;
	}
	// A timeline key names no target: only verbs that need none come this way (terminals go through the ability)
	Request.Spec.VerbTag = Request.Payload.VerbTag;
	Request.Spec.TargetSource = EGSTN_VerbTargetSource::None;
	ProcessConfirmed(Request);
}

UGSTN_RequestableComponent* UGSTN_VerbSubsystem::ResolveRequestable(const FGSTN_VerbConfirmRequest& Request, const AActor* Instigator, EGSTN_VerbRefusal& OutRefusal) const
{
	OutRefusal = EGSTN_VerbRefusal::NoTarget;
	UGSTN_InteractSubsystem* Interact = UGSTN_InteractSubsystem::Get(GetWorld());
	if (!Interact || !Instigator)
	{
		return nullptr;
	}

	const FGSTN_InteractRules& Rules = Interact->GetDefaultRules();
	const int32 ObjectiveMeaning = Interact->GetMeanings().IndexOf(GSTNModeVerbTags::Interact_Target_Objective);
	const FVector From = Instigator->GetActorLocation();
	const int32 Frame = Request.Payload.ConfirmFrame;
	const bool bNamed = Request.TargetKey != 0;

	UGSTN_RequestableComponent* Best = nullptr;
	const FGSTN_InteractCandidate* BestRecord = nullptr;
	float BestDistance = TNumericLimits<float>::Max();

	auto Consider = [&](const FGSTN_InteractCandidate& Record)
	{
		if (ObjectiveMeaning != 0 && Record.MeaningIndex != ObjectiveMeaning)
		{
			return;
		}
		UGSTN_RequestableComponent* Prop = Cast<UGSTN_RequestableComponent>(Interact->ResolveComponent(Record.Id));
		// Without a named target the nearest prop that takes this verb wins; a nearer prop that does not is not "the target"
		if (!Prop || (!bNamed && !Prop->AcceptsVerb(Request.Payload.VerbTag)))
		{
			return;
		}
		const float Distance = FGSTN_RequestableGate::DistanceToSurfaceCm(From, Record.PositionCm, Record.RadiusCm);
		// Total order: distance, then the lower id (never gather order)
		if (!Best || Distance < BestDistance || (Distance == BestDistance && Record.Id < BestRecord->Id))
		{
			Best = Prop;
			BestRecord = &Record;
			BestDistance = Distance;
		}
	};

	if (bNamed)
	{
		if (const FGSTN_InteractCandidate* Record = Interact->GetRegistry().Find(FGSTN_InteractCandidateId::FromKey(Request.TargetKey)))
		{
			Consider(*Record);
		}
	}
	else
	{
		Interact->GetRegistry().QueryAround(From, Frame, Consider);
	}
	if (!Best || !BestRecord)
	{
		return nullptr;
	}

	float Reach = Rules.ReachCm;
	if (Request.Spec.MaxRange > 0.f)
	{
		Reach = Request.Spec.MaxRange;
	}
	else if (BestRecord->ReachOverrideCm > 0)
	{
		Reach = FMath::Min(static_cast<float>(BestRecord->ReachOverrideCm), Rules.HardMaxReachCm);
	}

	FGSTN_RequestableGate::FInput Gate;
	Gate.Frame = Frame;
	Gate.ActionableFrom = BestRecord->ActionableFrom;
	Gate.ToFrame = BestRecord->ToFrame;
	Gate.DistanceCm = BestDistance;
	Gate.ReachCm = Reach;
	Gate.SlackCm = Rules.ServerReachSlackCm;
	Gate.bVerbAccepted = Best->AcceptsVerb(Request.Payload.VerbTag);
	Gate.UsesLeft = Best->GetUsesLeft();
	OutRefusal = FGSTN_RequestableGate::Evaluate(Gate);

	// The owner's own last word (IGSTN_InteractTarget::CanInteractServer): a locked door, a terminal waiting for a key
	if (OutRefusal == EGSTN_VerbRefusal::None && !Best->CanInteractServer(const_cast<AActor*>(Instigator), EGSTN_InteractVerb::Hold))
	{
		OutRefusal = EGSTN_VerbRefusal::NotActionable;
	}
	return Best;
}

EGSTN_VerbRefusal UGSTN_VerbSubsystem::ProcessConfirmed(const FGSTN_VerbConfirmRequest& Request)
{
	if (!HasAuthority())
	{
		return EGSTN_VerbRefusal::NoTarget;
	}

	AActor* Instigator = Request.Payload.Instigator.Get();
	FGSTN_ModeVerbPayload Payload = Request.Payload;
	FGameplayTagContainer Identity;
	UGSTN_RequestableComponent* Prop = nullptr;
	EGSTN_VerbRefusal Refusal = EGSTN_VerbRefusal::None;

	switch (Request.Spec.TargetSource)
	{
	case EGSTN_VerbTargetSource::InteractFocus:
		Prop = ResolveRequestable(Request, Instigator, Refusal);
		if (Prop)
		{
			Payload.Target = Prop->GetOwner();
		}
		break;

	case EGSTN_VerbTargetSource::AimPoint:
		if (Request.bHasAim && Instigator)
		{
			// The aim is the command's (deterministic); the point is made here, outside the simulation, from where the pawn is
			const UGSTN_InteractSubsystem* Interact = UGSTN_InteractSubsystem::Get(GetWorld());
			const float Range = Request.Spec.MaxRange > 0.f ? Request.Spec.MaxRange : (Interact ? Interact->GetDefaultRules().ReachCm : 0.f);
			Payload.AimPointCm = FGSTN_ModeVerbPayload::QuantiseAimCm(Instigator->GetActorLocation() + Request.Aim.Vector() * Range);
		}
		break;

	case EGSTN_VerbTargetSource::None:
	default:
		break;
	}

	if (Refusal != EGSTN_VerbRefusal::None)
	{
		PublishRefusal(Payload, Refusal);
		return Refusal;
	}

	if (Prop)
	{
		Identity = Prop->IdentityTags;
		Prop->ConsumeUse();
		Prop->NotifyInteractedServer(Instigator, EGSTN_InteractVerb::Hold);
	}

	// The one event: missions and respawn count this and nothing else
	if (UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(GetWorld()))
	{
		Events->Broadcast(Payload.ToEvent(Identity));
	}
	OnModeVerb.Broadcast(Payload);
	return EGSTN_VerbRefusal::None;
}

void UGSTN_VerbSubsystem::PublishRefusal(const FGSTN_ModeVerbPayload& Payload, EGSTN_VerbRefusal Reason)
{
	UE_LOG(LogGSTNModeVerbs, Verbose, TEXT("Verb %s refused at frame %d (reason %d)"), *Payload.VerbTag.ToString(), Payload.ConfirmFrame, static_cast<int32>(Reason));

	FGSTN_SimEvent Event;
	Event.EventType = GSTNModeVerbTags::Message_Mode_VerbRefused;
	if (Payload.VerbTag.IsValid())
	{
		Event.Tags.AddTag(Payload.VerbTag);
	}
	Event.Frame = Payload.ConfirmFrame;
	Event.Instigator = Payload.Instigator;
	Event.Target = Payload.Target;
	Event.Magnitude = static_cast<float>(static_cast<uint8>(Reason));
	if (UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(GetWorld()))
	{
		Events->Broadcast(Event);
	}
	OnModeVerbRefused.Broadcast(Payload, Reason);
}

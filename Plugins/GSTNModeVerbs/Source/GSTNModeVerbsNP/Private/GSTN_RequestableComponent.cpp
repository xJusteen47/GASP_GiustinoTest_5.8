// Mode verbs, Network Prediction half: the requestable prop (game-modes 2.4, interaction.md 3.2).

#include "GSTN_RequestableComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GSTN_InteractSubsystem.h"
#include "GSTN_ModeVerbTags.h"
#include "GSTN_VerbSubsystem.h"
#include "Net/UnrealNetwork.h"

UGSTN_RequestableComponent::UGSTN_RequestableComponent()
{
	Meaning = GSTNModeVerbTags::Interact_Target_Objective;
	// A terminal behind a wall must not be usable through it (interaction.md 2.4). Static geometry only, so host and client agree
	LosPolicy = EGSTN_InteractLosPolicy::StaticGeometry;
}

void UGSTN_RequestableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGSTN_RequestableComponent, Arming);
	DOREPLIFETIME(UGSTN_RequestableComponent, UsesDone);
}

int32 UGSTN_RequestableComponent::NextSimFrame() const
{
	const UGSTN_VerbSubsystem* Verbs = UGSTN_VerbSubsystem::Get(GetWorld());
	return FMath::Max((Verbs ? Verbs->GetSimFrame() : INDEX_NONE) + 1, 0);
}

void UGSTN_RequestableComponent::WithSimFrame(TFunctionRef<void()> Call)
{
	UGSTN_InteractSubsystem* Interact = UGSTN_InteractSubsystem::Get(GetWorld());
	if (!Interact)
	{
		Call();
		return;
	}
	// The stamps must be in the frames the interact probe and the host's validation query with (the combat simulation's); the interact
	// subsystem's own clock (server time x frame rate) runs ahead of them
	Interact->SetFrameOverride(NextSimFrame());
	Call();
	Interact->ClearFrameOverride();
}

void UGSTN_RequestableComponent::BeginPlay()
{
	if (bStartArmed)
	{
		// The interactable component's own record: level-placed from frame 0 on every machine, a spawned prop through its stamp
		WithSimFrame([this]() { Super::BeginPlay(); });
	}
	else
	{
		// Not armed: no record until Arm(). The interactable component would register one here, so its BeginPlay is skipped on purpose
		UActorComponent::BeginPlay();
	}
	// A client may have received the arming before it began play
	ApplyArming();
}

void UGSTN_RequestableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The record Arm() made is ours to end; the interactable component ends its own
	if (ArmedId.IsValid() && ArmedId != GetCandidateId())
	{
		if (UGSTN_InteractSubsystem* Interact = UGSTN_InteractSubsystem::Get(GetWorld()))
		{
			const FGSTN_InteractCandidateId Id = ArmedId;
			WithSimFrame([Interact, Id]() { Interact->UnregisterRecord(Id); });
		}
	}
	ArmedId.Reset();
	WithSimFrame([this, EndPlayReason]() { Super::EndPlay(EndPlayReason); });
}

void UGSTN_RequestableComponent::Arm(int32 DelayFrames)
{
	UGSTN_InteractSubsystem* Interact = UGSTN_InteractSubsystem::Get(GetWorld());
	const AActor* Owner = GetOwner();
	if (!Interact || !Owner || !Owner->HasAuthority() || !HasBegunPlay() || IsArmed())
	{
		return;
	}
	const int32 Delay = DelayFrames >= 0 ? DelayFrames : Interact->GetDefaultRules().MinAgeFrames;

	FGSTN_RequestableArming Next;
	Next.Handle = Interact->AllocateSpawnedHandle();
	Next.ArmFrame = NextSimFrame() + Delay;
	Next.DisarmFrame = MAX_int32;
	Arming = Next;
	ApplyArming();
}

void UGSTN_RequestableComponent::Disarm()
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !HasBegunPlay() || !IsArmed())
	{
		return;
	}
	FGSTN_RequestableArming Next = Arming;
	Next.DisarmFrame = NextSimFrame();
	Arming = Next;
	ApplyArming();
}

bool UGSTN_RequestableComponent::IsArmed() const
{
	return GetActiveId().IsValid() && Arming.DisarmFrame == MAX_int32;
}

bool UGSTN_RequestableComponent::AcceptsVerb(const FGameplayTag& Verb) const
{
	return AcceptedVerbs.IsEmpty() || AcceptedVerbs.HasTag(Verb);
}

int32 UGSTN_RequestableComponent::GetUsesLeft() const
{
	return MaxUses < 0 ? -1 : FMath::Max(MaxUses - UsesDone, 0);
}

void UGSTN_RequestableComponent::ConsumeUse()
{
	if (MaxUses < 0)
	{
		return;
	}
	++UsesDone;
	if (GetUsesLeft() == 0)
	{
		Disarm();
	}
}

void UGSTN_RequestableComponent::OnRep_Arming()
{
	ApplyArming();
}

void UGSTN_RequestableComponent::ApplyArming()
{
	UGSTN_InteractSubsystem* Interact = UGSTN_InteractSubsystem::Get(GetWorld());
	if (!Interact || !HasBegunPlay())
	{
		return;
	}

	// A new handle is a new record (records are immutable: a re-arm is a removal and a new record, never an edit)
	if (Arming.Handle != 0 && (!ArmedId.IsValid() || ArmedId.Handle != Arming.Handle))
	{
		FGSTN_InteractRecordStamp Stamp;
		Stamp.Handle = Arming.Handle;
		Stamp.FromFrame = Arming.ArmFrame;
		Stamp.ActionableFrom = Arming.ArmFrame;
		const FGSTN_InteractCandidate Candidate = BuildCandidate(Stamp, *Interact, Interact->AddRequirement(InteractorRequires));
		if (Interact->RegisterRecord(Candidate, this))
		{
			ArmedId = Candidate.Id;
		}
	}

	// The end of the record in force, on every machine at the same frame. Remove never extends a shorter lifetime, so applying it twice is harmless
	if (Arming.DisarmFrame != MAX_int32 && GetActiveId().IsValid())
	{
		Interact->GetRegistry().Remove(GetActiveId(), Arming.DisarmFrame);
	}
}

// Interactable component (interaction.md §6 item 1, §3.2).

#include "GSTN_InteractableComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GSTN_InteractSubsystem.h"
#include "GSTN_InteractTarget.h"
#include "GSTN_InteractSettings.h"
#include "Net/UnrealNetwork.h"

UGSTN_InteractableComponent::UGSTN_InteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGSTN_InteractableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Only the handle and stamps of a spawned record; static records need nothing
	DOREPLIFETIME_CONDITION(UGSTN_InteractableComponent, RecordStamp, COND_InitialOnly);
}

void UGSTN_InteractableComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterRecord();
}

void UGSTN_InteractableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterRecord();
	Super::EndPlay(EndPlayReason);
}

void UGSTN_InteractableComponent::UnregisterRecord()
{
	if (CandidateId.IsValid())
	{
		if (UGSTN_InteractSubsystem* Subsystem = UGSTN_InteractSubsystem::Get(GetWorld()))
		{
			Subsystem->UnregisterRecord(CandidateId);
		}
		CandidateId.Reset();
	}
}

FGSTN_InteractCandidate UGSTN_InteractableComponent::BuildCandidate(const FGSTN_InteractRecordStamp& Stamp, const UGSTN_InteractSubsystem& Subsystem, int32 QueryIndex) const
{
	FGSTN_InteractCandidate Candidate;
	Candidate.Id = FGSTN_InteractCandidateId(GSTNInteractProviders::Registry, static_cast<uint32>(Stamp.Handle));
	Candidate.PositionCm = FGSTN_InteractCandidate::QuantiseCm(ResolveAnchor());
	float Radius = RadiusCm;
	if (bUseActorBounds && GetOwner())
	{
		FVector Origin, Extent;
		GetOwner()->GetActorBounds(true, Origin, Extent);
		Radius = static_cast<float>(Extent.Size());
	}
	Candidate.RadiusCm = FMath::Clamp(FMath::RoundToInt32(Radius), 0, 65535);
	Candidate.MeaningIndex = Subsystem.GetMeanings().IndexOf(Meaning);
	Candidate.Priority = Priority < 0 ? FGSTN_InteractCandidate::DefaultPriority : FMath::Clamp(Priority, 0, 255);
	Candidate.ReachOverrideCm = FMath::Clamp(FMath::RoundToInt32(ReachOverrideCm), 0, 65535);
	Candidate.LosPolicy = LosPolicy;
	Candidate.QueryIndex = QueryIndex;
	Candidate.FromFrame = Stamp.FromFrame;
	Candidate.ActionableFrom = Stamp.ActionableFrom;
	Candidate.Flags = bFocusWhenUnactionable ? FGSTN_InteractCandidate::FlagFocusWhenUnactionable : 0;
	return Candidate;
}

FVector UGSTN_InteractableComponent::ResolveAnchor() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}
	FVector Anchor = bUseActorBounds ? Owner->GetComponentsBoundingBox(true).GetCenter() : Owner->GetActorTransform().TransformPosition(AnchorOffset);
	if (Owner->Implements<UGSTN_InteractTarget>())
	{
		IGSTN_InteractTarget::Execute_GetAnchor(Owner, Anchor);
	}
	return Anchor;
}

void UGSTN_InteractableComponent::RegisterRecord()
{
	AActor* Owner = GetOwner();
	UGSTN_InteractSubsystem* Subsystem = UGSTN_InteractSubsystem::Get(GetWorld());
	if (!Owner || !Subsystem || CandidateId.IsValid())
	{
		return;
	}
	if (Subsystem->GetMeanings().IndexOf(Meaning) == 0)
	{
		UE_LOG(LogGSTNInteract, Warning, TEXT("%s: meaning %s is not in the Interact meaning table; it can be focused but projects no tag"), *GetNameSafe(Owner), *Meaning.ToString());
	}

	if (Owner->IsNetStartupActor())
	{
		// Level-placed: both machines derive the same handle and the record exists from frame 0, nothing replicates
		RecordStamp.Handle = UGSTN_InteractSubsystem::MakeStableHandle(Owner->GetPathName(GetWorld()));
		RecordStamp.FromFrame = 0;
		RecordStamp.ActionableFrom = 0;
		RegisterFromStamp();
	}
	else if (Owner->HasAuthority())
	{
		// Spawned: the server stamps MinAgeFrames ahead so a client has the record before it simulates FromFrame
		const int32 Frame = Subsystem->GetFrame();
		const int32 MinAge = Subsystem->GetDefaultRules().MinAgeFrames;
		RecordStamp.Handle = Subsystem->AllocateSpawnedHandle();
		RecordStamp.FromFrame = Frame + MinAge;
		RecordStamp.ActionableFrom = RecordStamp.FromFrame;
		RegisterFromStamp();
	}
	// A client of a spawned actor registers in OnRep_RecordStamp
}

void UGSTN_InteractableComponent::OnRep_RecordStamp()
{
	if (RecordStamp.IsSet() && !CandidateId.IsValid())
	{
		RegisterFromStamp();
	}
}

void UGSTN_InteractableComponent::RegisterFromStamp()
{
	UGSTN_InteractSubsystem* Subsystem = UGSTN_InteractSubsystem::Get(GetWorld());
	if (!Subsystem || !RecordStamp.IsSet())
	{
		return;
	}
	const FGSTN_InteractCandidate Candidate = BuildCandidate(RecordStamp, *Subsystem, Subsystem->AddRequirement(InteractorRequires));
	if (Subsystem->RegisterRecord(Candidate, this))
	{
		CandidateId = Candidate.Id;
	}
}

void UGSTN_InteractableComponent::UpdateRecord()
{
	UGSTN_InteractSubsystem* Subsystem = UGSTN_InteractSubsystem::Get(GetWorld());
	if (!Subsystem || !CandidateId.IsValid() || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	// Records are immutable: the old one ends now, a new one with a new handle starts MinAgeFrames from now
	Subsystem->UnregisterRecord(CandidateId);
	CandidateId.Reset();
	RecordStamp = FGSTN_InteractRecordStamp();
	const int32 Frame = Subsystem->GetFrame();
	RecordStamp.Handle = Subsystem->AllocateSpawnedHandle();
	RecordStamp.FromFrame = Frame + Subsystem->GetDefaultRules().MinAgeFrames;
	RecordStamp.ActionableFrom = RecordStamp.FromFrame;
	RegisterFromStamp();
}

FGSTN_InteractDisplayInfo UGSTN_InteractableComponent::ResolveDisplayInfo(const UGSTN_InteractSubsystem& Subsystem) const
{
	FGSTN_InteractDisplayInfo Info;
	Info.Name = DisplayName;
	Info.Icon = Icon;
	if (const FGSTN_InteractMeaning* Entry = Subsystem.GetMeanings().Get(Subsystem.GetMeanings().IndexOf(Meaning)))
	{
		Info.TapVerb = Entry->TapVerb;
		Info.HoldVerb = Entry->HoldVerb;
	}
	if (!TapVerbOverride.IsEmpty())
	{
		Info.TapVerb = TapVerbOverride;
	}
	if (!HoldVerbOverride.IsEmpty())
	{
		Info.HoldVerb = HoldVerbOverride;
	}
	if (const AActor* Owner = GetOwner(); Owner && Owner->Implements<UGSTN_InteractTarget>())
	{
		IGSTN_InteractTarget::Execute_GetDisplayInfo(Owner, Info);
	}
	return Info;
}

bool UGSTN_InteractableComponent::CanInteractServer(AActor* Interactor, EGSTN_InteractVerb Verb) const
{
	const AActor* Owner = GetOwner();
	return !(Owner && Owner->Implements<UGSTN_InteractTarget>()) || IGSTN_InteractTarget::Execute_CanInteractServer(Owner, Interactor, Verb);
}

void UGSTN_InteractableComponent::NotifyInteractedServer(AActor* Interactor, EGSTN_InteractVerb Verb)
{
	if (AActor* Owner = GetOwner(); Owner && Owner->Implements<UGSTN_InteractTarget>())
	{
		IGSTN_InteractTarget::Execute_OnInteractedServer(Owner, Interactor, Verb);
	}
	OnInteracted.Broadcast(Interactor, Verb);
}

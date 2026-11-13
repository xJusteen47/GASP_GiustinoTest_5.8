// Game thread interactor (interaction.md §3.4).

#include "GSTN_InteractorComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagAssetInterface.h"
#include "GSTN_InteractableComponent.h"
#include "GSTN_InteractPresenter.h"
#include "GSTN_InteractRulesAsset.h"
#include "GSTN_InteractSettings.h"
#include "GSTN_InteractSubsystem.h"
#include "Stats/Stats.h"

DECLARE_STATS_GROUP(TEXT("GSTNInteract"), STATGROUP_GSTNInteract, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Probe"), STAT_GSTNInteractProbe, STATGROUP_GSTNInteract);

UGSTN_InteractorComponent::UGSTN_InteractorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(false);
}

void UGSTN_InteractorComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UGSTN_InteractPresentationSubsystem* Presentation = UGSTN_InteractPresentationSubsystem::Get(GetWorld()))
	{
		Presentation->RegisterInteractor(this);
	}
}

void UGSTN_InteractorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGSTN_InteractPresentationSubsystem* Presentation = UGSTN_InteractPresentationSubsystem::Get(GetWorld()))
	{
		Presentation->UnregisterInteractor(this);
	}
	Super::EndPlay(EndPlayReason);
}

const FGSTN_InteractRules& UGSTN_InteractorComponent::GetRules() const
{
	if (RulesAsset)
	{
		return RulesAsset->Rules;
	}
	if (const UGSTN_InteractSubsystem* Subsystem = UGSTN_InteractSubsystem::Get(GetWorld()))
	{
		return Subsystem->GetDefaultRules();
	}
	static const FGSTN_InteractRules Defaults;
	return Defaults;
}

bool UGSTN_InteractorComponent::IsLocallyPresented() const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !GetWorld())
	{
		return false;
	}
	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		return Pawn->IsLocallyControlled();
	}
	// A non-pawn owner (a prop, a test rig) is local wherever something renders
	return GetWorld()->GetNetMode() != NM_DedicatedServer;
}

bool UGSTN_InteractorComponent::ShouldProbe() const
{
	return UGSTN_InteractSettings::Get().bEnabled && GSTNInteractCVars::IsEnabled() && IsLocallyPresented();
}

void UGSTN_InteractorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!ShouldProbe())
	{
		return;
	}
	const UGSTN_InteractSubsystem* Subsystem = UGSTN_InteractSubsystem::Get(GetWorld());
	if (!Subsystem)
	{
		return;
	}
	// ProbeEveryNFrames: probing on a stride halves the cost at one stride of staleness
	if (++ProbeCounter % FMath::Max(GetRules().ProbeEveryNFrames, 1) != 0)
	{
		return;
	}
	ProbeAtFrame(Subsystem->GetFrame());
}

void UGSTN_InteractorComponent::GatherPointing(FGSTN_InteractPointingInput& Out) const
{
	const AActor* Owner = GetOwner();
	Out.Origin = Owner->GetActorLocation();
	Out.Up = Owner->GetActorUpVector();
	Out.Facing = Owner->GetActorForwardVector();
	// Walking towards an item points at it: with no real stick the velocity stands in for the intent
	const FVector Velocity = Owner->GetVelocity();
	const FVector Flat = Velocity - Out.Up * (Velocity | Out.Up);
	if (Flat.SizeSquared() > FMath::Square(20.f))
	{
		Out.StickDirection = Flat.GetSafeNormal();
		Out.StickMagnitude = 1.f;
	}
	if (Owner->Implements<UGSTN_InteractPointingSource>())
	{
		IGSTN_InteractPointingSource::Execute_GetPointingInput(Owner, Out);
	}
}

void UGSTN_InteractorComponent::GatherTags(FGameplayTagContainer& Out) const
{
	if (const IGameplayTagAssetInterface* TagSource = Cast<IGameplayTagAssetInterface>(GetOwner()))
	{
		TagSource->GetOwnedGameplayTags(Out);
	}
}

FGSTN_InteractFocusResult UGSTN_InteractorComponent::ProbeAtFrame(int32 Frame)
{
	SCOPE_CYCLE_COUNTER(STAT_GSTNInteractProbe);
	const UGSTN_InteractSubsystem* Subsystem = UGSTN_InteractSubsystem::Get(GetWorld());
	if (!Subsystem || !GetOwner())
	{
		return Result;
	}

	FGSTN_InteractPointingInput Pointing;
	GatherPointing(Pointing);
	FGameplayTagContainer Tags;
	GatherTags(Tags);

	FGSTN_InteractQuery Query;
	Query.Frame = Frame;
	Query.Origin = Pointing.Origin;
	Query.Up = Pointing.Up;
	Query.Facing = Pointing.Facing;
	Query.StickDirection = Pointing.StickDirection;
	Query.StickMagnitude = Pointing.StickMagnitude;
	Query.AimDirection = Pointing.AimDirection;
	Query.bHasAim = Pointing.bHasAim;
	Query.bInteractHeld = bHeld;
	Query.bCycleRequested = bCycleRequested;
	Query.Tags = &Tags;
	Query.Rules = &GetRules();
	Query.Registry = &Subsystem->GetRegistry();
	Query.Meanings = &Subsystem->GetMeanings();
	Query.Requirements = Subsystem->GetRequirements();
	Query.Providers = Subsystem->GetProviders();
	Query.Visibility = Subsystem;
	Query.Filter = CandidateFilter;

	const FGSTN_InteractCandidateId Before = State.FocusId;
	Result = UGSTN_InteractMath::UpdateFocus(Query, State);
	bCycleRequested = false;
	LastHeldFrames = bHeld ? FMath::Max(0, Frame - HeldSinceFrame) : 0;

	if (Result.FocusId != Before)
	{
		OnFocusChanged.Broadcast(Result.FocusId);
	}
	return Result;
}

void UGSTN_InteractorComponent::SetInteractHeld(bool bNewHeld)
{
	if (bNewHeld && !bHeld)
	{
		const UGSTN_InteractSubsystem* Subsystem = UGSTN_InteractSubsystem::Get(GetWorld());
		HeldSinceFrame = Subsystem ? Subsystem->GetFrame() : 0;
		LastHeldFrames = 0;
	}
	if (!bNewHeld)
	{
		LastHeldFrames = 0;
	}
	bHeld = bNewHeld;
}

float UGSTN_InteractorComponent::GetHoldProgress() const
{
	if (!bHeld || !Result.HasFocus())
	{
		return 0.f;
	}
	const UGSTN_InteractSubsystem* Subsystem = UGSTN_InteractSubsystem::Get(GetWorld());
	const FGSTN_InteractMeaning* Meaning = Subsystem ? Subsystem->GetMeanings().Get(Result.MeaningIndex) : nullptr;
	if (!Meaning || Meaning->HoldFrames <= 0)
	{
		return 0.f;
	}
	return FMath::Clamp(static_cast<float>(LastHeldFrames) / Meaning->HoldFrames, 0.f, 1.f);
}

AActor* UGSTN_InteractorComponent::GetFocusActor() const
{
	const UGSTN_InteractSubsystem* Subsystem = UGSTN_InteractSubsystem::Get(GetWorld());
	const UGSTN_InteractableComponent* Component = Subsystem ? Subsystem->ResolveComponent(Result.FocusId) : nullptr;
	return Component ? Component->GetOwner() : nullptr;
}

bool UGSTN_InteractorComponent::Interact(EGSTN_InteractVerb Verb)
{
	if (!Result.HasFocus() || Result.IsGhost() || Result.IsUnactionable() || !GetOwner())
	{
		return false;
	}
	if (GetOwner()->HasAuthority())
	{
		// Standalone or the listen host: nothing to send, validate and act here
		if (!TryInteractServer(Result.FocusId, Verb))
		{
			OnInteractRefused.Broadcast(Result.FocusId, Verb);
			return false;
		}
		return true;
	}
	ServerInteract(Result.FocusId, Verb);
	return true;
}

bool UGSTN_InteractorComponent::ServerInteract_Validate(FGSTN_InteractCandidateId TargetId, EGSTN_InteractVerb Verb)
{
	return TargetId.IsValid();
}

void UGSTN_InteractorComponent::ServerInteract_Implementation(FGSTN_InteractCandidateId TargetId, EGSTN_InteractVerb Verb)
{
	if (!TryInteractServer(TargetId, Verb))
	{
		// A refusal changes nothing: the press ends once and the hold bar disappears
		ClientInteractRefused(TargetId, Verb);
	}
}

void UGSTN_InteractorComponent::ClientInteractRefused_Implementation(FGSTN_InteractCandidateId TargetId, EGSTN_InteractVerb Verb)
{
	OnInteractRefused.Broadcast(TargetId, Verb);
}

bool UGSTN_InteractorComponent::TryInteractServer(const FGSTN_InteractCandidateId& TargetId, EGSTN_InteractVerb Verb)
{
	UGSTN_InteractSubsystem* Subsystem = UGSTN_InteractSubsystem::Get(GetWorld());
	AActor* Owner = GetOwner();
	if (!Subsystem || !Owner || !Owner->HasAuthority())
	{
		return false;
	}
	const FGSTN_InteractCandidate* Record = Subsystem->GetRegistry().Find(TargetId);
	UGSTN_InteractableComponent* Component = Subsystem->ResolveComponent(TargetId);
	if (!Record || !Component)
	{
		return false;
	}

	FGSTN_InteractPointingInput Pointing;
	GatherPointing(Pointing);
	FGameplayTagContainer Tags;
	GatherTags(Tags);
	if (!UGSTN_InteractMath::IsStillInteractable(*Record, Pointing.Origin, &Tags, Subsystem->GetRequirements(), Subsystem->GetFrame(), GetRules()))
	{
		return false;
	}
	if (!Component->CanInteractServer(Owner, Verb))
	{
		return false;
	}
	Component->NotifyInteractedServer(Owner, Verb);
	return true;
}

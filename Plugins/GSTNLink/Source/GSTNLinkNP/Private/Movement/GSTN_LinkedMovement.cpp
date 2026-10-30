// Links, Network Prediction half: the parked mode, its modifier and its transition.

#include "Movement/GSTN_LinkedModeTransition.h"
#include "Movement/GSTN_LinkedModifier.h"
#include "Movement/GSTN_LinkedMovementMode.h"

#include "GameFramework/Actor.h"
#include "GSTN_LinkTags.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

// ---- FGSTN_LinkedModifier

FGSTN_LinkedModifier::FGSTN_LinkedModifier()
{
	// Endless: it ends by cancellation on the frame the parked role ends
	DurationMs = -1.f;
}

bool FGSTN_LinkedModifier::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	const FGameplayTag Linked = GSTNLinkTags::Mover_Linked;
	return bExactMatch ? TagToFind == Linked : Linked.MatchesTag(TagToFind);
}

void FGSTN_LinkedModifier::GetGameplayTags(FGameplayTagContainer& InOutTags) const
{
	InOutTags.AddTag(GSTNLinkTags::Mover_Linked);
}

FMovementModifierBase* FGSTN_LinkedModifier::Clone() const
{
	return new FGSTN_LinkedModifier(*this);
}

void FGSTN_LinkedModifier::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
}

UScriptStruct* FGSTN_LinkedModifier::GetScriptStruct() const
{
	return FGSTN_LinkedModifier::StaticStruct();
}

FString FGSTN_LinkedModifier::ToSimpleString() const
{
	return TEXT("GSTN Linked");
}

bool FGSTN_LinkedModifier::Queue(const AActor* Owner)
{
	UMoverComponent* Mover = Owner ? Owner->FindComponentByClass<UMoverComponent>() : nullptr;
	return Mover && Mover->QueueMovementModifier(MakeShared<FGSTN_LinkedModifier>()).IsValid();
}

void FGSTN_LinkedModifier::Cancel(const AActor* Owner)
{
	if (UMoverComponent* Mover = Owner ? Owner->FindComponentByClass<UMoverComponent>() : nullptr)
	{
		Mover->CancelFeaturesWithTag(GSTNLinkTags::Mover_Linked, true);
	}
}

// ---- UGSTN_LinkedModeTransition

UGSTN_LinkedModeTransition::UGSTN_LinkedModeTransition()
	: LinkedModeName(GetDefaultLinkedModeName())
	, ExitModeName(DefaultModeNames::Falling)
{
}

FName UGSTN_LinkedModeTransition::GetDefaultLinkedModeName()
{
	static const FName Name(TEXT("GSTN_Linked"));
	return Name;
}

FTransitionEvalResult UGSTN_LinkedModeTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	const UMoverComponent* Mover = GetMoverComponent();
	if (!Mover)
	{
		return FTransitionEvalResult::NoTransition;
	}
	const FMoverSyncState& Start = Params.StartState.SyncState;
	const bool bLinked = Mover->HasGameplayTagInState(Start, GSTNLinkTags::Mover_Linked, true);
	if (bExitTransition)
	{
		return !bLinked && Start.MovementMode == LinkedModeName ? FTransitionEvalResult(ExitModeName) : FTransitionEvalResult::NoTransition;
	}
	return bLinked && Start.MovementMode != LinkedModeName ? FTransitionEvalResult(LinkedModeName) : FTransitionEvalResult::NoTransition;
}

void UGSTN_LinkedModeTransition::EnsureMoverSetup(UMoverComponent* Mover)
{
	if (!Mover)
	{
		return;
	}
	const FName ModeName = GetDefaultLinkedModeName();
	if (!Mover->MovementModes.Contains(ModeName))
	{
		Mover->AddMovementModeFromClass(ModeName, UGSTN_LinkedMovementMode::StaticClass());
	}
	for (const TPair<FName, TObjectPtr<UBaseMovementMode>>& Pair : Mover->MovementModes)
	{
		UBaseMovementMode* Mode = Pair.Value;
		if (!Mode || Pair.Key == ModeName)
		{
			continue;
		}
		const bool bHasEntry = Mode->Transitions.ContainsByPredicate([](const UBaseMovementModeTransition* Transition)
		{
			return Transition && Transition->IsA<UGSTN_LinkedModeTransition>();
		});
		if (!bHasEntry)
		{
			// The state machine reads the current mode's transitions at every evaluation: one appended now is live from the next tick
			UGSTN_LinkedModeTransition* Entry = NewObject<UGSTN_LinkedModeTransition>(Mode);
			Entry->LinkedModeName = ModeName;
			Mode->Transitions.Add(Entry);
			Entry->OnRegistered();
		}
	}
}

// ---- UGSTN_LinkedMovementMode

UGSTN_LinkedMovementMode::UGSTN_LinkedMovementMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UGSTN_LinkedModeTransition* Exit = CreateDefaultSubobject<UGSTN_LinkedModeTransition>(TEXT("ExitTransition"));
	Exit->bExitTransition = true;
	// The mode carries no Combat.Mover.Linked of its own: the exit reads the tag of the modifier alone (the mode's tags count in the state query)
	Transitions.Add(Exit);
}

void UGSTN_LinkedMovementMode::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep,
	FProposedMove& OutProposedMove) const
{
	// No input, no gravity: nothing moves
	OutProposedMove.LinearVelocity = FVector::ZeroVector;
	OutProposedMove.AngularVelocityDegrees = FVector::ZeroVector;
}

void UGSTN_LinkedMovementMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const FMoverDefaultSyncState* Start = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	FMoverDefaultSyncState& Out = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	if (Start)
	{
		// The parked transform: where the capsule was, with no velocity and no movement base
		Out.SetTransforms_WorldSpace(Start->GetLocation_WorldSpace(), Start->GetOrientation_WorldSpace(), FVector::ZeroVector, FVector::ZeroVector);
		Out.MoveDirectionIntent = FVector::ZeroVector;
	}
	OutputState.MovementEndState.RemainingMs = 0.f;
}

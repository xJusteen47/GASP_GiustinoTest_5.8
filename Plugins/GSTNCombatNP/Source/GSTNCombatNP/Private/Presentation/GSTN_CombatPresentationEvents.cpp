// Combat presentation (GAS-NPP fork).

#include "Presentation/GSTN_CombatPresentationEvents.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "GSTN_PresentationComponent.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "MoverComponent.h"
#include "Presentation/GSTN_CombatStateTypes.h"
#include "Presentation/GSTN_RunPresentationComponent.h"
#include "Simulation/GSTN_CombatSimComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_CombatPresentationEvents)

namespace GSTNCombatPresentationEvents
{
	/** The fixed simulation step (62.5 Hz, owner: untouchable), only to read the skid's slide speed */
	constexpr float StepSeconds = 0.016f;
	const FName KitSource(TEXT("Kit"));

	static EGSTN_PresentationSkidPhase ToPresentation(EGSTN_RunSkidPhase Phase)
	{
		switch (Phase)
		{
		case EGSTN_RunSkidPhase::Enter: return EGSTN_PresentationSkidPhase::Enter;
		case EGSTN_RunSkidPhase::Active: return EGSTN_PresentationSkidPhase::Loop;
		case EGSTN_RunSkidPhase::Exit: return EGSTN_PresentationSkidPhase::Exit;
		default: return EGSTN_PresentationSkidPhase::None;
		}
	}
}

UGSTN_CombatPresentationEvents::UGSTN_CombatPresentationEvents()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(false);
}

UGSTN_CombatPresentationEvents* UGSTN_CombatPresentationEvents::FindOrAdd(AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}
	if (UGSTN_CombatPresentationEvents* Existing = Owner->FindComponentByClass<UGSTN_CombatPresentationEvents>())
	{
		return Existing;
	}
	UGSTN_CombatPresentationEvents* Added = NewObject<UGSTN_CombatPresentationEvents>(Owner, TEXT("GSTN_CombatPresentationEvents"));
	Added->RegisterComponent();
	return Added;
}

bool UGSTN_CombatPresentationEvents::ReadSnapshot(const AActor* Owner, FGSTN_PresentationSnapshot& Out, float RunMinSpeed)
{
	using namespace GSTNCombatPresentationEvents;
	Out = FGSTN_PresentationSnapshot();
	const UGSTN_CombatSimComponent* Combat = Owner ? Owner->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	if (!Combat)
	{
		return false;
	}
	const FGSTN_CombatStateView View = Combat->GetCombatStateView();
	if (!View.bValid)
	{
		return false;
	}
	Out.bValid = true;
	Out.Frame = View.Frame;
	Out.Location = Owner->GetActorLocation();
	Out.bGrounded = !View.bAirborne;
	Out.AirJumpsUsed = View.AirJumpsUsed;
	Out.bReacting = View.Status == EGSTN_CombatStatus::Hitstun || View.Status == EGSTN_CombatStatus::Tumble;
	Out.bSplat = View.Status == EGSTN_CombatStatus::Splat;
	Out.MoveTags = Combat->GetCurrentMoveTags();
	Out.MoveStartFrame = View.bInMove ? View.MoveStartFrame : INDEX_NONE;

	if (const UMoverComponent* Mover = Owner->FindComponentByClass<UMoverComponent>())
	{
		Out.Velocity = Mover->GetVelocity();
		Out.Up = Mover->GetUpDirection();
		Out.SkidPhase = ToPresentation(UGSTN_RunPresentationComponent::ReadFootSlide(Mover->GetSyncState(), StepSeconds).SkidPhase);
	}
	// The snapshot counts stages from 1 with 0 = not running; the simulation's stage is 0-based and stays 0 at rest. Running = stages on
	// and faster than RunMinSpeed (in the air too: a jump keeps the sprint lines) (polish M2: with the raw index RunStart fired only when sprint 1 began, ~40 frames late)
	const FVector Horizontal = Out.Velocity - Out.Up * FVector::DotProduct(Out.Velocity, Out.Up);
	Out.RunStage = View.bHasRunStages && Horizontal.Size() >= RunMinSpeed ? View.RunStage + 1 : 0;

	FGSTN_CombatHitEventData Hit;
	if (Combat->GetLastHitReceived(Hit))
	{
		Out.LastHitFrame = Hit.Frame;
		Out.LastHitStrength = static_cast<float>(Hit.HitstunFrames);
		Out.LastHitDirection = Hit.LaunchVelocity.GetSafeNormal();
	}

	if (const UAbilitySystemComponent* Asc = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
	{
		Out.StateTags = Asc->GetOwnedGameplayTags();
	}
	return true;
}

void UGSTN_CombatPresentationEvents::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* Owner = GetOwner();
	if (!bEnabled || !Owner)
	{
		return;
	}
	if (!Presentation)
	{
		Presentation = UGSTN_PresentationComponent::FindOrAdd(Owner);
	}

	// The kit's table, when the loadout changes kit (static kit data)
	const UGSTN_LoadoutComponent* Loadout = Owner->FindComponentByClass<UGSTN_LoadoutComponent>();
	const UGSTN_PrimaryKitDefinition* Kit = Loadout && Loadout->HasLoadout() ? Loadout->GetResolvedLoadout().Kit : nullptr;
	if (AppliedKit.Get() != Kit)
	{
		AppliedKit = Kit;
		const FGSTN_PresentationKitExtension* Extension = Kit ? Kit->Extensions.Find<FGSTN_PresentationKitExtension>() : nullptr;
		Presentation->SetTableLayer(GSTNCombatPresentationEvents::KitSource, Extension ? Extension->Table.Get() : nullptr, 0);
	}

	FGSTN_PresentationSnapshot Snapshot;
	if (ReadSnapshot(Owner, Snapshot, RunMinSpeed))
	{
		Presentation->SubmitSnapshot(Snapshot);
	}
}

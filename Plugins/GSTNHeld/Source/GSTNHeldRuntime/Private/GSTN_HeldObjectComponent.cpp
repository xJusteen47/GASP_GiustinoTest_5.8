// Held objects, world side.

#include "GSTN_HeldObjectComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GSTN_HeldSettings.h"
#include "GSTN_HeldSubsystem.h"
#include "GSTN_HeldTags.h"
#include "GSTN_InteractRegistry.h"
#include "GSTN_InteractSubsystem.h"
#include "GSTN_ProjectileActor.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimMontage.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if WITH_EDITOR
EDataValidationResult UGSTN_HeldObjectDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto Fail = [&Context, &Result](const FString& Why)
	{
		Context.AddError(FText::FromString(Why));
		Result = EDataValidationResult::Invalid;
	};
	if (Tuning.PickupRadius <= 0.f)
	{
		Fail(TEXT("PickupRadius is 0: nobody can pick this object up."));
	}
	if (Tuning.Throw.Speed <= 0.f)
	{
		Fail(TEXT("Throw.Speed is 0: the throw would never leave the hand."));
	}
	if (Tuning.SizeTag.IsValid() && !UGSTN_HeldSettings::Get().FindSizeClass(Tuning.SizeTag))
	{
		Context.AddWarning(FText::FromString(FString::Printf(TEXT("SizeTag %s is not one of the project's size classes (GSTN Held settings): no load, no bans."),
			*Tuning.SizeTag.ToString())));
	}
	if (Tuning.bOverrideWindup && !Tuning.Windup.IsInstant())
	{
		const UAnimMontage* Montage = Tuning.Windup.Montage.LoadSynchronous();
		// The release is counted in fixed frames at the project's 62.5 Hz; the montage only shows it
		const float ReleaseSeconds = Tuning.Windup.ReleaseFrames / 62.5f * Tuning.Windup.MontagePlayRate;
		if (!Montage)
		{
			Context.AddWarning(FText::FromString(TEXT("The wind-up has frames but no montage: the thrower holds still until the object leaves.")));
		}
		else if (ReleaseSeconds > Montage->GetPlayLength())
		{
			Context.AddWarning(FText::FromString(FString::Printf(TEXT("The wind-up releases at %.2f s into its montage, which ends at %.2f s."),
				ReleaseSeconds, Montage->GetPlayLength())));
		}
	}
	return Result;
}
#endif

UGSTN_HeldObjectComponent::UGSTN_HeldObjectComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the holder moved this frame, so a held object does not trail by one frame
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(true);
}

const FGSTN_HeldObjectTuning& UGSTN_HeldObjectComponent::GetTuning() const
{
	static const FGSTN_HeldObjectTuning TestTuning;
	return Definition ? Definition->Tuning : TestTuning;
}

const FGSTN_HeldBreakRules& UGSTN_HeldObjectComponent::GetBreakRules() const
{
	const FGSTN_HeldObjectTuning& Tuning = GetTuning();
	return Tuning.bOverrideBreakRules ? Tuning.BreakRules : UGSTN_HeldSettings::Get().DefaultBreakRules;
}

TSubclassOf<UGameplayAbility> UGSTN_HeldObjectComponent::FindCommandAction(const FGameplayTag& Command) const
{
	const TSubclassOf<UGameplayAbility>* Found = Definition ? Definition->CommandActions.Find(Command) : nullptr;
	return Found ? *Found : nullptr;
}

void UGSTN_HeldObjectComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGSTN_HeldObjectComponent, State);
	DOREPLIFETIME(UGSTN_HeldObjectComponent, Holder);
	DOREPLIFETIME(UGSTN_HeldObjectComponent, Flight);
	DOREPLIFETIME(UGSTN_HeldObjectComponent, RestLocation);
	DOREPLIFETIME(UGSTN_HeldObjectComponent, RecordStamp);
}

void UGSTN_HeldObjectComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		// Placement is computed on every machine; the engine's movement replication would fight it
		Owner->SetReplicatingMovement(false);
	}
	if (UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(GetWorld()))
	{
		Held->RegisterObject(*this);
	}
	if (AActor* Owner = GetOwner(); Owner && Owner->HasAuthority())
	{
		RestLocation = Owner->GetActorLocation();
		if (State == EGSTN_HeldObjectState::Idle)
		{
			OpenRecord();
		}
	}
	else
	{
		ApplyRecord();
	}
}

void UGSTN_HeldObjectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(GetWorld()))
	{
		Held->UnregisterObject(*this);
	}
	// The object is gone: its records go at once (a destroyed actor has no later stamp to send)
	if (UGSTN_InteractSubsystem* Interact = UGSTN_InteractSubsystem::Get(GetWorld()))
	{
		for (const int32 Handle : { RecordStamp.Handle, RecordStamp.PreviousHandle })
		{
			if (Handle != 0)
			{
				Interact->GetRegistry().RemoveImmediate(FGSTN_InteractCandidateId(GSTNInteractProviders::Registry, static_cast<uint32>(Handle)));
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UGSTN_HeldObjectComponent::SetHeld(AActor* InHolder)
{
	CloseRecord();
	State = EGSTN_HeldObjectState::Held;
	Holder = InHolder;
	++HoldSerial;
	Flight = nullptr;
	FlightEndFrame = INDEX_NONE;
	GetOwner()->ForceNetUpdate();
	UpdatePlacement();
}

void UGSTN_HeldObjectComponent::SetFlying(AGSTN_Projectile& InFlight)
{
	CloseRecord();
	State = EGSTN_HeldObjectState::Flying;
	Holder = nullptr;
	Flight = &InFlight;
	FlightSegment = InFlight.GetSegment();
	FlightEndFrame = FlightSegment.StartFrame + InFlight.GetLifetimeFrames();
	GetOwner()->ForceNetUpdate();
	UpdatePlacement();
}

void UGSTN_HeldObjectComponent::SetBroken(int32 InRespawnFrame)
{
	CloseRecord();
	State = EGSTN_HeldObjectState::Broken;
	Holder = nullptr;
	Flight = nullptr;
	FlightEndFrame = INDEX_NONE;
	RespawnFrame = InRespawnFrame;
	GetOwner()->ForceNetUpdate();
	UpdatePlacement();
}

void UGSTN_HeldObjectComponent::SetIdleAt(const FVector& Location)
{
	if (!bHasHome)
	{
		HomeLocation = Location;
		bHasHome = true;
	}
	Bounces = 0;
	RespawnFrame = INDEX_NONE;
	State = EGSTN_HeldObjectState::Idle;
	Holder = nullptr;
	Flight = nullptr;
	FlightEndFrame = INDEX_NONE;
	RestLocation = Location;
	OpenRecord();
	GetOwner()->ForceNetUpdate();
	UpdatePlacement();
}

void UGSTN_HeldObjectComponent::OnRep_RestLocation()
{
	UpdatePlacement();
}

void UGSTN_HeldObjectComponent::OnRep_RecordStamp()
{
	ApplyRecord();
}

void UGSTN_HeldObjectComponent::OpenRecord()
{
	UGSTN_InteractSubsystem* Interact = UGSTN_InteractSubsystem::Get(GetWorld());
	const UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(GetWorld());
	if (!Interact || !Held)
	{
		return;
	}
	CloseRecord();
	const int32 LastFrame = Held->GetLastFrame();
	RecordStamp.PreviousHandle = RecordStamp.Handle;
	RecordStamp.PreviousToFrame = RecordStamp.ToFrame;
	RecordStamp.Handle = Interact->AllocateSpawnedHandle();
	RecordStamp.PositionCm = FGSTN_InteractCandidate::QuantiseCm(FVector(RestLocation));
	RecordStamp.FromFrame = LastFrame == INDEX_NONE ? 0 : LastFrame + 1 + FMath::Max(0, Interact->GetDefaultRules().MinAgeFrames);
	RecordStamp.ToFrame = MAX_int32;
	ApplyRecord();
}

void UGSTN_HeldObjectComponent::CloseRecord()
{
	const UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(GetWorld());
	if (RecordStamp.Handle == 0 || RecordStamp.ToFrame != MAX_int32 || !Held)
	{
		return;
	}
	RecordStamp.ToFrame = FMath::Max(RecordStamp.FromFrame, Held->GetLastFrame() + 1);
	ApplyRecord();
}

void UGSTN_HeldObjectComponent::ApplyRecord()
{
	UGSTN_InteractSubsystem* Interact = UGSTN_InteractSubsystem::Get(GetWorld());
	if (!Interact)
	{
		return;
	}
	FGSTN_InteractRegistry& Registry = Interact->GetRegistry();
	auto Close = [&Registry](int32 Handle, int32 ToFrame)
	{
		const FGSTN_InteractCandidateId Id(GSTNInteractProviders::Registry, static_cast<uint32>(Handle));
		const FGSTN_InteractCandidate* Existing = Handle != 0 && ToFrame != MAX_int32 ? Registry.Find(Id) : nullptr;
		if (Existing && Existing->ToFrame == MAX_int32)
		{
			Registry.Remove(Id, ToFrame);
		}
	};
	Close(RecordStamp.PreviousHandle, RecordStamp.PreviousToFrame);
	if (RecordStamp.Handle == 0)
	{
		return;
	}
	const FGSTN_InteractCandidateId Id(GSTNInteractProviders::Registry, static_cast<uint32>(RecordStamp.Handle));
	if (Registry.Find(Id))
	{
		Close(RecordStamp.Handle, RecordStamp.ToFrame);
	}
	else
	{
		// Unknown here: added with every stamp it has (a closed record arrives as its tombstone), so frame F sees the same on every machine
		const int32 Meaning = Interact->GetMeanings().IndexOf(GSTNHeldTags::Interact_Target_Pickup);
		if (Meaning == 0)
		{
			UE_LOG(LogGSTNHeld, Warning, TEXT("HeldRecord %s: no Interact.Target.Pickup meaning in the GSTNInteract settings, no pickup record"), *GetNameSafe(GetOwner()));
			return;
		}
		FGSTN_InteractCandidate Record;
		Record.Id = Id;
		Record.PositionCm = RecordStamp.PositionCm;
		Record.MeaningIndex = Meaning;
		Record.ReachOverrideCm = FMath::Clamp(FMath::RoundToInt32(GetTuning().PickupRadius), 0, 65535);
		Record.FromFrame = RecordStamp.FromFrame;
		Record.ActionableFrom = RecordStamp.FromFrame;
		Record.ToFrame = RecordStamp.ToFrame;
		Registry.Add(Record);
	}
	if (const UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(GetWorld()); Held && Held->GetLastFrame() != INDEX_NONE)
	{
		Registry.Purge(Held->GetLastFrame(), Interact->GetDefaultRules().TombstoneFrames);
	}
}

void UGSTN_HeldObjectComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (State != EGSTN_HeldObjectState::Idle)
	{
		UpdatePlacement();
	}
	UpdatePrompt();
}

void UGSTN_HeldObjectComponent::UpdatePrompt()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || Owner->IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	bool bShown = false;
	const APlayerController* Player = World->GetFirstPlayerController();
	const APawn* Pawn = Player && Player->IsLocalController() ? Player->GetPawn() : nullptr;
	const UAbilitySystemComponent* Abilities = Pawn ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn) : nullptr;
	if (State == EGSTN_HeldObjectState::Idle && Abilities && Abilities->HasMatchingGameplayTag(GSTNHeldTags::Interact_Target_Pickup))
	{
		const UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(World);
		bShown = Held && Held->FindPickupTarget(Pawn->GetActorLocation()) == this;
	}
	if (bShown && !Prompt)
	{
		Prompt = NewObject<UTextRenderComponent>(Owner, TEXT("HeldPrompt"), RF_Transient);
		Prompt->SetupAttachment(Owner->GetRootComponent());
		Prompt->SetText(FText::FromString(TEXT("B")));
		Prompt->SetHorizontalAlignment(EHTA_Center);
		Prompt->SetWorldSize(40.f);
		Prompt->SetTextRenderColor(FColor::White);
		Prompt->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
		Prompt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Prompt->RegisterComponent();
	}
	if (Prompt)
	{
		Prompt->SetVisibility(bShown);
		if (bShown)
		{
			// Faces the camera of the local player
			if (const APlayerCameraManager* Camera = Player->PlayerCameraManager)
			{
				const FVector ToCamera = Camera->GetCameraLocation() - Prompt->GetComponentLocation();
				Prompt->SetWorldRotation(FRotator(0.f, ToCamera.Rotation().Yaw, 0.f));
			}
		}
	}
}

void UGSTN_HeldObjectComponent::UpdatePlacement()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	// M10 G7: a broken object is hidden and lets everything through until it respawns (every machine, from the replicated state)
	const bool bBroken = State == EGSTN_HeldObjectState::Broken;
	if (Owner->IsHidden() != bBroken)
	{
		Owner->SetActorHiddenInGame(bBroken);
		Owner->SetActorEnableCollision(!bBroken);
	}
	// M10 G8: on the throwing client the object leaves the hand with the throw's ghost (then the host's projectile that took it over) until the
	// replicated state catches up; an unmatched ghost puts it back where the host says it is (still in hand: the throw did not happen)
	if (bPredicting)
	{
		const AGSTN_Projectile* Predicted = PredictedFlight.Get();
		const bool bCaughtUp = State == EGSTN_HeldObjectState::Idle || State == EGSTN_HeldObjectState::Broken
			|| (State == EGSTN_HeldObjectState::Flying && IsValid(Flight));
		if (!IsValid(Predicted) || Predicted->IsActorBeingDestroyed() || bCaughtUp)
		{
			if (!bPredictionHanded && !bCaughtUp)
			{
				if (UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(GetWorld()))
				{
					Held->NoteGhostExpired(*this);
				}
			}
			bPredicting = false;
			PredictedFlight.Reset();
		}
		else
		{
			Owner->SetActorLocation(Predicted->GetActorLocation());
			return;
		}
	}
	switch (State)
	{
	case EGSTN_HeldObjectState::Broken:
		break;
	case EGSTN_HeldObjectState::Held:
		if (IsValid(Holder))
		{
			const FTransform Hold = FGSTN_HeldRules::HoldTransform(Holder->GetActorTransform(), GetTuning().HoldOffset);
			Owner->SetActorLocationAndRotation(Hold.GetLocation(), Hold.GetRotation());
		}
		break;
	case EGSTN_HeldObjectState::Flying:
		// The projectile places itself at its frame; until a client has it, the object stays where it was (at the thrower)
		if (IsValid(Flight))
		{
			Owner->SetActorLocation(Flight->GetActorLocation());
		}
		break;
	default:
		Owner->SetActorLocation(RestLocation);
		break;
	}
}

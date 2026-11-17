// Presentation events and their effects (no GAS, no Network Prediction, no Mover).

#include "GSTN_PresentationComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_PresentationComponent)

UGSTN_PresentationComponent::UGSTN_PresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;  // only while a short additive plays
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(false);
	DeriveRules.TagRules = FGSTN_PresentationDeriveRules::MakeDefaultTagRules();
}

UGSTN_PresentationComponent* UGSTN_PresentationComponent::FindOrAdd(AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}
	if (UGSTN_PresentationComponent* Existing = Owner->FindComponentByClass<UGSTN_PresentationComponent>())
	{
		return Existing;
	}
	UGSTN_PresentationComponent* Added = NewObject<UGSTN_PresentationComponent>(Owner, TEXT("GSTN_Presentation"));
	Added->RegisterComponent();
	return Added;
}

void UGSTN_PresentationComponent::BindEventByTag(FGameplayTag Tag, FGSTN_OnPresentationEventSingle Delegate)
{
	if (Tag.IsValid() && Delegate.IsBound())
	{
		Bound.Add({Tag, Delegate});
	}
}

void UGSTN_PresentationComponent::UnbindEventByTag(FGameplayTag Tag, FGSTN_OnPresentationEventSingle Delegate)
{
	Bound.RemoveAll([&](const FBound& Entry) { return Entry.Tag == Tag && Entry.Delegate == Delegate; });
}

void UGSTN_PresentationComponent::SubmitSnapshot(const FGSTN_PresentationSnapshot& Snapshot)
{
	Scratch.Reset();
	Deriver.Step(Snapshot, DeriveRules, Scratch);
	for (const FGSTN_PresentationEvent& Event : Scratch)
	{
		ReportEvent(Event);
	}
}

void UGSTN_PresentationComponent::SetTableLayer(FName Source, UGSTN_PresentationTable* Table, int32 Priority)
{
	Layers.RemoveAll([Source](const FGSTN_PresentationTableLayer& Layer) { return Layer.Source == Source; });
	if (Table)
	{
		FGSTN_PresentationTableLayer Layer;
		Layer.Source = Source;
		Layer.Table = Table;
		Layer.Priority = Priority;
		Layers.Add(Layer);
		Layers.StableSort([](const FGSTN_PresentationTableLayer& A, const FGSTN_PresentationTableLayer& B) { return A.Priority > B.Priority; });
	}
}

UGSTN_PresentationTable* UGSTN_PresentationComponent::GetTableLayer(FName Source) const
{
	const FGSTN_PresentationTableLayer* Layer = Layers.FindByPredicate([Source](const FGSTN_PresentationTableLayer& L) { return L.Source == Source; });
	return Layer ? Layer->Table.Get() : nullptr;
}

const FGSTN_PresentationCue* UGSTN_PresentationComponent::ResolveCue(const FGameplayTag& EventTag) const
{
	for (const FGSTN_PresentationTableLayer& Layer : Layers)
	{
		if (const FGSTN_PresentationCue* Cue = Layer.Table ? Layer.Table->FindCue(EventTag) : nullptr)
		{
			return Cue;
		}
	}
	return DefaultTable ? DefaultTable->FindCue(EventTag) : nullptr;
}

bool UGSTN_PresentationComponent::FindCue(FGameplayTag EventTag, FGSTN_PresentationCue& OutCue) const
{
	if (const FGSTN_PresentationCue* Cue = ResolveCue(EventTag))
	{
		OutCue = *Cue;
		return true;
	}
	return false;
}

float UGSTN_PresentationComponent::CueIntensity(const FGSTN_PresentationCue* Cue, const FGSTN_PresentationEvent& Event)
{
	if (!Cue)
	{
		return 1.f;
	}
	if (Event.Magnitude < Cue->MinMagnitude)
	{
		return -1.f;
	}
	return FMath::Max(Cue->Scale.Evaluate(Event.Magnitude), 0.f);
}

bool UGSTN_PresentationComponent::ReportEvent(const FGSTN_PresentationEvent& InEvent)
{
	Dedupe.FrameWindow = DedupeFrameWindow;
	if (!InEvent.EventTag.IsValid() || !Dedupe.Accept(InEvent.EventTag, InEvent.Frame, InEvent.bExactFrame))
	{
		return false;
	}
	FGSTN_PresentationEvent Event = InEvent;
	if (Event.Location.IsZero() && GetOwner())
	{
		Event.Location = GetOwner()->GetActorLocation();
	}

	const FGSTN_PresentationCue* Cue = ResolveCue(Event.EventTag);
	const float Intensity = CueIntensity(Cue, Event);
	if (Intensity < 0.f)
	{
		return false;  // below the row's threshold: not an event worth showing
	}
	if (Cue && bPlayEffects && Cue->HasEffect() && GetNetMode() != NM_DedicatedServer)
	{
		PlayCue(*Cue, Event, Intensity);
	}

	OnPresentationEvent.Broadcast(Event, Intensity);
	// A copy: a Blueprint may bind or unbind from inside its event
	const TArray<FBound> Listeners = Bound;
	for (const FBound& Listener : Listeners)
	{
		if (Event.EventTag.MatchesTag(Listener.Tag))
		{
			Listener.Delegate.ExecuteIfBound(Event, Intensity);
		}
	}
	return true;
}

void UGSTN_PresentationComponent::PlayCue(const FGSTN_PresentationCue& Cue, const FGSTN_PresentationEvent& Event, float Intensity)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}
	USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>();

	if (Cue.Niagara)
	{
		UNiagaraComponent* System = nullptr;
		if (!Cue.AttachSocket.IsNone() && Mesh)
		{
			System = UNiagaraFunctionLibrary::SpawnSystemAttached(Cue.Niagara, Mesh, Cue.AttachSocket, Cue.Offset, FRotator::ZeroRotator,
				EAttachLocation::KeepRelativeOffset, true);
		}
		else
		{
			const FRotator Rotation = Cue.bOrientToDirection && !Event.Direction.IsNearlyZero() ? Event.Direction.Rotation() : FRotator::ZeroRotator;
			System = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Cue.Niagara, Event.Location + Cue.Offset, Rotation,
				FVector(Cue.bScaleSize ? Intensity : 1.f), true);
		}
		if (System)
		{
			if (Cue.bScaleSize && !Cue.AttachSocket.IsNone())
			{
				System->SetRelativeScale3D(FVector(Intensity));
			}
			if (!Cue.IntensityParameter.IsNone())
			{
				System->SetVariableFloat(Cue.IntensityParameter, Intensity);
			}
		}
	}

	if (Cue.Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, Cue.Sound, Event.Location, Cue.bScaleVolume ? Intensity : 1.f);
	}

	if (Cue.CameraShake)
	{
		if (Cue.ShakeScope == EGSTN_PresentationShakeScope::World)
		{
			UGameplayStatics::PlayWorldCameraShake(World, Cue.CameraShake, Event.Location, Cue.ShakeInnerRadius, Cue.ShakeOuterRadius);
		}
		else
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* Controller = It->Get();
				if (Controller && Controller->IsLocalController() && Controller->PlayerCameraManager && Controller->GetViewTarget() == Owner)
				{
					Controller->PlayerCameraManager->StartCameraShake(Cue.CameraShake, Intensity);
				}
			}
		}
	}

	if (Cue.Additive)
	{
		FActiveAdditive Added;
		Added.Clip = Cue.Additive.Get();
		Added.Weight = FMath::Clamp(Cue.AdditiveWeight * (Cue.bScaleAdditive ? FMath::Min(Intensity, 1.f) : 1.f), 0.f, 1.f);
		Added.Direction = Event.Direction;
		Additives.Insert(Added, 0);
		Additives.SetNum(FMath::Min(Additives.Num(), FGSTN_AdditiveReactionRequest::MaxLayers));
		SetComponentTickEnabled(true);
	}
}

void UGSTN_PresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	for (FActiveAdditive& Additive : Additives)
	{
		Additive.Time += DeltaTime;
	}
	Additives.RemoveAll([](const FActiveAdditive& Additive)
	{
		const UAnimSequenceBase* Clip = Additive.Clip.Get();
		return !Clip || Additive.Time >= Clip->GetPlayLength();
	});
	if (Additives.IsEmpty())
	{
		SetComponentTickEnabled(false);
	}
}

void UGSTN_PresentationComponent::GetAdditiveReaction(FGSTN_AdditiveReactionRequest& Out) const
{
	for (int32 Index = 0; Index < Additives.Num() && Index < FGSTN_AdditiveReactionRequest::MaxLayers; ++Index)
	{
		const FActiveAdditive& Additive = Additives[Index];
		FGSTN_AdditiveReactionLayer& Layer = Out.Layers[Index];
		Layer.Additive = Additive.Clip.Get();
		Layer.Time = Additive.Time;
		Layer.Weight = Additive.Weight;
		Layer.PushDirection = Additive.Direction;
		Layer.PlaceholderAngleDeg = 0.f;
	}
}

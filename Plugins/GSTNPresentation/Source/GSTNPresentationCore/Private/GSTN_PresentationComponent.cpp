// Presentation events and their effects (no GAS, no Network Prediction, no Mover).

#include "GSTN_PresentationComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_PresentationComponent)

namespace GSTNPresentationCVars
{
	// Effects off with the events still firing: the "VFX off" half of a review capture (same frames, rule 10), or a player's choice later
	static TAutoConsoleVariable<int32> Effects(TEXT("GSTN.Presentation.Effects"), 1,
		TEXT("1 = presentation cues play their Niagara/sound/shake/additive; 0 = events still fire, no effect plays"));
	// One log line per accepted event (owner, tag, frame, magnitude, intensity, what played): the numbers of a review capture
	static TAutoConsoleVariable<int32> Log(TEXT("GSTN.Presentation.Log"), 0, TEXT("1 = log every accepted presentation event (LogGSTNPresentation)"));
}

DEFINE_LOG_CATEGORY_STATIC(LogGSTNPresentation, Log, All);

UGSTN_PresentationComponent::UGSTN_PresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;  // only while a short additive plays
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(false);
	DeriveRules.TagRules = FGSTN_PresentationDeriveRules::MakeDefaultTagRules();
	DeriveRules.QuietAirTags = FGSTN_PresentationDeriveRules::MakeDefaultQuietTags();
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
	if (DefaultTable)
	{
		return DefaultTable->FindCue(EventTag);
	}
	const UGSTN_PresentationTable* Project = ProjectDefaultTable.IsNull() ? nullptr : ProjectDefaultTable.LoadSynchronous();
	return Project ? Project->FindCue(EventTag) : nullptr;
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
	StopSustained(Event.EventTag);  // before this event's own cue: an event may end one effect and start the next
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
	if (Cue && bPlayEffects && GSTNPresentationCVars::Effects.GetValueOnGameThread() != 0 && Cue->HasEffect() && GetNetMode() != NM_DedicatedServer)
	{
		PlayCue(*Cue, Event, Intensity);
	}

	if (GSTNPresentationCVars::Log.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogGSTNPresentation, Log, TEXT("PRES %s %s frame=%d mag=%.1f intensity=%.2f niagara=%s netmode=%d"), *GetNameSafe(GetOwner()),
			*Event.EventTag.ToString(), Event.Frame, Event.Magnitude, Intensity, Cue && Cue->Niagara ? *Cue->Niagara->GetName() : TEXT("-"),
			static_cast<int32>(GetNetMode()));
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

bool UGSTN_PresentationComponent::IsBodyGrounded(const FGSTN_PresentationCue& Cue, FGSTN_PresentationEvent& Event) const
{
	const AActor* Owner = GetOwner();
	const USkeletalMeshComponent* Mesh = Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (!Mesh || Mesh->GetBoneIndex(Cue.GroundedBone) == INDEX_NONE)
	{
		return true;  // nothing to wait for
	}
	const FVector Bone = Mesh->GetBoneLocation(Cue.GroundedBone);
	const FVector Ground = Event.Location + Cue.Offset;
	if (Bone.Z - Ground.Z > Cue.GroundedHeight)
	{
		return false;
	}
	// Under the body as drawn, at the event's height (the floor)
	Event.Location.X = Bone.X - Cue.Offset.X;
	Event.Location.Y = Bone.Y - Cue.Offset.Y;
	return true;
}

void UGSTN_PresentationComponent::PlayCue(const FGSTN_PresentationCue& Cue, const FGSTN_PresentationEvent& InEvent, float Intensity)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}
	FGSTN_PresentationEvent Event = InEvent;
	if (!Cue.GroundedBone.IsNone() && !IsBodyGrounded(Cue, Event))
	{
		if (Cue.GroundedMaxWait > 0.f)
		{
			FPendingGrounded& Pending = PendingGrounded.AddDefaulted_GetRef();
			Pending.Cue = Cue;
			Pending.Event = InEvent;
			Pending.Intensity = Intensity;
			Pending.Deadline = World->GetTimeSeconds() + Cue.GroundedMaxWait;
			UpdateTickEnabled();
			return;
		}
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
			// One-shots come from the world pool (no allocation per hit on a toaster PC); a sustained effect is held by this component
			// until its stop event, so it is never pooled (a pooled component could be handed to another effect meanwhile)
			const bool bPooled = !Cue.IsSustained();
			System = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Cue.Niagara, Event.Location + Cue.Offset, Rotation,
				FVector(Cue.bScaleSize ? Intensity : 1.f), !bPooled, true, bPooled ? ENCPoolMethod::AutoRelease : ENCPoolMethod::None);
		}
		if (System)
		{
			if (Cue.bScaleSize && !Cue.AttachSocket.IsNone())
			{
				System->SetRelativeScale3D(FVector(Intensity));
			}
			if (Cue.bScaleSize && !Cue.SizeParameter.IsNone())
			{
				System->SetVariableVec2(Cue.SizeParameter, FVector2D(Intensity, Intensity));
			}
			if (!Cue.IntensityParameter.IsNone())
			{
				System->SetVariableFloat(Cue.IntensityParameter, Intensity);
			}
			if (!Cue.ColorParameter.IsNone())
			{
				System->SetVariableLinearColor(Cue.ColorParameter, Cue.Tint);
			}
			if (!Cue.AccentColorParameter.IsNone())
			{
				System->SetVariableLinearColor(Cue.AccentColorParameter, Cue.AccentTint);
			}
			if (!Cue.StrongParameter.IsNone())
			{
				System->SetVariableFloat(Cue.StrongParameter, Cue.StrongIntensity > 0.f && Intensity >= Cue.StrongIntensity ? 1.f : 0.f);
			}
			if (Cue.IsSustained())
			{
				FSustained& Entry = Sustained.AddDefaulted_GetRef();
				Entry.System = System;
				Entry.StopOn = Cue.StopOn;
				Entry.EndTime = Cue.MaxDuration > 0.f ? World->GetTimeSeconds() + Cue.MaxDuration : 0.0;
				UpdateTickEnabled();
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
		UpdateTickEnabled();
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
	if (const UWorld* World = GetWorld(); World && !Sustained.IsEmpty())
	{
		const double Now = World->GetTimeSeconds();
		for (FSustained& Entry : Sustained)
		{
			if (Entry.EndTime > 0.0 && Now >= Entry.EndTime && Entry.System.IsValid())
			{
				Entry.System->Deactivate();
				Entry.System.Reset();
			}
		}
		Sustained.RemoveAll([](const FSustained& Entry) { return !Entry.System.IsValid() || !Entry.System->IsActive(); });
	}
	if (const UWorld* World = GetWorld(); World && !PendingGrounded.IsEmpty())
	{
		const double Now = World->GetTimeSeconds();
		// Moved out first: PlayCue may queue again (it never does here, the bone check is skipped) and must not touch the array we walk
		TArray<FPendingGrounded> Waiting = MoveTemp(PendingGrounded);
		for (FPendingGrounded& Pending : Waiting)
		{
			const bool bDue = IsBodyGrounded(Pending.Cue, Pending.Event) || Now >= Pending.Deadline;
			if (!bDue)
			{
				PendingGrounded.Add(MoveTemp(Pending));
				continue;
			}
			Pending.Cue.GroundedBone = NAME_None;  // due: play as is
			PlayCue(Pending.Cue, Pending.Event, Pending.Intensity);
		}
	}
	UpdateTickEnabled();
}

void UGSTN_PresentationComponent::StopSustained(const FGameplayTag& EventTag)
{
	for (FSustained& Entry : Sustained)
	{
		if (Entry.System.IsValid() && EventTag.MatchesAny(Entry.StopOn))
		{
			Entry.System->Deactivate();  // not DestroyComponent: the live particles finish their life (a trail fades, never pops)
			Entry.System.Reset();
		}
	}
	Sustained.RemoveAll([](const FSustained& Entry) { return !Entry.System.IsValid(); });
}

void UGSTN_PresentationComponent::UpdateTickEnabled()
{
	SetComponentTickEnabled(!Additives.IsEmpty() || !Sustained.IsEmpty() || !PendingGrounded.IsEmpty());
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

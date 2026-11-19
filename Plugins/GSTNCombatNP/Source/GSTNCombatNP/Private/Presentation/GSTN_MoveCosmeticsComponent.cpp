// Combat presentation (CP4 of the Blueprint seams): plays the cosmetic windows of the moves a character runs.

#include "Presentation/GSTN_MoveCosmeticsComponent.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTableSet.h"
#include "Sound/SoundBase.h"

UGSTN_MoveCosmeticsComponent::UGSTN_MoveCosmeticsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// After the frame's simulation and its finalize, so the state read is the one the frame shows
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(false);
}

void UGSTN_MoveCosmeticsComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		// Nothing is drawn here
		SetComponentTickEnabled(false);
		return;
	}
	Combat = GetOwner() ? GetOwner()->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
}

void UGSTN_MoveCosmeticsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FGSTN_MoveCosmeticTracker::FDelta Delta;
	Tracker.EndAll(Delta);
	for (const FGSTN_MoveCosmeticTracker::FEnded& Ended : Delta.Ended)
	{
		End(Ended);
	}
	Playing.Reset();
	LoadedContent.Reset();
	Super::EndPlay(EndPlayReason);
}

FString UGSTN_MoveCosmeticsComponent::GetCosmeticsInfo() const
{
	return FString::Printf(TEXT("%d,%d,%d,%d"), NumBegan, NumEnded, NumInterrupted, Playing.Num());
}

USkeletalMeshComponent* UGSTN_MoveCosmeticsComponent::ResolveMesh()
{
	if (Mesh.IsValid())
	{
		return Mesh.Get();
	}
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}
	if (!MeshComponentName.IsNone())
	{
		TArray<USkeletalMeshComponent*, TInlineAllocator<4>> Meshes;
		Owner->GetComponents<USkeletalMeshComponent>(Meshes);
		for (USkeletalMeshComponent* Candidate : Meshes)
		{
			if (Candidate->GetFName() == MeshComponentName)
			{
				Mesh = Candidate;
				return Candidate;
			}
		}
	}
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		Mesh = Character->GetMesh();
	}
	if (!Mesh.IsValid())
	{
		Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	}
	return Mesh.Get();
}

const UGSTN_GA_BaseCombat* UGSTN_MoveCosmeticsComponent::FindMove(const UGSTN_CombatTableSet* Tables, int32 Entry) const
{
	if (!Tables || !Tables->TimelineRegistry.IsValidIndex(Entry) || !*Tables->TimelineRegistry[Entry])
	{
		return nullptr;
	}
	return GetDefault<UGSTN_GA_BaseCombat>(Tables->TimelineRegistry[Entry]);
}

void UGSTN_MoveCosmeticsComponent::PreloadTables(const UGSTN_CombatTableSet* Tables)
{
	if (!Tables || PreloadedTables.Get() == Tables)
	{
		return;
	}
	PreloadedTables = Tables;
	for (const TSubclassOf<UGSTN_GA_BaseCombat>& Class : Tables->TimelineRegistry)
	{
		const UGSTN_GA_BaseCombat* Move = *Class ? GetDefault<UGSTN_GA_BaseCombat>(Class) : nullptr;
		if (!Move)
		{
			continue;
		}
		for (const FGSTN_MoveCosmeticWindow& Window : Move->CosmeticWindows)
		{
			if (!Window.Effect.IsNull())
			{
				if (UNiagaraSystem* System = Window.Effect.LoadSynchronous())
				{
					LoadedContent.AddUnique(System);
				}
			}
			if (!Window.Sound.IsNull())
			{
				if (USoundBase* Sound = Window.Sound.LoadSynchronous())
				{
					LoadedContent.AddUnique(Sound);
				}
			}
		}
	}
}

void UGSTN_MoveCosmeticsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UGSTN_CombatSimComponent* CombatComponent = Combat.Get();
	const FGSTN_CombatSyncState* Sync = CombatComponent ? CombatComponent->GetSyncState() : nullptr;
	if (!bEnabled || !Sync)
	{
		if (!Playing.IsEmpty())
		{
			FGSTN_MoveCosmeticTracker::FDelta Delta;
			Tracker.EndAll(Delta);
			for (const FGSTN_MoveCosmeticTracker::FEnded& Ended : Delta.Ended)
			{
				End(Ended);
			}
		}
		return;
	}

	const FGSTN_CombatSyncCore& Core = Sync->Core;
	const int32 Frame = Core.Input.History.GetLatestFrame();
	const UGSTN_CombatTableSet* Tables = CombatComponent->FindTableSet(Core.TableRevision);
	PreloadTables(Tables);

	// The timeline frame the combat tick evaluated (UGSTN_CombatSimComponent::GetEvaluatedTimelineFrame, from the presented state): none outside a
	// timeline and while a ricochet recoil replaces the move's frames
	const bool bRunLive = Frame != INDEX_NONE && Core.ActionState == EGSTN_ActionState::InTimeline && Core.ActiveTimeline != INDEX_NONE
		&& Core.TimelineStartFrame != INDEX_NONE && Core.RecoilStartFrame == INDEX_NONE;
	const int32 Entry = bRunLive ? Core.ActiveTimeline : INDEX_NONE;
	const int32 TimelineFrame = bRunLive ? Frame - Core.TimelineStartFrame - Core.TimelinePausedFrames : INDEX_NONE;
	const UGSTN_GA_BaseCombat* Move = bRunLive ? FindMove(Tables, Entry) : nullptr;
	const TConstArrayView<FGSTN_MoveCosmeticWindow> Windows = Move ? TConstArrayView<FGSTN_MoveCosmeticWindow>(Move->CosmeticWindows)
		: TConstArrayView<FGSTN_MoveCosmeticWindow>();

	FGSTN_MoveCosmeticTracker::FDelta Delta;
	Tracker.Update(Move ? Entry : INDEX_NONE, Core.TimelineStartFrame, TimelineFrame, Windows, GetWorld()->GetTimeSeconds(), MemorySeconds, Delta);
	for (const FGSTN_MoveCosmeticKey& Key : Delta.Began)
	{
		Begin(Key, Move);
	}
	for (const FGSTN_MoveCosmeticTracker::FEnded& Ended : Delta.Ended)
	{
		End(Ended);
	}

	const bool bHitstop = Frame != INDEX_NONE && Core.IsInHitstop(Frame);
	if (bHitstop != bWasInHitstop || bHitstop)
	{
		SetHitstopPause(bHitstop);
		bWasInHitstop = bHitstop;
	}
}

void UGSTN_MoveCosmeticsComponent::Begin(const FGSTN_MoveCosmeticKey& Key, const UGSTN_GA_BaseCombat* Move)
{
	if (!Move || !Move->CosmeticWindows.IsValidIndex(Key.Window))
	{
		return;
	}
	++NumBegan;
	FPlaying& Entry = Playing.AddDefaulted_GetRef();
	Entry.Key = Key;
	Entry.Window = Move->CosmeticWindows[Key.Window];
	Entry.Event.CosmeticTag = Entry.Window.CosmeticTag;
	Entry.Event.Move = Move->GetClass();
	Entry.Event.WindowIndex = Key.Window;
	Entry.Event.MoveStartFrame = Key.RunStartFrame;

	USkeletalMeshComponent* MeshComponent = ResolveMesh();
	const FGSTN_MoveCosmeticWindow& Window = Entry.Window;
	if (bPlayWindowEffects && MeshComponent)
	{
		UNiagaraSystem* System = Window.Effect.IsNull() ? nullptr : Window.Effect.LoadSynchronous();
		if (System)
		{
			// Pooled: released (never destroyed) when the window ends, so nothing here may touch it after End
			UNiagaraComponent* Effect = nullptr;
			if (Window.bAttach)
			{
				Effect = UNiagaraFunctionLibrary::SpawnSystemAttached(System, MeshComponent, Window.Socket, Window.LocationOffset, Window.RotationOffset,
					Window.Scale, EAttachLocation::KeepRelativeOffset, false, ENCPoolMethod::ManualRelease, true, true);
			}
			else
			{
				const FTransform Socket = MeshComponent->GetSocketTransform(Window.Socket);
				const FTransform Spawn = FTransform(Window.RotationOffset, Window.LocationOffset) * Socket;
				Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(MeshComponent, System, Spawn.GetLocation(), Spawn.Rotator(), Window.Scale,
					false, true, ENCPoolMethod::ManualRelease, true);
			}
			Entry.Effect = Effect;
			Entry.Event.SpawnedEffect = Effect;
		}
		USoundBase* Sound = Window.Sound.IsNull() ? nullptr : Window.Sound.LoadSynchronous();
		if (Sound)
		{
			Entry.Audio = Window.bAttach
				? UGameplayStatics::SpawnSoundAttached(Sound, MeshComponent, Window.Socket, Window.LocationOffset, EAttachLocation::KeepRelativeOffset, true)
				: UGameplayStatics::SpawnSoundAtLocation(MeshComponent, Sound, MeshComponent->GetSocketLocation(Window.Socket) + Window.LocationOffset);
		}
	}

	const FGSTN_MoveCosmeticEvent Event = Entry.Event;
	K2_OnMoveCosmeticBegin(Event);
	OnMoveCosmeticBegin.Broadcast(Event);
}

void UGSTN_MoveCosmeticsComponent::End(const FGSTN_MoveCosmeticTracker::FEnded& Ended)
{
	const int32 Index = Playing.IndexOfByPredicate([&Ended](const FPlaying& Entry) { return Entry.Key == Ended.Key; });
	if (Index == INDEX_NONE)
	{
		return;
	}
	FPlaying Entry = Playing[Index];
	Playing.RemoveAt(Index);
	++NumEnded;
	NumInterrupted += Ended.bInterrupted ? 1 : 0;

	const bool bKill = Ended.bInterrupted && Entry.Window.bKillWhenInterrupted;
	if (UNiagaraComponent* Effect = Entry.Effect.Get())
	{
		Effect->SetPaused(false);
		if (bKill)
		{
			Effect->DeactivateImmediate();
		}
		else
		{
			// The particles finish on their own; the pool takes the component back when they are done
			Effect->Deactivate();
		}
		Effect->ReleaseToPool();
	}
	if (UAudioComponent* Audio = Entry.Audio.Get())
	{
		if (bKill)
		{
			Audio->Stop();
		}
	}

	FGSTN_MoveCosmeticEvent Event = Entry.Event;
	// Released to the pool above: a handler must not get it
	Event.SpawnedEffect = nullptr;
	K2_OnMoveCosmeticEnd(Event, Ended.bInterrupted);
	OnMoveCosmeticEnd.Broadcast(Event, Ended.bInterrupted);
}

void UGSTN_MoveCosmeticsComponent::SetHitstopPause(bool bHitstop)
{
	for (FPlaying& Entry : Playing)
	{
		const bool bPause = bHitstop && Entry.Window.bFreezeInHitstop;
		UNiagaraComponent* Effect = Entry.Effect.Get();
		if (Effect && Entry.bPaused != bPause)
		{
			Effect->SetPaused(bPause);
			Entry.bPaused = bPause;
		}
	}
}

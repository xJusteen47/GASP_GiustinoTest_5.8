// Presentation events and their effects (no GAS, no Network Prediction, no Mover).

#include "GSTN_PresentationComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraTypes.h"
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
	// Captures and tests: "r,g,b" (linear) replaces DefaultSurfaceColor on every character. A cvar, not a property write: a property set
	// from Python in Play reruns the Blueprint construction script and rebuilds the pawn's NP proxies (VFX v3 r2 capture crash)
	static TAutoConsoleVariable<FString> SurfaceColor(TEXT("GSTN.Presentation.SurfaceColor"), TEXT(""),
		TEXT("Debug: 'r,g,b' linear floor colour for dust and snow when the floor has no surface colour; empty = DefaultSurfaceColor"));
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
	ContactShadowMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/GSTNPresentation/VFX/Materials/MI_GSTN_VFX_ContactShadow.MI_GSTN_VFX_ContactShadow")));
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
	UpdateContactShadow(Snapshot);
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
		WarmTable(Table);
		Layers.StableSort([](const FGSTN_PresentationTableLayer& A, const FGSTN_PresentationTableLayer& B) { return A.Priority > B.Priority; });
	}
}

void UGSTN_PresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	WarmTable(DefaultTable);
	if (!ProjectDefaultTable.IsNull())
	{
		WarmTable(ProjectDefaultTable.LoadSynchronous());  // here, not on the first event that falls back on it
	}
	for (const FGSTN_PresentationTableLayer& Layer : Layers)
	{
		WarmTable(Layer.Table);
	}
}

void UGSTN_PresentationComponent::WarmTable(const UGSTN_PresentationTable* Table)
{
#if WITH_EDITORONLY_DATA
	if (!Table)
	{
		return;
	}
	for (const FGSTN_PresentationCue& Cue : Table->Cues)
	{
		for (UNiagaraSystem* System : {Cue.Niagara.Get(), Cue.MatterNiagara.Get()})
		{
			if (System)
			{
				System->WaitForCompilationComplete(/*bIncludingGPUShaders*/ false, /*bShowProgress*/ false);  // no-op once compiled; one wait per system per editor session
			}
		}
	}
#endif
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

	// Floor contact (bSnapToFloor): the event of a body hitting the floor sits at the body (a bounce: the torso), the effect belongs on the floor
	if (Cue.bSnapToFloor)
	{
		const FGSTN_PresentationSnapshot& State = Deriver.GetPrevious();
		const FVector Up = State.bValid && !State.Up.IsNearlyZero() ? State.Up.GetSafeNormal() : FVector::UpVector;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTN_PresentationFloorSnap), false, Owner);
		if (World->LineTraceSingleByObjectType(Hit, Event.Location + Up * 20.0, Event.Location - Up * 400.0,
			FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			Event.Location = Hit.ImpactPoint;
		}
	}

	// Two parts (owner decision 2A, 2026-11-19): energy (flash, star, streaks) keeps full speed during a hitstop, matter (dust, debris,
	// smoke) slows to HitstopVfxTimeScale. The main system is energy only when HitstopTime says so; MatterNiagara is always matter
	for (int32 Part = 0; Part < 2; ++Part)
	{
		UNiagaraSystem* Asset = Part == 0 ? Cue.Niagara.Get() : Cue.MatterNiagara.Get();
		if (!Asset)
		{
			continue;
		}
		const bool bEnergy = Part == 0 && Cue.HitstopTime == EGSTN_PresentationHitstopTime::Energy;
		UNiagaraComponent* System = nullptr;
		if (!Cue.AttachSocket.IsNone() && Mesh)
		{
			System = UNiagaraFunctionLibrary::SpawnSystemAttached(Asset, Mesh, Cue.AttachSocket, Cue.Offset, FRotator::ZeroRotator,
				EAttachLocation::KeepRelativeOffset, true);
		}
		else
		{
			const FRotator Rotation = Cue.bOrientToDirection && !Event.Direction.IsNearlyZero() ? Event.Direction.Rotation() : FRotator::ZeroRotator;
			// One-shots come from the world pool (no allocation per hit on a toaster PC); a sustained effect is held by this component
			// until its stop event, so it is never pooled (a pooled component could be handed to another effect meanwhile)
			const bool bPooled = !Cue.IsSustained();
			FVector Location = Event.Location + Cue.Offset;
			if (Cue.TowardCamera > 0.f)
			{
				// The first local player's view: split screen shares the effect, the main view decides (cosmetic only)
				const APlayerController* Viewer = World->GetFirstPlayerController();
				if (Viewer && Viewer->PlayerCameraManager)
				{
					const FVector ToCamera = Viewer->PlayerCameraManager->GetCameraLocation() - Location;
					Location += ToCamera.GetSafeNormal() * FMath::Min<double>(Cue.TowardCamera, ToCamera.Size() * 0.5);
				}
			}
			System = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Asset, Location, Rotation,
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
			const bool bStrong = Cue.StrongIntensity > 0.f && Intensity >= Cue.StrongIntensity;
			if (Cue.CountMax > 0 && !Cue.CountParameter.IsNone())
			{
				System->SetVariableInt(Cue.CountParameter, FMath::Clamp(FMath::RoundToInt(Cue.CountMax * Intensity), 1, Cue.CountMax));
			}
			if (Cue.StrongCount > 0 && !Cue.StrongCountParameter.IsNone())
			{
				System->SetVariableInt(Cue.StrongCountParameter, bStrong ? Cue.StrongCount : 0);
			}
			if (!Cue.StrongParameter.IsNone())
			{
				System->SetVariableFloat(Cue.StrongParameter, Cue.StrongIntensity > 0.f && Intensity >= Cue.StrongIntensity ? 1.f : 0.f);
			}
			// A pooled component may come back from an effect that was slowed by a hitstop
			System->SetCustomTimeDilation(1.f);
			SetGroundPlane(System, Event);
			if (!Cue.IsSustained())
			{
				OneShots.Add({System, World->GetTimeSeconds(), bEnergy});
				UpdateTickEnabled();
			}
			if (Cue.IsSustained())
			{
				FSustained& Entry = Sustained.AddDefaulted_GetRef();
				Entry.System = System;
				Entry.StopOn = Cue.StopOn;
				Entry.EndTime = Cue.MaxDuration > 0.f ? World->GetTimeSeconds() + Cue.MaxDuration : 0.0;
				Entry.MinSpeed = Cue.SustainMinSpeed;
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
		const FGSTN_PresentationSnapshot& State = Deriver.GetPrevious();
		const FVector Up = State.Up.IsNearlyZero() ? FVector::UpVector : State.Up.GetSafeNormal();
		const double Speed = State.bValid ? FVector::VectorPlaneProject(State.Velocity, Up).Size() : 0.0;
		for (FSustained& Entry : Sustained)
		{
			const bool bTooSlow = Entry.MinSpeed > 0.f && State.bValid && Speed < Entry.MinSpeed;
			if (((Entry.EndTime > 0.0 && Now >= Entry.EndTime) || bTooSlow) && Entry.System.IsValid())
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
	if (const UWorld* World = GetWorld(); World && !OneShots.IsEmpty())
	{
		UpdateHitstopTime(World->GetTimeSeconds());
	}
	UpdateTickEnabled();
}

void UGSTN_PresentationComponent::UpdateHitstopTime(double Now)
{
	static const FGameplayTag HitstopTag = FGameplayTag::RequestGameplayTag(TEXT("Combat.State.Hitstop"), false);
	const FGSTN_PresentationSnapshot& State = Deriver.GetPrevious();
	const bool bHitstop = HitstopTag.IsValid() && State.bValid && State.StateTags.HasTag(HitstopTag);
	const double FreeTime = HitstopVfxFreeFrames / 60.0;
	for (FOneShot& Shot : OneShots)
	{
		UNiagaraComponent* System = Shot.System.Get();
		if (!System)
		{
			continue;
		}
		if (!System->IsActive())
		{
			System->SetCustomTimeDilation(1.f);  // back to the pool at full speed
			Shot.System.Reset();
			continue;
		}
		const bool bSlow = !Shot.bEnergy && bHitstop && Now - Shot.SpawnTime >= FreeTime;
		System->SetCustomTimeDilation(bSlow ? FMath::Clamp(HitstopVfxTimeScale, 0.f, 1.f) : 1.f);
	}
	OneShots.RemoveAll([](const FOneShot& Shot) { return !Shot.System.IsValid(); });
}

void UGSTN_PresentationComponent::SetGroundPlane(UNiagaraComponent* System, const FGSTN_PresentationEvent& Event) const
{
	static const FNiagaraVariable PointVar(FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.GroundPoint"));
	static const FNiagaraVariable SurfaceVar(FNiagaraTypeDefinition::GetColorDef(), TEXT("User.SurfaceColor"));
	UNiagaraSystem* Asset = System ? System->GetAsset() : nullptr;
	const bool bWantsPlane = Asset && Asset->GetExposedParameters().FindParameterOffset(PointVar);
	const bool bWantsSurface = Asset && Asset->GetExposedParameters().FindParameterOffset(SurfaceVar);
	if (!bWantsPlane && !bWantsSurface)
	{
		return;  // no bounce and no floor colour in this effect: no trace
	}
	const UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	const FGSTN_PresentationSnapshot& State = Deriver.GetPrevious();
	const FVector Up = State.bValid && !State.Up.IsNearlyZero() ? State.Up.GetSafeNormal() : FVector::UpVector;
	// Fallback: the feet (the event of an airborne hit is at the body)
	FVector Point = Owner ? Owner->GetActorLocation() - Up * Owner->GetSimpleCollisionHalfHeight() : Event.Location;
	FVector Normal = Up;
	FLinearColor Surface = DefaultSurfaceColor;
	TArray<FString> Rgb;
	if (GSTNPresentationCVars::SurfaceColor.GetValueOnGameThread().ParseIntoArray(Rgb, TEXT(",")) == 3)
	{
		Surface = FLinearColor(FCString::Atof(*Rgb[0]), FCString::Atof(*Rgb[1]), FCString::Atof(*Rgb[2]), 1.f);
	}
	if (World)
	{
		FHitResult Hit;
		const FVector Start = Event.Location + Up * 20.0;
		const FVector End = Event.Location - Up * 800.0;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTN_PresentationGround), false, Owner);
		Params.bReturnPhysicalMaterial = bWantsSurface;
		if (World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			Point = Hit.ImpactPoint;
			Normal = Hit.ImpactNormal;
			if (const FLinearColor* Found = SurfaceColors.Find(UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get())))
			{
				Surface = *Found;
			}
		}
	}
	if (bWantsSurface)
	{
		System->SetVariableLinearColor(TEXT("SurfaceColor"), Surface);
	}
	if (!bWantsPlane)
	{
		return;
	}
	const FTransform& Transform = System->GetComponentTransform();
	System->SetVariableVec3(TEXT("GroundPoint"), Transform.InverseTransformPosition(Point));
	System->SetVariableVec3(TEXT("GroundNormal"), Transform.InverseTransformVectorNoScale(Normal).GetSafeNormal());
	System->SetVariableFloat(TEXT("GroundHeight"), Point.Z);
}

void UGSTN_PresentationComponent::UpdateContactShadow(const FGSTN_PresentationSnapshot& Snapshot)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (!bContactShadow)
	{
		if (ContactShadow)
		{
			ContactShadow->SetVisibility(false);
		}
		return;
	}
	if (ContactShadow && !ContactShadowActor.IsValid())
	{
		ContactShadow = nullptr;
	}
	if (!ContactShadow)
	{
		UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		UMaterialInterface* Material = ContactShadowMaterial.LoadSynchronous();
		if (!Plane || !Material)
		{
			bContactShadow = false;  // missing asset: off for this character, no retry every frame
			return;
		}
		// Its own transient actor, never a component of the pawn: a component registered on a running Mover pawn can end up as its root
		// and break the Network Prediction proxies (VFX v3 r1 capture: editor crash at the end of Play)
		FActorSpawnParameters Spawn;
		Spawn.ObjectFlags |= RF_Transient;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform::Identity, Spawn);
		if (!Actor)
		{
			bContactShadow = false;
			return;
		}
		ContactShadowActor = Actor;
		ContactShadow = Actor->GetStaticMeshComponent();
		ContactShadow->SetMobility(EComponentMobility::Movable);
		ContactShadow->SetStaticMesh(Plane);
		ContactShadow->SetMaterial(0, Material);
		ContactShadow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ContactShadow->SetCastShadow(false);
		ContactShadow->SetReceivesDecals(false);
	}
	const FVector Up = Snapshot.bValid && !Snapshot.Up.IsNearlyZero() ? Snapshot.Up.GetSafeNormal() : FVector::UpVector;
	const FVector Feet = Owner->GetActorLocation() - Up * Owner->GetSimpleCollisionHalfHeight();
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTN_ContactShadow), false, Owner);
	if (!World->LineTraceSingleByObjectType(Hit, Feet + Up * 30.0, Feet - Up * ContactShadowMaxHeight,
			FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		ContactShadow->SetVisibility(false);
		return;
	}
	const float Height = FMath::Max(0.f, static_cast<float>(FVector::DotProduct(Feet - Hit.ImpactPoint, Up)));
	const float K = FMath::Clamp(Height / ContactShadowMaxHeight, 0.f, 1.f);
	const float Radius = ContactShadowRadius > 0.f ? ContactShadowRadius : Owner->GetSimpleCollisionRadius() * 1.15f;
	const float Diameter = 2.f * Radius * FMath::Lerp(1.f, 0.6f, K);
	ContactShadow->SetVisibility(true);
	// 1 cm above the floor: never z-fights, never floats visibly. The basic plane is 100 cm across
	ContactShadow->SetWorldLocationAndRotation(Hit.ImpactPoint + Hit.ImpactNormal * 1.0, FRotationMatrix::MakeFromZ(Hit.ImpactNormal).ToQuat());
	ContactShadow->SetWorldScale3D(FVector(Diameter / 100.f, Diameter / 100.f, 1.f));
	ContactShadow->SetCustomPrimitiveDataFloat(0, 1.f - K);
}

void UGSTN_PresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (AStaticMeshActor* Actor = ContactShadowActor.Get())
	{
		Actor->Destroy();
	}
	ContactShadowActor.Reset();
	ContactShadow = nullptr;
	Super::EndPlay(Reason);
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
	SetComponentTickEnabled(!Additives.IsEmpty() || !Sustained.IsEmpty() || !PendingGrounded.IsEmpty() || !OneShots.IsEmpty());
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

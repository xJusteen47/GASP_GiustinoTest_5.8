#include "GSTN_DepthMarkerSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GSTN_DepthMarkerComponent.h"
#include "GSTN_DepthMarkerMath.h"
#include "GSTN_DepthMarkerPresenter.h"
#include "GSTN_DepthMarkerSettings.h"
#include "GSTN_DepthMarkerStyle.h"
#include "GSTN_DepthSubject.h"
#include "GSTN_DepthTargetSelector.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace GSTNDepthMarkerSubsystemPrivate
{
	static TAutoConsoleVariable<int32> CVarEnable(TEXT("gstn.DepthMarker.Enable"), 1, TEXT("0 turns every depth marker off on this machine."));
	static TAutoConsoleVariable<int32> CVarMaxMarkers(TEXT("gstn.DepthMarker.MaxMarkers"), -1, TEXT("Markers drawn at once; negative uses the settings."));
	static TAutoConsoleVariable<int32> CVarTraces(TEXT("gstn.DepthMarker.TracesPerFrame"), -1, TEXT("Traces issued per frame; negative uses the settings."));

	/** What a trace answers: the user data of an async query is (marker id << 2) | kind */
	enum class EQueryKind : uint32
	{
		Probe = 0,
		Footprint = 1
	};

	static uint32 PackUserData(int32 Id, EQueryKind Kind) { return (static_cast<uint32>(Id) << 2) | static_cast<uint32>(Kind); }

	static int64 KeyOf(const UPrimitiveComponent* Component)
	{
		return Component ? static_cast<int64>(Component->GetUniqueID()) + 1 : 0;
	}
}

using namespace GSTNDepthMarkerSubsystemPrivate;

struct UGSTN_DepthMarkerSubsystem::FMarker
{
	int32 Id = 0;
	TWeakObjectPtr<UGSTN_DepthMarkerComponent> Component;

	FGSTN_DepthSelectionState State;
	FGSTN_DepthSubjectState Subject;
	bool bSubjectValid = false;
	bool bAllowed = false;
	bool bActive = false;
	bool bLocal = false;

	/** Last probe, kept raw (heights are recomputed against the feet of the frame that evaluates them) */
	TArray<FHitResult> Hits;
	FGSTN_DepthFootprintHit Footprint;
	bool bProbeInFlight = false;
	bool bFootprintInFlight = false;
	bool bFootprintReady = false;
	bool bHasNewHits = false;
	double LastProbeTime = -1.0e9;
	TMap<int64, TWeakObjectPtr<UPrimitiveComponent>> HitComponents;

	FGSTN_DepthTarget LastBelow;
	FGSTN_DepthResult Result;
	double ResetTime = -1.0e9;
	double SwitchTime = -1.0e9;
	int64 LastBelowKey = 0;

	TStrongObjectPtr<UGSTN_DepthMarkerStyle> Style;
	TArray<TStrongObjectPtr<UGSTN_DepthMarkerPresenter>> Presenters;
};

bool UGSTN_DepthMarkerSubsystem::ShouldCreateFor(ENetMode NetMode, bool bGameWorld, bool bEnabled)
{
	return bEnabled && bGameWorld && NetMode != NM_DedicatedServer;
}

UGSTN_DepthMarkerSubsystem* UGSTN_DepthMarkerSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UGSTN_DepthMarkerSubsystem>() : nullptr;
}

bool UGSTN_DepthMarkerSubsystem::IsExcludedObjectType(ECollisionChannel ObjectType, const TArray<TEnumAsByte<ECollisionChannel>>& Excluded)
{
	return Excluded.Contains(ObjectType) || (Excluded.IsEmpty() && ObjectType == ECC_Pawn);
}

bool UGSTN_DepthMarkerSubsystem::MakeCandidate(const FHitResult& Hit, const FGSTN_DepthSubjectState& Subject,
	const TArray<TEnumAsByte<ECollisionChannel>>& Excluded, FGSTN_DepthCandidate& OutCandidate)
{
	const UPrimitiveComponent* Component = Hit.GetComponent();
	if (!Component)
	{
		return false;
	}
	const FVector Up = Subject.Up.GetSafeNormal();
	OutCandidate = FGSTN_DepthCandidate();
	// Overlap hits (triggers, volumes) come back non-blocking: flagged, never a target
	OutCandidate.bBlocking = Hit.bBlockingHit;
	OutCandidate.bStartPenetrating = Hit.bStartPenetrating;
	OutCandidate.Point = FVector(Hit.ImpactPoint);
	OutCandidate.Normal = FVector(Hit.ImpactNormal);
	OutCandidate.Height = static_cast<float>(FVector::DotProduct(Subject.Feet - OutCandidate.Point, Up));
	OutCandidate.SurfaceKey = KeyOf(Component);
	OutCandidate.ComponentTransform = Component->GetComponentTransform();
	OutCandidate.bMovingBase = Component->Mobility == EComponentMobility::Movable;
	OutCandidate.bExcluded = IsExcludedObjectType(Component->GetCollisionObjectType(), Excluded);
	return true;
}

bool UGSTN_DepthMarkerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && Super::ShouldCreateSubsystem(Outer)
		&& ShouldCreateFor(World->GetNetMode(), World->IsGameWorld(), UGSTN_DepthMarkerSettings::Get().bEnabled);
}

void UGSTN_DepthMarkerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ProbeDelegate.BindUObject(this, &UGSTN_DepthMarkerSubsystem::OnProbeDone);
	// After every actor ticked, so the presented transforms of this frame are the ones measured
	PostTickHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &UGSTN_DepthMarkerSubsystem::OnPostActorTick);
}

void UGSTN_DepthMarkerSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldPostActorTick.Remove(PostTickHandle);
	for (const TPair<TWeakObjectPtr<UGSTN_DepthMarkerComponent>, TSharedPtr<FMarker>>& Pair : Markers)
	{
		ReleasePresenters(*Pair.Value);
	}
	Markers.Reset();
	MarkersById.Reset();
	StyleOverrides.Reset();
	if (AActor* HolderActor = Holder.Get())
	{
		HolderActor->Destroy();
	}
	Holder.Reset();
	BuiltinStyle.Reset();
	BuiltinSelector.Reset();
	Super::Deinitialize();
}

void UGSTN_DepthMarkerSubsystem::RegisterMarker(UGSTN_DepthMarkerComponent* Component)
{
	if (!Component || Markers.Contains(Component))
	{
		return;
	}
	TSharedPtr<FMarker> Marker = MakeShared<FMarker>();
	Marker->Id = NextMarkerId++;
	Marker->Component = Component;
	Markers.Add(Component, Marker);
	MarkersById.Add(Marker->Id, Marker);
}

void UGSTN_DepthMarkerSubsystem::UnregisterMarker(UGSTN_DepthMarkerComponent* Component)
{
	if (TSharedPtr<FMarker>* Found = Markers.Find(Component))
	{
		ReleasePresenters(**Found);
		MarkersById.Remove((*Found)->Id);
		Markers.Remove(Component);
	}
}

void UGSTN_DepthMarkerSubsystem::PushStyleOverride(const UObject* Source, UGSTN_DepthMarkerStyle* Style, int32 Priority)
{
	PopStyleOverride(Source);
	if (Style)
	{
		FStyleOverride& Entry = StyleOverrides.AddDefaulted_GetRef();
		Entry.Source = Source;
		Entry.Style = TStrongObjectPtr<UGSTN_DepthMarkerStyle>(Style);
		Entry.Priority = Priority;
	}
}

void UGSTN_DepthMarkerSubsystem::PopStyleOverride(const UObject* Source)
{
	StyleOverrides.RemoveAll([Source](const FStyleOverride& Entry) { return Entry.Source == Source; });
}

UGSTN_DepthMarkerStyle* UGSTN_DepthMarkerSubsystem::GetBuiltinStyle()
{
	if (!BuiltinStyle)
	{
		UGSTN_DepthMarkerStyle* Style = NewObject<UGSTN_DepthMarkerStyle>(this);
		Style->PopulateDefaultPresenters();
		BuiltinStyle = TStrongObjectPtr<UGSTN_DepthMarkerStyle>(Style);
	}
	return BuiltinStyle.Get();
}

AActor* UGSTN_DepthMarkerSubsystem::GetHolder()
{
	if (AActor* Existing = Holder.Get())
	{
		return Existing;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Actor = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (Actor)
	{
		// Presenter components live here, never on a pawn: nothing about the pawn changes and nothing replicates
		USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		Holder = TStrongObjectPtr<AActor>(Actor);
	}
	return Actor;
}

UGSTN_DepthMarkerStyle* UGSTN_DepthMarkerSubsystem::ResolveStyle(const UGSTN_DepthMarkerComponent& Component)
{
	const FStyleOverride* Best = nullptr;
	for (const FStyleOverride& Entry : StyleOverrides)
	{
		if (Entry.Source.IsValid() && (!Best || Entry.Priority >= Best->Priority))
		{
			Best = &Entry;
		}
	}
	if (Best)
	{
		return Best->Style.Get();
	}
	if (!Component.Style.IsNull())
	{
		if (UGSTN_DepthMarkerStyle* Style = Component.Style.LoadSynchronous())
		{
			return Style;
		}
	}
	const UGSTN_DepthMarkerSettings& Settings = UGSTN_DepthMarkerSettings::Get();
	if (!Settings.DefaultStyle.IsNull())
	{
		if (UGSTN_DepthMarkerStyle* Style = Settings.DefaultStyle.LoadSynchronous())
		{
			return Style;
		}
	}
	return GetBuiltinStyle();
}

UGSTN_DepthTargetSelector* UGSTN_DepthMarkerSubsystem::ResolveSelector(const UGSTN_DepthMarkerComponent& Component)
{
	if (Component.Selector)
	{
		return Component.Selector;
	}
	if (!BuiltinSelector)
	{
		const UGSTN_DepthMarkerSettings& Settings = UGSTN_DepthMarkerSettings::Get();
		TSubclassOf<UGSTN_DepthTargetSelector> Class = !Settings.DefaultSelector.IsNull() ? Settings.DefaultSelector.LoadSynchronous() : nullptr;
		if (!Class || Class->HasAnyClassFlags(CLASS_Abstract))
		{
			Class = UGSTN_DepthTargetSelector_Gravity::StaticClass();
		}
		UGSTN_DepthTargetSelector* Selector = NewObject<UGSTN_DepthTargetSelector>(this, Class);
		Selector->Params = Settings.DefaultParams;
		Selector->TargetBehaviours = Settings.TargetBehaviours;
		BuiltinSelector = TStrongObjectPtr<UGSTN_DepthTargetSelector>(Selector);
	}
	return BuiltinSelector.Get();
}

bool UGSTN_DepthMarkerSubsystem::ReadSubject(const UGSTN_DepthMarkerComponent& Component, FGSTN_DepthSubjectState& Out) const
{
	const AActor* Owner = Component.GetOwner();
	if (!Owner)
	{
		return false;
	}
	if (Owner->Implements<UGSTN_DepthSubject>())
	{
		return IGSTN_DepthSubject::Execute_GetDepthSubjectState(Owner, Out);
	}
	const UGSTN_DepthSubjectAdapter* Adapter = FGSTN_DepthAdapterRegistry::Resolve(Owner);
	return Adapter && Adapter->ReadState(Owner, Out);
}

void UGSTN_DepthMarkerSubsystem::BindPresenters(FMarker& Marker, UGSTN_DepthMarkerStyle* Style)
{
	ReleasePresenters(Marker);
	Marker.Style = TStrongObjectPtr<UGSTN_DepthMarkerStyle>(Style);
	if (!Style)
	{
		return;
	}
	for (const UGSTN_DepthMarkerPresenter* Template : Style->Presenters)
	{
		if (Template)
		{
			Marker.Presenters.Add(TStrongObjectPtr<UGSTN_DepthMarkerPresenter>(DuplicateObject<UGSTN_DepthMarkerPresenter>(Template, this)));
		}
	}
}

void UGSTN_DepthMarkerSubsystem::ReleasePresenters(FMarker& Marker)
{
	for (TStrongObjectPtr<UGSTN_DepthMarkerPresenter>& Presenter : Marker.Presenters)
	{
		if (Presenter->LastState != EGSTN_DepthMarkerState::Hidden)
		{
			Presenter->OnMarkerReleased();
			Presenter->LastState = EGSTN_DepthMarkerState::Hidden;
		}
	}
	Marker.Presenters.Reset();
	Marker.Style.Reset();
	Marker.bActive = false;
}

void UGSTN_DepthMarkerSubsystem::OnPostActorTick(UWorld* TickedWorld, ELevelTick TickType, float DeltaSeconds)
{
	if (TickedWorld == GetWorld() && TickType == LEVELTICK_All)
	{
		TickMarkers(DeltaSeconds);
	}
}

void UGSTN_DepthMarkerSubsystem::IssueProbe(FMarker& Marker, const TArray<AActor*>& Ignored, double Time)
{
	UWorld* World = GetWorld();
	const UGSTN_DepthMarkerComponent* Component = Marker.Component.Get();
	if (!World || !Component)
	{
		return;
	}
	const UGSTN_DepthTargetSelector* Selector = ResolveSelector(*Component);
	FVector Start;
	FVector End;
	Selector->BuildProbe(Marker.Subject, Start, End);

	FCollisionQueryParams Query(SCENE_QUERY_STAT(GSTNDepthMarkerProbe), false);
	Query.AddIgnoredActors(Ignored);
	World->AsyncLineTraceByChannel(EAsyncTraceType::Multi, Start, End, UGSTN_DepthMarkerSettings::Get().TraceChannel, Query,
		FCollisionResponseParams::DefaultResponseParam, &ProbeDelegate, PackUserData(Marker.Id, EQueryKind::Probe));
	Marker.bProbeInFlight = true;
	Marker.LastProbeTime = Time;
}

void UGSTN_DepthMarkerSubsystem::IssueFootprint(FMarker& Marker, const TArray<AActor*>& Ignored, double Time)
{
	UWorld* World = GetWorld();
	const UGSTN_DepthMarkerComponent* Component = Marker.Component.Get();
	if (!World || !Component)
	{
		return;
	}
	const FGSTN_DepthSelectorParams& Params = ResolveSelector(*Component)->Params;
	const FVector Down = -Marker.Subject.Up.GetSafeNormal();
	const float Radius = FMath::Max(Marker.Subject.FootRadius * Params.FootprintRadiusScale, 1.f);
	// Only as far as the previous surface plus the sphere: a short sweep, not a second probe
	const float Reach = FMath::Clamp(Marker.State.PreviousHeight, 0.f, Params.ProbeLength) + Radius * 2.f + 20.f;

	FCollisionQueryParams Query(SCENE_QUERY_STAT(GSTNDepthMarkerFootprint), false);
	Query.AddIgnoredActors(Ignored);
	World->AsyncSweepByChannel(EAsyncTraceType::Multi, Marker.Subject.Feet, Marker.Subject.Feet + Down * Reach, FQuat::Identity,
		UGSTN_DepthMarkerSettings::Get().TraceChannel, FCollisionShape::MakeSphere(Radius), Query,
		FCollisionResponseParams::DefaultResponseParam, &ProbeDelegate, PackUserData(Marker.Id, EQueryKind::Footprint));
	Marker.bFootprintInFlight = true;
	++LastTraceCount;
}

void UGSTN_DepthMarkerSubsystem::OnProbeDone(const FTraceHandle& Handle, FTraceDatum& Datum)
{
	const int32 Id = static_cast<int32>(Datum.UserData >> 2);
	const EQueryKind Kind = static_cast<EQueryKind>(Datum.UserData & 3u);
	const TWeakPtr<FMarker>* Weak = MarkersById.Find(Id);
	const TSharedPtr<FMarker> Marker = Weak ? Weak->Pin() : nullptr;
	if (!Marker)
	{
		return;
	}
	if (Kind == EQueryKind::Probe)
	{
		Marker->Hits = Datum.OutHits;
		Marker->bProbeInFlight = false;
		Marker->bHasNewHits = true;
		for (const FHitResult& Hit : Marker->Hits)
		{
			if (const UPrimitiveComponent* Component = Hit.GetComponent())
			{
				Marker->HitComponents.Add(KeyOf(Component), const_cast<UPrimitiveComponent*>(Component));
			}
		}
		return;
	}

	// Footprint: does the sphere still touch the previously selected surface, and how far from the column (R3)
	Marker->bFootprintInFlight = false;
	Marker->bFootprintReady = true;
	Marker->Footprint = FGSTN_DepthFootprintHit();
	const FVector Up = Marker->Subject.Up.GetSafeNormal();
	for (const FHitResult& Hit : Datum.OutHits)
	{
		if (KeyOf(Hit.GetComponent()) == Marker->State.PreviousKey && Hit.bBlockingHit)
		{
			const FVector Offset = FVector(Hit.ImpactPoint) - Marker->Subject.Feet;
			Marker->Footprint.bValid = true;
			Marker->Footprint.SurfaceKey = Marker->State.PreviousKey;
			Marker->Footprint.Point = FVector(Hit.ImpactPoint);
			Marker->Footprint.LateralCm = static_cast<float>((Offset - Up * FVector::DotProduct(Offset, Up)).Size());
			break;
		}
	}
}

void UGSTN_DepthMarkerSubsystem::RefreshMarker(FMarker& Marker, double Time, bool bAllowed)
{
	const UGSTN_DepthMarkerComponent* Component = Marker.Component.Get();
	if (!Component || !Marker.bSubjectValid)
	{
		return;
	}
	const UGSTN_DepthMarkerSettings& Settings = UGSTN_DepthMarkerSettings::Get();
	const UGSTN_DepthTargetSelector* Selector = ResolveSelector(*Component);
	const FGSTN_DepthSubjectState& Subject = Marker.Subject;
	if (!Subject.bAirborne && Subject.bGrounded && !Marker.State.bVisible)
	{
		Marker.LastBelow = FGSTN_DepthTarget();
	}

	// Hits wait for the footprint answer when the previous surface is in doubt (R3)
	bool bProbed = false;
	TArray<FGSTN_DepthCandidate> Raw;
	if (Marker.bHasNewHits && !Marker.bFootprintInFlight)
	{
		for (const FHitResult& Hit : Marker.Hits)
		{
			FGSTN_DepthCandidate Candidate;
			if (MakeCandidate(Hit, Subject, Settings.ExcludedObjectTypes, Candidate))
			{
				Raw.Add(Candidate);
			}
		}
		if (Marker.State.bHasPrevious && !Marker.bFootprintReady)
		{
			TArray<FGSTN_DepthCandidate> Filtered;
			FGSTN_DepthMarkerMath::FilterCandidates(Raw, Subject.Up, Selector->Params, Selector->TargetBehaviours, Filtered);
			FGSTN_DepthCandidate Chosen;
			const bool bSame = FGSTN_DepthMarkerMath::ChooseFirstBelow(Filtered, Chosen) && Chosen.SurfaceKey == Marker.State.PreviousKey;
			if (!bSame)
			{
				TArray<AActor*> Ignored;
				Ignored.Add(const_cast<AActor*>(Component->GetOwner()));
				IssueFootprint(Marker, Ignored, Time);
				return;
			}
		}
		bProbed = true;
		Marker.bHasNewHits = false;
	}
	// A "no ground" ring is re-derived every frame from the plane; it needs the probe flag but no new hits
	const bool bRing = !bProbed && Marker.LastBelow.Kind == EGSTN_DepthTargetKind::ReferencePlane;

	FGSTN_DepthResult Result = Selector->ChooseTarget(Subject, Raw, Marker.Footprint, FGSTN_DepthReferenceOverride(), Marker.State, Time, bProbed || bRing);
	Marker.Footprint = FGSTN_DepthFootprintHit();
	Marker.bFootprintReady = false;

	if (Result.bGravityReset)
	{
		Marker.ResetTime = Time;
		Marker.LastBelow = FGSTN_DepthTarget();
	}
	if (bProbed || bRing)
	{
		Marker.LastBelow = Result.Below;
	}
	else if (Marker.LastBelow.Kind == EGSTN_DepthTargetKind::Surface)
	{
		// Between traces the marker follows the stored plane in the hit component local space (§2.3 step 7)
		const TWeakObjectPtr<UPrimitiveComponent>* Hit = Marker.HitComponents.Find(Marker.LastBelow.SurfaceKey);
		if (const UPrimitiveComponent* HitComponent = Hit ? Hit->Get() : nullptr)
		{
			Result.Below = FGSTN_DepthMarkerMath::ReprojectTarget(Marker.LastBelow, HitComponent->GetComponentTransform(), Subject.Feet, Subject.Up);
		}
		else
		{
			Result.Below = Marker.LastBelow;
		}
	}
	if (Result.Below.Kind == EGSTN_DepthTargetKind::Surface && Result.Below.SurfaceKey != Marker.LastBelowKey)
	{
		Marker.LastBelowKey = Result.Below.SurfaceKey;
		Marker.SwitchTime = Time;
	}
	Marker.Result = Result;
}

void UGSTN_DepthMarkerSubsystem::DriveMarker(FMarker& Marker, double Time, bool bAllowed)
{
	UGSTN_DepthMarkerComponent* Component = Marker.Component.Get();
	if (!Component)
	{
		return;
	}
	UGSTN_DepthMarkerStyle* Style = ResolveStyle(*Component);
	if (Style != Marker.Style.Get())
	{
		BindPresenters(Marker, Style);
	}
	const UGSTN_DepthTargetSelector* Selector = ResolveSelector(*Component);
	const FGSTN_DepthSubjectState& Subject = Marker.Subject;
	const FGSTN_DepthResult& Result = Marker.Result;
	const FGSTN_DepthFadeParams& Fade = Style->Fade;

	FGSTN_DepthMarkerView View;
	View.Owner = Component->GetOwner();
	View.Subject = Subject;
	View.Below = Result.Below;
	View.Above = Result.Above;
	View.TimeSeconds = static_cast<float>(FMath::Fmod(Time, 10000.0));

	if (Component->bOverrideTint)
	{
		View.Tint = Component->TintOverride;
	}
	else if (const FLinearColor* TeamTint = Style->TeamTints.Find(Subject.TeamId))
	{
		View.Tint = *TeamTint;
	}
	else
	{
		View.Tint = Style->DefaultTint;
	}

	// Gravity cross-fade and the fade-in of a switch to a new surface (§2.3, §2.4)
	float Blend = 1.f;
	if (Selector->Params.GravityFadeMs > 0.f)
	{
		Blend = FMath::Min(Blend, static_cast<float>((Time - Marker.ResetTime) / (Selector->Params.GravityFadeMs * 0.001)));
	}
	if (Result.Below.BlendMs > 0.f)
	{
		Blend = FMath::Min(Blend, static_cast<float>((Time - Marker.SwitchTime) / (Result.Below.BlendMs * 0.001)));
	}
	Blend = FMath::Clamp(Blend, 0.f, 1.f);

	float Alpha = 1.f;
	float Radius = 1.f;
	if (Result.Below.IsValid())
	{
		FGSTN_DepthMarkerMath::ComputeFade(Result.Below.Height, Fade, Alpha, Radius);
		View.RadiusScale = Radius;
		if (Result.Below.Kind == EGSTN_DepthTargetKind::Surface)
		{
			View.bBelowVisible = bAllowed && FGSTN_DepthMarkerMath::UpdateVisibility(Marker.State, Subject.bAirborne, Result.Below.Height, Fade);
		}
		else
		{
			// The "no ground" ring shows near the plane whatever the height (§3)
			View.bBelowVisible = bAllowed;
		}
		View.BelowAlpha = Alpha * Result.Below.EdgeAlpha * Blend;
	}
	else
	{
		Marker.State.bVisible = false;
	}
	if (Result.Above.IsValid())
	{
		float AboveAlpha = 1.f;
		float AboveRadius = 1.f;
		FGSTN_DepthMarkerMath::ComputeFade(Result.Above.Height, Fade, AboveAlpha, AboveRadius);
		View.bAboveVisible = bAllowed;
		View.AboveAlpha = AboveAlpha * Blend;
		if (!Result.Below.IsValid())
		{
			View.RadiusScale = AboveRadius;
		}
	}

	for (TStrongObjectPtr<UGSTN_DepthMarkerPresenter>& Presenter : Marker.Presenters)
	{
		const bool bDraw = Presenter->ShouldDraw(View);
		const EGSTN_DepthMarkerState NewState = bDraw ? View.GetTarget(Presenter->Layer).State : EGSTN_DepthMarkerState::Hidden;
		if (NewState != Presenter->LastState)
		{
			Presenter->OnStateChanged(Presenter->LastState, NewState);
			if (NewState == EGSTN_DepthMarkerState::Hidden)
			{
				Presenter->OnMarkerReleased();
			}
			Presenter->LastState = NewState;
		}
		if (bDraw)
		{
			Presenter->OnMarkerUpdated(View, GetHolder());
		}
	}
	Component->LastResult = Result;
}

void UGSTN_DepthMarkerSubsystem::TickMarkers(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const UGSTN_DepthMarkerSettings& Settings = UGSTN_DepthMarkerSettings::Get();
	const double Time = World->GetTimeSeconds();
	LastActiveCount = 0;
	LastTraceCount = 0;

	// Drop markers whose component is gone
	for (auto It = Markers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			ReleasePresenters(*It.Value());
			MarkersById.Remove(It.Value()->Id);
			It.RemoveCurrent();
		}
	}

	if (!Settings.bEnabled || CVarEnable.GetValueOnGameThread() == 0)
	{
		for (const TPair<TWeakObjectPtr<UGSTN_DepthMarkerComponent>, TSharedPtr<FMarker>>& Pair : Markers)
		{
			ReleasePresenters(*Pair.Value);
		}
		return;
	}

	// Local pawns: their positions drive the distance cull, their teams the audience
	TArray<const APawn*> LocalPawns;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		if (Controller && Controller->IsLocalController() && Controller->GetPawn())
		{
			LocalPawns.Add(Controller->GetPawn());
		}
	}

	TArray<FGSTN_DepthRankEntry> Entries;
	TArray<AActor*> AllOwners;
	TArray<int32> LocalTeams;
	for (const TPair<TWeakObjectPtr<UGSTN_DepthMarkerComponent>, TSharedPtr<FMarker>>& Pair : Markers)
	{
		FMarker& Marker = *Pair.Value;
		const UGSTN_DepthMarkerComponent* Component = Pair.Key.Get();
		AllOwners.Add(Component->GetOwner());
		Marker.bSubjectValid = ReadSubject(*Component, Marker.Subject);
		const APawn* OwnerPawn = Cast<APawn>(Component->GetOwner());
		Marker.bLocal = OwnerPawn && OwnerPawn->IsLocallyControlled();
		if (Marker.bLocal && Marker.bSubjectValid && Marker.Subject.TeamId != INDEX_NONE)
		{
			LocalTeams.AddUnique(Marker.Subject.TeamId);
		}
	}

	for (const TPair<TWeakObjectPtr<UGSTN_DepthMarkerComponent>, TSharedPtr<FMarker>>& Pair : Markers)
	{
		FMarker& Marker = *Pair.Value;
		const UGSTN_DepthMarkerComponent* Component = Pair.Key.Get();
		const AActor* Owner = Component->GetOwner();
		Marker.bAllowed = false;
		if (!Marker.bSubjectValid)
		{
			ReleasePresenters(Marker);
			continue;
		}

		// 0 the local player, 1 an ally (same team id as a local pawn), 2 everyone else
		int32 Priority = 2;
		if (Marker.bLocal)
		{
			Priority = 0;
		}
		else if (Marker.Subject.TeamId != INDEX_NONE && LocalTeams.Contains(Marker.Subject.TeamId))
		{
			Priority = 1;
		}

		float DistanceSq = 0.f;
		if (!LocalPawns.IsEmpty())
		{
			DistanceSq = TNumericLimits<float>::Max();
			for (const APawn* Local : LocalPawns)
			{
				DistanceSq = FMath::Min(DistanceSq, static_cast<float>(FVector::DistSquared(Local->GetActorLocation(), Marker.Subject.Feet)));
			}
		}

		bool bAllowed = Component->bMarkerEnabled && !Marker.Subject.bSuppressed;
		switch (Component->Audience)
		{
		case EGSTN_DepthAudience::LocalOnly: bAllowed &= Priority == 0; break;
		case EGSTN_DepthAudience::LocalAndAllies: bAllowed &= Priority <= 1; break;
		default: break;
		}
		// The local pawn is never culled by distance; others beyond the view distance get no probe and no marker
		if (Priority != 0 && DistanceSq > FMath::Square(Settings.MaxViewDistanceCm))
		{
			bAllowed = false;
		}
		// Parked entities keep no marker of their own (the holder keeps its own); the style's policy is data
		if (Marker.Subject.bParked)
		{
			const UGSTN_DepthMarkerStyle* Style = ResolveStyle(*Component);
			bAllowed &= Style && Style->LinkPolicy == EGSTN_DepthLinkPolicy::ShowOnGround;
		}
		if (bAllowed && Owner->Implements<UGSTN_DepthTagSource>())
		{
			FGameplayTagContainer Tags;
			IGSTN_DepthTagSource::Execute_GetDepthMarkerTags(Owner, Tags);
			bAllowed &= Component->HideWhen.IsEmpty() || !Component->HideWhen.Matches(Tags);
			bAllowed &= Component->ShowOnlyWhen.IsEmpty() || Component->ShowOnlyWhen.Matches(Tags);
		}
		Marker.bAllowed = bAllowed;
		if (!bAllowed)
		{
			ReleasePresenters(Marker);
			Marker.LastBelow = FGSTN_DepthTarget();
			continue;
		}

		FGSTN_DepthRankEntry Entry;
		Entry.Id = Marker.Id;
		Entry.Priority = Priority;
		Entry.DistanceSq = DistanceSq;
		Entry.LastProbeTime = Marker.LastProbeTime;
		Entry.bWantsProbe = !Marker.bProbeInFlight && FGSTN_DepthMarkerScheduler::WantsProbe(Marker.Subject, Marker.State.bVisible);
		Entries.Add(Entry);
	}

	FGSTN_DepthSchedulerConfig Config;
	const int32 MaxMarkersCVar = CVarMaxMarkers.GetValueOnGameThread();
	const int32 TracesCVar = CVarTraces.GetValueOnGameThread();
	Config.MaxMarkers = MaxMarkersCVar >= 0 ? MaxMarkersCVar : Settings.MaxMarkers;
	Config.MaxTraces = TracesCVar >= 0 ? TracesCVar : Settings.MaxTracesPerFrame;
	Config.FullRateRanks = Settings.FullRateRanks;
	Config.MidRateRanks = Settings.MidRateRanks;
	Config.MidRateHz = Settings.MidRateHz;
	Config.LowRateHz = Settings.LowRateHz;

	TArray<int32> Dropped;
	FGSTN_DepthMarkerScheduler::Rank(Entries, Config.MaxMarkers, Dropped);
	for (const int32 Id : Dropped)
	{
		if (const TWeakPtr<FMarker>* Weak = MarkersById.Find(Id))
		{
			if (TSharedPtr<FMarker> Marker = Weak->Pin())
			{
				ReleasePresenters(*Marker);
			}
		}
	}

	TArray<int32> ProbeIds;
	FGSTN_DepthMarkerScheduler::PlanProbes(Entries, Config, Time, ProbeIds);
	for (const int32 Id : ProbeIds)
	{
		if (const TWeakPtr<FMarker>* Weak = MarkersById.Find(Id))
		{
			if (TSharedPtr<FMarker> Marker = Weak->Pin())
			{
				// Every other registered subject is ignored, so a pawn standing below never becomes the ground
				IssueProbe(*Marker, AllOwners, Time);
				++LastTraceCount;
			}
		}
	}

	for (const FGSTN_DepthRankEntry& Entry : Entries)
	{
		const TWeakPtr<FMarker>* Weak = MarkersById.Find(Entry.Id);
		TSharedPtr<FMarker> Marker = Weak ? Weak->Pin() : nullptr;
		if (!Marker)
		{
			continue;
		}
		Marker->bActive = true;
		RefreshMarker(*Marker, Time, Marker->bAllowed);
		DriveMarker(*Marker, Time, Marker->bAllowed);
		++LastActiveCount;
	}
}

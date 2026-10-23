#include "GSTN_DepthMarkerSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GSTN_DepthMarkerComponent.h"
#include "GSTN_DepthMarkerMath.h"
#include "GSTN_DepthMarkerPlane.h"
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
		Footprint = 1,
		/** One slot of the approach probe pattern */
		ApproachProbe = 2
	};

	/** (marker id << 8) | (approach slot << 2) | kind */
	static uint32 PackUserData(int32 Id, EQueryKind Kind, uint32 Slot = 0)
	{
		return (static_cast<uint32>(Id) << 8) | ((Slot & 63u) << 2) | static_cast<uint32>(Kind);
	}

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

	/** Fade-in bookkeeping of the below layer: the blend length of the last switch (the target reports it for one frame only) */
	float BelowBlendMs = 0.f;

	/** Approach marker B: a short-lived cache of probe results per slot, the pattern cursor and its own presenters */
	struct FApproachSample
	{
		uint32 Slot = 0;
		TWeakObjectPtr<UPrimitiveComponent> Component;
		FVector LocalPoint = FVector::ZeroVector;
		FVector LocalNormal = FVector::UpVector;
		int64 Key = 0;
		double Time = 0.0;
	};
	TArray<FApproachSample> ApproachCache;
	uint32 ApproachNextSlot = 0;
	double LastApproachProbeTime = -1.0e9;
	int64 LastApproachKey = 0;
	double ApproachSwitchTime = -1.0e9;
	float ApproachBlendMs = 0.f;
	TStrongObjectPtr<UGSTN_DepthMarkerStyle> ApproachStyle;
	TArray<TStrongObjectPtr<UGSTN_DepthMarkerPresenter>> ApproachPresenters;
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
	Planes.Reset();
	if (AActor* HolderActor = Holder.Get())
	{
		HolderActor->Destroy();
	}
	Holder.Reset();
	BuiltinStyle.Reset();
	BuiltinApproachStyle.Reset();
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

void UGSTN_DepthMarkerSubsystem::RegisterPlane(UGSTN_DepthMarkerPlaneComponent* Plane)
{
	if (Plane)
	{
		Planes.AddUnique(Plane);
	}
}

void UGSTN_DepthMarkerSubsystem::UnregisterPlane(UGSTN_DepthMarkerPlaneComponent* Plane)
{
	Planes.Remove(Plane);
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

UGSTN_DepthMarkerStyle* UGSTN_DepthMarkerSubsystem::GetBuiltinApproachStyle()
{
	if (!BuiltinApproachStyle)
	{
		UGSTN_DepthMarkerStyle* Style = NewObject<UGSTN_DepthMarkerStyle>(this);
		Style->PopulateApproachPresenters();
		BuiltinApproachStyle = TStrongObjectPtr<UGSTN_DepthMarkerStyle>(Style);
	}
	return BuiltinApproachStyle.Get();
}

UGSTN_DepthMarkerStyle* UGSTN_DepthMarkerSubsystem::ResolveApproachStyle(const UGSTN_DepthMarkerComponent& Component, const UGSTN_DepthMarkerStyle* PrimaryStyle)
{
	// Component slot, then the style's own ApproachStyle slot, then the built-in approach look
	if (!Component.ApproachStyle.IsNull())
	{
		if (UGSTN_DepthMarkerStyle* Style = Component.ApproachStyle.LoadSynchronous())
		{
			return Style;
		}
	}
	if (PrimaryStyle && !PrimaryStyle->ApproachStyle.IsNull())
	{
		if (UGSTN_DepthMarkerStyle* Style = PrimaryStyle->ApproachStyle.LoadSynchronous())
		{
			return Style;
		}
	}
	return GetBuiltinApproachStyle();
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

void UGSTN_DepthMarkerSubsystem::BindApproachPresenters(FMarker& Marker, UGSTN_DepthMarkerStyle* Style)
{
	for (TStrongObjectPtr<UGSTN_DepthMarkerPresenter>& Presenter : Marker.ApproachPresenters)
	{
		if (Presenter->LastState != EGSTN_DepthMarkerState::Hidden)
		{
			Presenter->OnMarkerReleased();
			Presenter->LastState = EGSTN_DepthMarkerState::Hidden;
		}
	}
	Marker.ApproachPresenters.Reset();
	Marker.ApproachStyle = TStrongObjectPtr<UGSTN_DepthMarkerStyle>(Style);
	if (!Style)
	{
		return;
	}
	for (const UGSTN_DepthMarkerPresenter* Template : Style->Presenters)
	{
		if (Template)
		{
			UGSTN_DepthMarkerPresenter* Presenter = DuplicateObject<UGSTN_DepthMarkerPresenter>(Template, this);
			// Whatever the approach style lists draws the approach layer, in the one state that layer has
			Presenter->Layer = EGSTN_DepthLayer::Approach;
			Presenter->ActiveStates = { EGSTN_DepthMarkerState::Approach };
			Marker.ApproachPresenters.Add(TStrongObjectPtr<UGSTN_DepthMarkerPresenter>(Presenter));
		}
	}
}

void UGSTN_DepthMarkerSubsystem::ReleasePresenters(FMarker& Marker)
{
	BindApproachPresenters(Marker, nullptr);
	Marker.ApproachCache.Reset();
	Marker.LastApproachKey = 0;
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
	const int32 Id = static_cast<int32>(Datum.UserData >> 8);
	const uint32 Slot = (Datum.UserData >> 2) & 63u;
	const EQueryKind Kind = static_cast<EQueryKind>(Datum.UserData & 3u);
	const TWeakPtr<FMarker>* Weak = MarkersById.Find(Id);
	const TSharedPtr<FMarker> Marker = Weak ? Weak->Pin() : nullptr;
	if (!Marker)
	{
		return;
	}
	if (Kind == EQueryKind::ApproachProbe)
	{
		// The first blocking top under the probe; start-penetrating hits (the probe began inside a solid) and excluded types do not count
		Marker->ApproachCache.RemoveAll([Slot](const FMarker::FApproachSample& Sample) { return Sample.Slot == Slot; });
		for (const FHitResult& Hit : Datum.OutHits)
		{
			const UPrimitiveComponent* Component = Hit.GetComponent();
			if (!Hit.bBlockingHit || !Component)
			{
				continue;
			}
			if (!Hit.bStartPenetrating && !IsExcludedObjectType(Component->GetCollisionObjectType(), UGSTN_DepthMarkerSettings::Get().ExcludedObjectTypes))
			{
				FMarker::FApproachSample& Sample = Marker->ApproachCache.AddDefaulted_GetRef();
				const FTransform Xf = Component->GetComponentTransform();
				Sample.Slot = Slot;
				Sample.Component = const_cast<UPrimitiveComponent*>(Component);
				Sample.LocalPoint = Xf.InverseTransformPosition(FVector(Hit.ImpactPoint));
				Sample.LocalNormal = Xf.InverseTransformVectorNoScale(FVector(Hit.ImpactNormal));
				Sample.Key = KeyOf(Component);
				Sample.Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
			}
			break;
		}
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

void UGSTN_DepthMarkerSubsystem::BuildApproachCandidates(FMarker& Marker, double Time, TArray<FGSTN_DepthPlaneCandidate>& OutProbed,
	TArray<FGSTN_DepthPlaneCandidate>& OutExplicit)
{
	const UGSTN_DepthMarkerComponent* Component = Marker.Component.Get();
	if (!Component)
	{
		return;
	}
	const FGSTN_DepthSelectorParams& Params = ResolveSelector(*Component)->Params;
	if (!Params.bApproachEnabled)
	{
		return;
	}

	// Probe results expire, so the platform that was left behind stops being offered
	const double Ttl = Params.ApproachCacheMs * 0.001;
	Marker.ApproachCache.RemoveAll([&](const FMarker::FApproachSample& Sample) { return !Sample.Component.IsValid() || Time - Sample.Time > Ttl; });
	for (const FMarker::FApproachSample& Sample : Marker.ApproachCache)
	{
		// Re-expressed with the platform's CURRENT transform: a lift or a sliding platform does not leave its marker behind
		const FTransform Xf = Sample.Component->GetComponentTransform();
		FGSTN_DepthPlaneCandidate& Candidate = OutProbed.AddDefaulted_GetRef();
		Candidate.Point = Xf.TransformPosition(Sample.LocalPoint);
		Candidate.Normal = Xf.TransformVectorNoScale(Sample.LocalNormal).GetSafeNormal();
		Candidate.SurfaceKey = Sample.Key;
		Candidate.ComponentTransform = Xf;
	}

	// Designer planes that overlap the character: they win over probing
	const FVector Centre = Marker.Subject.Feet + Marker.Subject.Up.GetSafeNormal() * (Marker.Subject.TopOffset * 0.5f);
	for (const TWeakObjectPtr<UGSTN_DepthMarkerPlaneComponent>& Weak : Planes)
	{
		const UGSTN_DepthMarkerPlaneComponent* Plane = Weak.Get();
		if (Plane && Plane->bPlaneEnabled && Plane->ContainsPoint(Centre))
		{
			OutExplicit.Add(Plane->MakeCandidate());
		}
	}
}

void UGSTN_DepthMarkerSubsystem::IssueApproachProbes(FMarker& Marker, int32 Rank, const FGSTN_DepthSchedulerConfig& Config,
	const TArray<AActor*>& Ignored, double Time, int32& InOutRemaining)
{
	UWorld* World = GetWorld();
	const UGSTN_DepthMarkerComponent* Component = Marker.Component.Get();
	if (!World || !Component || InOutRemaining <= 0 || !Marker.Subject.bAirborne)
	{
		return;
	}
	const FGSTN_DepthSelectorParams& Params = ResolveSelector(*Component)->Params;
	const int32 Slots = FGSTN_DepthMarkerMath::NumApproachProbeSlots(Params);
	// The same rank rates as the primary probes: the first ranks every frame, the rest less often
	if (!Params.bApproachEnabled || Slots <= 0 || (Time - Marker.LastApproachProbeTime) < FGSTN_DepthMarkerScheduler::RankInterval(Rank, Config))
	{
		return;
	}
	Marker.LastApproachProbeTime = Time;

	FCollisionQueryParams Query(SCENE_QUERY_STAT(GSTNDepthMarkerApproach), false);
	Query.AddIgnoredActors(Ignored);
	// A few slots per tick, rotating: the whole pattern is covered over a few ticks instead of in one burst
	for (int32 Index = 0; Index < Params.ApproachProbesPerTick && InOutRemaining > 0; ++Index)
	{
		const uint32 Slot = Marker.ApproachNextSlot++ % static_cast<uint32>(Slots);
		FVector Start;
		FVector End;
		if (!FGSTN_DepthMarkerMath::ApproachProbeSegment(Marker.Subject, Params, static_cast<int32>(Slot), Start, End))
		{
			// A velocity slot while the character is slow: nothing to trace, and its old sample is stale
			Marker.ApproachCache.RemoveAll([Slot](const FMarker::FApproachSample& Sample) { return Sample.Slot == Slot; });
			continue;
		}
		World->AsyncLineTraceByChannel(EAsyncTraceType::Multi, Start, End, UGSTN_DepthMarkerSettings::Get().TraceChannel, Query,
			FCollisionResponseParams::DefaultResponseParam, &ProbeDelegate, PackUserData(Marker.Id, EQueryKind::ApproachProbe, Slot));
		--InOutRemaining;
		++LastTraceCount;
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
		// Probed tops belong to the old gravity: a floor of the old Up is not a platform of the new one
		Marker.ApproachCache.Reset();
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
		Marker.BelowBlendMs = Result.Below.BlendMs;
	}

	// Approach marker B: independent of the below marker, which it only reads to stay off the primary plane
	TArray<FGSTN_DepthPlaneCandidate> ProbedPlanes;
	TArray<FGSTN_DepthPlaneCandidate> ExplicitPlanes;
	BuildApproachCandidates(Marker, Time, ProbedPlanes, ExplicitPlanes);
	Result.Approach = Selector->ChooseApproach(Subject, Result.Below, ProbedPlanes, ExplicitPlanes, Marker.State, Time);
	if (Result.Approach.IsValid())
	{
		if (Result.Approach.SurfaceKey != Marker.LastApproachKey)
		{
			Marker.LastApproachKey = Result.Approach.SurfaceKey;
			Marker.ApproachSwitchTime = Time;
			Marker.ApproachBlendMs = Result.Approach.BlendMs;
		}
	}
	else
	{
		Marker.LastApproachKey = 0;
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
	View.Approach = Result.Approach;
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
	const float GravityBlend = FMath::Clamp(Blend, 0.f, 1.f);
	if (Marker.BelowBlendMs > 0.f)
	{
		Blend = FMath::Min(Blend, static_cast<float>((Time - Marker.SwitchTime) / (Marker.BelowBlendMs * 0.001)));
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

	// Marker B draws with its own style slot: another colour or effect, at the same time as the others
	UGSTN_DepthMarkerStyle* ApproachStyle = ResolveApproachStyle(*Component, Style);
	if (ApproachStyle != Marker.ApproachStyle.Get())
	{
		BindApproachPresenters(Marker, ApproachStyle);
	}
	if (Result.Approach.IsValid() && ApproachStyle)
	{
		float ApproachAlpha = 1.f;
		float ApproachRadius = 1.f;
		FGSTN_DepthMarkerMath::ComputeFade(FMath::Max(Result.Approach.Height, 0.f), ApproachStyle->Fade, ApproachAlpha, ApproachRadius);
		float ApproachBlend = GravityBlend;
		if (Marker.ApproachBlendMs > 0.f)
		{
			ApproachBlend = FMath::Min(ApproachBlend, static_cast<float>((Time - Marker.ApproachSwitchTime) / (Marker.ApproachBlendMs * 0.001)));
		}
		View.bApproachVisible = bAllowed;
		View.ApproachAlpha = ApproachAlpha * Result.Approach.EdgeAlpha * FMath::Clamp(ApproachBlend, 0.f, 1.f);
		View.ApproachRadiusScale = ApproachRadius;
		View.ApproachTint = ApproachStyle->DefaultTint;
	}
	for (TStrongObjectPtr<UGSTN_DepthMarkerPresenter>& Presenter : Marker.ApproachPresenters)
	{
		// The approach presenters see the approach tint and radius where the others see theirs
		FGSTN_DepthMarkerView ApproachView = View;
		ApproachView.Tint = View.ApproachTint;
		ApproachView.RadiusScale = View.ApproachRadiusScale;
		const bool bDraw = Presenter->ShouldDraw(ApproachView);
		const EGSTN_DepthMarkerState NewState = bDraw ? EGSTN_DepthMarkerState::Approach : EGSTN_DepthMarkerState::Hidden;
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
			Presenter->OnMarkerUpdated(ApproachView, GetHolder());
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

	// Approach probes come last: they use only what the primary probes and the footprint sweeps left of the cap
	int32 Remaining = FMath::Max(Config.MaxTraces - LastTraceCount, 0);
	for (int32 Rank = 0; Rank < Entries.Num() && Remaining > 0; ++Rank)
	{
		const TWeakPtr<FMarker>* Weak = MarkersById.Find(Entries[Rank].Id);
		if (TSharedPtr<FMarker> Marker = Weak ? Weak->Pin() : nullptr)
		{
			IssueApproachProbes(*Marker, Rank, Config, AllOwners, Time, Remaining);
		}
	}
}

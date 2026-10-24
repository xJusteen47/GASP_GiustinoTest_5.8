// Depth marker subsystem: registry, ranking, probe scheduler, presenter drive (depth-marker.md §3, §4.5, §5).

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "GSTN_DepthMarkerStyle.h"
#include "GSTN_DepthTargetSelector.h"
#include "UObject/StrongObjectPtr.h"
#include "GSTN_DepthMarkerRuntimeTypes.h"
#include "GSTN_DepthMarkerScheduler.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldCollision.h"
#include "GSTN_DepthMarkerSubsystem.generated.h"

class UGSTN_DepthMarkerComponent;
class UGSTN_DepthMarkerPlaneComponent;
class UGSTN_DepthMarkerPresenter;
class UGSTN_DepthMarkerStyle;
class UGSTN_DepthTargetSelector;

/**
 * Computes and draws the depth markers of one world. Local and presentation only: it is never created on a dedicated server,
 * reads the PRESENTED transforms after movement and animation (the post actor tick delegate follows TG_PostUpdateWork), and
 * writes nothing the simulation reads. Traces are asynchronous and capped per frame; between traces the marker point follows
 * the stored plane in the hit component local space.
 */
UCLASS()
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthMarkerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** The rule behind ShouldCreateSubsystem, on plain values so it is testable: nothing on a dedicated server or in a non game world */
	static bool ShouldCreateFor(ENetMode NetMode, bool bGameWorld, bool bEnabled);

	/** Null when the world has none (dedicated server, disabled, not a game world) */
	static UGSTN_DepthMarkerSubsystem* Get(const UWorld* World);

	/** True for an object type the settings never target (pawns, projectiles), or when excluded is empty and the type is Pawn */
	static bool IsExcludedObjectType(ECollisionChannel ObjectType, const TArray<TEnumAsByte<ECollisionChannel>>& Excluded);

	/**
	 * One trace hit reduced to a candidate (§2.3 step 4): blocking, start penetration, moving base and exclusion by object type.
	 * Non-blocking hits (triggers, volumes) and excluded types come back flagged, never as a target. False for a hit with no component.
	 */
	static bool MakeCandidate(const FHitResult& Hit, const FGSTN_DepthSubjectState& Subject,
		const TArray<TEnumAsByte<ECollisionChannel>>& Excluded, FGSTN_DepthCandidate& OutCandidate);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void RegisterMarker(UGSTN_DepthMarkerComponent* Component);
	void UnregisterMarker(UGSTN_DepthMarkerComponent* Component);
	int32 GetNumMarkers() const { return Markers.Num(); }

	/** Designer projection planes for the approach marker (volumes and components register themselves) */
	void RegisterPlane(UGSTN_DepthMarkerPlaneComponent* Plane);
	void UnregisterPlane(UGSTN_DepthMarkerPlaneComponent* Plane);
	int32 GetNumPlanes() const { return Planes.Num(); }

	/** Markers the last tick drew (after the cap) and traces it issued, for the tests and stat displays */
	int32 GetLastActiveCount() const { return LastActiveCount; }
	int32 GetLastTraceCount() const { return LastTraceCount; }

	/**
	 * A local look on top of every other source, per machine (a camera section wants a stronger line). Source is an opaque key;
	 * the highest Priority wins. Presentation only: nothing replicates.
	 */
	void PushStyleOverride(const UObject* Source, UGSTN_DepthMarkerStyle* Style, int32 Priority);
	void PopStyleOverride(const UObject* Source);

	/** One frame of work; called from the post actor tick delegate, public for the tests */
	void TickMarkers(float DeltaSeconds);

	/** The built-in look (made once), used when nothing names a style */
	UGSTN_DepthMarkerStyle* GetBuiltinStyle();

	/** The built-in approach look (made once): a ring and a dashed line in another colour */
	UGSTN_DepthMarkerStyle* GetBuiltinApproachStyle();

private:
	struct FMarker;

	void OnPostActorTick(UWorld* TickedWorld, ELevelTick TickType, float DeltaSeconds);
	void OnProbeDone(const FTraceHandle& Handle, FTraceDatum& Datum);

	bool ReadSubject(const UGSTN_DepthMarkerComponent& Component, FGSTN_DepthSubjectState& Out) const;
	UGSTN_DepthMarkerStyle* ResolveStyle(const UGSTN_DepthMarkerComponent& Component);
	UGSTN_DepthTargetSelector* ResolveSelector(const UGSTN_DepthMarkerComponent& Component);
	UGSTN_DepthMarkerStyle* ResolveApproachStyle(const UGSTN_DepthMarkerComponent& Component, const UGSTN_DepthMarkerStyle* PrimaryStyle);
	void BindApproachPresenters(FMarker& Marker, UGSTN_DepthMarkerStyle* Style);
	void IssueApproachProbes(FMarker& Marker, int32 Rank, const FGSTN_DepthSchedulerConfig& Config, const TArray<AActor*>& Ignored, double Time,
		int32& InOutRemaining);
	void BuildApproachCandidates(FMarker& Marker, double Time, TArray<FGSTN_DepthPlaneCandidate>& OutProbed, TArray<FGSTN_DepthPlaneCandidate>& OutExplicit);
	void BindPresenters(FMarker& Marker, UGSTN_DepthMarkerStyle* Style);
	void ReleasePresenters(FMarker& Marker);
	void IssueProbe(FMarker& Marker, const TArray<AActor*>& Ignored, double Time);
	void IssueFootprint(FMarker& Marker, const TArray<AActor*>& Ignored, double Time);
	void IssueClearance(FMarker& Marker, const TArray<AActor*>& Ignored, double Time);
	void FinishHits(FMarker& Marker, const TArray<FHitResult>& Hits, double Time);
	void RefreshMarker(FMarker& Marker, double Time, bool bAllowed);
	void DriveMarker(FMarker& Marker, double Time, bool bAllowed);
	AActor* GetHolder();

	/** Plain data per marker; the subsystem owns them (TSharedPtr: stable address for the async callbacks) */
	TMap<TWeakObjectPtr<UGSTN_DepthMarkerComponent>, TSharedPtr<FMarker>> Markers;
	TMap<int32, TWeakPtr<FMarker>> MarkersById;
	int32 NextMarkerId = 1;

	struct FStyleOverride
	{
		TWeakObjectPtr<const UObject> Source;
		TStrongObjectPtr<UGSTN_DepthMarkerStyle> Style;
		int32 Priority = 0;
	};
	TArray<FStyleOverride> StyleOverrides;

	TArray<TWeakObjectPtr<UGSTN_DepthMarkerPlaneComponent>> Planes;

	TStrongObjectPtr<UGSTN_DepthMarkerStyle> BuiltinStyle;
	TStrongObjectPtr<UGSTN_DepthMarkerStyle> BuiltinApproachStyle;
	TStrongObjectPtr<UGSTN_DepthTargetSelector> BuiltinSelector;
	TStrongObjectPtr<AActor> Holder;

	FTraceDelegate ProbeDelegate;
	FDelegateHandle PostTickHandle;
	int32 LastActiveCount = 0;
	int32 LastTraceCount = 0;
};

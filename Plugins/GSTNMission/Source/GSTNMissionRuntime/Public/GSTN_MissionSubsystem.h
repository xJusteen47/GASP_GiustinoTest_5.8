// Missions, server side.

#pragma once

#include "CoreMinimal.h"
#include "Events/GSTN_SimEventSubsystem.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Info.h"
#include "GSTN_MissionTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_MissionSubsystem.generated.h"

class UBoxComponent;
class USphereComponent;

/** A mission's replicated state (game-modes 4.3: global, outside the rollback simulation, a few bytes per objective) */
USTRUCT(BlueprintType)
struct GSTNMISSIONRUNTIME_API FGSTN_MissionReplicated
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FGameplayTag MissionTag;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	TObjectPtr<const UGSTN_MissionDefinition> Definition;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	EGSTN_MissionState State = EGSTN_MissionState::NotStarted;

	/** Server world time of the mission's time limit, negative = none */
	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	float MissionEnd = -1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	TArray<FGSTN_ObjectiveState> Objectives;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSTN_OnMissionStateChanged);

/**
 * The missions every machine sees, for UI and markers. Always relevant; written by the server's mission subsystem only. A plain replicated array:
 * at most a handful of missions of at most 32 objectives each, changing a few times per second at most (a fast array is not worth it at this size).
 */
UCLASS(NotPlaceable, Transient)
class GSTNMISSIONRUNTIME_API AGSTN_MissionStateInfo : public AInfo
{
	GENERATED_BODY()

public:
	AGSTN_MissionStateInfo();

	UFUNCTION(BlueprintPure, Category = "Mission", meta = (WorldContext = "WorldContextObject"))
	static AGSTN_MissionStateInfo* GetMissionState(const UObject* WorldContextObject);

	UPROPERTY(ReplicatedUsing = OnRep_Missions, BlueprintReadOnly, Category = "Mission")
	TArray<FGSTN_MissionReplicated> Missions;

	UPROPERTY(BlueprintAssignable, Category = "Mission")
	FGSTN_OnMissionStateChanged OnStateChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Missions();
};

/**
 * An objective zone (game-modes 4.2 AGSTN_ObjectiveZone): Reach objectives with the same tag count the alive players inside. Server-side positions
 * polled by the mission subsystem; no collision, no overlap events, nothing in the simulation.
 */
UCLASS(Blueprintable, meta = (DisplayName = "GSTN Objective Zone"))
class GSTNMISSIONRUNTIME_API AGSTN_ObjectiveZone : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_ObjectiveZone();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
	TObjectPtr<UBoxComponent> Area;

	/** The identity Reach objectives name (Mission.Objective.<Name>, e.g. Mission.Objective.Goal) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission", meta = (Categories = "Mission.Objective"))
	FGameplayTag ZoneTag;

	UFUNCTION(BlueprintCallable, Category = "Mission")
	bool ContainsLocation(const FVector& Location) const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};

/**
 * A collectible (game-modes 5, Mario 3D World stars): taken once on the server by the first player within Radius, which publishes
 * Message.Mission.Collected with CollectibleTags; Count-events objectives count it. v1 counts per level, no save (GM.5, GM.3 open).
 */
UCLASS(Blueprintable, meta = (DisplayName = "GSTN Collectible"))
class GSTNMISSIONRUNTIME_API AGSTN_Collectible : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_Collectible();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
	TObjectPtr<USphereComponent> Area;

	/** Added to the Collected event (e.g. Mission.Objective.Star): an objective's query picks which collectibles count */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FGameplayTagContainer CollectibleTags;

	/** Worth this much to an objective with bUseMagnitude */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission", meta = (ClampMin = "1"))
	int32 Value = 1;

	/** Comes back when the missions reset (level restart) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	bool bRestoreOnReset = true;

	UPROPERTY(ReplicatedUsing = OnRep_Taken, BlueprintReadOnly, Category = "Mission")
	bool bTaken = false;

	/** Server: taken by Player */
	void Take(AActor* Player);

	/** Server: back in the level */
	void Restore();

	UFUNCTION(BlueprintPure, Category = "Mission")
	float GetRadius() const;

	/** Look of the take or the restore (every machine); the default hides the actor while taken */
	UFUNCTION(BlueprintNativeEvent, Category = "Mission")
	void OnTakenChanged(bool bIsTaken);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_Taken();
};

/**
 * The missions of a match (M12 G4; game-modes 4): the level's GameRules piece names them, the server runs one FGSTN_MissionTracker each, feeds it
 * every event of the authoritative stream (published once by the host, never from a re-simulation) and polls zones, collectibles and timers at
 * PollHz. Changes go out as Message.Mission.* on the same stream and as replicated state (AGSTN_MissionStateInfo). Nothing here touches the
 * simulation; a reward that must change simulated state goes through an existing channel (game-modes 2.5).
 */
UCLASS()
class GSTNMISSIONRUNTIME_API UGSTN_MissionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Mission", meta = (WorldContext = "WorldContextObject"))
	static UGSTN_MissionSubsystem* GetMissionSubsystem(const UObject* WorldContextObject);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Mission")
	bool IsRunning() const { return bRunning; }

	/** Server: every mission from zero (and collectibles with bRestoreOnReset back) */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	void ResetMissions();

	/** Server: start one more mission now (scripted flows, chains) */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	void StartMission(UGSTN_MissionDefinition* Definition);

	/** Server, debug: completes every active objective of every running mission */
	void CompleteActiveObjectives();

	FString Describe() const;

	void RegisterZone(AGSTN_ObjectiveZone* Zone) { Zones.AddUnique(Zone); }
	void UnregisterZone(AGSTN_ObjectiveZone* Zone) { Zones.Remove(Zone); }
	void RegisterCollectible(AGSTN_Collectible* Collectible) { Collectibles.AddUnique(Collectible); }
	void UnregisterCollectible(AGSTN_Collectible* Collectible) { Collectibles.Remove(Collectible); }

private:
	void Start();
	void Stop();
	void Poll();
	void HandleEvent(const FGSTN_SimEvent& Event);
	void DrainQueue();
	void Apply(int32 MissionIndex, const TArray<FGSTN_MissionChange>& Changes);
	void SyncState();
	bool HasAuthority() const;
	float Now() const;

	struct FRun
	{
		TObjectPtr<UGSTN_MissionDefinition> Definition;
		FGSTN_MissionTracker Tracker;
	};
	TArray<FRun> Runs;

	/** Keeps the definitions loaded (the trackers hold them weakly) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UGSTN_MissionDefinition>> Loaded;

	/** Events that arrive while one is being handled (our own Message.Mission.* re-enter): handled in order after it */
	TArray<FGSTN_SimEvent> Queue;
	bool bDraining = false;

	TArray<TWeakObjectPtr<AGSTN_ObjectiveZone>> Zones;
	TArray<TWeakObjectPtr<AGSTN_Collectible>> Collectibles;
	TWeakObjectPtr<AGSTN_MissionStateInfo> StateInfo;
	bool bRunning = false;
	bool bMergeSameFrame = true;
	FGameplayTagContainer ResetOn;
	FGameplayTagContainer DownOn;
	FGameplayTagContainer UpOn;
	/** Players knocked out and not back yet (DownEvents / UpEvents) */
	TSet<TWeakObjectPtr<AActor>> Downed;
	FTimerHandle PollTimer;
	FGSTN_SimEventListenerHandle Listener;
};

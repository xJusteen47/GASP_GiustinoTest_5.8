// Missions, pure core (no GAS, no Network Prediction, no world).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Events/GSTN_SimEvent.h"
#include "GameplayTagContainer.h"
#include "GameRules/GSTN_GameRules.h"
#include "NativeGameplayTags.h"
#include "GSTN_MissionTypes.generated.h"

/**
 * Tags of GSTNMission (game-modes 8.2). Message.Mission.* are published on the authoritative event stream (server only) for UI, audio, the
 * director and chained missions. Mission.Def.* and Mission.Objective.* are roots; designers add the leaves in ini (Config/Tags/<Mode>.ini).
 */
namespace GSTNMissionTags
{
	GSTNMISSIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mission_Def);
	GSTNMISSIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mission_Objective);
	GSTNMISSIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Mission_ObjectiveActivated);
	GSTNMISSIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Mission_ObjectiveProgressed);
	GSTNMISSIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Mission_ObjectiveCompleted);
	GSTNMISSIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Mission_ObjectiveFailed);
	GSTNMISSIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Mission_MissionCompleted);
	GSTNMISSIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Mission_MissionFailed);
	/** A collectible was taken (Instigator = the player, Tags = its identity) */
	GSTNMISSIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Mission_Collected);
}

UENUM(BlueprintType)
enum class EGSTN_ObjectiveState : uint8
{
	/** Waiting for its prerequisites */
	Locked,
	Active,
	Completed,
	Failed
};

UENUM(BlueprintType)
enum class EGSTN_MissionState : uint8
{
	NotStarted,
	Running,
	Completed,
	Failed
};

/** One objective's state, replicated to every machine for UI (game-modes 4.2: about 9 bytes of real content) */
USTRUCT(BlueprintType)
struct GSTNMISSIONCORE_API FGSTN_ObjectiveState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FGameplayTag ObjectiveTag;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	EGSTN_ObjectiveState State = EGSTN_ObjectiveState::Locked;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 Progress = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 Target = 1;

	/** Server world time when its timer ends (time limit or defend time), negative = none */
	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	float TimerEnd = -1.f;

	bool operator==(const FGSTN_ObjectiveState& Other) const
	{
		return ObjectiveTag == Other.ObjectiveTag && State == Other.State && Progress == Other.Progress && Target == Other.Target && TimerEnd == Other.TimerEnd;
	}
};

/** What the server sees when it polls the objectives (2-10 Hz, outside the simulation) */
USTRUCT(BlueprintType)
struct GSTNMISSIONCORE_API FGSTN_ObjectivePollContext
{
	GENERATED_BODY()

	/** Server world time */
	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	float Now = 0.f;

	/** Players with a pawn that is not knocked out */
	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 AlivePlayers = 0;

	/** Alive players inside each objective zone, by the zone's tag (server-side positions) */
	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	TMap<FGameplayTag, int32> PlayersInZone;
};

/**
 * How an objective progresses (game-modes 4.2). Instanced inside a mission definition and shared by every run of it, so it holds data only: the
 * run's state is the FGSTN_ObjectiveState it is given. A new kind is a Blueprint child overriding OnEvent and/or OnPoll; it runs on the server,
 * outside the simulation, so it may read the world (latent nodes are discouraged).
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class GSTNMISSIONCORE_API UGSTN_ObjectiveRule : public UObject
{
	GENERATED_BODY()

public:
	/** Seconds from activation until the timer ends; 0 = no timer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (ClampMin = "0", ForceUnits = "s"))
	float TimerSeconds = 0.f;

	/** At the end of the timer the objective completes (Defend: survive T seconds); off = it fails (a time limit) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	bool bTimerCompletes = false;

	/** An event that fails the objective while it is active (e.g. the KO of the defended target). Empty = nothing fails it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FGameplayTagQuery FailQuery;

	/** How much progress completes it */
	UFUNCTION(BlueprintNativeEvent, Category = "Objective")
	int32 GetTargetCount() const;

	/** Progress added by one authoritative event while active (0 = not this objective's event) */
	UFUNCTION(BlueprintNativeEvent, Category = "Objective")
	int32 OnEvent(const FGSTN_SimEvent& Event, const FGSTN_ObjectiveState& State) const;

	/** At each server poll while active: the new progress, or -1 to leave it */
	UFUNCTION(BlueprintNativeEvent, Category = "Objective")
	int32 OnPoll(const FGSTN_ObjectivePollContext& Context, const FGSTN_ObjectiveState& State) const;
};

/** Counts authoritative events matching a query: Eliminate (Combat.Event.KO + Minion.Type.X), Collect (Message.Mission.Collected + id), Activate (a verb's event key) */
UCLASS(meta = (DisplayName = "Count events (eliminate, collect, activate)"))
class GSTNMISSIONCORE_API UGSTN_ObjectiveRule_CountEvents : public UGSTN_ObjectiveRule
{
	GENERATED_BODY()

public:
	/** The events that count; matched against the event's type and tags (UGSTN_SimEventSubsystem rule). Empty counts nothing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FGameplayTagQuery EventQuery;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (ClampMin = "1"))
	int32 Count = 1;

	/** Each event adds its Magnitude (rounded, at least 1) instead of 1: e.g. a pickup worth 5 coins */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	bool bUseMagnitude = false;

	virtual int32 GetTargetCount_Implementation() const override { return FMath::Max(1, Count); }
	virtual int32 OnEvent_Implementation(const FGSTN_SimEvent& Event, const FGSTN_ObjectiveState& State) const override;
};

UENUM(BlueprintType)
enum class EGSTN_ReachRequire : uint8
{
	/** One player inside is enough */
	AnyPlayer,
	/** Every alive player inside (the Levels goal default, GM.5) */
	AllAlivePlayers
};

/** Players enter a zone (AGSTN_ObjectiveZone with this tag). The Levels goal is a Reach */
UCLASS(meta = (DisplayName = "Reach a zone"))
class GSTNMISSIONCORE_API UGSTN_ObjectiveRule_Reach : public UGSTN_ObjectiveRule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FGameplayTag ZoneTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	EGSTN_ReachRequire Require = EGSTN_ReachRequire::AllAlivePlayers;

	virtual int32 OnPoll_Implementation(const FGSTN_ObjectivePollContext& Context, const FGSTN_ObjectiveState& State) const override;
};

/** Survive a time: completes when its timer ends, fails on FailQuery (e.g. the defended target's KO) */
UCLASS(meta = (DisplayName = "Defend for a time"))
class GSTNMISSIONCORE_API UGSTN_ObjectiveRule_Defend : public UGSTN_ObjectiveRule
{
	GENERATED_BODY()

public:
	UGSTN_ObjectiveRule_Defend()
	{
		TimerSeconds = 60.f;
		bTimerCompletes = true;
	}
};

/** One objective of a mission */
USTRUCT(BlueprintType)
struct GSTNMISSIONCORE_API FGSTN_ObjectiveSpec
{
	GENERATED_BODY()

	/** Identity (Mission.Objective.<Name>, ini leaf); prerequisites name it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (Categories = "Mission.Objective"))
	FGameplayTag ObjectiveTag;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Objective")
	TObjectPtr<UGSTN_ObjectiveRule> Rule;

	/** Objectives of the same mission that must be completed first (graph order); with Sequential the previous required one is added */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (Categories = "Mission.Objective"))
	FGameplayTagContainer Prerequisites;

	/** Not needed to complete the mission, and its failure does not fail it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	bool bOptional = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FText DisplayText;
};

UENUM(BlueprintType)
enum class EGSTN_MissionOrder : uint8
{
	/** One required objective after the other; an optional one opens once the required one before it is completed */
	Sequential,
	/** All at once, except those whose Prerequisites are not completed yet (a graph) */
	Parallel
};

/** A mission (game-modes 4.2): data only, a designer makes one per level or per mission type */
UCLASS(BlueprintType)
class GSTNMISSIONCORE_API UGSTN_MissionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission", meta = (Categories = "Mission.Def"))
	FGameplayTag MissionTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TArray<FGSTN_ObjectiveSpec> Objectives;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	EGSTN_MissionOrder Order = EGSTN_MissionOrder::Sequential;

	/** An event that fails the whole mission (e.g. a key target's KO). Empty = none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FGameplayTagQuery FailQuery;

	/** Seconds to complete it, 0 = no limit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission", meta = (ClampMin = "0", ForceUnits = "s"))
	float TimeLimitSeconds = 0.f;

	/** A mission whose result does not end the level (a secondary, e.g. the collectibles) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	bool bOptional = false;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("GSTN_Mission"), GetFName()); }
};

/**
 * The missions' piece of the game rules (M12 G4): which missions a level runs and how. Chosen per level or mode by the GameRules asset, so it is
 * the same with a classic game mode and with Experiences. Class defaults = off (maps without an asset run no mission).
 */
UCLASS(config = GSTNMission, meta = (DisplayName = "Missions"))
class GSTNMISSIONCORE_API UGSTN_MissionRules : public UGSTN_GameRulesPiece
{
	GENERATED_BODY()

public:
	static const UGSTN_MissionRules& Get() { return FGSTN_ActiveGameRules::GetPiece<UGSTN_MissionRules>(); }

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Missions")
	bool bEnabled = false;

	/** The missions of this level, all started when the world begins play */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Missions")
	TArray<TSoftObjectPtr<UGSTN_MissionDefinition>> Missions;

	/** Events that restart every mission from zero (default: the respawn framework's level restart, Message.Respawn.RestartLevel) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Missions")
	FGameplayTagContainer ResetOnEvents;

	/**
	 * Events with the same type, target and frame are one fact (the credit partner's copy of a KO): count it once. Events without a frame are
	 * never merged
	 */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Missions")
	bool bMergeSameFrameEvents = true;

	/**
	 * Events whose Target is a player that is down (not alive for Reach "all alive players"), and events that bring the Target back. Tags only, so
	 * this plugin needs neither combat nor respawn. Defaults (ini): Combat.Event.KO and Message.Respawn.Respawned. A destroyed pawn no longer counts
	 */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Missions")
	FGameplayTagContainer DownEvents;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Missions")
	FGameplayTagContainer UpEvents;

	/** Server poll of zones, collectibles and timers, outside the simulation (game-modes 4.2: 2-10 Hz) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Missions", meta = (ClampMin = "1", ClampMax = "30", ForceUnits = "Hz"))
	float PollHz = 4.f;
};

/** One change the tracker made, for the runtime to publish and replicate. Objective INDEX_NONE = the mission itself */
struct FGSTN_MissionChange
{
	int32 Objective = INDEX_NONE;
	FGameplayTag EventType;
	int32 Progress = 0;
};

/**
 * One running mission: the rules of game-modes 4 as plain C++, so the runtime and the tests share them. Fed only with authoritative events (the
 * server's stream publishes each once, never a re-simulation) and with server polls. Holds the definition weakly: the owner keeps it loaded.
 */
class GSTNMISSIONCORE_API FGSTN_MissionTracker
{
public:
	void Start(const UGSTN_MissionDefinition* Definition, float Now, bool bMergeSameFrame, TArray<FGSTN_MissionChange>& OutChanges);
	void HandleEvent(const FGSTN_SimEvent& Event, float Now, TArray<FGSTN_MissionChange>& OutChanges);
	void Poll(const FGSTN_ObjectivePollContext& Context, TArray<FGSTN_MissionChange>& OutChanges);

	/** Debug and scripted flows: completes every active objective */
	void CompleteActive(float Now, TArray<FGSTN_MissionChange>& OutChanges);

	EGSTN_MissionState GetState() const { return State; }
	const TArray<FGSTN_ObjectiveState>& GetObjectives() const { return Objectives; }
	const UGSTN_MissionDefinition* GetDefinition() const { return Definition.Get(); }
	float GetMissionEnd() const { return MissionEnd; }

	/** "Title: state | obj progress/target state; ..." */
	FString Describe() const;

private:
	void SetObjectiveState(int32 Index, EGSTN_ObjectiveState NewState, float Now, TArray<FGSTN_MissionChange>& OutChanges);
	void UnlockReady(float Now, TArray<FGSTN_MissionChange>& OutChanges);
	void EvaluateMission(TArray<FGSTN_MissionChange>& OutChanges);
	void SetProgress(int32 Index, int32 NewProgress, float Now, TArray<FGSTN_MissionChange>& OutChanges);
	bool IsDuplicate(const FGSTN_SimEvent& Event);

	TWeakObjectPtr<const UGSTN_MissionDefinition> Definition;
	TArray<FGSTN_ObjectiveState> Objectives;
	EGSTN_MissionState State = EGSTN_MissionState::NotStarted;
	float MissionEnd = -1.f;
	bool bMergeSameFrame = true;

	struct FEventKey
	{
		FGameplayTag Type;
		const void* Target = nullptr;
		int32 Frame = INDEX_NONE;
	};
	TArray<FEventKey> RecentEvents;
};

namespace GSTNMissionLogic
{
	/** A non-empty query that matches the event (type + tags); an empty query never matches here (unlike the stream's listeners) */
	GSTNMISSIONCORE_API bool QueryMatches(const FGameplayTagQuery& Query, const FGSTN_SimEvent& Event);
}

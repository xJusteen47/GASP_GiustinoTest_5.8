// Combat abilities (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/BaseTaskData.h"
#include "GameplayTagContainer.h"
#include "Tasks/BasePredictionTask.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_CombatWaitTasks.generated.h"

class UGSTN_CombatSimComponent;
class UNpAbilitySystemComponent;

/** Who gets a command first when a Wait Command and a binding of the kit both want the same press */
UENUM(BlueprintType)
enum class EGSTN_WaitCommandPriority : uint8
{
	/**
	 * The wait takes it: while the node waits, the bindings of the command do not start their ability (the press stays in the buffer for the
	 * wait, as a locked command keeps it). Use it for a follow-up of the move or a stance that owns the button
	 */
	BeforeBindings,

	/** The bindings come first: the wait sees only the presses no binding used (a binding that fires takes the press, and the move it starts usually ends this one) */
	AfterBindings,
};

/** One step of Wait Command, decided by FGSTN_CombatWaitRules::StepWaitCommand */
enum class EGSTN_WaitCommandStep : uint8
{
	/** Nothing this frame */
	Keep,

	/** The command was entered: On Command fires (and the press is consumed, if asked) */
	Fire,

	/** The window closed with no command: On Window Expired fires */
	Expire,
};

/**
 * The rules of the wait nodes, split out of the tasks so they run in a test without a world or an ability system. Both count COMBAT frames
 * (UGSTN_CombatSimComponent::GetCombatClock): a hitstop frame does not count, so a wait stands still while the move does.
 */
struct GSTNCOMBATNP_API FGSTN_CombatWaitRules
{
	/** Wait Combat Frames: whether Frames combat frames have passed since StartClock. A wait of 0 or less lasts one frame, like the fork's Wait */
	static bool HasElapsed(int32 StartClock, int32 Clock, int32 Frames);

	/** Combat frames still to wait (0 once elapsed) */
	static int32 FramesLeft(int32 StartClock, int32 Clock, int32 Frames);

	/** Wait Command: whether the window is closed. WindowFrames <= 0 = no window (it waits until the ability ends or the node is cancelled) */
	static bool IsWindowOver(int32 StartClock, int32 Clock, int32 WindowFrames);

	/** Whether an input completed on TerminalFrame is not the one that fired the node last (INDEX_NONE = none fired yet) */
	static bool IsNewInput(int32 TerminalFrame, int32 LastFiredInputFrame);

	/**
	 * What a Wait Command does this frame. bCanTake: the combat state allows taking a command now (not in hitstop, not in a hit reaction, the
	 * command not locked, the condition holds); bEntered: its input was completed in the buffer by presses no command used. A command entered
	 * on the last frame of the window still fires: the window is [start, start + WindowFrames)
	 */
	static EGSTN_WaitCommandStep StepWaitCommand(int32 StartClock, int32 Clock, int32 WindowFrames, bool bCanTake, bool bEntered);
};

/** The sync data of Wait Combat Frames: rolled back with the task, so a re-simulation restores the wait */
USTRUCT()
struct GSTNCOMBATNP_API FGSTN_WaitCombatFramesTaskData : public FAbilityTaskDataBase
{
	GENERATED_BODY()

	virtual ~FGSTN_WaitCombatFramesTaskData() override {}

	UPROPERTY(Transient)
	int32 StartClock = 0;

	UPROPERTY(Transient)
	int32 Frames = 0;

	virtual bool NetSerialize(const FNetSerializeParams& Params) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual bool ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const override;
	virtual void Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct) override;
};

/** The sync data of Wait Command: everything its pins were given (as numbers: no tag goes into the sync state) and its start */
USTRUCT()
struct GSTNCOMBATNP_API FGSTN_WaitCommandTaskData : public FAbilityTaskDataBase
{
	GENERATED_BODY()

	virtual ~FGSTN_WaitCommandTaskData() override {}

	/** Combat clock at the start */
	UPROPERTY(Transient)
	int32 StartClock = 0;

	/** Simulation frame of the start: the tick of that frame does not look again (the start already did) */
	UPROPERTY(Transient)
	int32 StartFrame = INDEX_NONE;

	UPROPERTY(Transient)
	int32 WindowFrames = 0;

	/** Frame of the input that fired the node last (its terminal anchor), INDEX_NONE before the first */
	UPROPERTY(Transient)
	int32 LastFiredInputFrame = INDEX_NONE;

	/** Index in the command table, INDEX_NONE when the command is not in it */
	UPROPERTY(Transient)
	int16 CommandIndex = INDEX_NONE;

	/** -1 = the command's own buffer */
	UPROPERTY(Transient)
	int16 BufferFrames = -1;

	/** FGSTN_WaitCommandTaskData::Flag* */
	UPROPERTY(Transient)
	uint8 Flags = 0;

	/** EGSTN_CancelCondition */
	UPROPERTY(Transient)
	uint8 Condition = 0;

	static constexpr uint8 FlagConsume = 1u << 0;
	static constexpr uint8 FlagOnce = 1u << 1;
	static constexpr uint8 FlagBeforeBindings = 1u << 2;

	virtual bool NetSerialize(const FNetSerializeParams& Params) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual bool ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const override;
	virtual void Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct) override;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSTN_WaitCombatFramesDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSTN_WaitCommandDelegate, int32, FramesWaited);

/**
 * Wait Combat Frames (move-timeline-decoupling 3A): the combat counterpart of the engine's Wait Delay. Waits Frames combat frames (62.5 Hz;
 * hitstop frames do not count, so the wait stands still while the move does) and fires On Finish. A prediction task of the fork: one instance
 * per node, its start and length in the task's sync data, so a rollback restores the wait and a re-simulation fires it on the same frame on
 * every machine. Several can run at once (one per node). Works in any combat ability (a child of GSTN_GA_Base); the older function of the same
 * name on GSTN_GA_Base (one wait per ability, the On Combat Frames Elapsed event) keeps working
 */
UCLASS(BlueprintType, meta = (DisplayName = "Wait Combat Frames"))
class GSTNCOMBATNP_API UGSTN_WaitCombatFramesTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()

public:
	/** Frames combat frames have passed */
	UPROPERTY(BlueprintAssignable)
	FGSTN_WaitCombatFramesDelegate OnFinish;

	/** Waits Frames combat frames (at least 1). Calling the node again while it waits restarts it */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks|Combat", meta = (BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTask(int32 Frames = 1);

	/** Combat frames still to wait, 0 when the node is not waiting */
	UFUNCTION(BlueprintPure, Category = "Ability|Tasks|Combat")
	int32 GetFramesLeft() const;

	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;
	virtual void WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite) override;
	virtual void ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead) override;

protected:
	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

private:
	int32 StartClock = 0;
	int32 Frames = 0;
};

/**
 * Wait Command (move-timeline-decoupling 2A): the combat counterpart of the engine's Wait Input Press, on top of the input buffer instead of the
 * raw key. Waits until Command (a Command.* tag of the command table: a button, a motion, a chord, a double tap, matched with the command's own
 * rules) is entered by presses no command used yet, and fires On Command; On Window Expired if Window Frames pass first.
 *
 * - Buffer: a press made up to Buffer Frames before (the command's own buffer by default) counts, so a press just before the node starts or
 *   during a hitstop is not lost. Hitstop frames neither age a press nor count toward the window, and the node does not fire during one: it
 *   fires on the first frame after, as a buffered cancel does.
 * - Consume (default on): the press is used, so a binding cannot fire on it later and another Wait Command does not see it.
 * - Priority: Before Bindings (default) keeps the kit's bindings of Command from starting their ability while the node waits; After Bindings
 *   lets them have the press first. Either way the node never fires in a hit reaction and never on a locked command (Input.Lock.Command.*).
 * - Condition: Always, or only once the running (or last) timeline hit / whiffed / was blocked / parried, as a cancel window's condition.
 *
 * A prediction task of the fork: one instance per node, everything it was given in its sync data (the command as its index in the table), so a
 * rollback restores it. It reads the input buffer of the frame the ability system simulates and consumes inside the same simulation step, so
 * every machine and every re-simulation sees the same answer. Works in any combat ability (a child of GSTN_GA_Base) of a character with a
 * combat component.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Wait Command"))
class GSTNCOMBATNP_API UGSTN_WaitCommandTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()

public:
	/** The command was entered. FramesWaited: combat frames since the node started */
	UPROPERTY(BlueprintAssignable)
	FGSTN_WaitCommandDelegate OnCommand;

	/** Window Frames passed with no command */
	UPROPERTY(BlueprintAssignable)
	FGSTN_WaitCommandDelegate OnWindowExpired;

	/**
	 * Waits for Command.
	 * @param Command        A Command.* tag of the character's command table (Input.Button.* are buttons, not commands)
	 * @param WindowFrames   Combat frames the node waits; 0 = until the ability ends
	 * @param BufferFrames   How old a press may be; -1 = the command's own buffer
	 * @param bConsume       Use the press, so nothing else fires on it
	 * @param Priority       Before Bindings: the kit's bindings of Command do not start while the node waits
	 * @param Condition      Fire only when this holds for the running (or last) timeline, as a cancel window
	 * @param bOnlyTriggerOnce  End the node after the first command; off = keep waiting (the window still closes it)
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks|Combat", meta = (BlueprintInternalUseOnly = "TRUE", AdvancedDisplay = "BufferFrames,bConsume,Priority,Condition,bOnlyTriggerOnce"))
	void ExecuteTask(UPARAM(meta = (Categories = "Command")) FGameplayTag Command, int32 WindowFrames = 0, int32 BufferFrames = -1, bool bConsume = true,
		EGSTN_WaitCommandPriority Priority = EGSTN_WaitCommandPriority::BeforeBindings, EGSTN_CancelCondition Condition = EGSTN_CancelCondition::Always,
		bool bOnlyTriggerOnce = true);

	/** Whether this node, active, keeps the bindings of the command at CommandIndex from starting (Priority Before Bindings) */
	bool ClaimsCommand(int32 CommandIndex) const;

	/**
	 * Whether an active Wait Command of any running ability of AbilitySystem claims the command at CommandIndex. The combat tick asks it when a
	 * binding of the command could start: it reads the tasks as the previous frame left them (rolled back with the abilities), the same rule as
	 * the command lock tags
	 */
	static bool IsCommandClaimed(const UNpAbilitySystemComponent& AbilitySystem, int32 CommandIndex);

	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;
	virtual void WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite) override;
	virtual void ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead) override;

protected:
	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

private:
	/** Looks at the buffer this frame and fires a pin if it is time. Inside the simulation step only */
	void Step();

	UGSTN_CombatSimComponent* FindCombat() const;

	FGSTN_WaitCommandTaskData State;
};

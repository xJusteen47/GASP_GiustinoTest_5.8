// Combat abilities (GAS-NPP fork).

#include "Abilities/GSTN_CombatWaitTasks.h"

#include "Abilities/GSTN_GA_Base.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Abilities/NpGameplayAbility.h"
#include "GameFramework/Actor.h"
#include "Simulation/GSTN_CombatSimComponent.h"

#define LOCTEXT_NAMESPACE "GSTNCombatWaitTasks"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNWaitTasks, Log, All);

namespace GSTNWaitTasks
{
	/** The colour of the engine's latent ability task nodes (as Play Combat Timeline And Wait) */
	const FLinearColor TaskNodeColor(0.15f, 0.42f, 0.70f, 1.f);

	UGSTN_CombatSimComponent* FindCombat(const UBasePredictionTask& Task)
	{
		if (const UGSTN_GA_Base* Ability = Cast<UGSTN_GA_Base>(Task.GetOwningAbility()))
		{
			if (UGSTN_CombatSimComponent* Combat = Ability->GetCombatComponent())
			{
				return Combat;
			}
		}
		const AActor* Avatar = Task.GetAvatarActor();
		return Avatar ? Avatar->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	}

	/** The combat clock, 0 before the first simulated frame (as GSTN_GA_Base::WaitCombatFrames: a wait started then must still elapse) */
	int32 ClockOf(const UGSTN_CombatSimComponent* Combat)
	{
		return Combat ? FMath::Max(Combat->GetCombatClock(), 0) : 0;
	}

	void WriteInt(FArchive& Ar, int32& Value)
	{
		// Zigzag + packed: small positive and INDEX_NONE values stay one byte
		uint32 Packed = (static_cast<uint32>(Value) << 1) ^ static_cast<uint32>(Value >> 31);
		Ar.SerializeIntPacked(Packed);
		if (Ar.IsLoading())
		{
			Value = static_cast<int32>(Packed >> 1) ^ -static_cast<int32>(Packed & 1u);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Rules

bool FGSTN_CombatWaitRules::HasElapsed(int32 StartClock, int32 Clock, int32 Frames)
{
	return Clock - StartClock >= FMath::Max(Frames, 1);
}

int32 FGSTN_CombatWaitRules::FramesLeft(int32 StartClock, int32 Clock, int32 Frames)
{
	return FMath::Max(FMath::Max(Frames, 1) - (Clock - StartClock), 0);
}

bool FGSTN_CombatWaitRules::IsWindowOver(int32 StartClock, int32 Clock, int32 WindowFrames)
{
	return WindowFrames > 0 && Clock - StartClock >= WindowFrames;
}

bool FGSTN_CombatWaitRules::IsNewInput(int32 TerminalFrame, int32 LastFiredInputFrame)
{
	return LastFiredInputFrame == INDEX_NONE || TerminalFrame > LastFiredInputFrame;
}

EGSTN_WaitCommandStep FGSTN_CombatWaitRules::StepWaitCommand(int32 StartClock, int32 Clock, int32 WindowFrames, bool bCanTake, bool bEntered)
{
	if (IsWindowOver(StartClock, Clock, WindowFrames))
	{
		return EGSTN_WaitCommandStep::Expire;
	}
	return bCanTake && bEntered ? EGSTN_WaitCommandStep::Fire : EGSTN_WaitCommandStep::Keep;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Sync data

bool FGSTN_WaitCombatFramesTaskData::NetSerialize(const FNetSerializeParams& Params)
{
	GSTNWaitTasks::WriteInt(Params.Ar, StartClock);
	GSTNWaitTasks::WriteInt(Params.Ar, Frames);
	return true;
}

UScriptStruct* FGSTN_WaitCombatFramesTaskData::GetScriptStruct() const
{
	return FGSTN_WaitCombatFramesTaskData::StaticStruct();
}

void FGSTN_WaitCombatFramesTaskData::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("      Start clock : %d  Frames : %d\n", StartClock, Frames);
}

bool FGSTN_WaitCombatFramesTaskData::ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const
{
	const FGSTN_WaitCombatFramesTaskData& Auth = static_cast<const FGSTN_WaitCombatFramesTaskData&>(AuthorityState);
	return StartClock != Auth.StartClock || Frames != Auth.Frames;
}

void FGSTN_WaitCombatFramesTaskData::Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct)
{
	*this = static_cast<const FGSTN_WaitCombatFramesTaskData&>(To);
}

bool FGSTN_WaitCommandTaskData::NetSerialize(const FNetSerializeParams& Params)
{
	FArchive& Ar = Params.Ar;
	GSTNWaitTasks::WriteInt(Ar, StartClock);
	GSTNWaitTasks::WriteInt(Ar, StartFrame);
	GSTNWaitTasks::WriteInt(Ar, WindowFrames);
	GSTNWaitTasks::WriteInt(Ar, LastFiredInputFrame);
	int32 Command = CommandIndex;
	GSTNWaitTasks::WriteInt(Ar, Command);
	int32 Buffer = BufferFrames;
	GSTNWaitTasks::WriteInt(Ar, Buffer);
	if (Ar.IsLoading())
	{
		CommandIndex = static_cast<int16>(Command);
		BufferFrames = static_cast<int16>(Buffer);
	}
	Ar << Flags;
	Ar << Condition;
	return true;
}

UScriptStruct* FGSTN_WaitCommandTaskData::GetScriptStruct() const
{
	return FGSTN_WaitCommandTaskData::StaticStruct();
}

void FGSTN_WaitCommandTaskData::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("      Command : %d  Start clock : %d  Window : %d  Buffer : %d  Flags : %d  Condition : %d\n", CommandIndex, StartClock, WindowFrames,
		BufferFrames, Flags, Condition);
}

bool FGSTN_WaitCommandTaskData::ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const
{
	const FGSTN_WaitCommandTaskData& Auth = static_cast<const FGSTN_WaitCommandTaskData&>(AuthorityState);
	return StartClock != Auth.StartClock || StartFrame != Auth.StartFrame || LastFiredInputFrame != Auth.LastFiredInputFrame || WindowFrames != Auth.WindowFrames || CommandIndex != Auth.CommandIndex
		|| BufferFrames != Auth.BufferFrames || Flags != Auth.Flags || Condition != Auth.Condition;
}

void FGSTN_WaitCommandTaskData::Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct)
{
	*this = static_cast<const FGSTN_WaitCommandTaskData&>(To);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Wait Combat Frames

UGSTN_WaitCombatFramesTask::UGSTN_WaitCombatFramesTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DataType = FGSTN_WaitCombatFramesTaskData::StaticStruct();
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UGSTN_WaitCombatFramesTask, ExecuteTask);
	ShouldTaskTick = true;
}

void UGSTN_WaitCombatFramesTask::ExecuteTask(int32 InFrames)
{
	if (!GetOwningAbility() || IsInRollback())
	{
		return;
	}
	const UGSTN_CombatSimComponent* Combat = GSTNWaitTasks::FindCombat(*this);
	if (!Combat)
	{
		UE_LOG(LogGSTNWaitTasks, Warning, TEXT("%s: Wait Combat Frames needs a combat component on the avatar; it never finishes"), *GetNameSafe(GetOwningAbility()));
	}
	UE_CLOG(InFrames < 1, LogGSTNWaitTasks, Log, TEXT("%s: Wait Combat Frames(%d) waits one frame"), *GetNameSafe(GetOwningAbility()), InFrames);
	StartClock = GSTNWaitTasks::ClockOf(Combat);
	Frames = FMath::Max(InFrames, 1);
	ActivateTask();
}

int32 UGSTN_WaitCombatFramesTask::GetFramesLeft() const
{
	return IsActive() ? FGSTN_CombatWaitRules::FramesLeft(StartClock, GSTNWaitTasks::ClockOf(GSTNWaitTasks::FindCombat(*this)), Frames) : 0;
}

void UGSTN_WaitCombatFramesTask::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	const UGSTN_CombatSimComponent* Combat = GSTNWaitTasks::FindCombat(*this);
	if (!IsActive() || !Combat || !FGSTN_CombatWaitRules::HasElapsed(StartClock, GSTNWaitTasks::ClockOf(Combat), Frames))
	{
		return;
	}
	// Inactive before the pin runs: the pin may start this node again
	DeactivateTask(false);
	if (!IsInRollback())
	{
		OnFinish.Broadcast();
	}
}

void UGSTN_WaitCombatFramesTask::WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite)
{
	FGSTN_WaitCombatFramesTaskData* Data = static_cast<FGSTN_WaitCombatFramesTaskData*>(DataToWrite.Get());
	Data->StartClock = StartClock;
	Data->Frames = Frames;
}

void UGSTN_WaitCombatFramesTask::ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead)
{
	const FGSTN_WaitCombatFramesTaskData* Data = static_cast<const FGSTN_WaitCombatFramesTaskData*>(DataToRead.Get());
	StartClock = Data->StartClock;
	Frames = Data->Frames;
}

FText UGSTN_WaitCombatFramesTask::GetNodeTitle() const
{
	return LOCTEXT("WaitCombatFrames", "Wait Combat Frames");
}

FLinearColor UGSTN_WaitCombatFramesTask::GetNodeTitleColor() const
{
	return GSTNWaitTasks::TaskNodeColor;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Wait Command

UGSTN_WaitCommandTask::UGSTN_WaitCommandTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DataType = FGSTN_WaitCommandTaskData::StaticStruct();
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UGSTN_WaitCommandTask, ExecuteTask);
	ShouldTaskTick = true;
}

UGSTN_CombatSimComponent* UGSTN_WaitCommandTask::FindCombat() const
{
	return GSTNWaitTasks::FindCombat(*this);
}

void UGSTN_WaitCommandTask::ExecuteTask(FGameplayTag Command, int32 WindowFrames, int32 BufferFrames, bool bConsume, EGSTN_WaitCommandPriority Priority,
	EGSTN_CancelCondition Condition, bool bOnlyTriggerOnce)
{
	if (!GetOwningAbility() || IsInRollback())
	{
		return;
	}
	const UGSTN_CombatSimComponent* Combat = FindCombat();
	State = FGSTN_WaitCommandTaskData();
	State.CommandIndex = static_cast<int16>(Combat ? Combat->FindCommandIndex(Command) : INDEX_NONE);
	if (State.CommandIndex == INDEX_NONE)
	{
		// Logged, not refused: the window still closes, so a graph that waits for a command the kit lacks goes on through On Window Expired
		UE_LOG(LogGSTNWaitTasks, Warning, TEXT("%s: Wait Command for %s, which the command table of %s does not have (or no combat component): it never fires"),
			*GetNameSafe(GetOwningAbility()), *Command.ToString(), *GetNameSafe(GetAvatarActor()));
	}
	State.StartClock = GSTNWaitTasks::ClockOf(Combat);
	State.StartFrame = Combat ? Combat->GetSimulationFrame() : INDEX_NONE;
	State.WindowFrames = FMath::Max(WindowFrames, 0);
	State.BufferFrames = static_cast<int16>(FMath::Clamp(BufferFrames, -1, 255));
	State.Flags = (bConsume ? FGSTN_WaitCommandTaskData::FlagConsume : 0) | (bOnlyTriggerOnce ? FGSTN_WaitCommandTaskData::FlagOnce : 0)
		| (Priority == EGSTN_WaitCommandPriority::BeforeBindings ? FGSTN_WaitCommandTaskData::FlagBeforeBindings : 0);
	State.Condition = static_cast<uint8>(Condition);
	ActivateTask();
	// A press already in the buffer answers on this frame (a press buffered into the window, as into a cancel window)
	Step();
}

void UGSTN_WaitCommandTask::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	const UGSTN_CombatSimComponent* Combat = FindCombat();
	// The frame of the start looked already (ExecuteTask): a second look in the same frame could fire twice on a press it does not consume
	if (IsActive() && Combat && Combat->GetSimulationFrame() != State.StartFrame)
	{
		Step();
	}
}

void UGSTN_WaitCommandTask::Step()
{
	UGSTN_CombatSimComponent* Combat = FindCombat();
	if (!IsActive() || IsInRollback() || !Combat)
	{
		return;
	}
	const int32 Clock = GSTNWaitTasks::ClockOf(Combat);
	const bool bCanTake = State.CommandIndex != INDEX_NONE && Combat->CanTakeCommandForWait(State.CommandIndex, static_cast<EGSTN_CancelCondition>(State.Condition));
	// A press fires the node once: kept (Consume off) and the node still waiting (Only Trigger Once off), it stays in the buffer for a few
	// frames, so only an input completed after the last one that fired counts
	int32 TerminalFrame = INDEX_NONE;
	const bool bEntered = bCanTake && Combat->IsCommandEnteredForWait(State.CommandIndex, State.BufferFrames, &TerminalFrame)
		&& FGSTN_CombatWaitRules::IsNewInput(TerminalFrame, State.LastFiredInputFrame);
	const int32 Waited = Clock - State.StartClock;
	switch (FGSTN_CombatWaitRules::StepWaitCommand(State.StartClock, Clock, State.WindowFrames, bCanTake, bEntered))
	{
	case EGSTN_WaitCommandStep::Fire:
		if ((State.Flags & FGSTN_WaitCommandTaskData::FlagConsume) && !Combat->ConsumeCommandForWait(State.CommandIndex, State.BufferFrames))
		{
			// Nothing written (cannot happen inside the step with the input there): no pin, so no machine fires without consuming
			return;
		}
		State.LastFiredInputFrame = TerminalFrame;
		if (State.Flags & FGSTN_WaitCommandTaskData::FlagOnce)
		{
			// Inactive before the pin runs: the pin may start this node again
			DeactivateTask(false);
		}
		OnCommand.Broadcast(Waited);
		break;
	case EGSTN_WaitCommandStep::Expire:
		DeactivateTask(false);
		OnWindowExpired.Broadcast(Waited);
		break;
	default:
		break;
	}
}

bool UGSTN_WaitCommandTask::ClaimsCommand(int32 CommandIndex) const
{
	return IsActive() && CommandIndex != INDEX_NONE && State.CommandIndex == CommandIndex && (State.Flags & FGSTN_WaitCommandTaskData::FlagBeforeBindings) != 0;
}

bool UGSTN_WaitCommandTask::IsCommandClaimed(const UNpAbilitySystemComponent& AbilitySystem, int32 CommandIndex)
{
	if (CommandIndex == INDEX_NONE)
	{
		return false;
	}
	for (const FGameplayAbilitySpec& Spec : AbilitySystem.GetActivatableAbilities())
	{
		if (!Spec.IsActive())
		{
			continue;
		}
		// The instances the fork ticks (UNpAbilitySystemComponent::TickAbilities): replicated ones, and the server-only ones it keeps apart
		for (const TArray<TObjectPtr<UGameplayAbility>>* Instances : { &Spec.ReplicatedInstances, &Spec.NonReplicatedInstances })
		{
			for (const UGameplayAbility* Instance : *Instances)
			{
				const UNpGameplayAbility* Ability = Cast<UNpGameplayAbility>(Instance);
				if (!Ability || !Ability->IsActive())
				{
					continue;
				}
				for (const UBasePredictionTask* Task : Ability->PredictionTasksInstances)
				{
					const UGSTN_WaitCommandTask* Wait = Cast<UGSTN_WaitCommandTask>(Task);
					if (Wait && Wait->ClaimsCommand(CommandIndex))
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

void UGSTN_WaitCommandTask::WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite)
{
	*static_cast<FGSTN_WaitCommandTaskData*>(DataToWrite.Get()) = State;
}

void UGSTN_WaitCommandTask::ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead)
{
	State = *static_cast<const FGSTN_WaitCommandTaskData*>(DataToRead.Get());
}

FText UGSTN_WaitCommandTask::GetNodeTitle() const
{
	return LOCTEXT("WaitCommand", "Wait Command");
}

FLinearColor UGSTN_WaitCommandTask::GetNodeTitleColor() const
{
	return GSTNWaitTasks::TaskNodeColor;
}

#undef LOCTEXT_NAMESPACE

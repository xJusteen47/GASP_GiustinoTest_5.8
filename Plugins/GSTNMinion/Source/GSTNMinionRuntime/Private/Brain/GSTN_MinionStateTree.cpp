// Minion runtime (M8 G8): the StateTree schema, tasks and conditions of a minion brain.

#include "Brain/GSTN_MinionStateTree.h"

#include "StateTreeEvaluatorBase.h"
#include "StateTreePropertyFunctionBase.h"

bool UGSTN_MinionStateTreeSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	// The "common" bases are the StateTree convention for "safe in any schema" (comparisons, delays, ...); ours derive from them too
	return InScriptStruct
		&& (InScriptStruct->IsChildOf(FStateTreeConditionCommonBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FStateTreeTaskCommonBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FStateTreeEvaluatorCommonBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FStateTreePropertyFunctionCommonBase::StaticStruct()));
}

bool UGSTN_MinionStateTreeSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	return &InStruct == FGSTN_MinionBrainContext::StaticStruct();
}

// ---------------------------------------------------------------------------------------------------------------------------------

bool FGSTN_MoveToTargetTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(ContextHandle);
	return true;
}

EStateTreeRunStatus FGSTN_MoveToTargetTask::Run(FStateTreeExecutionContext& Context) const
{
	FGSTN_MinionBrainContext& Brain = Context.GetExternalData(ContextHandle);
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	FGSTN_MinionBrainActions::MoveToTarget(Brain, Data.Speed, Data.StopRadius, Data.SlowRadius, Data.SeparationRadius, Data.SeparationWeight);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FGSTN_MoveToTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	return Run(Context);
}

EStateTreeRunStatus FGSTN_MoveToTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	return Run(Context);
}

bool FGSTN_FaceTargetTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(ContextHandle);
	return true;
}

EStateTreeRunStatus FGSTN_FaceTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FGSTN_MinionBrainActions::FaceTarget(Context.GetExternalData(ContextHandle));
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FGSTN_FaceTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FGSTN_MinionBrainActions::FaceTarget(Context.GetExternalData(ContextHandle));
	return EStateTreeRunStatus::Running;
}

bool FGSTN_RequestAttackTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(ContextHandle);
	return true;
}

EStateTreeRunStatus FGSTN_RequestAttackTask::Run(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	FGSTN_MinionBrainActions::RequestAttack(Context.GetExternalData(ContextHandle), Data.Command);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FGSTN_RequestAttackTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	return Run(Context);
}

EStateTreeRunStatus FGSTN_RequestAttackTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	return Run(Context);
}

// ---------------------------------------------------------------------------------------------------------------------------------

bool FGSTN_TargetInRangeCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(ContextHandle);
	return true;
}

bool FGSTN_TargetInRangeCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	const bool bInRange = FGSTN_MinionBrainActions::IsTargetInRange(Context.GetExternalData(ContextHandle), Data.MinRange, Data.MaxRange);
	return bInRange != Data.bInvert;
}

bool FGSTN_IsLODAtLeastCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(ContextHandle);
	return true;
}

bool FGSTN_IsLODAtLeastCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	return FGSTN_MinionBrainActions::IsLODAtLeast(Context.GetExternalData(ContextHandle), Data.Tier);
}

GSTNMinion::FTestBrainTreeFactory& GSTNMinion::GetTestBrainTreeFactory()
{
	static FTestBrainTreeFactory Factory;
	return Factory;
}

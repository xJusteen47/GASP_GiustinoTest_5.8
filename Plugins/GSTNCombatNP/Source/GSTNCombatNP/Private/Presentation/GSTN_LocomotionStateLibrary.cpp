// Copyright Giustino. All Rights Reserved.

#include "Presentation/GSTN_LocomotionStateLibrary.h"

#include "Animation/AnimExecutionContext.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeReference.h"
#include "Animation/AnimNode_StateMachine.h"
#include "HAL/IConsoleManager.h"
#include "Presentation/GSTN_RunPresentationComponent.h"

const FName UGSTN_LocomotionStateLibrary::SetBlendStackFunctionName(TEXT("SetBlendStackAnimFromChooser"));

namespace GSTNLocomotionState
{
	static int32 LogEntries = 0;
	static FAutoConsoleVariableRef CVarLog(TEXT("gstn.LocoState.Log"), LogEntries,
		TEXT("1 = log every state entry routed to the Blend Stack (state, chooser key, chosen clip)"));

	static const FName RunStop(TEXT("Run Stop"));
	static const FName RunStopL(TEXT("Run Stop L"));
	static const FName RunStopR(TEXT("Run Stop R"));
	// Parameter names of the ABP's SetBlendStackAnimFromChooser (GASP)
	static const FName ParamStateName(TEXT("State Name"));
	static const FName ParamForceBlend(TEXT("ForceBlend"));
	static const FName ChosenAnimVar(TEXT("ChosenAnim"));
}

FName UGSTN_LocomotionStateLibrary::ChooserKeyForState(const UAnimInstance* AnimInstance, FName StateName)
{
	if (StateName == GSTNLocomotionState::RunStop)
	{
		return UGSTN_RunPresentationComponent::IsAnimOwnerRunStopLeftFoot(AnimInstance) ? GSTNLocomotionState::RunStopL : GSTNLocomotionState::RunStopR;
	}
	return StateName;
}

void UGSTN_LocomotionStateLibrary::SetBlendStackFromStateName(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	UE_CLOG(GSTNLocomotionState::LogEntries > 1, LogTemp, Log, TEXT("LocoState: entry function called"));
	// Base context: state entry can come from an update or from the machine's (re)initialization, and the wrapper only hands out the
	// typed context it was made from (GetContext() is null otherwise)
	const FAnimationBaseContext* UpdateContext = Context.GetBaseContext();
	if (!UpdateContext || !UpdateContext->AnimInstanceProxy)
	{
		UE_CLOG(GSTNLocomotionState::LogEntries, LogTemp, Warning, TEXT("LocoState: no update context"));
		return;
	}
	UAnimInstance* AnimInstance = Cast<UAnimInstance>(UpdateContext->AnimInstanceProxy->GetAnimInstanceObject());
	const FAnimNode_StateMachine* Machine = Node.GetAnimNodePtr<FAnimNode_StateMachine>();
	if (!AnimInstance || !Machine)
	{
		UE_CLOG(GSTNLocomotionState::LogEntries, LogTemp, Warning, TEXT("LocoState: anim instance %d, node is a state machine %d"),
			AnimInstance != nullptr, Machine != nullptr);
		return;
	}
	const FName StateName = Machine->GetStateInfo(Machine->GetCurrentState()).StateName;
	const FName Key = ChooserKeyForState(AnimInstance, StateName);

	UFunction* Function = AnimInstance->FindFunction(SetBlendStackFunctionName);
	if (!Function)
	{
		UE_CLOG(GSTNLocomotionState::LogEntries, LogTemp, Warning, TEXT("LocoState: %s has no %s"), *GetNameSafe(AnimInstance->GetClass()),
			*SetBlendStackFunctionName.ToString());
		return;
	}
	// Same call as a GASP state's OnStateEntry_ function: parameters by name, the enum left at its default (unused by the function)
	uint8* Params = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
	FMemory::Memzero(Params, Function->ParmsSize);
	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->InitializeValue_InContainer(Params);
	}
	if (const FNameProperty* NameParam = FindFProperty<FNameProperty>(Function, GSTNLocomotionState::ParamStateName))
	{
		NameParam->SetPropertyValue_InContainer(Params, Key);
	}
	if (const FBoolProperty* ForceParam = FindFProperty<FBoolProperty>(Function, GSTNLocomotionState::ParamForceBlend))
	{
		ForceParam->SetPropertyValue_InContainer(Params, true);
	}
	AnimInstance->ProcessEvent(Function, Params);
	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(Params);
	}

	if (GSTNLocomotionState::LogEntries)
	{
		const FObjectProperty* Chosen = FindFProperty<FObjectProperty>(AnimInstance->GetClass(), GSTNLocomotionState::ChosenAnimVar);
		const UObject* Clip = Chosen ? Chosen->GetObjectPropertyValue_InContainer(AnimInstance) : nullptr;
		UE_LOG(LogTemp, Log, TEXT("LocoState enter %s f=%llu state=%s key=%s clip=%s"), *GetNameSafe(AnimInstance->GetOwningActor()), GFrameCounter,
			*StateName.ToString(), *Key.ToString(), *GetNameSafe(Clip));
	}
}

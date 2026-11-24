// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backends/MoverNetworkPredictionLiaison.h"
#include "NetworkPredictionModelDefRegistry.h"
#include "NetworkPredictionProxyInit.h"
#include "NetworkPredictionProxyWrite.h"
#include "GameFramework/Actor.h"
#include "InputContainerStruct.h"
#include "KinematicActorSimulation.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverLog.h"
#include "HAL/IConsoleManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverNetworkPredictionLiaison)


#define LOCTEXT_NAMESPACE "Mover"

// ----------------------------------------------------------------------------------------------------------
//	FMoverActorModelDef: the piece that ties everything together that we use to register with the NP system.
// ----------------------------------------------------------------------------------------------------------

class FMoverActorModelDef : public FNetworkPredictionModelDef
{
public:

	NP_MODEL_BODY();

	using Simulation = UMoverNetworkPredictionLiaisonComponent;
	using StateTypes = KinematicMoverStateTypes;
	using Driver = UMoverNetworkPredictionLiaisonComponent;

	static const TCHAR* GetName() { return TEXT("MoverActor"); }
	static constexpr int32 GetSortPriority() { return (int32)ENetworkPredictionSortPriority::PreKinematicMovers; }
};

NP_MODEL_REGISTER(FMoverActorModelDef);



UMoverNetworkPredictionLiaisonComponent::UMoverNetworkPredictionLiaisonComponent()
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bCanEverTick = true;

	bWantsInitializeComponent = true;
	bAutoActivate = true;
	SetIsReplicatedByDefault(true);

	SimulationClass = UKinematicActorSimulation::StaticClass();
}

void UMoverNetworkPredictionLiaisonComponent::ProduceInput(const int32 DeltaTimeMS, FMoverInputCmdContext* Cmd)
{
	check(MoverComp);
	MoverComp->ProduceInput(DeltaTimeMS, Cmd);
}

#if !UE_BUILD_SHIPPING
// GSTN (2026-11-23, neck hold desync diagnosis): name the part of the Mover state that asks for a rollback. Diagnostics only, off by default,
// never changes the verdict.
static TAutoConsoleVariable<int32> CVarGSTNLogMoverReconcile(TEXT("gstn.Mover.LogReconcile"), 0,
	TEXT("1: log which Mover sync/aux field triggers a reconcile (mode, sync struct with location delta and base, modifiers)."));

static void GSTN_LogMoverReconcile(const UMoverComponent* MoverComp, const FMoverSyncState& Predicted, const FMoverSyncState& Authority)
{
	const FString Owner = MoverComp && MoverComp->GetOwner() ? MoverComp->GetOwner()->GetName() : TEXT("?");
	const int32 Role = MoverComp && MoverComp->GetOwner() ? (int32)MoverComp->GetOwner()->GetLocalRole() : -1;
	if (Predicted.MovementMode != Authority.MovementMode)
	{
		UE_LOG(LogMover, Log, TEXT("GSTNMoverReconcile %s role=%d field=MovementMode local=%s auth=%s"), *Owner, Role,
			*Predicted.MovementMode.ToString(), *Authority.MovementMode.ToString());
	}
	const FMoverDataCollection& Local = Predicted.SyncStateCollection;
	const FMoverDataCollection& Auth = Authority.SyncStateCollection;
	if (Local.GetDataArray().Num() != Auth.GetDataArray().Num())
	{
		UE_LOG(LogMover, Log, TEXT("GSTNMoverReconcile %s role=%d field=SyncCollectionCount local=%d auth=%d"), *Owner, Role, Local.GetDataArray().Num(), Auth.GetDataArray().Num());
	}
	for (const TSharedPtr<FMoverDataStructBase>& Element : Local.GetDataArray())
	{
		if (!Element.IsValid())
		{
			continue;
		}
		const FMoverDataStructBase* Other = Auth.FindDataByType(Element->GetDataScriptStruct());
		if (!Other || !Element->ShouldReconcile(*Other))
		{
			if (!Other)
			{
				UE_LOG(LogMover, Log, TEXT("GSTNMoverReconcile %s role=%d field=%s missing on authority"), *Owner, Role, *Element->GetDataScriptStruct()->GetName());
			}
			continue;
		}
		if (Element->GetDataScriptStruct() == FMoverDefaultSyncState::StaticStruct())
		{
			const FMoverDefaultSyncState& L = static_cast<const FMoverDefaultSyncState&>(*Element);
			const FMoverDefaultSyncState& A = static_cast<const FMoverDefaultSyncState&>(*Other);
			const FVector LocalWorld = L.GetLocation_WorldSpace();
			const FVector AuthWorld = A.GetLocation_WorldSpace();
			UE_LOG(LogMover, Log, TEXT("GSTNMoverReconcile %s role=%d field=DefaultSyncState dist=%.2f delta=%s localBase=%s/%s authBase=%s/%s localVel=%s authVel=%s"),
				*Owner, Role, FVector::Dist(LocalWorld, AuthWorld), *(LocalWorld - AuthWorld).ToCompactString(),
				*GetNameSafe(L.GetMovementBase()), *L.GetMovementBaseBoneName().ToString(),
				*GetNameSafe(A.GetMovementBase()), *A.GetMovementBaseBoneName().ToString(),
				*L.GetVelocity_WorldSpace().ToCompactString(), *A.GetVelocity_WorldSpace().ToCompactString());
		}
		else
		{
			TAnsiStringBuilder<512> LocalText;
			TAnsiStringBuilder<512> AuthText;
			Element->ToString(LocalText);
			Other->ToString(AuthText);
			UE_LOG(LogMover, Log, TEXT("GSTNMoverReconcile %s role=%d field=%s local={%s} auth={%s}"), *Owner, Role, *Element->GetDataScriptStruct()->GetName(),
				*FString(LocalText.ToString()).Replace(TEXT("\n"), TEXT(" ")), *FString(AuthText.ToString()).Replace(TEXT("\n"), TEXT(" ")));
		}
	}
	if (Predicted.MovementModifiers.ShouldReconcile(Authority.MovementModifiers))
	{
		UE_LOG(LogMover, Log, TEXT("GSTNMoverReconcile %s role=%d field=MovementModifiers"), *Owner, Role);
	}
}
#endif

bool UMoverNetworkPredictionLiaisonComponent::ShouldReconcile(const FMoverSyncState* PredictedState, const FMoverSyncState* AuthorityState) const
{
	const bool bReconcile = PredictedState->ShouldReconcile(*AuthorityState);
#if !UE_BUILD_SHIPPING
	if (bReconcile && CVarGSTNLogMoverReconcile.GetValueOnAnyThread() != 0)
	{
		GSTN_LogMoverReconcile(MoverComp, *PredictedState, *AuthorityState);
	}
#endif
	return bReconcile;
}

bool UMoverNetworkPredictionLiaisonComponent::ShouldReconcile(const FMoverAuxStateContext* PredictedState, const FMoverAuxStateContext* AuthorityState) const
{
	const bool bReconcile = PredictedState->ShouldReconcile(*AuthorityState);
#if !UE_BUILD_SHIPPING
	if (bReconcile && CVarGSTNLogMoverReconcile.GetValueOnAnyThread() != 0)
	{
		UE_LOG(LogMover, Log, TEXT("GSTNMoverReconcile %s field=AuxState"), MoverComp && MoverComp->GetOwner() ? *MoverComp->GetOwner()->GetName() : TEXT("?"));
	}
#endif
	return bReconcile;
}

void UMoverNetworkPredictionLiaisonComponent::RestoreFrame(const FMoverSyncState* SyncState, const FMoverAuxStateContext* AuxState)
{
	check(MoverComp);
	check(Simulation);

	int32 NewBaseSimTimeMs = 0;
	int32 NextFrameNum = 0;

	switch (UNetworkPredictionWorldManager::ActiveInstance->PreferredDefaultTickingPolicy())
	{
		default:	// fall through
		case ENetworkPredictionTickingPolicy::Fixed:
		{
			const FFixedTickState& FixedTickState = UNetworkPredictionWorldManager::ActiveInstance->GetFixedTickState();
			FNetSimTimeStep TimeStep = FixedTickState.GetNextTimeStep();
			NewBaseSimTimeMs = TimeStep.TotalSimulationTime;
			NextFrameNum = TimeStep.Frame;
		}
		break; 

		case ENetworkPredictionTickingPolicy::Independent:
		{
			const FVariableTickState& VariableTickState = UNetworkPredictionWorldManager::ActiveInstance->GetVariableTickState();
			const FNetSimTimeStep NextVariableTimeStep = VariableTickState.GetNextTimeStep(VariableTickState.Frames[VariableTickState.ConfirmedFrame]);
			NewBaseSimTimeMs = NextVariableTimeStep.TotalSimulationTime;
			NextFrameNum = NextVariableTimeStep.Frame;

		}
		break;
	}

	FMoverTimeStep MoverTimeStep;

	MoverTimeStep.ServerFrame = NextFrameNum;
	MoverTimeStep.BaseSimTimeMs = NewBaseSimTimeMs;
	MoverTimeStep.StepMs = 0;

	Simulation->RollbackToPriorState(MoverTimeStep, *SyncState, *AuxState);

	MoverComp->OnSimulationRollback(SyncState, AuxState, MoverTimeStep);

}

void UMoverNetworkPredictionLiaisonComponent::FinalizeFrame(const FMoverSyncState* SyncState, const FMoverAuxStateContext* AuxState)
{
	ensureMsgf(IsInGameThread(), TEXT("%hs must only be called on the game thread"), __FUNCTION__);

	check(MoverComp);

	const FNetworkPredictionSettings NetworkPredictionSettings = UNetworkPredictionWorldManager::ActiveInstance->GetSettings();
	if (MoverComp->GetOwnerRole() == ROLE_SimulatedProxy && NetworkPredictionSettings.SimulatedProxyNetworkLOD == ENetworkLOD::Interpolated)
	{
		FMoverInputCmdContext InputCmd;
		MoverComp->TickInterpolatedSimProxy(MoverComp->GetLastTimeStep(), InputCmd, MoverComp, MoverComp->GetSyncState(), *SyncState, *AuxState);
	}
	
	MoverComp->FinalizeFrame(SyncState, AuxState);
}

void UMoverNetworkPredictionLiaisonComponent::FinalizeSmoothingFrame(const FMoverSyncState* SyncState, const FMoverAuxStateContext* AuxState)
{
	check(MoverComp);
	MoverComp->FinalizeSmoothingFrame(SyncState, AuxState);
}

void UMoverNetworkPredictionLiaisonComponent::InitializeSimulationState(FMoverSyncState* OutSync, FMoverAuxStateContext* OutAux)
{
	check(MoverComp);
	check(Simulation);
	StartingOutSync = OutSync;
	StartingOutAux = OutAux;

	Simulation->InitializeSimulationState(*StartingOutSync, *StartingOutAux);

	MoverComp->OnSimulationStateInitialized(*OutSync, *OutAux);

}

void UMoverNetworkPredictionLiaisonComponent::SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<KinematicMoverStateTypes>& SimInput, const TNetSimOutput<KinematicMoverStateTypes>& SimOutput)
{
	check(MoverComp);
	check(Simulation);

	if (!HasBegunPlay())	// If we're not ready to run yet, maintain the same simulation state
	{
		*SimOutput.Sync = *SimInput.Sync;
		*SimOutput.Aux.Get() = *SimInput.Aux;
		return;
	}

	FMoverSimContext SimContext;
	SimContext.Simulation = Simulation;
	SimContext.Blackboard = Simulation->GetRollbackBlackboardSimWrapper();
	SimContext.SimulationOwner = MoverComp;

	FMoverTickStartData StartData;
	FMoverTickEndData EndData;

	StartData.InputCmd  = *SimInput.Cmd;
	StartData.SyncState = *SimInput.Sync;
	StartData.AuxState  = *SimInput.Aux;

	// Ensure persistent SyncStates are present in the start state for a SimTick.
	// TODO: Consider moving this functionality to a common utility in the Simulation
	for (const FMoverDataPersistence& PersistentSyncEntry : MoverComp->PersistentSyncStateDataTypes)
	{
		StartData.SyncState.SyncStateCollection.FindOrAddDataByType(PersistentSyncEntry.RequiredType);
	}
	
	FMoverTimeStep MoverTimeStep;

	MoverTimeStep.ServerFrame	= TimeStep.Frame;
	MoverTimeStep.BaseSimTimeMs = TimeStep.TotalSimulationTime;
	MoverTimeStep.StepMs		= TimeStep.StepMS;
	// GAS-NPP fork: the forked Network Prediction plugin reports re-simulation steps, forward it to Mover.
	MoverTimeStep.bIsResimulating = TimeStep.IsResimulating;

	MoverComp->OnPreSimulate(MoverTimeStep, StartData);

	Simulation->DoSimulationTick(MoverTimeStep, StartData, OUT EndData);

	MoverComp->OnPostSimulate(MoverTimeStep, StartData, EndData);

	*SimOutput.Sync = EndData.SyncState;
    *SimOutput.Aux.Get() = EndData.AuxState;
}


double UMoverNetworkPredictionLiaisonComponent::GetCurrentSimTimeMs()
{
	return NetworkPredictionProxy.GetTotalSimTimeMS();
}

int32 UMoverNetworkPredictionLiaisonComponent::GetCurrentSimFrame()
{
	return NetworkPredictionProxy.GetPendingFrame();
}


bool UMoverNetworkPredictionLiaisonComponent::ReadPendingSyncState(OUT FMoverSyncState& OutSyncState)
{
	if (const FMoverSyncState* PendingSyncState = NetworkPredictionProxy.ReadSyncState<FMoverSyncState>(ENetworkPredictionStateRead::Simulation))
	{
		OutSyncState = *PendingSyncState;
		return true;
	}

	return false;
}

bool UMoverNetworkPredictionLiaisonComponent::WritePendingSyncState(const FMoverSyncState& SyncStateToWrite)
{
	bool bDidWriteSucceed = NetworkPredictionProxy.WriteSyncState<FMoverSyncState>([&SyncStateToWrite](FMoverSyncState& PendingSyncStateRef)
		{
			PendingSyncStateRef = SyncStateToWrite;
		}) != nullptr;

	return bDidWriteSucceed;
}


bool UMoverNetworkPredictionLiaisonComponent::ReadPresentationSyncState(OUT FMoverSyncState& OutSyncState)
{
	if (const FMoverSyncState* PendingSyncState = NetworkPredictionProxy.ReadSyncState<FMoverSyncState>(ENetworkPredictionStateRead::Presentation))
	{
		OutSyncState = *PendingSyncState;
		return true;
	}

	return false;
}


bool UMoverNetworkPredictionLiaisonComponent::WritePresentationSyncState(const FMoverSyncState& SyncStateToWrite)
{
	bool bDidWriteSucceed = NetworkPredictionProxy.WritePresentationSyncState<FMoverSyncState>([&SyncStateToWrite](FMoverSyncState& PresentationSyncStateRef)
		{
			PresentationSyncStateRef = SyncStateToWrite;
		}) != nullptr;

	return bDidWriteSucceed;
}


bool UMoverNetworkPredictionLiaisonComponent::ReadPrevPresentationSyncState(FMoverSyncState& OutSyncState)
{
	if (const FMoverSyncState* PrevPresentationSyncState = NetworkPredictionProxy.ReadPrevPresentationSyncState<FMoverSyncState>())
	{
		OutSyncState = *PrevPresentationSyncState;
		return true;
	}

	return false;
}


bool UMoverNetworkPredictionLiaisonComponent::WritePrevPresentationSyncState(const FMoverSyncState& SyncStateToWrite)
{
	bool bDidWriteSucceed = NetworkPredictionProxy.WritePrevPresentationSyncState<FMoverSyncState>([&SyncStateToWrite](FMoverSyncState& PresentationSyncStateRef)
		{
			PresentationSyncStateRef = SyncStateToWrite;
		}) != nullptr;

	return bDidWriteSucceed;
}


#if WITH_EDITOR
EDataValidationResult UMoverNetworkPredictionLiaisonComponent::ValidateData(FDataValidationContext& Context, const UMoverComponent& ValidationMoverComp) const
{
	if (const AActor* OwnerActor = ValidationMoverComp.GetOwner())
	{
		if (OwnerActor->IsReplicatingMovement())
		{
			Context.AddError(FText::Format(LOCTEXT("ConflictingReplicateMovementProperty", "The owning actor ({0}) has the ReplicateMovement property enabled. This will conflict with Network Prediction and cause poor quality movement. Please disable it."),
				FText::FromString(GetNameSafe(OwnerActor))));

			return EDataValidationResult::Invalid;
		}
	}

	return EDataValidationResult::Valid;
}
#endif // WITH_EDITOR

void UMoverNetworkPredictionLiaisonComponent::BeginPlay()
{
	Super::BeginPlay();

	if (StartingOutSync && StartingOutAux)
	{
		if (FMoverDefaultSyncState* StartingSyncState = StartingOutSync->SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>())
		{
			const FTransform UpdatedComponentTransform = MoverComp->GetUpdatedComponentTransform();
			// if our location has changed between initialization and begin play (ex: Actors sharing an exact start location and one gets "pushed" to make them fit) lets write the new location to avoid any disagreements
			if (!UpdatedComponentTransform.GetLocation().Equals(StartingSyncState->GetLocation_WorldSpace()))
			{
				StartingSyncState->SetTransforms_WorldSpace(UpdatedComponentTransform.GetLocation(),
													 UpdatedComponentTransform.GetRotation().Rotator(),
													 FVector::ZeroVector,
													 FVector::ZeroVector);	// no initial velocity
			}
		}
	}
}


void UMoverNetworkPredictionLiaisonComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Simulation)
	{
		Simulation->Uninit();
		Simulation = nullptr;
	}

	MoverComp = nullptr;

	Super::EndPlay(Reason);
}

void UMoverNetworkPredictionLiaisonComponent::InitializeComponent()
{
	SetupSimulation();

	Super::InitializeComponent();
}


void UMoverNetworkPredictionLiaisonComponent::UninitializeComponent()
{
	NetworkPredictionProxy.EndPlay();

	if (Simulation)
	{
		Simulation->Uninit();
		Simulation = nullptr;
	}

	Super::UninitializeComponent();
}


void UMoverNetworkPredictionLiaisonComponent::RegisterComponentTickFunctions(bool bRegister)
{
	Super::RegisterComponentTickFunctions(bRegister);
}

void UMoverNetworkPredictionLiaisonComponent::InitializeNetworkPredictionProxy()
{
	MoverComp = GetOwner()->FindComponentByClass<UMoverComponent>();


	if (ensureAlwaysMsgf(MoverComp, TEXT("UMoverNetworkPredictionLiaisonComponent on actor %s failed to find associated Mover component. This actor's movement will not be simulated. Verify its setup."), *GetNameSafe(GetOwner())))
	{
		NetworkPredictionProxy.Init<FMoverActorModelDef>(GetWorld(), GetReplicationProxies(), this, this);
	}
}

void UMoverNetworkPredictionLiaisonComponent::ResetSimulation()
{
	SetupSimulation();
}

void UMoverNetworkPredictionLiaisonComponent::SetupSimulation()
{
	MoverComp = GetOwner()->FindComponentByClass<UMoverComponent>();

	if (Simulation)
	{
		Simulation->Uninit();
		Simulation = nullptr;
	}

	if (ensureMsgf(MoverComp, TEXT("Use of UMoverNetworkPredictionLiaisonComponent requires a sibling UMoverComponent. No movement will occur.")))
	{
		Simulation = NewObject<UKinematicActorSimulation>(MoverComp, SimulationClass, *SimulationClass->GetName(), RF_Transient);

		UKinematicActorSimulation::FInitParams SimInitParams;

		if (MoverComp)
		{
			MoverComp->SetSimulation(*Simulation);

			SimInitParams.StartingMovementMode = MoverComp->StartingMovementMode;
			SimInitParams.World = GetWorld();
			SimInitParams.MoverComp = MoverComp;
			SimInitParams.UpdatedComp = MoverComp->GetUpdatedComponent();

			for (const TPair<FName, TObjectPtr<UBaseMovementMode>>& Element : MoverComp->MovementModes)
			{
				if (Element.Key != NAME_None && Element.Value.Get() != nullptr)
				{
					SimInitParams.ModesToRegister.Add(Element.Key, TWeakObjectPtr<UBaseMovementMode>(Element.Value.Get()));
				}
			}

			for (const TObjectPtr<UBaseMovementModeTransition>& Transition : MoverComp->Transitions)
			{
				if (Transition != nullptr)
				{
					SimInitParams.TransitionsToRegister.Add(TWeakObjectPtr<UBaseMovementModeTransition>(Transition.Get()));
				}
			}
		}

		Simulation->Init(SimInitParams);
	}

}


void UMoverNetworkPredictionLiaisonComponent::OnRegister()
{
	Super::OnRegister();
}


#undef LOCTEXT_NAMESPACE

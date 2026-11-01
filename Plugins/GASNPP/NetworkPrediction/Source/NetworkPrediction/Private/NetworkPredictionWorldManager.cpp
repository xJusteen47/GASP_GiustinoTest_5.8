// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkPredictionWorldManager.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "HAL/PlatformTime.h"
#include "PBDRigidsSolver.h"
#include "ChaosSolversModule.h"
#include "NetworkPredictionLagCompensation.h"
#include "NetworkPredictionReplicatedManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Services/NetworkPredictionService_Finalize.inl"
#include "Services/NetworkPredictionService_Input.inl"
#include "Services/NetworkPredictionService_Interpolate.inl"
#include "Services/NetworkPredictionService_Rollback.inl"
#include "Services/NetworkPredictionService_ServerRPC.inl"


NETSIM_DEVCVAR_SHIPCONST_INT(ToggleLagCompensationDebug, 0, "np.DrawLagCompensationDebug", "Toggle Lag Compensation Debug , 1 : Enabled , 0 : Disabled");

// Spike S-FP: measures what the re-simulation of rolled back frames costs, to size the extra cost of forward-predicting more entities.
namespace NetworkPredictionCVars
{
	NETSIM_DEVCVAR_SHIPCONST_INT(SpikeLogResimCost, 0, "np.Spike.LogResimCost", "Spike S-FP. 1: on clients, accumulate wall time and frame count of rollback re-simulations and log one SpikeResim line every 5 seconds. 0: off.");
	NETSIM_DEVCVAR_SHIPCONST_INT(SpikeLogInputSend, 0, "np.Spike.LogInputSend", "Diagnostics of the fixed tick input stream. 1: a client logs every engine frame whose input send is not the steady one (3+ fixed ticks in the frame, a backlog sent without redundant copies, frames held back by a connection that is not net ready); the server logs every fixed tick it is starved of a client's input. 0: off.");
}

#include UE_INLINE_GENERATED_CPP_BY_NAME(NetworkPredictionWorldManager)


UNetworkPredictionWorldManager* UNetworkPredictionWorldManager::ActiveInstance=nullptr;

// -----------------------------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------------------------

UNetworkPredictionWorldManager::UNetworkPredictionWorldManager()
{

}

void UNetworkPredictionWorldManager::Initialize(FSubsystemCollectionBase& Collection)
{
	UWorld* World = GetWorld();
	check(World);
	if (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game)
	{
		PreTickDispatchHandle = FWorldDelegates::OnWorldTickStart.AddUObject(this, &UNetworkPredictionWorldManager::OnWorldPreTick);
		PostTickDispatchHandle = World->OnPostTickDispatch().AddUObject(this, &UNetworkPredictionWorldManager::ReconcileSimulationsPostNetworkUpdate);
		PreWorldActorTickHandle = FWorldDelegates::OnWorldPreActorTick.AddUObject(this, &UNetworkPredictionWorldManager::BeginNewSimulationFrame);
		SyncNetworkPredictionSettings(GetDefault<UNetworkPredictionSettingsObject>());
	}
}

void UNetworkPredictionWorldManager::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (PreTickDispatchHandle.IsValid())
		{
			FWorldDelegates::OnWorldTickStart.Remove(PreTickDispatchHandle);
		}
		if (PostTickDispatchHandle.IsValid())
		{
			World->OnPostTickDispatch().Remove(PostTickDispatchHandle);
		}
		if (PreWorldActorTickHandle.IsValid())
		{
			FWorldDelegates::OnWorldPreActorTick.Remove(PreWorldActorTickHandle);
		}
		EnableLocalPlayerControllersTicking();
	}
}

void UNetworkPredictionWorldManager::SyncNetworkPredictionSettings(const UNetworkPredictionSettingsObject* SettingsObj)
{
	this->Settings = SettingsObj->Settings;
}

float UNetworkPredictionWorldManager::GetCurrentLagCompensationTimeMS(const AActor* Actor) const
{
	if (!Actor || !Actor->GetWorld())
	{
		return 0.f;
	}
	// Authority Always Uses Current Sim Time. Pending Frame is For Next Sim Tick So We Remove A Tick in Fixed Tick.
	// stand alone gets interp time so it will always early out when trying to rewind actors and return latest
	// when queering the history
	if (Actor->GetLocalRole() == ROLE_Authority || UKismetSystemLibrary::IsStandalone(Actor)) 
	{
		if (Settings.PreferredTickingPolicy == ENetworkPredictionTickingPolicy::Fixed)
		{
			return FixedTickState.GetTotalSimTimeMS() - FixedTickState.FixedStepMS;
		}
		return VariableTickState.GetNextTimeStep(VariableTickState.Frames[VariableTickState.PendingFrame]).TotalSimulationTime;
	}
	// SP and AP both use the interpolation time 
	if (Settings.PreferredTickingPolicy == ENetworkPredictionTickingPolicy::Fixed) 
	{
		return FixedTickState.Interpolation.InterpolatedTimeMS;
	}
	return VariableTickState.Interpolation.fTimeMS;
}

FNpLagCompensationData UNetworkPredictionWorldManager::GetActorDefaultStateAtTime(AActor* RequestingActor,
	AActor* TargetActor, const float TargetSimTimeMS)
{
	if (!RequestingActor || !TargetActor)
	{
		return FNpLagCompensationData();
	}
	FNpLagCompensationData ReturnData;
	TSharedPtr<FNpLagCompensationData> FoundData = GetActorStateAtTime(RequestingActor,TargetActor,TargetSimTimeMS);
	if (FoundData)
	{
		ReturnData = *FoundData;
		return ReturnData;
	}
	ReturnData.Location = TargetActor->GetActorTransform().GetLocation();
	ReturnData.Rotation = TargetActor->GetActorTransform().GetRotation();
	ReturnData.SimTimeMs = GetCurrentLagCompensationTimeMS(RequestingActor);
	ReturnData.CanRewindFurther = false;
	//ReturnData.CollisionExtent .. Todo Fix Me,
	return ReturnData;
}


void UNetworkPredictionWorldManager::OnInputReceived(const int32& Frame,const float& InterpolationTime,const TArray<FSimulationReplicatedInput>& Inputs
                                                     , UNetworkPredictionPlayerControllerComponent* RPCHandler)
{
	// make sure handler is in the input handlers array.
	if (!RPCHandler)
	{
		return;
	}
	RPCHandlers.AddUnique(RPCHandler);
	
	const bool ShouldEatCmd = RPCHandler->LastReceivedFrame >= Frame;
	if (!ShouldEatCmd)
	{
		for (TUniquePtr<IInputService>& Ptr : Services.FixedInputRemote.Array)
		{
			Ptr->OnFixedInputReceived(Frame,InterpolationTime,Inputs,RPCHandler,&FixedTickState);
		}
		RPCHandler->LastReceivedFrame = Frame;
	}
	//ToDo : Add for variable tick rate too.
}

void UNetworkPredictionWorldManager::OnReceivedAckedData(const FSerializedAckedFrames& AckedFrames,
	UNetworkPredictionPlayerControllerComponent* RPCHandler)
{
	check(AckedFrames.IDs.Num() == AckedFrames.AckedFrames.Num())
	if (!RPCHandler || !RPCHandler->GetNetConnection())
	{
		return;
	}
	FAckedFrames& ClientAckedFrames = FixedTickState.ServerAckedFrames.ConnectionsAckedFrames.FindOrAdd(RPCHandler->GetNetConnection());
	for (int32 i = 0 ; i < AckedFrames.IDs.Num() ; ++i)
	{
		uint32 ID = AckedFrames.IDs[i];
		uint32 AckedFrame = AckedFrames.AckedFrames[i];
		uint32& AddedFrame = ClientAckedFrames.IDsToAckedFrames.FindOrAdd(ID);
		AddedFrame = AckedFrame;
	}
}

void UNetworkPredictionWorldManager::RegisterRPCHandler(UNetworkPredictionPlayerControllerComponent* RPCHandler)
{
	RPCHandlers.AddUnique(RPCHandler);
}

void UNetworkPredictionWorldManager::UnRegisterRPCHandler(UNetworkPredictionPlayerControllerComponent* RPCHandler)
{
	RPCHandlers.Remove(RPCHandler);
}

void UNetworkPredictionWorldManager::SetTimeDilation(const FSimTimeDilation& TimeDilation)
{
	FixedTickState.TimeDilationState.TimeDilation = TimeDilation.GetTimeDilation();
}

// -----------------------------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------------------------

void UNetworkPredictionWorldManager::OnWorldPreTick(UWorld* InWorld, ELevelTick InLevelTick, float InDeltaSeconds)
{
	if (InWorld != GetWorld())
	{
		return;
	}

	UE_NP_TRACE_WORLD_FRAME_START(InWorld->GetGameInstance(), InDeltaSeconds);

	OnWorldPreTick_Internal(InDeltaSeconds, Settings.FixedTickFrameRate);

	// Instantiate replicated manager on server
	if (!ReplicatedManager && InWorld->GetNetMode() != NM_Client)
	{
		UClass* ReplicatedManagerClass = GetDefault<UNetworkPredictionSettingsObject>()->Settings.ReplicatedManagerClassOverride.Get();
		ReplicatedManager = ReplicatedManagerClass ? InWorld->SpawnActor<ANetworkPredictionReplicatedManager>(ReplicatedManagerClass) : InWorld->SpawnActor<ANetworkPredictionReplicatedManager>();
	}
}

void UNetworkPredictionWorldManager::OnWorldPreTick_Internal(float InDeltaSeconds, float InFixedFrameRate)
{
	// Update fixed tick rate, this can be changed via editor settings
	FixedTickState.FixedStepRealTimeMS = (1.f /  InFixedFrameRate) * 1000.f;
	FixedTickState.FixedStepMS = (int32)FixedTickState.FixedStepRealTimeMS;

	// *** Modified By Kai Smoothing Support *** //

	// Time Dilation : happens Only On Locally Controlled Client (Not On Listen Server Local Player)
	// This Is Calculated by the server based on its input buffer count and sent to local player inside TFixedTickReplicator_AP NetSend().
	//NOTE : Time Dilation is implemented the exact same way the physics simulation does time dilation. with bit more network bandwidth optimization
	FixedTickState.TimeDilationState.FixedStepDilatedTimeMS =  FixedTickState.FixedStepRealTimeMS;
	if (NetworkPredictionCVars::DisableTimeDilation() == 0)
	{
		FixedTickState.TimeDilationState.FixedStepDilatedTimeMS =  FixedTickState.FixedStepRealTimeMS * FixedTickState.TimeDilationState.TimeDilation;
	}
	// *** END Modified By Kai Smoothing Support *** //

	ActiveInstance = this;
}

void UNetworkPredictionWorldManager::ReconcileSimulationsPostNetworkUpdate()
{
	UWorld* World = GetWorld();
	if (World->GetNetMode() != NM_Client)
	{
		for (auto It = FixedTickState.ServerAckedFrames.ConnectionsAckedFrames.CreateIterator(); It; ++It)
		{
			if (!IsValid(It.Key()) || It.Key()->GetConnectionState() == USOCK_Closed)
			{
				It.RemoveCurrent();
			}
		}
		return;
	}

	EnableLocalPlayerControllersTicking();

	ReconcileSimulationsPostNetworkUpdate_Internal();
}

void UNetworkPredictionWorldManager::ReconcileSimulationsPostNetworkUpdate_Internal()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_NP_RECONCILE);
	TRACE_CPUPROFILER_EVENT_SCOPE(NetworkPrediction::Reconcile);

	ActiveInstance = this;
	bLockServices = true;

	// Trace Local->Server offset. We need to trace this so that we can flag reconciles that happened
	// due to this (usually caused by server being starved for input)
	const bool OffsetChanged = (FixedTickState.LastOffset != FixedTickState.Offset);
	UE_NP_TRACE_FIXED_TICK_OFFSET(FixedTickState.Offset, OffsetChanged);
	if (OffsetChanged)
	{
		UE_LOG(LogNetworkPrediction,Warning,TEXT("ReconcileFromInputOffset Old Offset %d, New Offset %d"),FixedTickState.LastOffset,FixedTickState.Offset)
	}
	FixedTickState.LastOffset = FixedTickState.Offset;

	// -------------------------------------------------------------------------
	//	Non-rollback reconcile services
	// -------------------------------------------------------------------------
	
	// Don't reconcile FixedTick interpolates until we've started interpolation
	// This makes the service's implementation easier if it can rely on a known
	// ToFrame while reconciling network updates
	if (FixedTickState.Interpolation.ToFrame != INDEX_NONE)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(NetworkPrediction::ReconcileFixedInterpolate);
		for (TUniquePtr<IFixedInterpolateService>& Ptr : Services.FixedInterpolate.Array)
		{
			Ptr->Reconcile(&FixedTickState);
		}
	}

	for (TUniquePtr<IIndependentInterpolateService>& Ptr : Services.IndependentInterpolate.Array)
	{
		Ptr->Reconcile(&VariableTickState);
	}
	
	// Does anyone need to rollback?
	TRACE_CPUPROFILER_EVENT_SCOPE(NetworkPrediction::ReconcileQueeryRollback);
	int32 RollbackFrame = INDEX_NONE;
	for (TUniquePtr<IFixedRollbackService>& Ptr : Services.FixedRollback.Array)
	{
		const int32 ReqFrame = Ptr->QueryRollback(&FixedTickState);
		if (ReqFrame != INDEX_NONE)
		{
			RollbackFrame = (RollbackFrame == INDEX_NONE ? ReqFrame : FMath::Min(RollbackFrame, ReqFrame));
		}
	}

	if (RollbackFrame != INDEX_NONE)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_NP_ROLLBACK);
		TRACE_CPUPROFILER_EVENT_SCOPE(NetworkPrediction::Rollback);

		if (RollbackFrame < FixedTickState.PendingFrame)
		{
			// Common case: rollback to previously ticked frame and resimulate			

			const int32 EndFrame = FixedTickState.PendingFrame;
			const int32 NumFrames = EndFrame - RollbackFrame;
			npEnsureSlow(NumFrames > 0);
			
			
			bool bFirstStep = true;

			// Spike S-FP: time the whole re-simulation loop (every subscribed model, so it includes the extra cost of each
			// forward-predicted proxy). Read once so the window stays consistent if the cvar flips mid-rollback.
			const bool bSpikeLogResim = NetworkPredictionCVars::SpikeLogResimCost() != 0;
			const double SpikeResimStartSeconds = bSpikeLogResim ? FPlatformTime::Seconds() : 0.0;

			// Do rollback as necessary
			for (int32 Frame=RollbackFrame; Frame < EndFrame; ++Frame)
			{
				FixedTickState.PendingFrame = Frame;
				FNetSimTimeStep Step = FixedTickState.GetNextTimeStep();
				FServiceTimeStep ServiceStep = FixedTickState.GetNextServiceTimeStep();
			
				const int32 ServerInputFrame = Frame + FixedTickState.Offset;
				UE_NP_TRACE_PUSH_TICK(Step.TotalSimulationTime, FixedTickState.FixedStepMS, Step.Frame);

				// Everyone must apply corrections and flush as necessary before anyone runs the next sim tick
				// bFirstStep will indicate that even if they don't have a correction, they need to rollback their historic state
				for (TUniquePtr<IFixedRollbackService>& Ptr : Services.FixedRollback.Array)
				{
					Ptr->PreStepRollback(Step, ServiceStep, FixedTickState.Offset, bFirstStep);
				}
				// Run Sim ticks
				for (TUniquePtr<IFixedRollbackService>& Ptr : Services.FixedRollback.Array)
				{
					Ptr->StepRollback(Step, ServiceStep);
				}
				
				bFirstStep = false;
			}
			FixedTickState.PendingFrame = EndFrame;

			if (bSpikeLogResim)
			{
				SpikeResimStats.TotalSeconds += FPlatformTime::Seconds() - SpikeResimStartSeconds;
				SpikeResimStats.Rollbacks++;
				SpikeResimStats.Frames += NumFrames;
			}
		}
		else if (RollbackFrame == FixedTickState.PendingFrame)
		{
			// Correction is at the PendingFrame (frame we haven't ticked yet)
			// For now, just do nothing. We are either in a really bad state of PL or are just starting up
			// As our input frames make the round trip, we'll get some slack and be doing corrections in the above code block
			// (Setting the correction data now most likely is still wrong and not worth the iteration time)
			
			UE_LOG(LogNetworkPrediction, Log, TEXT("RollbackFrame %d EQUAL PendingFrame %d... Offset: %d"), RollbackFrame, FixedTickState.PendingFrame, FixedTickState.Offset);
		}
		else if (RollbackFrame > FixedTickState.PendingFrame)
		{
			// Most likely we haven't had a confirmed frame yet so our local frame -> server mapping hasn't been set yet
			UE_LOG(LogNetworkPrediction, Log, TEXT("RollbackFrame %d AHEAD of PendingFrame %d... Offset: %d"), RollbackFrame, FixedTickState.PendingFrame, FixedTickState.Offset);
		}
	}

	// Spike S-FP: flushed here rather than inside the rollback block so a window without any rollback still logs a
	// "rollbacks=0" line, which is the baseline to compare against.
	if (NetworkPredictionCVars::SpikeLogResimCost() != 0)
	{
		const double SpikeNow = FPlatformTime::Seconds();
		if (SpikeResimStats.WindowStartSeconds == 0.0)
		{
			SpikeResimStats.WindowStartSeconds = SpikeNow;
		}
		else if (SpikeNow - SpikeResimStats.WindowStartSeconds >= 5.0)
		{
			const double TotalMs = SpikeResimStats.TotalSeconds * 1000.0;
			const double MsPerFrame = SpikeResimStats.Frames > 0 ? TotalMs / SpikeResimStats.Frames : 0.0;
			// The world is appended after the fixed part of the line so the line stays parseable by prefix; two PIE clients log from one process.
			UE_LOG(LogNetworkPrediction, Display, TEXT("SpikeResim: rollbacks=%d frames=%d totalMs=%.2f msPerFrame=%.3f world=%s"),
				SpikeResimStats.Rollbacks, SpikeResimStats.Frames, TotalMs, MsPerFrame, *GetNameSafe(GetWorld()));
			SpikeResimStats = FSpikeResimStats();
			SpikeResimStats.WindowStartSeconds = SpikeNow;
		}
	}

	// -------------------------------------------------------------------------
	//	Independent Tick rollback
	// -------------------------------------------------------------------------
	for (TUniquePtr<IIndependentRollbackService>& Ptr : Services.IndependentRollback.Array)
	{
		Ptr->Reconcile(&VariableTickState);
	}

	bLockServices = false;
	DeferredServiceConfigDelegate.Broadcast(this);
	DeferredServiceConfigDelegate.Clear();
}

void UNetworkPredictionWorldManager::BeginNewSimulationFrame(UWorld* InWorld, ELevelTick InLevelTick, float DeltaTimeSeconds)
{
	if (InWorld != GetWorld() || !InWorld->HasBegunPlay())
	{
		return;
	}
	TickLocalPlayerControllers(InLevelTick, DeltaTimeSeconds);
	BeginNewSimulationFrame_Internal(DeltaTimeSeconds);
}

void UNetworkPredictionWorldManager::BeginNewSimulationFrame_Internal(float DeltaTimeSeconds)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_NP_TICK);
	TRACE_CPUPROFILER_EVENT_SCOPE(NetworkPrediction::Tick);
	
	ActiveInstance = this;
	bLockServices = true;

	const float fEngineFrameDeltaTimeMS = DeltaTimeSeconds * 1000.f;

	// -------------------------------------------------------------------------
	//	Fixed Tick
	// -------------------------------------------------------------------------
	if (Services.FixedTick.Array.Num() > 0)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_NP_TICK_FIXED);
		TRACE_CPUPROFILER_EVENT_SCOPE(NetworkPrediction::FixedTick);

		FixedTickState.UnspentTimeMS += fEngineFrameDeltaTimeMS;
		const int32 FirstFrameThisUpdate = FixedTickState.PendingFrame;

		while ((FixedTickState.UnspentTimeMS + KINDA_SMALL_NUMBER) >= FixedTickState.TimeDilationState.FixedStepDilatedTimeMS)
		{
			//ToDo : Add SubStep Count here and get out if reaches max. don't allow 1 bad frame to freeze game
			
			FixedTickState.UnspentTimeMS -= FixedTickState.TimeDilationState.FixedStepDilatedTimeMS;
			if (FMath::IsNearlyZero(FixedTickState.UnspentTimeMS))
			{
				FixedTickState.UnspentTimeMS = 0.f;
			}

			FNetSimTimeStep Step = FixedTickState.GetNextTimeStep();
			FServiceTimeStep ServiceStep = FixedTickState.GetNextServiceTimeStep();

			
			const int32 ServerInputFrame = FixedTickState.PendingFrame + FixedTickState.Offset;
			// server that produces input doesn't interpolate, all entities tick for him so provide sim time as interp time
			const bool bIsServer = GetWorld()->GetNetMode() == NM_ListenServer || GetWorld()->GetNetMode() == NM_DedicatedServer;
			const float InterpTimeMs = bIsServer ? FixedTickState.GetTotalSimTimeMS() : FixedTickState.Interpolation.InterpolatedTimeMS;
			UE_NP_TRACE_PUSH_INPUT_FRAME(ServerInputFrame);
			if (Services.FixedInputRemote.Array.Num() > 0)
			{
				// Both counts must fit the input ring, which also holds the frame being consumed
				const int32 MaxBufferedInputs = NpFixedTickServerInputBufferSize - 1;
				const int32 DesiredBufferedInputs = FMath::Clamp(Settings.FixedTickDesiredBufferedInputCount, 0, MaxBufferedInputs);
				for (UNetworkPredictionPlayerControllerComponent*& InputHandler : RPCHandlers)
				{
					if (IsValid(InputHandler))
					{
						const int32 ConsumedBefore = InputHandler->LastConsumedFrame;
						InputHandler->AdvanceLastConsumedFrame(DesiredBufferedInputs, MaxBufferedInputs);
						if (NetworkPredictionCVars::SpikeLogInputSend() != 0 && ConsumedBefore != INDEX_NONE && InputHandler->LastConsumedFrame == ConsumedBefore)
						{
							UE_LOG(LogNetworkPrediction, Log, TEXT("NpInputSend server %s: starved at client frame %d (last received %d), engine frame %.1f ms"),
								*GetNameSafe(InputHandler->GetOwner()), ConsumedBefore, InputHandler->LastReceivedFrame, fEngineFrameDeltaTimeMS);
						}
					}
				}

				for (TUniquePtr<IInputService>& Ptr : Services.FixedInputRemote.Array)
				{
					Ptr->ProduceInput(FixedTickState.FixedStepMS,InterpTimeMs);
				}
			}
			for (TUniquePtr<IInputService>& Ptr : Services.FixedInputLocal.Array)
			{
				Ptr->ProduceInput(FixedTickState.FixedStepMS,InterpTimeMs);
			}

			UE_NP_TRACE_PUSH_TICK(Step.TotalSimulationTime, FixedTickState.FixedStepMS, Step.Frame);
			
			// Should we increment PendingFrame before or after the tick?
			// Before: sims that are spawned during Tick (of other sims) will not be ticked this frame.
			// So we want their seed state/cached pending frame to be set to the next pending frame, not this one.
			FixedTickState.PendingFrame++;

			for (TUniquePtr<ILocalTickService>& Ptr : Services.FixedTick.Array)
			{
				Ptr->Tick(Step, ServiceStep);
			}
			
			if (Settings.bEnableFixedTickSmoothing)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(NetworkPrediction::FixedSmoothing);
				for (TUniquePtr<IFixedSmoothingService>& Ptr : Services.FixedSmoothing.Array)
				{
					Ptr->UpdateSmoothing(ServiceStep, &FixedTickState);
				}
			}

			for (UNetworkPredictionLagCompensation* RegisteredComp : RegisteredLagCompComponents)
			{
				if (RegisteredComp->HasSimulation())
				{
					RegisteredComp->CaptureStateAndAddToHistory(ServiceStep.EndTotalSimulationTime);
				}
			}
		}

		// Once per engine frame, after all the fixed ticks it ran
		if (Services.FixedServerRPC.Array.Num() > 0)
		{
			SendFixedTickInputsToServer(FixedTickState.PendingFrame > FirstFrameThisUpdate, FixedTickState.PendingFrame - FirstFrameThisUpdate, fEngineFrameDeltaTimeMS);
		}
	}

	// -------------------------------------------------------------------------
	//	Local Independent Tick
	// -------------------------------------------------------------------------
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(NetworkPrediction::IndependentTick);

		// Update VariableTickState	
		constexpr int32 MinStepMS = 1;
		constexpr int32 MaxStepMS = 100;

		VariableTickState.UnspentTimeMS += fEngineFrameDeltaTimeMS;
		float fDeltaMS = FMath::FloorToFloat(VariableTickState.UnspentTimeMS);
		VariableTickState.UnspentTimeMS -= fDeltaMS;

		const int32 DeltaSimMS = FMath::Clamp((int32)fDeltaMS, MinStepMS, MaxStepMS);

		FVariableTickState::FFrame& PendingFrameData = VariableTickState.Frames[VariableTickState.PendingFrame];
		PendingFrameData.DeltaMS = DeltaSimMS;

		// Input
		UE_NP_TRACE_PUSH_INPUT_FRAME(VariableTickState.PendingFrame);
		for (TUniquePtr<IInputService>& Ptr : Services.IndependentLocalInput.Array)
		{
			Ptr->ProduceInput(DeltaSimMS,VariableTickState.Interpolation.fTimeMS);
		}

		// -------------------------------------------------------------------------
		// LocalTick
		// -------------------------------------------------------------------------

		FNetSimTimeStep Step = VariableTickState.GetNextTimeStep(PendingFrameData);
		FServiceTimeStep ServiceStep = VariableTickState.GetNextServiceTimeStep(PendingFrameData);
		UE_NP_TRACE_PUSH_TICK(Step.TotalSimulationTime, Step.StepMS, Step.Frame);

		for (TUniquePtr<ILocalTickService>& Ptr : Services.IndependentLocalTick.Array)
		{
			Ptr->Tick(Step, ServiceStep);
		}	

		// -------------------------------------------------------------------------
		//	Remote Independent Tick
		// -------------------------------------------------------------------------
		for (TUniquePtr<IRemoteIndependentTickService>& Ptr : Services.IndependentRemoteTick.Array)
		{
			Ptr->Tick(DeltaTimeSeconds, &VariableTickState);
		}

		// Increment local PendingFrame and set (next) pending frame's TotalMS
		const int32 EndTotalSimTimeMS = PendingFrameData.TotalMS + PendingFrameData.DeltaMS;
		VariableTickState.Frames[++VariableTickState.PendingFrame].TotalMS = EndTotalSimTimeMS;
	}
	
	// -------------------------------------------------------------------------
	// Interpolation
	// -------------------------------------------------------------------------
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(NetworkPrediction::Interpolation);

		if (Services.FixedInterpolate.Array.Num() > 0)
		{
			const int32 LatestRecvFrame = FMath::Max(FixedTickState.Interpolation.LatestRecvFrameAP, FixedTickState.Interpolation.LatestRecvFrameSP);
			if (LatestRecvFrame != INDEX_NONE)
			{
				// We want 100ms of buffered time. As long a actors replicate at >= 10hz, this is should be good
				// Its better to keep this simple with a single time rather than trying to coordinate lowest amount of buffered time
				// between all the registered instances in the different ModelDefs
				const int32 DesiredBufferedMS = Settings.FixedTickInterpolationBufferedMS;
				float InterpolateRate = 1.f;
				if (FixedTickState.Interpolation.ToFrame == INDEX_NONE)
				{
					const int32 NumBufferedFrames = LatestRecvFrame;
					const int32 BufferedMS = NumBufferedFrames * FixedTickState.FixedStepMS;

					//UE_LOG(LogTemp, Warning, TEXT("BufferedMS: %d Frames: %d (No ToFrame)"), BufferedMS, NumBufferedFrames);

					if (BufferedMS < DesiredBufferedMS)
					{
						// Not enough time buffered yet to start interpolating
						InterpolateRate = 0.f;
					}
					else
					{
						// Begin interpolation
						const int32 DesiredNumBufferedFrames = (DesiredBufferedMS / FixedTickState.FixedStepMS);
						FixedTickState.Interpolation.ToFrame = LatestRecvFrame - DesiredNumBufferedFrames;

						FixedTickState.Interpolation.PCT = 0.f;
						FixedTickState.Interpolation.AccumulatedTimeMS = 0.f;

						// We need to force a reconcile here since we supress the call until interpolation starts
						for (TUniquePtr<IFixedInterpolateService>& Ptr : Services.FixedInterpolate.Array)
						{
							Ptr->Reconcile(&FixedTickState);
						}
					}
				}
				else
				{
					const int32 NumBufferedFrames = LatestRecvFrame - FixedTickState.Interpolation.ToFrame;
					const int32 BufferedMS = NumBufferedFrames * FixedTickState.FixedStepMS;

					//UE_LOG(LogTemp, Warning, TEXT("BufferedMS: %d Frames: %d"), BufferedMS, NumBufferedFrames);

					if (NumBufferedFrames <= 0)
					{
						InterpolateRate = 0.f;
					}
				}

				int32 AdvanceFrames = 0;
				if (InterpolateRate > 0.f)
				{
					const float fScaledDeltaTimeMS = (InterpolateRate * fEngineFrameDeltaTimeMS);

					// If The Buffer Has more BufferedMS Than We Want , Interpolate to further frame by the amount of excess buffered MS
					//Eg. if we have 80ms buffered , in a 20ms tick simulation, and we want 60ms of buffered time, add an extra frame to target interpolation.
					// nothing we can do if we have too little time buffered. we will copy frame in the service.
					FixedTickState.Interpolation.AccumulatedTimeMS += fScaledDeltaTimeMS;
					AdvanceFrames = (int32)FixedTickState.Interpolation.AccumulatedTimeMS / FixedTickState.FixedStepRealTimeMS;
					
					const int32 NumBufferedFrames = LatestRecvFrame - (FixedTickState.Interpolation.ToFrame + AdvanceFrames);
					// Buffered time After Advancing.
					const int32 BufferedMS = ((LatestRecvFrame - AdvanceFrames) * FixedTickState.FixedStepMS) - FixedTickState.Interpolation.InterpolatedTimeMS;
					// Time Over the desired Buffered time after advancing
					const float ExcessTime = FMath::Max(0.f,BufferedMS - Settings.FixedTickInterpolationBufferedMS);
					// extra frames we should advance to bring buffered time lower , if it gets too big
					int32 ExtraFrames = 0;
					if(ExcessTime > FixedTickState.FixedStepRealTimeMS)
					{
						ExtraFrames = FMath::CeilToInt32(FMath::Max(ExcessTime / FixedTickState.FixedStepMS,0));
					}
					
					if (AdvanceFrames > 0)
					{
						FixedTickState.Interpolation.ToFrame += AdvanceFrames;
						// Add Extra Advance Frames to target interpolation frame
						FixedTickState.Interpolation.ToFrame += ExtraFrames;
						//Make Sure ToFrame doesn't go above latest received frame
						FixedTickState.Interpolation.ToFrame = FMath::Min(FixedTickState.Interpolation.ToFrame,LatestRecvFrame);
						FixedTickState.Interpolation.AccumulatedTimeMS -= (AdvanceFrames * FixedTickState.FixedStepRealTimeMS);
					}
					const float RawPCT = FixedTickState.Interpolation.AccumulatedTimeMS / (float)FixedTickState.FixedStepRealTimeMS;
					FixedTickState.Interpolation.PCT = FMath::Clamp<float>(RawPCT, 0.f, 1.f);
					npEnsureMsgf(FixedTickState.Interpolation.PCT >= 0.f && FixedTickState.Interpolation.PCT <= 1.f, TEXT("Interpolation PCT out of range. %f"), FixedTickState.Interpolation.PCT);

					const float PCTms = FixedTickState.Interpolation.PCT * (float)FixedTickState.FixedStepMS;
					FixedTickState.Interpolation.InterpolatedTimeMS = ((FixedTickState.Interpolation.ToFrame-1) * FixedTickState.FixedStepMS) + (int32)PCTms;
					//const float BufferedTimeMS = (LatestRecvFrame * FixedTickState.FixedStepMS) - FixedTickState.Interpolation.InterpolatedTimeMS;
					//UE_LOG(LogTemp,Error,TEXT("InterpTimeMs %d , SimTime %d , BufferedTime : %f"),FixedTickState.Interpolation.InterpolatedTimeMS
						//,FixedTickState.GetTotalSimTimeMS(),BufferedTimeMS);
					//UE_LOG(LogTemp, Warning, TEXT("[Interpolate] %s Interpolating ToFrame %d. PCT: %.2f. Buffered: %d"), *GetPathName(), FixedTickState.Interpolation.ToFrame, FixedTickState.Interpolation.PCT, FixedTickState.Interpolation.LatestRecvFrame - FixedTickState.Interpolation.ToFrame);

					{
						TRACE_CPUPROFILER_EVENT_SCOPE(NetworkPrediction::FinalizeFrame);
						for (TUniquePtr<IFixedInterpolateService>& Ptr : Services.FixedInterpolate.Array)
						{
							Ptr->FinalizeFrame(DeltaTimeSeconds, &FixedTickState);
						}
						for (UNetworkPredictionLagCompensation* RegisteredComp : RegisteredLagCompComponents)
						{
							if (!RegisteredComp->HasSimulation())
							{
								RegisteredComp->CaptureStateAndAddToHistory(FixedTickState.Interpolation.InterpolatedTimeMS);
							}
						}
					}
				}
			}
		}

		if (Services.IndependentInterpolate.Array.Num() > 0)
		{
			const int32 DesiredBufferedMS = Settings.IndependentTickInterpolationBufferedMS;
			const int32 MaxBufferedMS = Settings.IndependentTickInterpolationMaxBufferedMS;
		
			if (VariableTickState.Interpolation.LatestRecvTimeMS > DesiredBufferedMS)
			{
				float InterpolationRate = 1.f;

				const int32 BufferedMS = VariableTickState.Interpolation.LatestRecvTimeMS - (int32)VariableTickState.Interpolation.fTimeMS;
				if (BufferedMS > MaxBufferedMS)
				{
					UE_LOG(LogNetworkPrediction, Warning, TEXT("Independent Interpolation fell behind. BufferedMS: %d"), BufferedMS);
					VariableTickState.Interpolation.fTimeMS = (float)(VariableTickState.Interpolation.LatestRecvTimeMS - DesiredBufferedMS);
				}
				else if (BufferedMS <= 0)
				{
					UE_LOG(LogNetworkPrediction, Warning, TEXT("Independent Interpolation starved: %d"), BufferedMS);
					InterpolationRate = 0.f;
				}

				if (InterpolationRate > 0.f)
				{
					const float fScaledDeltaTimeMS = (InterpolationRate * fEngineFrameDeltaTimeMS);
					VariableTickState.Interpolation.fTimeMS += fScaledDeltaTimeMS;
				}

				{
					TRACE_CPUPROFILER_EVENT_SCOPE(NetworkPrediction::FinalizeFrame);
					for (TUniquePtr<IIndependentInterpolateService>& Ptr : Services.IndependentInterpolate.Array)
					{
						Ptr->FinalizeFrame(DeltaTimeSeconds, &VariableTickState);
					}
				}
			}
		}
	}


	//-------------------------------------------------------------------------------------------------------------
	// Handle newly spawned services right now, so that they can Finalize/SendRPCs on the very first frame of life
	//-------------------------------------------------------------------------------------------------------------

	bLockServices = false;
	DeferredServiceConfigDelegate.Broadcast(this);
	DeferredServiceConfigDelegate.Clear();

	// -------------------------------------------------------------------------
	//	Finalize
	// -------------------------------------------------------------------------
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(NetworkPrediction::FinalizeFrame);

		const int32 FixedTotalSimTimeMS = FixedTickState.GetTotalSimTimeMS();
		const int32 FixedServerFrame = FixedTickState.PendingFrame + FixedTickState.Offset;
		for (TUniquePtr<IFinalizeService>& Ptr : Services.FixedFinalize.Array)
		{
			Ptr->FinalizeFrame(DeltaTimeSeconds, FixedServerFrame, FixedTotalSimTimeMS, FixedTickState.FixedStepMS);
		}

		if (Settings.bEnableFixedTickSmoothing)
		{
			for (TUniquePtr<IFixedSmoothingService>& Ptr : Services.FixedSmoothing.Array)
			{
				Ptr->FinalizeSmoothingFrame(&FixedTickState);
			}
		}

		const int32 IndependentTotalSimTimeMS = VariableTickState.Frames[VariableTickState.PendingFrame].TotalMS;
		const int32 IndependentFrame = VariableTickState.PendingFrame;
		for (TUniquePtr<IFinalizeService>& Ptr : Services.IndependentLocalFinalize.Array)
		{
			Ptr->FinalizeFrame(DeltaTimeSeconds, IndependentFrame, IndependentTotalSimTimeMS, 0);
		}
	
		for (TUniquePtr<IRemoteFinalizeService>& Ptr : Services.IndependentRemoteFinalize.Array)
		{
			Ptr->FinalizeFrame(DeltaTimeSeconds);
		}
	}
	

	// -------------------------------------------------------------------------
	// Call server RPC (Independent)
	// -------------------------------------------------------------------------
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(NetworkPrediction::CallServerRPC);
		for (TUniquePtr<IServerRPCService>& Ptr : Services.ServerRPC.Array)
		{
			Ptr->CallServerRPC(DeltaTimeSeconds);
		}
	}
}

void UNetworkPredictionWorldManager::SendFixedTickInputsToServer(const bool bTickedThisFrame, const int32 NumTicksThisFrame, const float EngineFrameMS)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(NetworkPrediction::CallServerRPC);

	// PendingFrame doesn't have an input written yet, so don't send its contents
	const int32 EndFrame = FixedTickState.PendingFrame;
	const int32 NumInputToSend = FMath::Max(Settings.FixedTickInputSendCount, 1);
	// Older inputs are of no use: the server would skip them (AdvanceLastConsumedFrame), and the local frame buffer has
	// already overwritten some of them
	const int32 MaxBacklog = NpFixedTickServerInputBufferSize - 1;

	// Per handler: the oldest frame its redundant copies start at (EndFrame when it sends none). The copies cover the last
	// NumInputToSend frames AND every new frame of the previous send: a client whose engine frame runs several fixed ticks packs
	// several new frames in each packet, and a window of NumInputToSend frames would then re-send only part of a lost packet, which
	// the server can only fill with repeated inputs (a mismatch whenever the stick or a button changed in the missing frames).
	TArray<int32, TInlineAllocator<2>> CopyStartFrames;
	int32 StartFrame = EndFrame;
	for (UNetworkPredictionPlayerControllerComponent* InputHandler : RPCHandlers)
	{
		int32 CopyStart = EndFrame;
		if (IsValid(InputHandler))
		{
			int32& NextFrame = InputHandler->NextInputFrameToSend;
			NextFrame = NextFrame == INDEX_NONE ? FMath::Max(EndFrame - 1, 0) : FMath::Clamp(NextFrame, EndFrame - MaxBacklog, EndFrame);
			if (bTickedThisFrame && !InputHandler->bPreviousSendHeld)
			{
				CopyStart = EndFrame - NumInputToSend;
				if (InputHandler->PreviousSendFirstFrame != INDEX_NONE)
				{
					CopyStart = FMath::Min(CopyStart, InputHandler->PreviousSendFirstFrame);
				}
				CopyStart = FMath::Max3(CopyStart, EndFrame - MaxBacklog, 0);
			}
			StartFrame = FMath::Min3(StartFrame, NextFrame, CopyStart);
		}
		CopyStartFrames.Add(CopyStart);
	}
	StartFrame = FMath::Max(StartFrame, 0);

	// np.Spike.LogInputSend: per handler, what this engine frame's send looked like
	const bool bLogInputSend = NetworkPredictionCVars::SpikeLogInputSend() != 0;
	TArray<TPair<int32, int32>, TInlineAllocator<2>> LogBacklogAndHeld; // (backlog before the send, frames held back by a connection that is not net ready)
	TArray<int32, TInlineAllocator<2>> FirstNewFrames;
	TArray<bool, TInlineAllocator<2>> HeldThisSend;
	for (UNetworkPredictionPlayerControllerComponent* InputHandler : RPCHandlers)
	{
		LogBacklogAndHeld.Add(TPair<int32, int32>(IsValid(InputHandler) ? EndFrame - InputHandler->NextInputFrameToSend : 0, 0));
		FirstNewFrames.Add(INDEX_NONE);
		HeldThisSend.Add(false);
	}

	for (int32 Frame = StartFrame; Frame < EndFrame; ++Frame)
	{
		for (TUniquePtr<IFixedServerRPCService>& Ptr : Services.FixedServerRPC.Array)
		{
			Ptr->AddInputToHandler(Frame);
		}
		for (int32 Index = 0; Index < RPCHandlers.Num(); ++Index)
		{
			UNetworkPredictionPlayerControllerComponent* InputHandler = RPCHandlers[Index];
			if (!IsValid(InputHandler))
			{
				continue;
			}
			int32& NextFrame = InputHandler->NextInputFrameToSend;
			const bool bNewFrame = Frame == NextFrame;
			const bool bRedundantCopy = Frame < NextFrame && Frame >= CopyStartFrames[Index];
			const UNetConnection* NetConnection = InputHandler->GetNetConnection();
			if ((bNewFrame || bRedundantCopy) && NetConnection && NetConnection->IsNetReady())
			{
				InputHandler->SendServerRpc(Frame);
				if (bNewFrame)
				{
					if (FirstNewFrames[Index] == INDEX_NONE)
					{
						FirstNewFrames[Index] = Frame;
					}
					++NextFrame;
				}
			}
			else
			{
				if (bNewFrame || bRedundantCopy)
				{
					HeldThisSend[Index] = true;
					++LogBacklogAndHeld[Index].Value;
				}
				InputHandler->DiscardInputsToSend();
			}
		}
	}

	for (int32 Index = 0; Index < RPCHandlers.Num(); ++Index)
	{
		if (UNetworkPredictionPlayerControllerComponent* InputHandler = RPCHandlers[Index]; IsValid(InputHandler) && bTickedThisFrame)
		{
			InputHandler->bPreviousSendHeld = HeldThisSend[Index];
			if (FirstNewFrames[Index] != INDEX_NONE)
			{
				InputHandler->PreviousSendFirstFrame = FirstNewFrames[Index];
			}
		}
	}

	if (bLogInputSend)
	{
		for (int32 Index = 0; Index < RPCHandlers.Num() && Index < LogBacklogAndHeld.Num(); ++Index)
		{
			const int32 Backlog = LogBacklogAndHeld[Index].Key;
			const int32 Held = LogBacklogAndHeld[Index].Value;
			if (NumTicksThisFrame >= 3 || Backlog > NumInputToSend || Held > 0)
			{
				UE_LOG(LogNetworkPrediction, Log, TEXT("NpInputSend client %s: engine frame %.1f ms, %d fixed ticks, backlog %d, %d copies, %d held (not net ready), next frame to send %d"),
					*GetNameSafe(IsValid(RPCHandlers[Index]) ? RPCHandlers[Index]->GetOwner() : nullptr), EngineFrameMS, NumTicksThisFrame, Backlog,
					FMath::Max(0, (EndFrame - Backlog) - CopyStartFrames[Index]), Held,
					IsValid(RPCHandlers[Index]) ? RPCHandlers[Index]->NextInputFrameToSend : INDEX_NONE);
			}
		}
	}

	// we only need to send acked frames once
	if (FixedTickState.LocalAckedFrames.IDsToAckedFrames.Num() > 0)
	{
		for (UNetworkPredictionPlayerControllerComponent* InputHandler : RPCHandlers)
		{
			if (IsValid(InputHandler))
			{
				InputHandler->SendAckedFrames(FSerializedAckedFrames(FixedTickState.LocalAckedFrames));
			}
		}
		FixedTickState.LocalAckedFrames.IDsToAckedFrames.Reset();
	}
}


ENetworkPredictionTickingPolicy UNetworkPredictionWorldManager::PreferredDefaultTickingPolicy() const
{
	return Settings.PreferredTickingPolicy;
}

// Added to Tick Player Controllers before Simulation , Disable Their tick for that frame and then re-enable it when frame ends.
// this is to ensure if player controller changes worlds it will tick on its own fine.
void UNetworkPredictionWorldManager::TickLocalPlayerControllers(ELevelTick InLevelTick, float InDeltaSeconds) const
{
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = Iterator->Get();
		if (PC && PC->IsLocalPlayerController())
		{
			PC->TickActor(InDeltaSeconds,InLevelTick,PC->PrimaryActorTick);
			PC->PrimaryActorTick.SetTickFunctionEnable(false);
		}
	}
}

void UNetworkPredictionWorldManager::EnableLocalPlayerControllersTicking() const
{
	// Enable Tick Back on Local Player Controllers, in case we change worlds
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = Iterator->Get();
		if (PC && PC->IsLocalPlayerController())
		{
			PC->PrimaryActorTick.SetTickFunctionEnable(true);
		}
	}
}


// --------------------------------------- Lag Compensation ------------------------------//

void UNetworkPredictionWorldManager::RegisterRewindableComponent(UNetworkPredictionLagCompensation* RewindComp)
{
	if (RewindComp)
	{
		if (LagCompRegistrationLock > 0)
		{
			PendingAddLagCompComponents.AddUnique(RewindComp);
			return;
		}
		// we do this check so we don't initialize again, can use TSet or just AddUnique
		// but that doesn't tell me if it's newly added
		if (!RegisteredLagCompComponents.Contains(RewindComp))
		{
			int32 ActualMaxRewindTime = Settings.MaxRewindTimeMS + Settings.FixedTickInterpolationBufferedMS;
			const int32 TickTimeMS = FMath::Floor((1 / Settings.FixedTickFrameRate) * 1000);
			ActualMaxRewindTime += Settings.FixedTickDesiredBufferedInputCount * TickTimeMS;
			// 10 frames for safety
			ActualMaxRewindTime += TickTimeMS * 10.f;
			const int32 MaxSize = FMath::Max(ActualMaxRewindTime , Settings.MaxBufferedRewindHistoryTimeMS) / TickTimeMS;
			RewindComp->InitializeHistory(MaxSize);
			RegisteredLagCompComponents.Add(RewindComp);
		}
	}
}

void UNetworkPredictionWorldManager::UnregisterRewindableComponent(UNetworkPredictionLagCompensation* RewindComp)
{
	if (LagCompRegistrationLock > 0)
	{
		PendingRemoveLagCompComponents.AddUnique(RewindComp);
		return;
	}
	RegisteredLagCompComponents.Remove(RewindComp);
}

bool UNetworkPredictionWorldManager::RewindActors(AActor* RequestingActor, const float RewindSimTimeMS)
{
	if (!RequestingActor)
	{
		return false;
	}
	const float CurrentSimTimeMS = GetCurrentLagCompensationTimeMS(RequestingActor);
	if (UKismetSystemLibrary::IsStandalone(RequestingActor))
	{
		// This function can be called in simulation code without caring if it's server to client.
		// this would happen when local player is trying to rewind others, but he's at the current interpolation time.
		return false;
	}
	
	if (RegisteredLagCompComponents.Num() <= 0)
	{
		return false;
	}

	float FinalRewindTime = FMath::Clamp(RewindSimTimeMS,0.f,CurrentSimTimeMS);
	// if we are on server Clamp Rewind Time To A Maximum, client can rewind his own simulation to correct himself for as long as he likes.
	// Client Rewinds To make sure any targeting that happens during the simulation , has other actors in same place at the re-simulation time.
	if (RequestingActor->GetLocalRole() == ROLE_Authority)
	{
		FinalRewindTime = ClampRewindingTime(CurrentSimTimeMS,RewindSimTimeMS);
		if (FinalRewindTime > RewindSimTimeMS)
		{
			UE_LOG(LogNetworkPrediction,Warning,TEXT("Desired Lag Compensation Rewind Exceeded Supported ping for %s : Desired Time %f , FinalTime %f")
		,*GetNameSafe(RequestingActor),RewindSimTimeMS,FinalRewindTime);
		}
	}
	FLagCompensationRegistrationLock(this);
	bool DidRewind = false;
	for (UNetworkPredictionLagCompensation* RewindComp : RegisteredLagCompComponents)
	{
		if (!RewindComp || !RequestingActor)
		{
			continue;
		}
		const FNpLagCompensationHistory& History = RewindComp->GetLagCompensationHistory();
		if (RequestingActor == RewindComp->GetOwner() || History.Num() <= 0)
		{
			continue;
		}
		// draw debug on local client based on current state
		if (History.Last()->SimTimeMs == RewindSimTimeMS)
		{
#if WITH_EDITOR
			if (ToggleLagCompensationDebug() > 0)
			{
				const FColor Color = RewindComp->GetOwnerRole() == ROLE_Authority ? FColor::Red : FColor::Blue;
				const float SizeMultiplier = RewindComp->GetOwnerRole() == ROLE_Authority ? 1.02f : 1.f;
				DrawDebugBox(GetWorld(),History.Last()->Location,History.Last()->CollisionExtent
					,FQuat::Identity,FColor::Blue,false,5.f);
			}
#endif
		}
		// if we are already rewinding we already captured latest state
		if (History.bIsInRewind == false)
		{
			RewindComp->CapturePreRewindState();
		}

		
		TSharedPtr<FNpLagCompensationData> CurrentRewindData = GetRewindDataFromComponent(FinalRewindTime,RewindComp);
		if (CurrentRewindData == nullptr)
		{
			continue;
		}

		if (History.Last()->SimTimeMs != RewindSimTimeMS)
		{
			RewindComp->SetOwningActorState(CurrentRewindData);
			RewindComp->OnStartedRewind();
			DidRewind = true;
		}
#if WITH_EDITOR
		if (ToggleLagCompensationDebug() > 0)
		{
			const FColor Color = RewindComp->GetOwnerRole() == ROLE_Authority ? FColor::Red : FColor::Blue;
			DrawDebugBox(GetWorld(),CurrentRewindData->Location,CurrentRewindData->CollisionExtent
				,FQuat::Identity,Color,false,5.f);
		}
#endif
	}
	return DidRewind;
}

bool UNetworkPredictionWorldManager::UnwindActors()
{
	if (RegisteredLagCompComponents.Num() <= 0)
	{
		return false;
	}
	FLagCompensationRegistrationLock(this);
	bool DidUnwind = false;
	for ( UNetworkPredictionLagCompensation* RewindComp : RegisteredLagCompComponents)
	{
		if (!RewindComp)
		{
			continue;
		}
		const FNpLagCompensationHistory& History = RewindComp->GetLagCompensationHistory();
		if (!History.bIsInRewind)
		{
			continue;
		}
		if (History.Num() <= 0)
		{
			UE_LOG(LogNetworkPrediction, Log, TEXT("Trying To Unwind Actor %s That Has No History"),*GetNameSafe(RewindComp->GetOwner()));
			continue;
		}

		USceneComponent* RootComponent = RewindComp->GetOwner()->GetRootComponent();
		if (!RootComponent)
		{
			continue;
		}

		RewindComp->SetOwningActorState(History.PreRewindData);
		RewindComp->OnEndedRewind();
		DidUnwind = true;
	}
	return DidUnwind;
}

TSharedPtr<FNpLagCompensationData> UNetworkPredictionWorldManager::GetActorStateAtTime(AActor* RequestingActor,
	AActor* TargetActor, const float TargetSimTimeMS)
{
	if (!TargetActor)
	{
		return nullptr;
	}
	UNetworkPredictionLagCompensation* TargetComp = TargetActor->GetComponentByClass<UNetworkPredictionLagCompensation>();
	if (!TargetComp)
	{
		return nullptr;
	}
	return GetComponentStateAtTime(RequestingActor,TargetComp,TargetSimTimeMS);
}

TSharedPtr<FNpLagCompensationData> UNetworkPredictionWorldManager::GetComponentStateAtTime(AActor* RequestingActor,
								UNetworkPredictionLagCompensation* TargetComp, const float TargetSimTimeMS)
{
	if (!RequestingActor || !TargetComp)
	{
		return nullptr;
	}
	const float CurrentSimTimeMS = GetCurrentLagCompensationTimeMS(TargetComp->GetOwner());
	// this check is here because it should be impossible, every component 
	checkf(RegisteredLagCompComponents.Num() > 0,TEXT("Trying to get actor state from non-existent component ??"
												   " this valid UNetworkPredictionLagCompensation %s , didn't register with the subsystem, "
			   "possibly override of on component registered didn't call parent"),*GetNameSafe(TargetComp));
	if (FMath::IsNearlyEqual(CurrentSimTimeMS,TargetSimTimeMS))
	{
		// This function can be called in simulation code without caring if it's server to client.
		// this would happen when local player is trying to rewind others, but he's at the current interpolation time.
		// return latest, as in what the player sees.
		return GetLatestDataFromComponent(TargetComp);
	}

	float FinalRewindTime = FMath::Clamp(TargetSimTimeMS,0.f,CurrentSimTimeMS);
	// if we are on server Clamp Rewind Time To A Maximum, client can rewind his own simulation to correct himself for as long as he likes.
	// Client Rewinds To make sure any targeting that happens during the simulation , has other actors in same place at the re-simulation time.
	if (RequestingActor->GetLocalRole() == ROLE_Authority)
	{
		FinalRewindTime = ClampRewindingTime(CurrentSimTimeMS,TargetSimTimeMS);
		if (FinalRewindTime > TargetSimTimeMS)
		{
			UE_LOG(LogNetworkPrediction,Warning,TEXT("Desired Lag Compensation Rewind Exceeded Supported ping for %s : Desired Time %f , FinalTime %f")
		,*GetNameSafe(RequestingActor),TargetSimTimeMS,FinalRewindTime);
		}
	}
	return TargetComp->GetLagCompensationHistory().GetStateAtTime(FinalRewindTime);
}


float UNetworkPredictionWorldManager::GetInterpolationDelayMS(const UNetworkPredictionWorldManager* NetworkPredictionWorldManager)
{
	if (NetworkPredictionWorldManager)
	{
		if (NetworkPredictionWorldManager->GetSettings().PreferredTickingPolicy == ENetworkPredictionTickingPolicy::Fixed)
		{
			return NetworkPredictionWorldManager->GetSettings().FixedTickInterpolationBufferedMS;
		}
		else
		{
			return NetworkPredictionWorldManager->GetSettings().IndependentTickInterpolationMaxBufferedMS;
		}
	}
	return 0.f;
}

float UNetworkPredictionWorldManager::ClampRewindingTime( const float CurrentTime,const float InTargetRewindTime) const
{
	float ClampedTime = InTargetRewindTime;
	const UNetworkPredictionWorldManager* NetworkPredictionWorldManager = GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
	if (!NetworkPredictionWorldManager)
	{
		return InTargetRewindTime;
	}

	float MaxRewindDur = GetMaxRewindDuration(NetworkPredictionWorldManager);
	const float CurrentRewindDur = CurrentTime - InTargetRewindTime;
	MaxRewindDur = FMath::Min(CurrentRewindDur,MaxRewindDur);
	return CurrentTime - MaxRewindDur;
	//return ClampedTime;
}

float UNetworkPredictionWorldManager::GetMaxRewindDuration(const UNetworkPredictionWorldManager* NetworkPredictionWorldManager) const
{
	// the extra 1 frame is for lenience 
	float MaxRewindDur = Settings.MaxRewindTimeMS;
	if (NetworkPredictionWorldManager)
	{
		MaxRewindDur += GetInterpolationDelayMS(NetworkPredictionWorldManager);
		MaxRewindDur += (NetworkPredictionWorldManager->GetSettings().FixedTickDesiredBufferedInputCount + 1) * NetworkPredictionWorldManager->GetFixedTickState().FixedStepRealTimeMS;
	}
	return MaxRewindDur;
}

TSharedPtr<FNpLagCompensationData> UNetworkPredictionWorldManager::GetRewindDataFromComponent(const float TargetTimeMS, const UNetworkPredictionLagCompensation* LagCompComponent)
{
	if (!LagCompComponent)
	{
		return nullptr;
	}
	return  LagCompComponent->GetLagCompensationHistory().GetStateAtTime(TargetTimeMS);
}

TSharedPtr<FNpLagCompensationData> UNetworkPredictionWorldManager::GetLatestDataFromComponent(
	const UNetworkPredictionLagCompensation* LagCompComponent)
{
	return LagCompComponent->GetLagCompensationHistory().LastCopy();
}

const TArray<UNetworkPredictionLagCompensation*>& UNetworkPredictionWorldManager::GetRegisteredComponents()
{
	return RegisteredLagCompComponents;
}

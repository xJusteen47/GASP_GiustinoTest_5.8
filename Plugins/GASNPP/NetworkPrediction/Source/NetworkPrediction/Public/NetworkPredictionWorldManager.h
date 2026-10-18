// Copyright Epic Games, Inc. All Rights Reserved.
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NePredictionLagCompensationData.h"
#include "Subsystems/WorldSubsystem.h"

#include "Services/NetworkPredictionServiceRegistry.h"
#include "NetworkPredictionSerialization.h"

#include "NetworkPredictionWorldManager.generated.h"

class UNetworkPredictionLagCompensation;
class FChaosSolversModule;
class ANetworkPredictionReplicatedManager;

namespace UE::Net::Private
{
	struct FNetPredictionTestWorld;
}

UCLASS()
class NETWORKPREDICTION_API UNetworkPredictionWorldManager : public UWorldSubsystem
{
	GENERATED_BODY()
public:

	static UNetworkPredictionWorldManager* ActiveInstance;

	UNetworkPredictionWorldManager();

	// Server created, replicated manager (only used for centralized/system wide data replication)
	UPROPERTY()
	TObjectPtr<ANetworkPredictionReplicatedManager> ReplicatedManager = nullptr;

	// Subsystem Init/Deinit
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Returns unique ID for a new simulation instance regardless of type.
	// Clients set bForClient to get a negative, temp ID to use until server version comes in
	FNetworkPredictionID CreateSimulationID(const bool bForClient)
	{
		return FNetworkPredictionID(bForClient ? TempClientSpawnCount-- : InstanceSpawnCounter++);
	}

	template<typename ModelDef>
	void RemapClientSimulationID(FNetworkPredictionID ClientID, FNetworkPredictionID ServerID)
	{
		npEnsure((int32)ClientID < INDEX_NONE && (int32)ServerID >= 0);
		npEnsure(ClientID.GetTraceID() == ServerID.GetTraceID());
		
		TModelDataStore<ModelDef>* DataStore = Services.GetDataStore<ModelDef>();
		TInstanceData<ModelDef>& InstanceData = DataStore->Instances.FindOrAdd(ServerID);
		InstanceData = MoveTemp(DataStore->Instances.FindOrAdd(ClientID));
		DataStore->Instances.Remove(ClientID);
	}

	template<typename ModelDef>
	void RegisterInstance(FNetworkPredictionID ID, const TNetworkPredictionModelInfo<ModelDef>& ModelInfo)
	{
		// Register with data store
		TModelDataStore<ModelDef>* DataStore = Services.GetDataStore<ModelDef>();
		TInstanceData<ModelDef>& InstanceData = DataStore->Instances.FindOrAdd(ID);
		InstanceData.Info = ModelInfo;
		InstanceData.TraceID = ID.GetTraceID();
		InstanceData.CueDispatcher->Driver = ModelInfo.Driver; // Awkward: we should convert Cues to a service so this isn't needed.
		InstanceData.Info.View->CueDispatcher = &InstanceData.CueDispatcher.Get(); // Double awkward: we should move Cuedispatcher and clean up these weird links
	}

	template<typename ModelDef>
	void UnregisterInstance(FNetworkPredictionID ID)
	{
		if (!bLockServices)
		{
			Services.UnregisterInstance<ModelDef>(ID);
		}
		else
		{
			DeferredServiceConfigDelegate.AddLambda([ID](UNetworkPredictionWorldManager* Manager)
			{
				TModelDataStore<ModelDef>* DataStore = Manager->Services.GetDataStore<ModelDef>();
	            
				/// Clean up any Deferred Register Calls that are still pending, because apparently we get cleaned up instantly
				/// after being created, and since Delegate InvocationLists are reverse iterated, the deferred Unregister Call gets
				/// triggered first.
				FDelegateHandle* DeferredRegisterHandlePtr = DataStore->DeferredRegisterHandle.Find(ID);
				if (DeferredRegisterHandlePtr)
				{
					Manager->DeferredServiceConfigDelegate.Remove(*DeferredRegisterHandlePtr);
					DataStore->DeferredRegisterHandle.Remove(ID);
				}
	            
				Manager->Services.UnregisterInstance<ModelDef>(ID);
			});
		}
	}

	template<typename ModelDef>
	void ConfigureInstance(FNetworkPredictionID ID, const FNetworkPredictionInstanceArchetype& Archetype, const FNetworkPredictionInstanceConfig& Config, FReplicationProxySet RepProxies, ENetRole Role, bool bHasNetConnection
		, UNetworkPredictionPlayerControllerComponent* RPCHandler);
	

	ENetworkPredictionTickingPolicy PreferredDefaultTickingPolicy() const;

	void SyncNetworkPredictionSettings(const UNetworkPredictionSettingsObject* Settings);

	const FNetworkPredictionSettings& GetSettings() const { return Settings; }	

	const FFixedTickState& GetFixedTickState() const { return FixedTickState; }
	
	const FVariableTickState& GetVariableTickState() const { return VariableTickState; }

	// IMPORTANT: this makes variable tick state unusable since it's only implemented for fixed tick
	// this is very crude way of doing this, each service can add a lambda along with the ID to its own UNetworkPredictionPlayerControllerComponent
	// and when an input is received the component will loop through inputs received which has ID for each entry and call the lambda for correct IDs
	void OnInputReceived(const int32& Frame,const float& InterpolationTime,const TArray<FSimulationReplicatedInput>& Inputs,
		UNetworkPredictionPlayerControllerComponent* RPCHandler);
	void OnReceivedAckedData(const FSerializedAckedFrames& AckedFrames, UNetworkPredictionPlayerControllerComponent* RPCHandler);

	void RegisterRPCHandler(UNetworkPredictionPlayerControllerComponent* RPCHandler);
	void UnRegisterRPCHandler(UNetworkPredictionPlayerControllerComponent* RPCHandler);
	void SetTimeDilation(const FSimTimeDilation& TimeDilation);
private:
	
	
	FNetworkPredictionSettings Settings;

	FFixedTickState FixedTickState;
	FVariableTickState VariableTickState;
	FNetworkPredictionServiceRegistry Services;

	// Player controller component responsible for handling input and data that should be per net connection not per sim instance.
	UPROPERTY()
	TArray<UNetworkPredictionPlayerControllerComponent*> RPCHandlers;

	void OnWorldPreTick(UWorld* InWorld, ELevelTick InLevelTick, float InDeltaSeconds);
	void ReconcileSimulationsPostNetworkUpdate();
	void BeginNewSimulationFrame(UWorld* InWorld, ELevelTick InLevelTick, float InDeltaSeconds);

	void TickLocalPlayerControllers(ELevelTick InLevelTick, float InDeltaSeconds) const;
	void EnableLocalPlayerControllersTicking() const;
	// For ease of unit testing
	friend struct UE::Net::Private::FNetPredictionTestWorld;
	void OnWorldPreTick_Internal(float InDeltaSeconds, float InFixedFrameRate);
	void ReconcileSimulationsPostNetworkUpdate_Internal();
	void BeginNewSimulationFrame_Internal(float InDeltaSeconds);

	/**
	 * Client: sends the fixed tick input frames to the server, once per engine frame. Each frame goes in its own unreliable
	 * RPC, and the net driver silently drops unreliable RPCs on a saturated connection (UNetDriver::ProcessRemoteFunction),
	 * which the server can only fill with copies of older inputs. So a new frame is sent only when the connection can take it,
	 * otherwise it waits for the next engine frame (UNetworkPredictionPlayerControllerComponent::NextInputFrameToSend), in
	 * order; the FixedTickInputSendCount - 1 redundant copies of older frames (for packet loss) are sent only while there is
	 * no such backlog, and once per fixed tick (not on engine frames that ran none). After a hitch the catch-up frames then
	 * reach the server within a few engine frames, none lost.
	 */
	void SendFixedTickInputsToServer(const bool bTickedThisFrame);

	FDelegateHandle PreTickDispatchHandle;
	FDelegateHandle PostTickDispatchHandle;
	FDelegateHandle PreWorldActorTickHandle;

	int32 InstanceSpawnCounter = 1;
	int32 TempClientSpawnCount = -2; // negative IDs for client to use before getting server assigned id

	//Server Only Data
	FServerAckedFrames ServerAckedFrames;
	// Client Only Data
	FAckedFrames LocalAckedFrames;
	
	// Callbacks to change subscribed services, that couldn't be made inline
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHandleDeferredServiceConfig, UNetworkPredictionWorldManager*);
	FOnHandleDeferredServiceConfig DeferredServiceConfigDelegate;
	bool bLockServices = false; // Services are locked and we can't
	
	// ---------------------------------------

	template<typename ModelDef>
	void BindServerNetRecv_Fixed(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore);

	template<typename ModelDef>
	void BindServerNetRecv_Independent(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore);

	// ----

	template<typename ReplicatorType, typename ModelDef=typename ReplicatorType::ModelDef>
	void BindClientNetRecv_Fixed(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore, ENetRole NetRole);

	template<typename ReplicatorType, typename ModelDef=typename ReplicatorType::ModelDef>
	void BindClientNetRecv_Independent(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore, ENetRole NetRole);

	// ----

	template<typename ReplicatorType, typename ModelDef=typename ReplicatorType::ModelDef>
	void BindNetSend_Fixed(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore);

	template<typename ReplicatorType, typename ModelDef=typename ReplicatorType::ModelDef>
	void BindNetSend_IndependentLocal(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore);

	template<typename ReplicatorType, typename ModelDef=typename ReplicatorType::ModelDef>
	void BindNetSend_IndependentRemote(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore);

	// ----
	
	template<typename ReplicatorType, typename ModelDef=typename ReplicatorType::ModelDef>
	void BindReplayNetSendRecv_Fixed(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore, ENetRole NetRole);

	template<typename ReplicatorType, typename ModelDef = typename ReplicatorType::ModelDef>
	void BindReplayNetSendRecv_IndependentLocal(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore, ENetRole NetRole);

	template<typename ReplicatorType, typename ModelDef = typename ReplicatorType::ModelDef>
	void BindReplayNetSendRecv_IndependentRemote(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore, ENetRole NetRole);

	// ---------------------------------------

	template<typename ModelDef>
	void InitClientRecvData(FNetworkPredictionID ID, TClientRecvData<ModelDef>& ClientRecvData, TModelDataStore<ModelDef>* DataStore, ENetRole NetRole);

   // --------------------------------------- Lag Compensation ------------------------------//
public:
	UFUNCTION(BlueprintCallable,Category = LagCompensation)
	void RegisterRewindableComponent(UNetworkPredictionLagCompensation* RewindComp);

	UFUNCTION(BlueprintCallable,Category = LagCompensation)
	void UnregisterRewindableComponent(UNetworkPredictionLagCompensation* RewindComp);

	/*
	 * CAUTION: Be very careful when using this function. if you don't call UnwindActors right after you are done,
	 * in the same execution you can cause big bugs, mainly server will move players and not bring them back
	 * effectively making them all teleport for no reason.
	 * Use Targeting Library Perform targeting , if you can.
	 * 
	 * normally this would not be exposed and targeting processors would only be used with this.
	 * but targeting didn't feel like it's something NP manager should be responsible for. maybe it should.
	 * or maybe we can use query only collision component proxies, lives on the actor,
	 * and we move them so we don't move the actors.
	 * and for safety beginning of every tick of movement we rest the proxies relative transform to default
	 * so even if Unwind is not called , it will be rest. and when rewind is called again it will rewind or reset.
	 * 
	 * but this will require game code to know when to use channels that main collision component ignores,
	 * and is block/overlap with these proxies.
	 * 
	 */
	UFUNCTION(BlueprintCallable,Category = LagCompensation)
	bool RewindActors(AActor* RequestingActor, const float RewindSimTimeMS);
	
	UFUNCTION(BlueprintCallable,Category = LagCompensation)
	bool UnwindActors();
	
	UFUNCTION(BlueprintPure,Category = LagCompensation)
	const TArray<UNetworkPredictionLagCompensation*>& GetRegisteredComponents();
	
	UFUNCTION(BlueprintPure,Category = LagCompensation)
	float GetCurrentLagCompensationTimeMS(const AActor* Actor) const;

	UFUNCTION(BlueprintPure,Category = LagCompensation)
	FNpLagCompensationData GetActorDefaultStateAtTime(AActor* RequestingActor,AActor* TargetActor,const float TargetSimTimeMS);

	TSharedPtr<FNpLagCompensationData> GetActorStateAtTime(AActor* RequestingActor,AActor* TargetActor,
	const float TargetSimTimeMS) ;
	
	TSharedPtr<FNpLagCompensationData> GetComponentStateAtTime(AActor* RequestingActor,UNetworkPredictionLagCompensation* TargetComp,
	const float TargetSimTimeMS) ;
	TSharedPtr<FNpLagCompensationData> GetRewindDataFromComponent(const float TargetTimeMS, const UNetworkPredictionLagCompensation* LagCompComponent);

	TSharedPtr<FNpLagCompensationData> GetLatestDataFromComponent(const UNetworkPredictionLagCompensation* LagCompComponent);

	// this does expose getting a specific state a simulation at a specific time,
	// but using this for lag compensation will require more templating.
	// - the driver would need the functions to :
	// "SetActorStateForLagCompensation" set the actor state based on a simulation state in the past
	// we perform rewind/unwind by calling SetActorStateForLagCompensation once from history and then back from pending
	// - Need a service that only registers instances that implement the needed functions and have right role
	// - Finally sync state will need to have CanRewind further bool to stop rewinding someone further when gameplay wants to.
	// the issue arises from the states and instances of simulations being completely separate
	// and there's no way of knowing when multiple simulation effect lag compensation,
	// say ability simulation wants to stop rewinding at a specific state "has tag Ability.Dash",
	// movement simulation has no idea of this and location will still be rewound,
	// this why lag compensation is implemented the way it is with its own buffer and filling its own state directly from actor state.
	template<typename ModelDef,typename TSyncType>
	bool GetSyncStateAtTime(int32 ID , const float TimeMS, TSyncType& OutState)
	{
		TModelDataStore<ModelDef>* DataStore = Services.GetDataStore<ModelDef>();
		if (!DataStore)
		{
			return false;
		}
		TInstanceFrameState<ModelDef>* Frames = DataStore->Frames.Find(FNetworkPredictionID(ID));
		if (!Frames)
		{
			return false;
		}

		
		// The total number of elapsed frames, as a floating-point value.
		float TotalFrames = TimeMS / FixedTickState.FixedStepMS;

		// The frame that has just completed (the integer part).
		const int32 OutFrameBefore = FMath::FloorToInt(TotalFrames);

		// The next frame.
		const int32 OutFrameAfter = OutFrameBefore + 1;

		// The alpha is the fractional part of the total frames.
		float Alpha = TotalFrames - OutFrameBefore;

		if (FMath::IsNearlyEqual(Alpha,1.f))
		{
			Frames->Buffer[OutFrameAfter].SyncState.CopyTo(&OutState);
			return true; 
		}
		if (FMath::IsNearlyEqual(Alpha,0.f))
		{
			Frames->Buffer[OutFrameBefore].SyncState.CopyTo(&OutState);
			return true; 
		}
		
		Frames->Buffer[OutFrameBefore].SyncState.CopyTo(&OutState);
		OutState.Interpolate(Frames->Buffer[OutFrameBefore].SyncState,Frames->Buffer[OutFrameAfter].SyncState,Alpha);
		return true;
		
	}
private:
	
	static float GetInterpolationDelayMS(const UNetworkPredictionWorldManager* NetworkPredictionWorldManager);
	
	float ClampRewindingTime(const float CurrentTime ,const float InTargetRewindTime) const;
	
	float GetMaxRewindDuration(const UNetworkPredictionWorldManager* NetworkPredictionWorldManager) const;
	
protected:

	struct FLagCompensationRegistrationLock
	{
		FLagCompensationRegistrationLock(UNetworkPredictionWorldManager* InManager)
		{
			Manager = InManager;
			Manager->LagCompRegistrationLock++;
		}
		~FLagCompensationRegistrationLock()
		{
			Manager->LagCompRegistrationLock--;
			if (Manager->LagCompRegistrationLock == 0)
			{
				for (UNetworkPredictionLagCompensation* PendingRemove : Manager->PendingRemoveLagCompComponents)
				{
					Manager->UnregisterRewindableComponent(PendingRemove);
				}
				Manager->PendingRemoveLagCompComponents.Empty();

				for (UNetworkPredictionLagCompensation* PendingAdd : Manager->PendingAddLagCompComponents)
				{
					Manager->RegisterRewindableComponent(PendingAdd);
				}
				Manager->PendingAddLagCompComponents.Empty();
			}
		}

		UNetworkPredictionWorldManager* Manager;
	};

	UPROPERTY(Transient)
	TArray<UNetworkPredictionLagCompensation*> RegisteredLagCompComponents;

	UPROPERTY(Transient)
	TArray<UNetworkPredictionLagCompensation*> PendingAddLagCompComponents;

	UPROPERTY(Transient)
	TArray<UNetworkPredictionLagCompensation*> PendingRemoveLagCompComponents;

	int32 LagCompRegistrationLock = 0;
};


// This is the function that takes the config and current network state (role/connection) and subscribes to 
// the appropriate internal NetworkPrediction services.
template<typename ModelDef>
void UNetworkPredictionWorldManager::ConfigureInstance(FNetworkPredictionID ID, const FNetworkPredictionInstanceArchetype& Archetype, const FNetworkPredictionInstanceConfig& Config, FReplicationProxySet RepProxies, ENetRole Role, bool bHasNetConnection
	, UNetworkPredictionPlayerControllerComponent* RPCHandler)
{
	static constexpr FNetworkPredictionModelDefCapabilities Capabilities = FNetworkPredictionDriver<ModelDef>::GetCapabilities();

	npCheckSlow((int32)ID > 0);
	npEnsure(Role != ROLE_None);

	TModelDataStore<ModelDef>* DataStore = Services.GetDataStore<ModelDef>();
	TInstanceData<ModelDef>& InstanceData = *DataStore->Instances.Find(ID);
	TInstanceFrameState<ModelDef>& FrameData = DataStore->Frames.FindOrAdd(ID);

	const int32 PrevPendingFrame = InstanceData.Info.View->PendingFrame;
	const bool bNewInstance = (InstanceData.NetRole == ROLE_None);
	InstanceData.NetRole = Role;
	InstanceData.Info.RPCHandler = RPCHandler;
	
	TRemoteInputService<ModelDef>::SetMaxFaultLimit(Settings.MaximumRemoteInputFaultLimit);
	TRemoteInputService<ModelDef>::SetDesiredBufferedInputs(Settings.FixedTickDesiredBufferedInputCount);
	TFixedSmoothingService<ModelDef>::SetSmoothingSpeed(Settings.SmoothingSpeed);
	

	ENetworkPredictionService ServiceMask = ENetworkPredictionService::None;
	
	if (Archetype.TickingMode == ENetworkPredictionTickingPolicy::Independent)
	{
		// Point cached view to the VariableTickState's pending frame
		InstanceData.Info.View->UpdateView(VariableTickState.PendingFrame, 
			VariableTickState.GetNextTimeStep().TotalSimulationTime,
			&FrameData.Buffer[VariableTickState.PendingFrame].InputCmd, 
			&FrameData.Buffer[VariableTickState.PendingFrame].SyncState, 
			&FrameData.Buffer[VariableTickState.PendingFrame].AuxState);
		
		switch (Role)
		{
			case ENetRole::ROLE_Authority:
			{
				if (bHasNetConnection)
				{
					// Remotely controlled
					BindServerNetRecv_Independent<ModelDef>(ID, RepProxies.ServerRPC, DataStore);
					BindNetSend_IndependentRemote<TIndependentTickReplicator_AP<ModelDef>>(ID, RepProxies.AutonomousProxy, DataStore);
					BindNetSend_IndependentRemote<TIndependentTickReplicator_SP<ModelDef>>(ID, RepProxies.SimulatedProxy, DataStore);
					BindReplayNetSendRecv_IndependentRemote<TIndependentTickReplicator_SP<ModelDef>>(ID, RepProxies.Replay, DataStore, Role);

					ServiceMask |= ENetworkPredictionService::IndependentRemoteTick;
					ServiceMask |= ENetworkPredictionService::IndependentRemoteFinalize;

					// Point view to the ServerRecv PendingFrame instead
					TServerRecvData_Independent<ModelDef>* ServerRecvData = DataStore->ServerRecv_IndependentTick.Find(ID);
					npCheckSlow(ServerRecvData);

					const int32 ServerRecvPendingFrame = ServerRecvData->PendingFrame;

					auto& PendingFrameData = FrameData.Buffer[ServerRecvPendingFrame];
					InstanceData.Info.View->UpdateView(ServerRecvPendingFrame,
						ServerRecvData->TotalSimTimeMS,
						&PendingFrameData.InputCmd, 
						&PendingFrameData.SyncState, 
						&PendingFrameData.AuxState);
				}
				else
				{
					// Locally controlled
					BindNetSend_IndependentLocal<TIndependentTickReplicator_SP<ModelDef>>(ID, RepProxies.SimulatedProxy, DataStore);
					BindReplayNetSendRecv_IndependentLocal<TIndependentTickReplicator_SP<ModelDef>>(ID, RepProxies.Replay, DataStore, Role);

					if (FNetworkPredictionDriver<ModelDef>::HasSimulation())
					{
						if (FNetworkPredictionDriver<ModelDef>::HasInput())
						{
							ServiceMask |= ENetworkPredictionService::IndependentLocalInput;
						}
						ServiceMask |= ENetworkPredictionService::IndependentLocalTick;
						ServiceMask |= ENetworkPredictionService::IndependentLocalFinalize;
					}
				}

				break;
			}
			case ENetRole::ROLE_AutonomousProxy:
			{
				BindClientNetRecv_Independent<TIndependentTickReplicator_AP<ModelDef>>(ID, RepProxies.AutonomousProxy, DataStore, Role);
				BindNetSend_IndependentLocal<TIndependentTickReplicator_Server<ModelDef>>(ID, RepProxies.ServerRPC, DataStore);
				BindReplayNetSendRecv_IndependentLocal<TIndependentTickReplicator_SP<ModelDef>>(ID, RepProxies.Replay, DataStore, Role);
				
				npCheckf(FNetworkPredictionDriver<ModelDef>::HasSimulation(), TEXT("AP must have Simulation."));
				npCheckf(FNetworkPredictionDriver<ModelDef>::HasInput(), TEXT("AP sim doesn't have Input?"));

				ServiceMask |= ENetworkPredictionService::IndependentLocalInput;
				ServiceMask |= ENetworkPredictionService::IndependentLocalTick;
				ServiceMask |= ENetworkPredictionService::IndependentLocalFinalize;

				ServiceMask |= ENetworkPredictionService::ServerRPC;
				ServiceMask |= ENetworkPredictionService::IndependentRollback;
				break;
			}
			case ENetRole::ROLE_SimulatedProxy:
			{
				BindClientNetRecv_Independent<TIndependentTickReplicator_AP<ModelDef>>(ID, RepProxies.AutonomousProxy, DataStore, Role);
				BindClientNetRecv_Independent<TIndependentTickReplicator_SP<ModelDef>>(ID, RepProxies.SimulatedProxy, DataStore, Role);
				BindReplayNetSendRecv_IndependentLocal<TIndependentTickReplicator_SP<ModelDef>>(ID, RepProxies.Replay, DataStore, Role);

				// Interpolation is the only supported mode for independently ticked SP simulations
				// (will add support for sim-extrapolate eventually)
				ServiceMask |= ENetworkPredictionService::IndependentInterpolate;
				break;
			}
		};
	}
	else if (Archetype.TickingMode == ENetworkPredictionTickingPolicy::Fixed)
	{
		// Point cached view to the FixedTickState's pending frame
		InstanceData.Info.View->UpdateView(FixedTickState.PendingFrame + FixedTickState.Offset,
			FixedTickState.GetTotalSimTimeMS(),
			&FrameData.Buffer[FixedTickState.PendingFrame].InputCmd, 
			&FrameData.Buffer[FixedTickState.PendingFrame].SyncState, 
			&FrameData.Buffer[FixedTickState.PendingFrame].AuxState);
		InstanceData.Info.View->UpdateInterpolationTime(&FrameData.Buffer[FixedTickState.PendingFrame].InterpolationTimeMS);
		// Bind NetSend/Recv and role-dependent services
		switch (Role)
		{
			case ENetRole::ROLE_Authority:
			{
				if (FNetworkPredictionDriver<ModelDef>::HasSimulation())
				{
					BindServerNetRecv_Fixed<ModelDef>(ID, RepProxies.ServerRPC, DataStore);
					BindNetSend_Fixed<TFixedTickReplicator_AP<ModelDef>>(ID, RepProxies.AutonomousProxy, DataStore);

					if (FNetworkPredictionDriver<ModelDef>::HasInput())
					{
						ServiceMask |= bHasNetConnection ? ENetworkPredictionService::FixedInputRemote : ENetworkPredictionService::FixedInputLocal;
					}
				}
				
				BindNetSend_Fixed<TFixedTickReplicator_SP<ModelDef>>(ID, RepProxies.SimulatedProxy, DataStore);
				BindReplayNetSendRecv_Fixed<TFixedTickReplicator_SP<ModelDef>>(ID, RepProxies.Replay, DataStore, Role);
				break;
			}
			case ENetRole::ROLE_AutonomousProxy:
			{
				npCheckf(FNetworkPredictionDriver<ModelDef>::HasSimulation(), TEXT("AP must have Simulation."));
				npCheckf(FNetworkPredictionDriver<ModelDef>::HasInput(), TEXT("AP sim doesn't have Input?"));

				BindClientNetRecv_Fixed<TFixedTickReplicator_AP<ModelDef>>(ID, RepProxies.AutonomousProxy, DataStore, Role);
				BindClientNetRecv_Fixed<TFixedTickReplicator_SP<ModelDef>>(ID, RepProxies.SimulatedProxy, DataStore, Role);

				BindNetSend_Fixed<TFixedTickReplicator_Server<ModelDef>>(ID, RepProxies.ServerRPC, DataStore);
				BindReplayNetSendRecv_Fixed<TFixedTickReplicator_SP<ModelDef>>(ID, RepProxies.Replay, DataStore, Role);
				
				// Poll local input and send to server services
				ServiceMask |= ENetworkPredictionService::FixedInputLocal;
				ServiceMask |= ENetworkPredictionService::FixedServerRPC;
				break;
			}
			case ENetRole::ROLE_SimulatedProxy:
			{
				BindClientNetRecv_Fixed<TFixedTickReplicator_AP<ModelDef>>(ID, RepProxies.AutonomousProxy, DataStore, Role);
				BindClientNetRecv_Fixed<TFixedTickReplicator_SP<ModelDef>>(ID, RepProxies.SimulatedProxy, DataStore, Role);

				BindReplayNetSendRecv_Fixed<TFixedTickReplicator_SP<ModelDef>>(ID, RepProxies.Replay, DataStore, Role);
				break;
			}
		};

		// Authority vs Non-Authority services
		if (Role == ROLE_Authority)
		{
			if (FNetworkPredictionDriver<ModelDef>::HasSimulation())
			{
				ServiceMask |= ENetworkPredictionService::FixedTick;
				ServiceMask |= ENetworkPredictionService::FixedFinalize;

				if (FNetworkPredictionDriver<ModelDef>::HasFinalizeSmoothingFrame && Settings.bEnableFixedTickSmoothing)
				{
					ServiceMask |= ENetworkPredictionService::FixedSmoothing;
				}
				
			}
		}
		else
		{
			// These services depend on NetworkLOD for non authority cases
			switch(Config.NetworkLOD)
			{
			case ENetworkLOD::ForwardPredict:
				ServiceMask |= ENetworkPredictionService::FixedRollback;

				if (FNetworkPredictionDriver<ModelDef>::HasSimulation())
				{
					ServiceMask |= ENetworkPredictionService::FixedTick;
					ServiceMask |= ENetworkPredictionService::FixedFinalize;

					if (FNetworkPredictionDriver<ModelDef>::HasFinalizeSmoothingFrame && Settings.bEnableFixedTickSmoothing)
					{
						ServiceMask |= ENetworkPredictionService::FixedSmoothing;
					}
				}
				
				break;

			case ENetworkLOD::Interpolated:
				ServiceMask |= ENetworkPredictionService::FixedInterpolate;
				break;
			}
		}
	}

	// Net Cues: set which replicated cues we should accept based on if we are FP or interpolated
	if (Role != ROLE_Authority)
	{
		if (EnumHasAnyFlags(ServiceMask, ENetworkPredictionService::FixedInterpolate | ENetworkPredictionService::IndependentInterpolate))
		{
			InstanceData.CueDispatcher->SetReceiveReplicationTarget(ENetSimCueReplicationTarget::Interpolators);
		}
		else
		{
			InstanceData.CueDispatcher->SetReceiveReplicationTarget( Role == ROLE_AutonomousProxy ? ENetSimCueReplicationTarget::AutoProxy : ENetSimCueReplicationTarget::SimulatedProxy);
		}
	}

	// Register with selected services
	if (!bLockServices)
	{
		Services.RegisterInstance<ModelDef>(ID, InstanceData, ServiceMask);
	}
	else
	{
		/// Keep track of the deferred register call so we can clean it up when Unregistering in case it's still around
		FDelegateHandle& DeferredRegisterHandle = DataStore->DeferredRegisterHandle.FindOrAdd(ID);
        
		DeferredRegisterHandle = DeferredServiceConfigDelegate.AddLambda([ID, ServiceMask](UNetworkPredictionWorldManager* Manager)
		{
			TModelDataStore<ModelDef>* DataStore = Manager->Services.GetDataStore<ModelDef>();
			TInstanceData<ModelDef>* InstanceData = DataStore->Instances.Find(ID);
			if (InstanceData)
			{
				Manager->Services.RegisterInstance<ModelDef>(ID, *InstanceData, ServiceMask);
			}

			DataStore->DeferredRegisterHandle.Remove(ID);
		});
	}
	
	// Call into driver to seed initial state if this is a new instance
	if (bNewInstance)
	{
		UE_NP_TRACE_SIM_CREATED(ID, InstanceData.Info.Driver, ModelDef);
		if (FNetworkPredictionDriver<ModelDef>::HasNpState())
		{
			FNetworkPredictionDriver<ModelDef>::InitializeSimulationState(InstanceData.Info.Driver, InstanceData.Info.View);
		}
	}
	else if (PrevPendingFrame != InstanceData.Info.View->PendingFrame)
	{
		// Not a new instance but PendingFrame changed, so copy contents from previous PendingFrame
		if (FNetworkPredictionDriver<ModelDef>::HasNpState())
		{
			FrameData.Buffer[InstanceData.Info.View->PendingFrame] = FrameData.Buffer[PrevPendingFrame];
		}
	}

	UE_NP_TRACE_SIM_CONFIG(ID.GetTraceID(), Role, bHasNetConnection, Archetype, Config, ServiceMask);
}

// ---------------------------------------------------------------------------------------
//	Server Receive Bindings
//		-Binds RepProxy serialize lambda to a Replicator function
//		-2 versions for Fixed/Independent
//		-Binds directly to TFixedTickReplicator_Server/TIndependentTickReplicator_Server
// ---------------------------------------------------------------------------------------
template<typename ModelDef>
void UNetworkPredictionWorldManager::BindServerNetRecv_Fixed(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore)
{
	if (!RepProxy)
		return;

	const int32 ServerRecvIdx = DataStore->ServerRecv.GetIndex(ID);

	TFixedTickReplicator_Server<ModelDef>::SetNumInputsPerSend(Settings.FixedTickInputSendCount);

	TServerRecvData_Fixed<ModelDef>& ServerRecvData = DataStore->ServerRecv.GetByIndexChecked(ServerRecvIdx);
	ServerRecvData.TraceID = ID.GetTraceID();
	ServerRecvData.ID = ID;
	FFixedTickState* TickState = &this->FixedTickState;
	RepProxy->NetSerializeFunc = [DataStore, ServerRecvIdx, TickState](const FNetSerializeParams& P)
	{
		npEnsure(P.Ar.IsLoading());
		TServerRecvData_Fixed<ModelDef>& ServerRecvData = DataStore->ServerRecv.GetByIndexChecked(ServerRecvIdx);
		
		UE_NP_TRACE_SIM(ServerRecvData.TraceID);
		TFixedTickReplicator_Server<ModelDef>::NetRecv(P, ServerRecvData, DataStore, TickState);
	};
}

template<typename ModelDef>
void UNetworkPredictionWorldManager::BindServerNetRecv_Independent(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore)
{
	if (!RepProxy)
		return;

	const int32 ServerRecvIdx = DataStore->ServerRecv_IndependentTick.GetIndex(ID);

	TIndependentTickReplicator_Server<ModelDef>::SetNumInputsPerSend(Settings.IndependentTickInputSendCount);

	TServerRecvData_Independent<ModelDef>& ServerRecvData = DataStore->ServerRecv_IndependentTick.GetByIndexChecked(ServerRecvIdx);
	ServerRecvData.TraceID = ID.GetTraceID();
	ServerRecvData.InstanceIdx = DataStore->Instances.GetIndex(ID);
	ServerRecvData.FramesIdx = DataStore->Frames.GetIndex(ID);
	ServerRecvData.PendingFrame = 0;
	ServerRecvData.TotalSimTimeMS = 0;
	ServerRecvData.UnspentTimeMS = 0.f;
	ServerRecvData.LastConsumedFrame = INDEX_NONE;
	ServerRecvData.LastRecvFrame = INDEX_NONE;

	RepProxy->NetSerializeFunc = [DataStore, ServerRecvIdx](const FNetSerializeParams& P)
	{
		npEnsure(P.Ar.IsLoading());		
		TServerRecvData_Independent<ModelDef>& ServerRecvData = DataStore->ServerRecv_IndependentTick.GetByIndexChecked(ServerRecvIdx);

		UE_NP_TRACE_SIM(ServerRecvData.TraceID);
		TIndependentTickReplicator_Server<ModelDef>::NetRecv(P, ServerRecvData, DataStore);
	};
}

// ---------------------------------------------------------------------------------------
//	Client Receive Bindings
//		-Binds RepProxy serialize lambda to a Replicator function
//		-2 versions for Fixed/Independent
//		-Bind to either AP, or SP versions based on ReplicatorType
//			TFixedTickReplicator_AP
//			TFixedTickReplicator_SP
//			TIndependentTickReplicator_AP
//			TIndependentTickReplicator_SP
// ---------------------------------------------------------------------------------------

template<typename ReplicatorType, typename ModelDef>
void UNetworkPredictionWorldManager::BindClientNetRecv_Fixed(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore, ENetRole NetRole)
{
	if (!RepProxy) 
		return;

	TFixedTickReplicator_Server<ModelDef>::SetNumInputsPerSend(Settings.FixedTickInputSendCount);

	const int32 ClientRecvIdx = DataStore->ClientRecv.GetIndex(ID);
	NpResizeAndSetBit(DataStore->ClientRecvBitMask, ClientRecvIdx, false);

	TClientRecvData<ModelDef>& ClientRecvData = DataStore->ClientRecv.GetByIndexChecked(ClientRecvIdx);
	InitClientRecvData<ModelDef>(ID, ClientRecvData, DataStore, NetRole);

	FFixedTickState* TickState = &this->FixedTickState;
	RepProxy->NetSerializeFunc = [DataStore, ClientRecvIdx, TickState](const FNetSerializeParams& P)
	{
		npEnsure(P.Ar.IsLoading());
		DataStore->ClientRecvBitMask[ClientRecvIdx] = true;
		auto& ClientRecvData = DataStore->ClientRecv.GetByIndexChecked(ClientRecvIdx);

		UE_NP_TRACE_SIM(ClientRecvData.TraceID);
		ReplicatorType::NetRecv(P, ClientRecvData, DataStore, TickState);
	};
}

template<typename ReplicatorType, typename ModelDef>
void UNetworkPredictionWorldManager::BindClientNetRecv_Independent(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore, ENetRole NetRole)
{
	if (!RepProxy) 
		return;

	TIndependentTickReplicator_Server<ModelDef>::SetNumInputsPerSend(Settings.IndependentTickInputSendCount);

	const int32 ClientRecvIdx = DataStore->ClientRecv.GetIndex(ID);
	NpResizeAndSetBit(DataStore->ClientRecvBitMask, ClientRecvIdx, false);

	TClientRecvData<ModelDef>& ClientRecvData = DataStore->ClientRecv.GetByIndexChecked(ClientRecvIdx);
	InitClientRecvData<ModelDef>(ID, ClientRecvData, DataStore, NetRole);

	FVariableTickState* TickState = &this->VariableTickState;
	RepProxy->NetSerializeFunc = [DataStore, ClientRecvIdx, TickState](const FNetSerializeParams& P)
	{
		npEnsure(P.Ar.IsLoading());
		DataStore->ClientRecvBitMask[ClientRecvIdx] = true;
		auto& ClientRecvData = DataStore->ClientRecv.GetByIndexChecked(ClientRecvIdx);

		UE_NP_TRACE_SIM(ClientRecvData.TraceID);
		ReplicatorType::NetRecv(P, ClientRecvData, DataStore, TickState);
	};
}

// ---------------------------------------------------------------------------------------
//	Send Bindings
//		-Binds RepProxy serialize lambda to a Replicator function
//		-3 versions: Fixed, Independent (Local Tick), Independent (Remote Ticked)
//		-Bind to either Server, AP, or SP versions based on ReplicatorType:
//			TFixedTickReplicator_Server
//			TIndependetTickReplicator_Server
//			TFixedTickReplicator_AP
//			TFixedTickReplicator_SP
//			TIndependentTickReplicator_AP
//			TIndependentTickReplicator_SP
// ---------------------------------------------------------------------------------------

template<typename ReplicatorType, typename ModelDef>
void UNetworkPredictionWorldManager::BindNetSend_Fixed(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore)
{
	if (!RepProxy) 
		return;

	// Tick is based on local FixedTickState
	FFixedTickState* TickState = &this->FixedTickState;
	RepProxy->NetSerializeFunc = [ID, DataStore, TickState](const FNetSerializeParams& P)
	{
		npEnsure(P.Ar.IsSaving());
		UE_NP_TRACE_SIM(ID.GetTraceID());
		ReplicatorType::NetSend(P, ID, DataStore, TickState);
	};
}

template<typename ReplicatorType, typename ModelDef>
void UNetworkPredictionWorldManager::BindNetSend_IndependentLocal(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore)
{
	if (!RepProxy) 
		return;

	// Tick is based on local VariableTickState
	FVariableTickState* TickState = &this->VariableTickState;
	RepProxy->NetSerializeFunc = [ID, DataStore, TickState](const FNetSerializeParams& P)
	{
		npEnsure(P.Ar.IsSaving());
		UE_NP_TRACE_SIM(ID.GetTraceID());
		ReplicatorType::NetSend(P, ID, DataStore, TickState);
	};
}

template<typename ReplicatorType, typename ModelDef>
void UNetworkPredictionWorldManager::BindNetSend_IndependentRemote(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore)
{
	if (!RepProxy) 
		return;
	
	// Tick is based on ServerRecv_IndependentTick data
	const int32 ServerRecvIdx = DataStore->ServerRecv_IndependentTick.GetIndex(ID);
	RepProxy->NetSerializeFunc = [ID, this, DataStore, ServerRecvIdx](const FNetSerializeParams& P)
	{
		npEnsureSlow(P.Ar.IsSaving());
		TServerRecvData_Independent<ModelDef>& ServerRecv = DataStore->ServerRecv_IndependentTick.GetByIndexChecked(ServerRecvIdx);
		UE_NP_TRACE_SIM(ID.GetTraceID());
		ReplicatorType::NetSend(P, ID, DataStore, ServerRecv, &this->VariableTickState);
	};
}

// ---------------------------------------------------------------------------------------
//	Replay Bindings
//		-Binds RepProxy serialize lambda to a Replicator function
//		-3 versions: Fixed, Independent (Local Tick), Independent (Remote Ticked)
// ---------------------------------------------------------------------------------------

template<typename ReplicatorType, typename ModelDef>
void UNetworkPredictionWorldManager::BindReplayNetSendRecv_Fixed(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore, ENetRole NetRole)
{
	if (!RepProxy)
	{
		return;
	}

	const int32 ClientRecvIdx = DataStore->ClientRecv.GetIndex(ID);
	NpResizeAndSetBit(DataStore->ClientRecvBitMask, ClientRecvIdx, false);

	TClientRecvData<ModelDef>& ClientRecvData = DataStore->ClientRecv.GetByIndexChecked(ClientRecvIdx);
	InitClientRecvData<ModelDef>(ID, ClientRecvData, DataStore, NetRole);

	FFixedTickState* TickState = &this->FixedTickState;
	RepProxy->NetSerializeFunc = [ID, DataStore, ClientRecvIdx, TickState](const FNetSerializeParams& P)
	{
		if (P.Ar.IsLoading()) // Receiving replay data
		{
			DataStore->ClientRecvBitMask[ClientRecvIdx] = true;
			auto& ClientRecvData = DataStore->ClientRecv.GetByIndexChecked(ClientRecvIdx);

			UE_NP_TRACE_SIM(ClientRecvData.TraceID);
			ReplicatorType::NetRecv(P, ClientRecvData, DataStore, TickState);
		}
		else // Sending replay data
		{
			UE_NP_TRACE_SIM(ID.GetTraceID());
			ReplicatorType::NetSend(P, ID, DataStore, TickState);
		}
	};
}


template<typename ReplicatorType, typename ModelDef>
void UNetworkPredictionWorldManager::BindReplayNetSendRecv_IndependentLocal(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore, ENetRole NetRole)
{
	if (!RepProxy)
	{
		return;
	}

	const int32 ClientRecvIdx = DataStore->ClientRecv.GetIndex(ID);
	NpResizeAndSetBit(DataStore->ClientRecvBitMask, ClientRecvIdx, false);

	TClientRecvData<ModelDef>& ClientRecvData = DataStore->ClientRecv.GetByIndexChecked(ClientRecvIdx);
	InitClientRecvData<ModelDef>(ID, ClientRecvData, DataStore, NetRole);

	FVariableTickState* TickState = &this->VariableTickState;
	RepProxy->NetSerializeFunc = [ID, DataStore, ClientRecvIdx, TickState](const FNetSerializeParams& P)
	{
		if (P.Ar.IsLoading())	// Receiving replay data
		{
			DataStore->ClientRecvBitMask[ClientRecvIdx] = true;
			auto& ClientRecvData = DataStore->ClientRecv.GetByIndexChecked(ClientRecvIdx);

			UE_NP_TRACE_SIM(ClientRecvData.TraceID);
			ReplicatorType::NetRecv(P, ClientRecvData, DataStore, TickState);
		}
		else // Sending replay data
		{
			UE_NP_TRACE_SIM(ID.GetTraceID());
			ReplicatorType::NetSend(P, ID, DataStore, TickState);
		}
	};
}


template<typename ReplicatorType, typename ModelDef>
void UNetworkPredictionWorldManager::BindReplayNetSendRecv_IndependentRemote(FNetworkPredictionID ID, FReplicationProxy* RepProxy, TModelDataStore<ModelDef>* DataStore, ENetRole NetRole)
{
	if (!RepProxy)
	{
		return;
	}

	const int32 ClientRecvIdx = DataStore->ClientRecv.GetIndex(ID);
	NpResizeAndSetBit(DataStore->ClientRecvBitMask, ClientRecvIdx, false);

	TClientRecvData<ModelDef>& ClientRecvData = DataStore->ClientRecv.GetByIndexChecked(ClientRecvIdx);
	InitClientRecvData<ModelDef>(ID, ClientRecvData, DataStore, NetRole);

	FVariableTickState* TickState = &this->VariableTickState;

	const int32 ServerRecvIdx = DataStore->ServerRecv_IndependentTick.GetIndex(ID);

	RepProxy->NetSerializeFunc = [ID, DataStore, ClientRecvIdx, TickState, ServerRecvIdx](const FNetSerializeParams& P)
	{
		if (P.Ar.IsLoading())	// Receiving replay data
		{
			DataStore->ClientRecvBitMask[ClientRecvIdx] = true;
			auto& ClientRecvData = DataStore->ClientRecv.GetByIndexChecked(ClientRecvIdx);

			UE_NP_TRACE_SIM(ClientRecvData.TraceID);
			ReplicatorType::NetRecv(P, ClientRecvData, DataStore, TickState);
		}
		else // Sending replay data
		{
			TServerRecvData_Independent<ModelDef>& ServerRecv = DataStore->ServerRecv_IndependentTick.GetByIndexChecked(ServerRecvIdx);
			UE_NP_TRACE_SIM(ID.GetTraceID());
			ReplicatorType::NetSend(P, ID, DataStore, ServerRecv, TickState);
		}
	};
}

// -----------------

template<typename ModelDef>
void UNetworkPredictionWorldManager::InitClientRecvData(FNetworkPredictionID ID, TClientRecvData<ModelDef>& ClientRecvData, TModelDataStore<ModelDef>* DataStore, ENetRole NetRole)
{
	ClientRecvData.ID = ID; //Added By Kai For Delta Serialization Support
	ClientRecvData.TraceID = ID.GetTraceID();
	ClientRecvData.InstanceIdx = DataStore->Instances.GetIndexChecked(ID);
	ClientRecvData.FramesIdx = DataStore->Frames.GetIndexChecked(ID);
	ClientRecvData.NetRole = NetRole;
}

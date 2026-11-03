// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetworkPredictionPlayerControllerComponent.h"
#include "NetworkPredictionReplicationProxy.h"
#include "NetworkPredictionTrace.h"
#include "Services/NetworkPredictionInstanceData.h"


namespace NetworkPredictionCVars
{
	// *** Modified By Kai Time Dilation Support *** //
	NETSIM_DEVCVAR_SHIPCONST_INT(DisableTimeDilation, 0, "np.TimeDilation.Disable", "Time Dilation Effects Autonomous Proxy Client, Suggested by Server to slow down or speed us To Make Input Buffer healthy (Healthy input Buffer Always has Input And Is buffering as low as possible)");
	NETSIM_DEVCVAR_SHIPCONST_FLOAT(TimeDilationAmount, 0.01f, "np.TimeDilation.Amount", "Server-side CVar, Disable TimeDilation by setting to 0 | Default: 0.01 | Value is in percent where 0.01 = 1% dilation. Example: 1.0/0.01 = 100, meaning that over the time it usually takes to tick 100 Fixed steps we will tick 99 or 101 depending on if we dilate up or down.");
	NETSIM_DEVCVAR_SHIPCONST_INT(TimeDilationEscalation, 1, "np.TimeDilation.Escalation", "Server-side CVar, Dilate the time more depending on how many ticks we need to adjust. When set to false we use the set TimeDilationAmount and wait the amount of time it takes to perform correct the offset. When set to true we multiply the TimeDilationAmount with the buffer offset count which will correct the offset in one TimeDilationAmount cycle.");
	NETSIM_DEVCVAR_SHIPCONST_FLOAT(TimeDilationEscalationDecay, 0.05f, "np.TimeDilation.EscalationDecay", "Value is a multiplier, Default: 0.05. For each escalated TimeDilation amount, also decay by this much. Disable by setting to 0");
	NETSIM_DEVCVAR_SHIPCONST_FLOAT(TimeDilationEscalationDecayMax, 0.5f, "np.TimeDilation.EscalationDecayMax", "Value is a multiplier, Default: 0.5. The max decay value for escalated time dilation. Lower value means higher decay.");
	NETSIM_DEVCVAR_SHIPCONST_FLOAT(TimeDilationMax, 1.1f, "np.TimeDilation.Max", "Max value of the time dilation multiplier.");
	NETSIM_DEVCVAR_SHIPCONST_FLOAT(TimeDilationMin, 0.9f, "np.TimeDilation.Min", "Min value of the time dilation multiplier.");
	// *** END Modified By Kai Smoothing Support *** //
}
// InputService's job is to write InputCmds to a subscribed instance's FrameBuffer[PendingFrame].InputCmd.

class IInputService
{
public:

	virtual ~IInputService() = default;
	virtual void ProduceInput(int32 DeltaTimeMS, const float& InterpolationTimeMS) = 0;
	virtual void OnFixedInputReceived(const int32& ClientFrame,const float& InterpolationTime , const TArray<FSimulationReplicatedInput>& Inputs
		,UNetworkPredictionPlayerControllerComponent* InputHandler
		,FFixedTickState* TickState) = 0;
};

// Calls ProduceInput on driver to get local input
//	Requires valid FNetworkPredictionDriver::ProduceInput function
template<typename InModelDef>
class TLocalInputService : public IInputService
{
public:

	using ModelDef = InModelDef;
	using DriverType = typename ModelDef::Driver;
	using StateTypes = typename ModelDef::StateTypes;
	using InputType= typename StateTypes::InputType;


	TLocalInputService(TModelDataStore<ModelDef>* InDataStore)
		: DataStore(InDataStore) { }

	void RegisterInstance(FNetworkPredictionID ID)
	{
		npCheckfSlow(!std::is_void_v<InputType>, TEXT("ModelDef %s with null InputCmd type was registered for local input service."), ModelDef::GetName());

		TInstanceData<ModelDef>* InstanceData = DataStore->Instances.Find(ID);
		npCheckSlow(InstanceData);

		InstanceMap.Add((int32)ID, FInstance{ID.GetTraceID(), InstanceData->Info.View, InstanceData->Info.Driver});
	}

	void UnregisterInstance(FNetworkPredictionID ID)
	{
		InstanceMap.Remove((int32)ID);
	}

	void ProduceInput(int32 DeltaTimeMS , const float& InterpolationTimeMS) final override
	{
		for (auto& MapIt : InstanceMap)
		{
			FInstance& Instance = MapIt.Value;

			npCheckSlow(Instance.Driver);
			npCheckSlow(Instance.View);
			npCheckSlow(Instance.View->PendingInputCmd);

			FNetworkPredictionDriver<ModelDef>::ProduceInput(Instance.Driver, DeltaTimeMS, (InputType*)Instance.View->PendingInputCmd);
			// The interpolation time pointer is only bound for fixed tick instances (and after the first tick).
			if (Instance.View->InterpolationTimeMS)
			{
				*Instance.View->InterpolationTimeMS = InterpolationTimeMS;
			}

			UE_NP_TRACE_PRODUCE_INPUT(Instance.TraceID);
			UE_NP_TRACE_USER_STATE_INPUT(ModelDef, (InputType*)Instance.View->PendingInputCmd);
		}
	}

	void OnFixedInputReceived(const int32& ClientFrame,const float& InterpolationTime, const TArray<FSimulationReplicatedInput>& Inputs
		, UNetworkPredictionPlayerControllerComponent* InputHandler
		,FFixedTickState* TickState) final override
	{
	};

private:
	
	struct FInstance
	{
		int32 TraceID;
		FNetworkPredictionStateView* View;
		DriverType* Driver;
	};

	TSortedMap<int32, FInstance, TInlineAllocator<1>> InstanceMap;
	TModelDataStore<ModelDef>* DataStore;
};

// Pulls input from ServerRecvData. E.g: server side input buffering
template<typename InModelDef>
class TRemoteInputService : public IInputService
{
public:

	using ModelDef = InModelDef;
	using DriverType = typename ModelDef::Driver;
	using StateTypes = typename ModelDef::StateTypes;
	using InputType= typename StateTypes::InputType;

	TRemoteInputService(TModelDataStore<ModelDef>* InDataStore)
		: DataStore(InDataStore) { }

	static void SetMaxFaultLimit(int32 InMaxFaultLimit) 
	{ 
		MaxFaultLimit = InMaxFaultLimit; 
	}

	static void SetDesiredBufferedInputs(int32 InDesiredBufferedInputs) 
	{ 
		DesiredBufferedInputs = InDesiredBufferedInputs; 
	}

	void RegisterInstance(FNetworkPredictionID ID)
	{
		const int32 InstanceIndex = DataStore->Instances.GetIndexChecked(ID);
		
		const int32 ServerRecvIdx = DataStore->ServerRecv.GetIndex(ID);
		InstanceMap.Add((int32)ID, FInstance{ID.GetTraceID(), InstanceIndex, ServerRecvIdx});
	}

	void UnregisterInstance(FNetworkPredictionID ID)
	{
		InstanceMap.Remove(ID);
	}

	void OnFixedInputReceived(const int32& ClientFrame,const float& InterpolationTime,const TArray<FSimulationReplicatedInput>& Inputs
		,UNetworkPredictionPlayerControllerComponent* RPCHandler
		,FFixedTickState* TickState) final override
	{
		npEnsure(ClientFrame >= 0);
		for (auto& MapIt : InstanceMap)
		{
			FInstance& Instance = MapIt.Value;
			TInstanceData<ModelDef>& InstanceData = DataStore->Instances.GetByIndexChecked(Instance.InstanceIndex);
			if (RPCHandler != InstanceData.Info.RPCHandler || !InstanceData.Info.RPCHandler)
			{
				continue; 
			}
			//ToDO : Make const TArray<FSimulationReplicatedInput> a TMap, Loop through InstanceMap and Lookup in TMap
			for (const FSimulationReplicatedInput& Input : Inputs)
			{
				if (Input.ID != MapIt.Key)
				{
					continue;
				}

				UNetConnection* NetConnection = InstanceData.Info.RPCHandler->GetNetConnection();
				FNetBitReader Reader(NetConnection->PackageMap,Input.InputData.GetData(),Input.DataSize);
				FNetSerializeParams Params(Reader,NetConnection->PackageMap,EReplicationProxyTarget::ServerRPC);
			
				UE_NP_TRACE_SIM(Instance.TraceID);
				TServerRecvData_Fixed<ModelDef>& ServerRecvData = DataStore->ServerRecv.GetByIndexChecked(Instance.ServerRecvIdx);
				// Only the frames still in the ring matter. Before the client's first input there is no gap, just the start of the
				// stream, which the server enters DesiredBufferedInputs deep: those few frames get the default input.
				const bool bStreamStarted = RPCHandler->LastReceivedFrame != INDEX_NONE;
				const int32 FirstDroppedFrame = FMath::Max3(RPCHandler->LastReceivedFrame + 1, ClientFrame - ServerRecvData.InputBuffer.Capacity() + 1, 1);
				for (int32 DroppedFrame = FirstDroppedFrame; DroppedFrame < ClientFrame; ++DroppedFrame)
				{
					if (bStreamStarted)
					{
						UE_NP_TRACE_SYSTEM_FAULT("Gap in input stream detected on server. Client frames involved: LastConsumedFrame: %d LastRecvFrame: %d. DroppedFrame: %d", ServerRecvData.LastConsumedFrame, ServerRecvData.LastRecvFrame, DroppedFrame);
					}
					// FixedTick can't skip frames like independent, so copy previous input
					ServerRecvData.InputBuffer[DroppedFrame] = ServerRecvData.InputBuffer[DroppedFrame-1];
					FNetworkPredictionDriver<ModelDef>::PrepareRepeatedInput(ServerRecvData.InputBuffer[DroppedFrame].Value.Get());
				}
			
				FNetworkPredictionDriver<ModelDef>::NetSerialize(ServerRecvData.InputBuffer[ClientFrame].Value, Params); // 2. InputCmd
				ServerRecvData.InputBuffer[ClientFrame].Key = InterpolationTime;
				ServerRecvData.LastRecvFrame = ClientFrame;
				// Trace what we received
				const int32 ExpectedFrameDelay = ClientFrame - RPCHandler->LastConsumedFrame;
				const int32 ExpectedConsumeFrame = TickState->PendingFrame + ExpectedFrameDelay - 1;
				UE_NP_TRACE_NET_RECV(ExpectedConsumeFrame, ExpectedConsumeFrame * TickState->FixedStepMS);
				UE_NP_TRACE_USER_STATE_INPUT(ModelDef, ServerRecvData.InputBuffer[ClientFrame].Value.Get());
			}
		}
	};

	void Register(FNetworkPredictionID ID,UNetworkPredictionPlayerControllerComponent* Component)
	{
		Component->RegisterInputReceiver((int32)ID,
			[this](int32 Frame, float Interp, const FSimulationReplicatedInput& Input
				,const FFixedTickState& TickState)
			{
				//OnFixedInputReceived(Frame, Interp, Input,TickState);
			});
	}

	void Unregister(FNetworkPredictionID ID,UNetworkPredictionPlayerControllerComponent* Component)
	{
		Component->UnregisterInputReceiver(ID);
	}

	void ProduceInput(int32 DeltaTimeMS, const float& InterpolationTime) final override
	{
		for (auto& MapIt : InstanceMap)
		{
			FInstance& Remote = MapIt.Value;
			TInstanceData<ModelDef>& InstanceData = DataStore->Instances.GetByIndexChecked(Remote.InstanceIndex);
			npCheckSlow(InstanceData.Info.View);
			npCheckSlow(InstanceData.Info.View->PendingInputCmd);
			npCheckSlow(InstanceData.Info.View->PendingFrame >= 0);
			
			UE_NP_TRACE_PRODUCE_INPUT(Remote.TraceID);
			UE_NP_TRACE_PUSH_INPUT_FRAME(InstanceData.Info.View->PendingFrame);

			TServerRecvData_Fixed<ModelDef>& ServerRecvData = DataStore->ServerRecv.GetByIndexChecked(Remote.ServerRecvIdx);
			if (!InstanceData.Info.RPCHandler)
			{
				continue;
			}
			
			// Consume next InputCmd. The RPC handler holds its frame while starved (AdvanceLastConsumedFrame): the same input is
			// then used again, without its edge-triggered data.
			const bool bRepeatedInput = ServerRecvData.LastConsumedFrame == InstanceData.Info.RPCHandler->LastConsumedFrame;
			ServerRecvData.LastConsumedFrame = InstanceData.Info.RPCHandler->LastConsumedFrame;
			ServerRecvData.InputBuffer[InstanceData.Info.RPCHandler->LastConsumedFrame].Value.CopyTo((InputType*)InstanceData.Info.View->PendingInputCmd);
			if (bRepeatedInput)
			{
				FNetworkPredictionDriver<ModelDef>::PrepareRepeatedInput((InputType*)InstanceData.Info.View->PendingInputCmd);
			}
			if (InstanceData.Info.View->InterpolationTimeMS)
			{
				*InstanceData.Info.View->InterpolationTimeMS = ServerRecvData.InputBuffer[InstanceData.Info.RPCHandler->LastConsumedFrame].Key;
			}
			// Set Time Dilation Based On Consumed Input
			
			if (NetworkPredictionCVars::DisableTimeDilation() == 0)
			{
				UNetConnection* NetConnection = InstanceData.Info.RPCHandler->GetNetConnection();
				const int32 LastReceivedFrame = InstanceData.Info.RPCHandler->LastReceivedFrame;
				const int32 LastConsumedFrame = InstanceData.Info.RPCHandler->LastConsumedFrame;
				const float PingMS = NetConnection->AvgLag * 1000.f;
				// Jitter up to fixed tick time is covered by the 1 frame of fixed buffered frames.
				const float PacketLossFrames = NetConnection->InPacketsLost + NetConnection->OutPacketsLost;
				int32 JitterFrames = FMath::RoundToInt32(NetConnection->GetAverageJitterInMS() / DeltaTimeMS);
				const int32 DesiredBufferedFrames = DesiredBufferedInputs + JitterFrames + PacketLossFrames;
				int32 BufferOffset =  (LastReceivedFrame - LastConsumedFrame)  - DesiredBufferedFrames;// - ServerRecvState->InputFault;
				if (LastReceivedFrame == INDEX_NONE || LastConsumedFrame == INDEX_NONE)
				{
					BufferOffset = INT8_MAX;
				}

				/*if (BufferOffset == 0)
				{
					if (ServerRecvState->InputFault > 0)
					{
						ServerRecvState->InputFault = FMath::Max(ServerRecvState->InputFault - 1,0);
					}
					else if (ServerRecvState->InputFault < 0)
					{
						ServerRecvState->InputFault = FMath::Min(ServerRecvState->InputFault + 1,6);
					}
				}
				else if (BufferOffset > 0)
				{
					ServerRecvState->InputFault = FMath::Min(ServerRecvState->InputFault + 1,6);
				}
				else
				{
					ServerRecvState->InputFault = FMath::Max(ServerRecvState->InputFault - 1,0);
				}*/
			
				float CalculatedTimeDilation = 1.f;
				if (BufferOffset != 0 && BufferOffset != INT8_MAX)
				{
					const float TimeDilationDecay = FMath::Clamp(1.0f - (NetworkPredictionCVars::TimeDilationEscalationDecay() * FMath::Abs(BufferOffset)), NetworkPredictionCVars::TimeDilationEscalationDecayMax(), 1.0f);
					CalculatedTimeDilation = 1.0f + ((NetworkPredictionCVars::TimeDilationAmount() * BufferOffset) * TimeDilationDecay);
					CalculatedTimeDilation = FMath::Clamp(CalculatedTimeDilation, NetworkPredictionCVars::TimeDilationMin(), NetworkPredictionCVars::TimeDilationMax());
				}
				InstanceData.Info.RPCHandler->UpdateTimeDilation(CalculatedTimeDilation);
			}
			else
			{
				InstanceData.Info.RPCHandler->UpdateTimeDilation(1.f);
			}
			
			const int32 NumBufferedInputCmds = InstanceData.Info.RPCHandler->LastReceivedFrame - InstanceData.Info.RPCHandler->LastConsumedFrame;
			UE_NP_TRACE_BUFFERED_INPUT(NumBufferedInputCmds, false);
			UE_NP_TRACE_USER_STATE_INPUT(ModelDef, (InputType*)InstanceData.Info.View->PendingInputCmd);
		}
	}

private:
	
	struct FInstance
	{
		int32 TraceID;
		int32 InstanceIndex; // idx into DataStore->Instances::GetByIndexChecked
		
		int32 ServerRecvIdx;  // idx into DataStore->ServerRecv::GetByIndexChecked
		int32 FaultLimit = 2; // InputBuffer must have >= this number of unprocessed commands before resuming consumption
		bool bFault=true;	  // Recently starved on input buffer, wait until we reach FaultLimit before consuming input again. (Note you start out in fault to let the buffer fill up)
	};

	TSortedMap<int32, FInstance> InstanceMap;
	TModelDataStore<ModelDef>* DataStore;
	
	inline static int32 MaxFaultLimit = 6;

	static void EatCmd(const FNetSerializeParams& P)
	{
		TConditionalState<InputType> Empty;
		FNetworkPredictionDriver<ModelDef>::NetSerialize(Empty, P);  // 2. InputCmd	
	}
	
	inline static int32 DesiredBufferedInputs = 4;
};


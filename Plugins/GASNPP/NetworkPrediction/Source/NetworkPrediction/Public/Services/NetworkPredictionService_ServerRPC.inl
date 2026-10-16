// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetworkPredictionDriver.h"
#include "NetworkPredictionPlayerControllerComponent.h"
#include "NetworkPredictionSerialization.h"
#include "Services/NetworkPredictionInstanceData.h"

// The ServerRPCService's job is to tell the Driver to call the Server RPC that sends InputCmds upstream.
// This happens every frame on autonomous proxy clients.
//
// The implementation here is very basic. It may make sense to have some scalability at this level,
// options for throttling send rate, etc.

class IServerRPCService
{
public:

	virtual ~IServerRPCService() = default;
	virtual void CallServerRPC(float DeltaTimeSeconds) = 0;
};

template<typename InModelDef>
class TServerRPCService : public IServerRPCService	
{
public:

	using ModelDef = InModelDef;
	using DriverType = typename ModelDef::Driver;

	TServerRPCService(TModelDataStore<ModelDef>* InDataStore)
		: DataStore(InDataStore) { }

	void RegisterInstance(FNetworkPredictionID ID)
	{
		TInstanceData<ModelDef>* InstanceData = DataStore->Instances.Find(ID);
		npCheckSlow(InstanceData);

		Instances.Add((int32)ID, FInstance{InstanceData->TraceID, InstanceData->Info.Driver});
	}

	void UnregisterInstance(FNetworkPredictionID ID)
	{
		Instances.Remove((int32)ID);
	}

	void CallServerRPC(float DeltaTimeSeconds) final override
	{
		for (auto& MapIt : Instances)
		{
			FInstance& Instance = MapIt.Value;
			FNetworkPredictionDriver<ModelDef>::CallServerRPC(Instance.Driver);
		}
	}

private:

	// The vast majority of the time there will be <= 1 instances that wants to call the ServerRPC.
	// Only split screen type situations will require more.
	struct FInstance
	{
		int32 TraceID;
		DriverType* Driver;
	};

	TSortedMap<int32, FInstance, TInlineAllocator<1>> Instances;
	TModelDataStore<ModelDef>* DataStore;
};


class IFixedServerRPCService
{
public:

	virtual ~IFixedServerRPCService() = default;
	virtual void CallServerRPC(float DeltaTimeSeconds) = 0;
	virtual void AddInputToHandler(const int32& Frame) = 0;
};

template<typename InModelDef>
class TFixedServerRPCService : public IFixedServerRPCService	
{
public:

	using ModelDef = InModelDef;
	using DriverType = typename ModelDef::Driver;
	using StateTypes = typename ModelDef::StateTypes;
	using InputType = typename StateTypes::InputType;

	TFixedServerRPCService(TModelDataStore<ModelDef>* InDataStore)
		: DataStore(InDataStore) { }

	void RegisterInstance(FNetworkPredictionID ID)
	{
		TInstanceData<ModelDef>* InstanceData = DataStore->Instances.Find(ID);
		npCheckSlow(InstanceData);
		int32 FramesID = DataStore->Frames.GetIndex(ID);
		const int32 InstanceIndex = DataStore->Instances.GetIndexChecked(ID);

		Instances.Add((int32)ID, FInstance{InstanceData->TraceID, FramesID,InstanceIndex,InstanceData->Info.Driver});
	}

	void UnregisterInstance(FNetworkPredictionID ID)
	{
		Instances.Remove((int32)ID);
	}

	void CallServerRPC(float DeltaTimeSeconds) final override
	{
		for (auto& MapIt : Instances)
		{
			FInstance& Instance = MapIt.Value;
			FNetworkPredictionDriver<ModelDef>::CallServerRPC(Instance.Driver);
		}
	}

	void AddInputToHandler(const int32& Frame) final override
	{
		for (auto& MapIt : Instances)
		{
			FInstance& Instance = MapIt.Value;
			UE_NP_TRACE_SIM(Instance.TraceID);
			TInstanceData<ModelDef>& InstanceData = DataStore->Instances.GetByIndexChecked(Instance.InstanceIndex);
			UNetworkPredictionPlayerControllerComponent* RPCHandler = InstanceData.Info.RPCHandler;
			if (RPCHandler && RPCHandler->GetNetConnection())
			{
				UPackageMap* Map = RPCHandler->GetNetConnection()->PackageMap;
				FNetBitWriter TempWriter(Map,0);
				FNetSerializeParams Params(TempWriter,Map,EReplicationProxyTarget::ServerRPC);
				TInstanceFrameState<ModelDef>& Frames = DataStore->Frames.GetByIndexChecked(Instance.FramesID);
				if (NetworkPredictionCVars::ForceSendDefaultInputCommands())
				{
					// for debugging, send blank default input instead of what we've produced locally
					static TConditionalState<InputType> DefaultInputCmd;
					FNetworkPredictionDriver<ModelDef>::NetSerialize(DefaultInputCmd, Params); 
				}
				else
				{
					FNetworkPredictionDriver<ModelDef>::NetSerialize(Frames.Buffer[Frame].InputCmd, Params); // 2. InputCmd
				}

				uint32 DataSize = (uint32)TempWriter.GetNumBits();
				RPCHandler->AddInputToSend(MapIt.Key, DataSize,*TempWriter.GetBuffer());
				RPCHandler->InterpolationTimeMS = Frames.Buffer[Frame].InterpolationTimeMS;
			}
		}
		
	}

private:

	// The vast majority of the time there will be <= 1 instances that wants to call the ServerRPC.
	// Only split screen type situations will require more.
	struct FInstance
	{
		int32 TraceID;
		int32 FramesID;
		int32 InstanceIndex;
		DriverType* Driver;
	};

	TSortedMap<int32, FInstance, TInlineAllocator<1>> Instances;
	TModelDataStore<ModelDef>* DataStore;
};
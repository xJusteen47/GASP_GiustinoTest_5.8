// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetworkPredictionCVars.h"
#include "NetworkPredictionTickState.h"
#include "NetworkPredictionTrace.h"
#include "Services/NetworkPredictionInstanceData.h"


namespace NetworkPredictionCVars
{
	NETSIM_DEVCVAR_SHIPCONST_INT(DisableSmoothing, 0, "np.Smoothing.Disable", "Disables smoothing and just finalizes using the latest simulation state");
}


// ------------------------------------------------------------------------------
//	FixedTick Smoothing
//
//  This first pass service simply performs interpolation between the most recent fixed tick states
//  and passes the smoothed state to the driver to handle however it chooses. 
//	
//  Future improvements could include smoothing out corrections after a reconcile, and expanding
//  that to smoothing for Independent ticking mode.
// ------------------------------------------------------------------------------
class IFixedSmoothingService
{
public:

	virtual ~IFixedSmoothingService() = default;
	virtual void UpdateSmoothing(const FServiceTimeStep& ServiceStep,const FFixedTickState* TickState) = 0;
	virtual void FinalizeSmoothingFrame(const FFixedTickState* TickState) = 0;
};

template<typename InModelDef>
class TFixedSmoothingService : public IFixedSmoothingService
{
public:

	using ModelDef = InModelDef;
	using StateTypes = typename ModelDef::StateTypes;
	using SyncType = typename StateTypes::SyncType;
	using AuxType = typename StateTypes::AuxType;
	using SyncAuxType = TSyncAuxPair<StateTypes>;
	

	TFixedSmoothingService(TModelDataStore<ModelDef>* InDataStore)
		: DataStore(InDataStore) { }

	void RegisterInstance(FNetworkPredictionID ID)
	{
		const int32 InstanceDataIdx = DataStore->Instances.GetIndex(ID);

		const int32 PreviousSize = Instances.GetAllocatedSize();

		FSparseArrayAllocationInfo AllocInfo = Instances.InsertUninitialized(InstanceDataIdx);

		// When the array resizes, other instances may have dangling pointers to Presentation/PrevPresentation views and will need updated
		const bool bNeedsAllViewsUpdated = PreviousSize != Instances.GetAllocatedSize();

		new (AllocInfo.Pointer) FInstance(ID.GetTraceID(), InstanceDataIdx, DataStore->Frames.GetIndex(ID));

		if (bNeedsAllViewsUpdated)
		{
			UpdateAllInstancePresentationViews();
		}
		else
		{
			TInstanceData<ModelDef>& InstanceData = DataStore->Instances.GetByIndexChecked(InstanceDataIdx);

			// Point the PresentationView to our managed state. Note this only has to be done once
			FInstance* InternalInstance = (FInstance*)AllocInfo.Pointer;
			InstanceData.Info.View->UpdatePresentationView(InternalInstance->SyncState, InternalInstance->AuxState);
			InstanceData.Info.View->UpdatePrevPresentationView(InternalInstance->LastSyncState, InternalInstance->LastAuxState);
		}
	}

	void UnregisterInstance(FNetworkPredictionID ID)
	{
		const int32 Idx = DataStore->Instances.GetIndex(ID);
		TInstanceData<ModelDef>& InstanceData = DataStore->Instances.GetByIndexChecked(Idx);
		InstanceData.Info.View->ClearPresentationView();
		Instances.RemoveAt(Idx);
	}
	static void SetSmoothingSpeed(const float& InSmoothingSpeed) 
	{ 
		SmoothingSpeed = InSmoothingSpeed; 
	}

	virtual void UpdateSmoothing(const FServiceTimeStep& ServiceStep, const FFixedTickState* TickState) final override
	{
		const int32 OutputFrame = ServiceStep.LocalOutputFrame;
		const int32 InputFrame = ServiceStep.LocalInputFrame;
		// GAS-NPP fork (2026-11-28, lag thread): smoothing cost by model in Insights
		TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(ModelDef::GetName());
		for (auto& It : Instances)
		{
			FInstance& Instance = It;
			
			TInstanceFrameState<ModelDef>& Frames = DataStore->Frames.GetByIndexChecked(Instance.FramesIdx);
			TInstanceData<ModelDef>& InstanceData = DataStore->Instances.GetByIndexChecked(Instance.InstanceIdx);

			typename TInstanceFrameState<ModelDef>::FFrame& InputFrameData = Frames.Buffer[InputFrame];
			typename TInstanceFrameState<ModelDef>::FFrame& OutputFrameData = Frames.Buffer[OutputFrame];
			
			if (NetworkPredictionCVars::DisableSmoothing() != 0 || !Instance.bHasTwoFrames)
			{
				CopySmoothedSync(OutputFrameData.SyncState, Instance.SyncState);
				OutputFrameData.AuxState.CopyTo(Instance.AuxState);
				CopySmoothedSync(OutputFrameData.SyncState, Instance.LastSyncState);
				OutputFrameData.AuxState.CopyTo(Instance.LastAuxState);
				Instance.bHasTwoFrames = true;
				continue;
			}

			// TODO: could improve this with a double-buffer that alternates, eliminating one copy
			// Set Last Presentation States To Current
			CopySmoothedSync(Instance.SyncState, Instance.LastSyncState);
			Instance.AuxState.CopyTo(Instance.LastAuxState);

			// Edited By kai : Add correction Smoothing
			TConditionalState<SyncType> DeltaSynState;
			TConditionalState<AuxType> DeltaAuxState;
			FNetworkPredictionDriver<ModelDef>::GetSmoothingStateDelta(InstanceData.Info.Driver,InputFrameData.SyncState, InputFrameData.AuxState, Instance.LastSyncState, Instance.LastAuxState,
				DeltaSynState, DeltaAuxState);
			float Scale =  FMath::Clamp(1.f - SmoothingSpeed,0.f,1.f);
			FNetworkPredictionDriver<ModelDef>::GetSmoothingStateScaled(InstanceData.Info.Driver,DeltaSynState, DeltaAuxState,Scale, DeltaSynState, DeltaAuxState);

			// Set Sync State To The Pending State
			CopySmoothedSync(OutputFrameData.SyncState, Instance.SyncState);
			OutputFrameData.AuxState.CopyTo(Instance.AuxState);

			FNetworkPredictionDriver<ModelDef>::GetSmoothingStateUnion(InstanceData.Info.Driver,OutputFrameData.SyncState, OutputFrameData.AuxState, DeltaSynState, DeltaAuxState, 
				Instance.SyncState, Instance.AuxState);
		}
	}
	
	virtual void FinalizeSmoothingFrame(const FFixedTickState* TickState) final override
	{
		const float RemainingTime = TickState->UnspentTimeMS;
		const float TimeStep = TickState->FixedStepMS;
		const float Alpha = FMath::Clamp(RemainingTime / TimeStep,0.f,1.f);
		
		if (NetworkPredictionCVars::DisableSmoothing() != 0)
		{
			for (auto& It : Instances)
			{
				const FInstance& Instance = It;
				{
					// Push non-smoothed results to driver
					const TInstanceData<ModelDef>& InstanceData = DataStore->Instances.GetByIndexChecked(Instance.InstanceIdx);
					FNetworkPredictionDriver<ModelDef>::FinalizeSmoothingFrame(InstanceData.Info.Driver, Instance.SyncState.Get(), Instance.AuxState.Get());
				}
			}

			return;
		}

		
		TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(ModelDef::GetName());
		for (auto& It : Instances)
		{
			const FInstance& Instance = It;
			
			// Interpolate and push smoothed state to driver
			{
				TConditionalState<SyncType> SmoothedSyncState;
				TConditionalState<AuxType> SmoothedAuxState;

				FNetworkPredictionDriver<ModelDef>::Interpolate(SyncAuxType{Instance.LastSyncState.Get(), Instance.LastAuxState.Get()}, SyncAuxType{Instance.SyncState.Get(), Instance.AuxState.Get()},
					Alpha, SmoothedSyncState, SmoothedAuxState);

				// Push smoothed results to driver
				const TInstanceData<ModelDef>& InstanceData = DataStore->Instances.GetByIndexChecked(Instance.InstanceIdx);
				FNetworkPredictionDriver<ModelDef>::FinalizeSmoothingFrame(InstanceData.Info.Driver, SmoothedSyncState, SmoothedAuxState);
				
			}
		}
	}

private:
	// GAS-NPP fork (2026-11-28, lag thread): a model whose driver presents only part of its sync state (the ability system: the montage)
	// declares ModelDef::CopySmoothedSync(From, To) and the smoothing copies that part. The full copy, four times per instance per step,
	// cost the ability system ~0.2 ms per pawn per step for a state nobody presented. Models without it keep the full copy
	template<typename T>
	static void CopySmoothedSync(const TConditionalState<T>& From, TConditionalState<T>& To)
	{
		if constexpr (requires(const T& A, T& B) { ModelDef::CopySmoothedSync(A, B); })
		{
			ModelDef::CopySmoothedSync(*From.Get(), *To.Get());
		}
		else
		{
			From.CopyTo(To);
		}
	}

	void UpdateAllInstancePresentationViews()
	{
		for (auto& It : Instances)
		{
			FInstance& InternalInstance = It;

			TInstanceData<ModelDef>& RemapInstanceData = DataStore->Instances.GetByIndexChecked(InternalInstance.InstanceIdx);
			RemapInstanceData.Info.View->UpdatePresentationView(InternalInstance.SyncState, InternalInstance.AuxState);
			RemapInstanceData.Info.View->UpdatePrevPresentationView(InternalInstance.LastSyncState, InternalInstance.LastAuxState);
		}
	}

	struct FInstance
	{
		FInstance(int32 InTraceID, int32 InInstanceIdx, int32 InFramesIdx)
			: TraceID(InTraceID), InstanceIdx(InInstanceIdx), FramesIdx(InFramesIdx) {}

		int32 TraceID;
		int32 InstanceIdx;
		int32 FramesIdx;
		uint8 bHasTwoFrames : 1 = false;
		
		// Latest states to smooth between. Stored here so that we can maintain FNetworkPredictionStateView to them
		TConditionalState<SyncType> SyncState;
		TConditionalState<AuxType> AuxState;
		TConditionalState<SyncType> LastSyncState;
		TConditionalState<AuxType> LastAuxState;
		
	};
	
	TSparseArray<FInstance> Instances; // Indices are shared with DataStore->ClientRecv
	
	TModelDataStore<ModelDef>* DataStore;

	inline static float SmoothingSpeed = 0.2f; // ToDo @Kai : Bring this back to settings
};


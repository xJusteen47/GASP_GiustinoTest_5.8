// Minion runtime (client presentation, M8 G7).

#include "Present/GSTN_MinionPresentationSubsystem.h"

#include "Data/GSTN_MinionDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GSTNMinionCoreModule.h"
#include "HAL/IConsoleManager.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Hits/GSTN_ReactionTypes.h"
#include "Modules/ModuleManager.h"
#include "Net/GSTN_MinionClientRegistry.h"
#include "Net/GSTN_MinionNetCodec.h"
#include "Net/GSTN_MinionPack.h"
#include "NetworkPredictionWorldManager.h"
#include "Present/GSTN_MinionPuppet.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Store/GSTN_MinionStore.h"
#include "World/GSTN_CombatWorldSubsystem.h"

namespace
{
	TAutoConsoleVariable<int32> CVarMinionPresentMaxVisible(
		TEXT("gstn.Minion.Present.MaxVisible"), 60,
		TEXT("Most minion puppets shown at once (MaxVisiblePuppets, combat-m8-minions.md 8.4), nearest to the local view first. The others keep their\n")
		TEXT("presentation state and stay hittable, without a puppet."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarMinionPresentLogFlash(
		TEXT("gstn.Minion.Present.LogFlash"), 0,
		TEXT("1 logs one 'MinionFlash' line per predicted hit and per cancelled prediction on a minion (victim, move start, hit group, the ")
		TEXT("minion's displayed health and flash state). Diagnostics for the harness; off by default."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarMinionPresentEnable(
		TEXT("gstn.Minion.Present.Enable"), 1,
		TEXT("0 stops the minion presentation tick (puppets stay where they are). For A/B measurements of spike 7."),
		ECVF_Default);

	FAutoConsoleCommandWithWorldAndArgs CmdPresentDump(
		TEXT("gstn.Minion.Present.Dump"),
		TEXT("Logs the minion presentation of this world: one 'MinionPresent puppet' line per presented minion (id, displayed position and frame,\n")
		TEXT("tier, flash state) and one 'MinionPresent summary' line with the pool and hit presentation counters."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& /*Args*/, UWorld* World)
		{
			if (const UGSTN_MinionPresentationSubsystem* Present = World ? World->GetSubsystem<UGSTN_MinionPresentationSubsystem>() : nullptr)
			{
				Present->DumpToLog();
			}
			else
			{
				UE_LOG(LogGSTNMinion, Log, TEXT("MinionPresent none world=%s (no presentation subsystem: dedicated server or editor world)"), *GetNameSafe(World));
			}
		}));

	const TCHAR* PresentModeName(EGSTN_MinionDisplayMode Mode)
	{
		switch (Mode)
		{
		case EGSTN_MinionDisplayMode::Interpolated: return TEXT("interp");
		case EGSTN_MinionDisplayMode::Extrapolated: return TEXT("extrap");
		case EGSTN_MinionDisplayMode::Held: return TEXT("held");
		case EGSTN_MinionDisplayMode::DeadReckoned: return TEXT("deadreckon");
		case EGSTN_MinionDisplayMode::Direct: return TEXT("direct");
		default: return TEXT("none");
		}
	}

	const TCHAR* PresentSourceName(EGSTN_MinionPresentSource Source)
	{
		switch (Source)
		{
		case EGSTN_MinionPresentSource::Store: return TEXT("store");
		case EGSTN_MinionPresentSource::Registry: return TEXT("registry");
		default: return TEXT("none");
		}
	}

	const TCHAR* PresentRenderSourceName(EGSTN_MinionRenderTimeSource Source)
	{
		switch (Source)
		{
		case EGSTN_MinionRenderTimeSource::NetworkPrediction: return TEXT("np");
		case EGSTN_MinionRenderTimeSource::FixedDelay: return TEXT("fixed");
		default: return TEXT("none");
		}
	}

	bool PresentIsDead(uint8 ActionState)
	{
		return ActionState == static_cast<uint8>(EGSTN_MinionActionState::Dead);
	}

	bool PresentIsDyingOrDead(uint8 ActionState)
	{
		return PresentIsDead(ActionState) || ActionState == static_cast<uint8>(EGSTN_MinionActionState::Dying);
	}

	/** The fighters' action state vocabulary for a client hittable entry (the same mapping as the authority's snapshot entry) */
	EGSTN_ActionState PresentToFighterActionState(uint8 ActionState, uint8 MoveId)
	{
		switch (static_cast<EGSTN_MinionActionState>(ActionState))
		{
		case EGSTN_MinionActionState::Hitstop:
		case EGSTN_MinionActionState::Hitstun:
		case EGSTN_MinionActionState::PushedBack:
		case EGSTN_MinionActionState::Launched:
		case EGSTN_MinionActionState::Knockdown:
		case EGSTN_MinionActionState::GetUp:
			return EGSTN_ActionState::Hitstun;
		case EGSTN_MinionActionState::Dying:
		case EGSTN_MinionActionState::Dead:
			return EGSTN_ActionState::KO;
		default:
			return MoveId >= 1 && MoveId <= GSTNMinion::MaxMoves ? EGSTN_ActionState::InTimeline : EGSTN_ActionState::Actionable;
		}
	}

	/** Once per process: what the animation budget stack of 8.4 has on this machine */
	void PresentLogAnimationBudgetOnce()
	{
		static bool bLogged = false;
		if (bLogged)
		{
			return;
		}
		bLogged = true;
		const bool bLoaded = FModuleManager::Get().IsModuleLoaded(TEXT("AnimationBudgetAllocator"));
		UE_LOG(LogGSTNMinion, Log, TEXT("MinionPresent animation budget allocator %s: puppets use update-rate optimization and OnlyTickPoseWhenRendered"),
			bLoaded ? TEXT("loaded but not wired (needs a plugin dependency of GSTNMinion)") : TEXT("not enabled"));
	}
}

UGSTN_MinionPresentationSubsystem* UGSTN_MinionPresentationSubsystem::GetActive(const UWorld* World)
{
	UGSTN_MinionPresentationSubsystem* Subsystem = World ? World->GetSubsystem<UGSTN_MinionPresentationSubsystem>() : nullptr;
	return Subsystem && Subsystem->bActive ? Subsystem : nullptr;
}

bool UGSTN_MinionPresentationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Nothing renders on a dedicated server (a PIE dedicated server world is caught at begin play: it shares the process)
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

bool UGSTN_MinionPresentationSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_MinionPresentationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const ENetMode NetMode = InWorld.GetNetMode();
	if (NetMode != NM_DedicatedServer)
	{
		Activate(NetMode == NM_Client);
	}
}

void UGSTN_MinionPresentationSubsystem::Deinitialize()
{
	if (bClientProvider)
	{
		if (UGSTN_CombatWorldSubsystem* CombatWorld = UGSTN_CombatWorldSubsystem::Get(GetWorld()))
		{
			CombatWorld->UnregisterProvider(this);
		}
		bClientProvider = false;
	}
	bActive = false;
	Records.Reset();
	FreePuppets.Reset();
	FreeShatters.Reset();
	AllPuppets.Reset();
	AllShatters.Reset();
	Super::Deinitialize();
}

void UGSTN_MinionPresentationSubsystem::ActivateForTests(bool bAsClient)
{
	Activate(bAsClient);
}

void UGSTN_MinionPresentationSubsystem::Activate(bool bAsClient)
{
	bActive = true;
	PresentLogAnimationBudgetOnce();
	if (bAsClient && !bClientProvider)
	{
		// The authority's minion subsystem never activates on a client, so the Minion kind's provider slot is free here
		if (UGSTN_CombatWorldSubsystem* CombatWorld = UGSTN_CombatWorldSubsystem::Get(GetWorld()))
		{
			bClientProvider = CombatWorld->RegisterProvider(this);
		}
	}
}

TStatId UGSTN_MinionPresentationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGSTN_MinionPresentationSubsystem, STATGROUP_Tickables);
}

int32 UGSTN_MinionPresentationSubsystem::GetProviderId() const
{
	// The same id as the authority's provider: a machine has one or the other, never both
	return GSTNMinion::SnapshotProviderId;
}

int32 UGSTN_MinionPresentationSubsystem::GetMaxVisible() const
{
	return MaxVisibleOverrideForTests >= 0 ? MaxVisibleOverrideForTests : FMath::Max(CVarMinionPresentMaxVisible.GetValueOnGameThread(), 0);
}

void UGSTN_MinionPresentationSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const UWorld* World = GetWorld();
	if (!bActive || !World || CVarMinionPresentEnable.GetValueOnGameThread() == 0)
	{
		return;
	}
	TickPresentation(World->GetTimeSeconds());
}

void UGSTN_MinionPresentationSubsystem::TickPresentation(double NowSeconds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GSTN_MinionPresentation);
	if (!bActive)
	{
		return;
	}
	++TickSerial;
	const UWorld* World = GetWorld();

	if (const UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(World))
	{
		Stats.Source = EGSTN_MinionPresentSource::Store;
		Stats.RenderSource = EGSTN_MinionRenderTimeSource::None;
		SyncFromStore(*Minions, NowSeconds);
	}
	else if (const UGSTN_MinionClientRegistry* Registry = World ? World->GetSubsystem<UGSTN_MinionClientRegistry>() : nullptr)
	{
		Stats.Source = EGSTN_MinionPresentSource::Registry;
		UpdateReferenceFrame();
		SyncFromRegistry(*Registry, NowSeconds);
		UpdateRenderTime(NowSeconds);
		for (TPair<int32, FGSTN_MinionPresentRecord>& Pair : Records)
		{
			FGSTN_MinionPresentRecord& Record = Pair.Value;
			Record.Pose = Record.Interpolator.Evaluate(Stats.RenderFrame, NowSeconds, Smoothing);
			Record.Tier = Record.Interpolator.GetInferredTier(Smoothing, NowSeconds);
		}
	}
	else
	{
		Stats.Source = EGSTN_MinionPresentSource::None;
	}

	// Minions the source no longer has: puppet back to the pool, record gone
	TArray<int32, TInlineAllocator<16>> Gone;
	for (const TPair<int32, FGSTN_MinionPresentRecord>& Pair : Records)
	{
		if (Pair.Value.SeenSerial != TickSerial)
		{
			Gone.Add(Pair.Key);
		}
	}
	for (const int32 Key : Gone)
	{
		RemoveRecord(Key);
	}

	UpdatePredictedHits(NowSeconds);
	for (TPair<int32, FGSTN_MinionPresentRecord>& Pair : Records)
	{
		Pair.Value.Flash.Tick(NowSeconds);
	}
	UpdateVisibleSet(NowSeconds);
	ApplyToPuppets(NowSeconds);
	TickShatters(NowSeconds);

	Stats.Active = Records.Num();
	Stats.Pooled = GetNumPooledPuppets();
	Stats.ShattersPooled = FreeShatters.Num();
}

void UGSTN_MinionPresentationSubsystem::SyncFromStore(const UGSTN_MinionCombatSubsystem& Minions, double NowSeconds)
{
	// Listen host (4d.1): the store as the batch left it, shown as is. The host's puppets are one batch old at most and need no buffer
	const FGSTN_MinionStore& Store = Minions.GetStore();
	const int32 Frame = Minions.GetLastBatchFrame();
	for (int32 Slot = Store.NextAliveSlot(0); Slot != INDEX_NONE; Slot = Store.NextAliveSlot(Slot + 1))
	{
		FGSTN_MinionPresentRecord* Record = Records.Find(Slot);
		if (Record && Record->Generation != Store.Generation[Slot])
		{
			// Another minion in a reused slot: nothing of the old one carries over
			RemoveRecord(Slot);
			Record = nullptr;
		}
		if (!Record)
		{
			Record = &Records.Add(Slot);
			Record->Key = Slot;
			Record->Slot = Slot;
			Record->Generation = Store.Generation[Slot];
		}
		Record->SeenSerial = TickSerial;
		Record->TypeIndex = Store.TypeIndex[Slot];
		Record->Definition = Minions.GetDefinition(Record->TypeIndex);
		Record->Tier = static_cast<EGSTN_MinionLODTier>(Store.LODTier[Slot]);
		Record->HealthFraction = Store.MaxHealth[Slot] > 0.f ? FMath::Clamp(Store.Health[Slot] / Store.MaxHealth[Slot], 0.f, 1.f) : 1.f;

		FGSTN_MinionDisplayPose& Pose = Record->Pose;
		Pose.Position = Store.Position[Slot];
		Pose.YawDegrees = Store.YawDegrees[Slot];
		Pose.Frame = Frame;
		Pose.ActionState = Store.ActionState[Slot];
		Pose.MoveId = Store.MoveId[Slot];
		Pose.MoveStartFrame = Store.MoveStartFrame[Slot];
		Pose.Speed = Store.Speed[Slot];
		Pose.Mode = EGSTN_MinionDisplayMode::Direct;
		Pose.bBlending = false;

		Record->Flash.ObserveCounter(Store.HitCounter[Slot], NowSeconds, FlashParams);
	}
}

void UGSTN_MinionPresentationSubsystem::UpdateReferenceFrame()
{
	const UNetworkPredictionWorldManager* NetworkPrediction = GetWorld() ? GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>() : nullptr;
	if (!NetworkPrediction)
	{
		return;
	}
	const FFixedTickState& TickState = NetworkPrediction->GetFixedTickState();
	if (TickState.FixedStepMS > 0)
	{
		Smoothing.FixedStepMs = static_cast<float>(TickState.FixedStepMS);
	}
	// The newest server frame NP received (interpolated proxies, else the local autonomous proxy): stamps are rebuilt nearest to it
	const int32 Latest = FMath::Max(TickState.Interpolation.LatestRecvFrameSP, TickState.Interpolation.LatestRecvFrameAP);
	if (Latest != INDEX_NONE)
	{
		ReferenceFrame = Latest;
	}
}

void UGSTN_MinionPresentationSubsystem::SyncFromRegistry(const UGSTN_MinionClientRegistry& Registry, double NowSeconds)
{
	for (const TPair<int32, FGSTN_MinionClientItem>& Pair : Registry.GetItems())
	{
		const FGSTN_MinionClientItem& Item = Pair.Value;
		FGSTN_MinionPresentRecord* Record = Records.Find(Pair.Key);
		if (!Record)
		{
			Record = &Records.Add(Pair.Key);
			Record->Key = Pair.Key;
		}
		Record->SeenSerial = TickSerial;
		if (const AGSTN_MinionPack* Pack = Item.Pack.Get())
		{
			Record->Slot = Pack->GetNetSlotOfItem(Item.ItemId);
		}
		if (Item.Updates == Record->SeenUpdates)
		{
			continue;
		}
		Record->SeenUpdates = Item.Updates;

		// The type travels with every update (G6), so a record always knows it
		if (Item.Wire.bHasType)
		{
			Record->TypeIndex = Item.Wire.TypeIndex;
			Record->Definition = Registry.GetDefinition(Record->TypeIndex);
		}
		FGSTN_MinionNetQuantization Quant;
		Quant.WorldOrigin = Registry.GetWorldOrigin();
		if (const UGSTN_MinionDefinition* Definition = Record->Definition.Get())
		{
			Quant.MaxSpeed = Definition->Stats.RunSpeed;
		}
		// No NP frame yet: rebuild stamps nearest to the newest one this client already has (consistent between items within a wrap)
		const int32 Reference = ReferenceFrame != INDEX_NONE ? ReferenceFrame : (FallbackNewestFrame != INDEX_NONE ? FallbackNewestFrame : 0);
		const FGSTN_MinionNetState State = FGSTN_MinionNetCodec::Dequantize(Item.Wire, Quant, Reference);

		FGSTN_MinionPresentSample Sample;
		Sample.Frame = State.FrameStamp;
		Sample.Position = State.Position;
		Sample.YawDegrees = State.YawDegrees;
		Sample.Speed = State.Speed;
		Sample.ActionState = State.ActionState;
		Sample.MoveId = State.MoveId;
		Sample.MoveStartFrame = State.MoveStartFrame;
		Sample.HitCounter = State.HitCounter;
		Sample.Flags = State.Flags;

		if ((Sample.Flags & GSTNMinion::NetFlags::Teleport) != 0 && Record->Interpolator.Num() > 0)
		{
			// A new minion under an id this client already presented: its counter and death are its own
			Record->Flash = FGSTN_MinionHitFlash();
			Record->bShattered = false;
		}
		Record->Interpolator.AddSample(Sample, NowSeconds);
		Record->HealthFraction = State.HealthFraction;
		Record->Flash.ObserveCounter(State.HitCounter, NowSeconds, FlashParams);

		if (FallbackNewestFrame == INDEX_NONE || State.FrameStamp > FallbackNewestFrame)
		{
			FallbackNewestFrame = State.FrameStamp;
			FallbackNewestSeconds = NowSeconds;
		}
	}
}

void UGSTN_MinionPresentationSubsystem::UpdateRenderTime(double NowSeconds)
{
	if (RenderFrameOverrideForTests.IsSet())
	{
		Stats.RenderFrame = RenderFrameOverrideForTests.GetValue();
		Stats.RenderSource = EGSTN_MinionRenderTimeSource::None;
		return;
	}
	const UNetworkPredictionWorldManager* NetworkPrediction = GetWorld() ? GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>() : nullptr;
	if (NetworkPrediction)
	{
		const FFixedTickState::FInterpolationState& Interpolation = NetworkPrediction->GetFixedTickState().Interpolation;
		if (Interpolation.ToFrame != INDEX_NONE)
		{
			// The time NP shows interpolated players at (InterpolatedTimeMS = (ToFrame - 1) * step + PCT * step), kept fractional
			Stats.RenderFrame = static_cast<double>(Interpolation.ToFrame - 1) + Interpolation.PCT;
			Stats.RenderSource = EGSTN_MinionRenderTimeSource::NetworkPrediction;
			return;
		}
	}
	if (FallbackNewestFrame != INDEX_NONE)
	{
		// The same 100 ms buffer as NP's default FixedTickInterpolationBufferedMS, behind the newest frame advanced on local time
		const double StepMs = FMath::Max(Smoothing.FixedStepMs, 1.f);
		Stats.RenderFrame = FallbackNewestFrame + (NowSeconds - FallbackNewestSeconds) * 1000.0 / StepMs - 100.0 / StepMs;
		Stats.RenderSource = EGSTN_MinionRenderTimeSource::FixedDelay;
		return;
	}
	Stats.RenderSource = EGSTN_MinionRenderTimeSource::None;
}

void UGSTN_MinionPresentationSubsystem::UpdatePredictedHits(double NowSeconds)
{
	const UWorld* World = GetWorld();
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = Controller && Controller->IsLocalController() ? Controller->GetPawn() : nullptr;
	const UGSTN_CombatSimComponent* Combat = Pawn ? Pawn->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	const FGSTN_CombatSyncState* Sync = Combat ? Combat->GetSyncState() : nullptr;
	const bool bLogFlash = CVarMinionPresentLogFlash.GetValueOnGameThread() != 0;
	auto LogFlash = [&](const TCHAR* Event, const FGSTN_HitPresentationKey& Key)
	{
		const FGSTN_MinionPresentRecord* Record = FindRecordBySlot(GSTNEntityKinds::MinionIndexOf(Key.VictimId));
		UE_LOG(LogGSTNMinion, Log, TEXT("MinionFlash %s world=%s victim=%d start=%d group=%d now=%.3f health=%.2f shattered=%d flash=%s"),
			Event, *GetNameSafe(World), Key.VictimId, Key.TimelineStartFrame, Key.HitGroup, NowSeconds,
			Record ? Record->HealthFraction : -1.f, Record && Record->bShattered ? 1 : 0,
			Record ? FGSTN_MinionHitFlash::StateName(Record->Flash.GetState()) : TEXT("norecord"));
	};

	// Vanished keys whose grace ran out are real cancels; old keys are forgotten
	for (int32 Index = PredictedMinionHits.Num() - 1; Index >= 0; --Index)
	{
		const FPredictedMinionHit& Hit = PredictedMinionHits[Index];
		if (Hit.CancelAtSeconds >= 0.0 && NowSeconds >= Hit.CancelAtSeconds)
		{
			if (bLogFlash)
			{
				LogFlash(TEXT("cancel"), Hit.Key);
			}
			OnPredictionCancelled(Hit.Key.VictimId, NowSeconds);
			PredictedMinionHits.RemoveAtSwap(Index);
		}
		else if (Hit.CancelAtSeconds < 0.0 && NowSeconds - Hit.SeenSeconds > PredictedHitMemorySeconds)
		{
			PredictedMinionHits.RemoveAtSwap(Index);
		}
	}

	if (!Sync)
	{
		HitTracker.Reset();
		return;
	}
	// The same reading of the target ring as UGSTN_HitPresentationComponent::OnAttackerFrame: new entries are predicted hits, entries
	// that vanish while the same move runs are corrections (a cancel only after the grace, see PredictedMinionHits)
	const FGSTN_CombatSyncCore& Core = Sync->Core;
	const bool bRunLive = Core.ActionState == EGSTN_ActionState::InTimeline && Core.TimelineStartFrame != INDEX_NONE;
	FGSTN_HitPresentationTracker::FDelta Delta;
	HitTracker.Update(Core.Hit, bRunLive, Core.TimelineStartFrame, Delta);
	auto FindPredicted = [this](const FGSTN_HitPresentationKey& Key)
	{
		return PredictedMinionHits.IndexOfByPredicate([&Key](const FPredictedMinionHit& Hit) { return Hit.Key == Key; });
	};
	for (const FGSTN_HitPresentationKey& Key : Delta.Cancelled)
	{
		if (GSTNEntityKinds::IsMinionEntityId(Key.VictimId))
		{
			const int32 Index = FindPredicted(Key);
			if (Index != INDEX_NONE && PredictedMinionHits[Index].CancelAtSeconds < 0.0)
			{
				if (bLogFlash)
				{
					LogFlash(TEXT("vanish"), Key);
				}
				PredictedMinionHits[Index].CancelAtSeconds = NowSeconds + HitCancelGraceSeconds;
			}
		}
	}
	for (const FGSTN_HitPresentationKey& Key : Delta.Started)
	{
		if (GSTNEntityKinds::IsMinionEntityId(Key.VictimId))
		{
			const int32 Index = FindPredicted(Key);
			if (Index != INDEX_NONE)
			{
				// The same hit came back after a correction (or after the tracker reset): not a new prediction
				if (bLogFlash)
				{
					LogFlash(TEXT("return"), Key);
				}
				PredictedMinionHits[Index].CancelAtSeconds = -1.0;
				PredictedMinionHits[Index].SeenSeconds = NowSeconds;
				continue;
			}
			FPredictedMinionHit& Hit = PredictedMinionHits.AddDefaulted_GetRef();
			Hit.Key = Key;
			Hit.SeenSeconds = NowSeconds;
			if (bLogFlash)
			{
				LogFlash(TEXT("predict"), Key);
			}
			OnPredictedHit(Key.VictimId, NowSeconds);
		}
	}
}

void UGSTN_MinionPresentationSubsystem::OnPredictedHit(int32 EntityId, double NowSeconds)
{
	if (FGSTN_MinionPresentRecord* Record = FindRecordBySlot(GSTNEntityKinds::MinionIndexOf(EntityId)))
	{
		Record->Flash.OnPredictedHit(NowSeconds, FlashParams);
		++Stats.PredictedHits;
	}
}

void UGSTN_MinionPresentationSubsystem::OnPredictionCancelled(int32 EntityId, double NowSeconds)
{
	if (FGSTN_MinionPresentRecord* Record = FindRecordBySlot(GSTNEntityKinds::MinionIndexOf(EntityId)))
	{
		Record->Flash.OnPredictionCancelled(NowSeconds);
		++Stats.PredictionCancels;
	}
}

FGSTN_MinionPresentRecord* UGSTN_MinionPresentationSubsystem::FindRecordBySlot(int32 Slot)
{
	if (Stats.Source == EGSTN_MinionPresentSource::Store)
	{
		return Records.Find(Slot);
	}
	// Client: keyed by net id; a linear walk, but only per predicted hit
	for (TPair<int32, FGSTN_MinionPresentRecord>& Pair : Records)
	{
		if (Pair.Value.Slot == Slot)
		{
			return &Pair.Value;
		}
	}
	return nullptr;
}

void UGSTN_MinionPresentationSubsystem::UpdateVisibleSet(double NowSeconds)
{
	const FVector Viewer = GetViewerLocation();
	ScratchKeys.Reset();
	ScratchDistances.Reset();
	ScratchHasPuppet.Reset();
	for (TPair<int32, FGSTN_MinionPresentRecord>& Pair : Records)
	{
		FGSTN_MinionPresentRecord& Record = Pair.Value;
		Record.bVisible = false;
		if (Record.Pose.Mode == EGSTN_MinionDisplayMode::None)
		{
			continue;
		}
		if (PresentIsDead(Record.Pose.ActionState))
		{
			// Dead: the placeholder shatter takes the puppet's place once (4d.4), the minion is no longer drawn
			if (!Record.bShattered)
			{
				Record.bShattered = true;
				PlayShatter(Record, NowSeconds);
			}
			continue;
		}
		ScratchKeys.Add(Pair.Key);
		ScratchDistances.Add(static_cast<float>(FVector::DistSquared(FVector(Record.Pose.Position), Viewer)));
		ScratchHasPuppet.Add(Record.Puppet.IsValid());
	}

	GSTNMinionPresent::SelectNearest(ScratchDistances, ScratchHasPuppet, GetMaxVisible(), VisibleStickyFraction, ScratchVisible);
	for (const int32 Index : ScratchVisible)
	{
		Records[ScratchKeys[Index]].bVisible = true;
	}
	Stats.Visible = ScratchVisible.Num();
	Stats.Culled = ScratchKeys.Num() - ScratchVisible.Num();

	// Release first so a puppet freed at the cut serves a minion that came in this frame
	for (TPair<int32, FGSTN_MinionPresentRecord>& Pair : Records)
	{
		FGSTN_MinionPresentRecord& Record = Pair.Value;
		if (!Record.bVisible && Record.Puppet.IsValid())
		{
			ReleasePuppet(Record.Puppet.Get());
			Record.Puppet = nullptr;
		}
	}
	for (TPair<int32, FGSTN_MinionPresentRecord>& Pair : Records)
	{
		FGSTN_MinionPresentRecord& Record = Pair.Value;
		if (Record.bVisible && !Record.Puppet.IsValid())
		{
			const FTransform Transform(FRotator(0.f, Record.Pose.YawDegrees, 0.f), FVector(Record.Pose.Position));
			Record.Puppet = AcquirePuppet(Record.Definition.Get(), Pair.Key, Transform);
		}
	}
}

void UGSTN_MinionPresentationSubsystem::ApplyToPuppets(double NowSeconds)
{
	for (TPair<int32, FGSTN_MinionPresentRecord>& Pair : Records)
	{
		FGSTN_MinionPresentRecord& Record = Pair.Value;
		AGSTN_MinionPuppet* Puppet = Record.Puppet.Get();
		if (!Puppet)
		{
			continue;
		}
		Puppet->SetPose(FVector(Record.Pose.Position), Record.Pose.YawDegrees);
		Puppet->SetFlash(Record.Flash.GetFlash(NowSeconds, FlashParams));
		Puppet->SetPaused(Record.Flash.IsPaused(NowSeconds));
	}
}

AGSTN_MinionPuppet* UGSTN_MinionPresentationSubsystem::AcquirePuppet(const UGSTN_MinionDefinition* Definition, int32 Key, const FTransform& Transform)
{
	AGSTN_MinionPuppet* Puppet = nullptr;
	if (TArray<TWeakObjectPtr<AGSTN_MinionPuppet>>* Free = FreePuppets.Find(Definition))
	{
		while (!Puppet && Free->Num() > 0)
		{
			Puppet = Free->Pop(EAllowShrinking::No).Get();
		}
	}
	if (!Puppet)
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return nullptr;
		}
		// A type may name its own puppet class; anything that is not a puppet falls back to the default (no world class is ever assumed)
		UClass* Class = AGSTN_MinionPuppet::StaticClass();
		if (Definition && !Definition->PuppetClass.IsNull())
		{
			UClass* TypeClass = Definition->PuppetClass.LoadSynchronous();
			if (TypeClass && TypeClass->IsChildOf(AGSTN_MinionPuppet::StaticClass()))
			{
				Class = TypeClass;
			}
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		Puppet = World->SpawnActor<AGSTN_MinionPuppet>(Class, Transform, Params);
		if (!Puppet)
		{
			return nullptr;
		}
		AllPuppets.Add(Puppet);
		++Stats.PuppetsSpawned;
	}
	Puppet->ActivatePuppet(Definition, Key, Transform);
	++Stats.Acquires;
	return Puppet;
}

void UGSTN_MinionPresentationSubsystem::ReleasePuppet(AGSTN_MinionPuppet* Puppet)
{
	if (!Puppet || !Puppet->IsInUse())
	{
		return;
	}
	const UGSTN_MinionDefinition* Definition = Puppet->GetDefinition();
	Puppet->DeactivatePuppet();
	FreePuppets.FindOrAdd(Definition).Add(Puppet);
	++Stats.Releases;
}

int32 UGSTN_MinionPresentationSubsystem::GetNumPooledPuppets() const
{
	int32 Num = 0;
	for (const TPair<const UGSTN_MinionDefinition*, TArray<TWeakObjectPtr<AGSTN_MinionPuppet>>>& Pair : FreePuppets)
	{
		Num += Pair.Value.Num();
	}
	return Num;
}

void UGSTN_MinionPresentationSubsystem::PlayShatter(const FGSTN_MinionPresentRecord& Record, double NowSeconds)
{
	AGSTN_MinionShatter* Shatter = nullptr;
	while (!Shatter && FreeShatters.Num() > 0)
	{
		Shatter = FreeShatters.Pop(EAllowShrinking::No).Get();
	}
	if (!Shatter)
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		Shatter = World->SpawnActor<AGSTN_MinionShatter>(AGSTN_MinionShatter::StaticClass(), FTransform(FVector(Record.Pose.Position)), Params);
		if (!Shatter)
		{
			return;
		}
		AllShatters.Add(Shatter);
	}
	Shatter->Play(FVector(Record.Pose.Position), NowSeconds + ShatterSeconds);
	++Stats.ShattersPlayed;
}

void UGSTN_MinionPresentationSubsystem::TickShatters(double NowSeconds)
{
	int32 Playing = 0;
	for (const TObjectPtr<AGSTN_MinionShatter>& Shatter : AllShatters)
	{
		if (!Shatter || !Shatter->IsPlaying())
		{
			continue;
		}
		if (NowSeconds >= Shatter->GetEndSeconds())
		{
			Shatter->Stop();
			FreeShatters.Add(Shatter.Get());
		}
		else
		{
			++Playing;
		}
	}
	Stats.ShattersActive = Playing;
}

void UGSTN_MinionPresentationSubsystem::RemoveRecord(int32 Key)
{
	if (FGSTN_MinionPresentRecord* Record = Records.Find(Key))
	{
		if (AGSTN_MinionPuppet* Puppet = Record->Puppet.Get())
		{
			ReleasePuppet(Puppet);
		}
		Records.Remove(Key);
	}
}

FVector UGSTN_MinionPresentationSubsystem::GetViewerLocation() const
{
	if (ViewerOverrideForTests.IsSet())
	{
		return ViewerOverrideForTests.GetValue();
	}
	const UWorld* World = GetWorld();
	if (const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr)
	{
		FVector Location;
		FRotator Rotation;
		Controller->GetPlayerViewPoint(Location, Rotation);
		return Location;
	}
	return FVector::ZeroVector;
}

const UGSTN_HurtboxSetDefinition* UGSTN_MinionPresentationSubsystem::GetHurtboxes(const UGSTN_MinionDefinition* Definition)
{
	if (!Definition)
	{
		return nullptr;
	}
	if (const TWeakObjectPtr<const UGSTN_HurtboxSetDefinition>* Cached = HurtboxCache.Find(Definition))
	{
		return Cached->Get();
	}
	// Once per type: a synchronous load the first time a client presents the type (the authority resolves the same set at registration)
	const UGSTN_HurtboxSetDefinition* Set = Definition->HurtboxSet.LoadSynchronous();
	HurtboxCache.Add(Definition, Set);
	BoundRadiusCache.Add(Definition, static_cast<float>(UGSTN_CombatWorldSubsystem::ComputeBoundRadius(Set)));
	return Set;
}

void UGSTN_MinionPresentationSubsystem::AppendSnapshotEntities(int32 Frame, TArray<FGSTN_SnapshotEntry>& OutEntries)
{
	if (!bActive || !bClientProvider)
	{
		return;
	}
	int32 Added = 0;
	for (const TPair<int32, FGSTN_MinionPresentRecord>& Pair : Records)
	{
		const FGSTN_MinionPresentRecord& Record = Pair.Value;
		// Hittable only with the server's id (the slot table arrived) and a shown pose; dying minions are intangible as on the authority
		if (Record.Slot == INDEX_NONE || Record.Pose.Mode == EGSTN_MinionDisplayMode::None || PresentIsDyingOrDead(Record.Pose.ActionState))
		{
			continue;
		}
		const UGSTN_MinionDefinition* Definition = Record.Definition.Get();
		const UGSTN_HurtboxSetDefinition* Set = GetHurtboxes(Definition);
		const float* BoundRadius = Definition ? BoundRadiusCache.Find(Definition) : nullptr;

		FGSTN_SnapshotEntry& Entry = OutEntries.AddDefaulted_GetRef();
		Entry.EntityId = GSTNEntityKinds::MakeMinionEntityId(static_cast<uint16>(Record.Slot));
		Entry.Generation = 0;
		Entry.Team = FGenericTeamId(Definition ? Definition->TeamId : 1);
		Entry.Transform = FTransform(FRotator(0.f, Record.Pose.YawDegrees, 0.f), FVector(Record.Pose.Position));
		Entry.Facing = Entry.Transform.GetUnitAxis(EAxis::X);
		Entry.Up = FVector::UpVector;
		Entry.Velocity = Entry.Facing * Record.Pose.Speed;
		Entry.Hurtboxes = Set;
		Entry.BoundRadius = BoundRadius ? *BoundRadius : 0.f;
		Entry.HealthFraction = Record.HealthFraction;
		Entry.ActionState = PresentToFighterActionState(Record.Pose.ActionState, Record.Pose.MoveId);
		Entry.Reaction = FGSTN_VictimReactionState::Make(0.0, 1.0, 0, Set ? Set->StaggerThreshold : FGSTN_StaggerThreshold());
		++Added;
	}
	Stats.HittableEntries = Added;
}

void UGSTN_MinionPresentationSubsystem::DumpToLog() const
{
	const UWorld* World = GetWorld();
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	const int32 PlayerId = Controller && Controller->PlayerState ? Controller->PlayerState->GetPlayerId() : INDEX_NONE;
	const FString WorldName = GetNameSafe(World);
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	TArray<int32> Keys;
	Records.GetKeys(Keys);
	Keys.Sort();
	int32 Predicted = 0;
	int32 Confirmed = 0;
	int32 Cancelled = 0;
	int32 Expired = 0;
	int32 Remote = 0;
	for (const int32 Key : Keys)
	{
		const FGSTN_MinionPresentRecord& Record = Records[Key];
		const FGSTN_MinionHitFlash& Flash = Record.Flash;
		Predicted += Flash.NumPredicted;
		Confirmed += Flash.NumConfirmed;
		Cancelled += Flash.NumCancelled;
		Expired += Flash.NumExpired;
		Remote += Flash.NumRemote;
		UE_LOG(LogGSTNMinion, Log,
			TEXT("MinionPresent puppet player=%d world=%s id=%d slot=%d type=%d pos=%.1f,%.1f,%.1f yaw=%.1f frame=%.2f tier=%d mode=%s visible=%d flash=%s flashValue=%.2f paused=%d state=%d hits=%d/%d/%d/%d/%d"),
			PlayerId, *WorldName, Key, Record.Slot, Record.TypeIndex, Record.Pose.Position.X, Record.Pose.Position.Y, Record.Pose.Position.Z,
			Record.Pose.YawDegrees, Record.Pose.Frame, static_cast<int32>(Record.Tier), PresentModeName(Record.Pose.Mode), Record.bVisible ? 1 : 0,
			FGSTN_MinionHitFlash::StateName(Flash.GetState()), Flash.GetFlash(Now, FlashParams), Flash.IsPaused(Now) ? 1 : 0,
			static_cast<int32>(Record.Pose.ActionState), Flash.NumPredicted, Flash.NumConfirmed, Flash.NumCancelled, Flash.NumExpired, Flash.NumRemote);
	}
	UE_LOG(LogGSTNMinion, Log,
		TEXT("MinionPresent summary player=%d world=%s source=%s renderSource=%s renderFrame=%.2f active=%d visible=%d culled=%d pooled=%d spawned=%d acquires=%d releases=%d maxVisible=%d shattersActive=%d shattersPooled=%d shattersPlayed=%d hittables=%d predicted=%d confirmed=%d cancelled=%d expired=%d remote=%d"),
		PlayerId, *WorldName, PresentSourceName(Stats.Source), PresentRenderSourceName(Stats.RenderSource), Stats.RenderFrame, Records.Num(), Stats.Visible, Stats.Culled,
		GetNumPooledPuppets(), Stats.PuppetsSpawned, Stats.Acquires, Stats.Releases, GetMaxVisible(), Stats.ShattersActive, FreeShatters.Num(),
		Stats.ShattersPlayed, Stats.HittableEntries, Predicted, Confirmed, Cancelled, Expired, Remote);
}

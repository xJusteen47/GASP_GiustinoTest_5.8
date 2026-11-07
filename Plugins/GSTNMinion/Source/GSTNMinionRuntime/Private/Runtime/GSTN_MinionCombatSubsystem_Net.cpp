// Minion runtime: the replication half of the authority subsystem (M8 G6, spec 8.1-8.3). Packs, per-item tier rates, dormancy order,
// budgeted relevancy and the registry hash. Runs after the batch's stages (TickBatch) and from FreeSlot, never inside a stage.

#include "Runtime/GSTN_MinionCombatSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GSTNMinionCoreModule.h"
#include "HAL/IConsoleManager.h"
#include "Net/GSTN_MinionClientRegistry.h"
#include "Net/GSTN_MinionNetManager.h"
#include "Net/GSTN_MinionNetRates.h"
#include "Net/GSTN_MinionPack.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	TAutoConsoleVariable<int32> CVarHostUploadBudgetKBps(
		TEXT("gstn.Minion.HostUploadBudgetKBps"), 40,
		TEXT("Upload the minion packs may plan to use on this host, KB/s (1 KB = 1000 B), split evenly over the remote connections. A pack is\n")
		TEXT("granted to a connection only while the planned bytes (20 B per item update at its tier's rate) fit its share (spec 8.1, 8.3)."));

	TAutoConsoleVariable<int32> CVarMaxRelevantPerClient(
		TEXT("gstn.Minion.Net.MaxRelevantPerClient"), 64,
		TEXT("Most minion items (not packs) granted to one connection (spec 8.1 MaxRelevantPerClient)."));

	TAutoConsoleVariable<int32> CVarRelevancyFrames(
		TEXT("gstn.Minion.Net.RelevancyFrames"), 6,
		TEXT("Fixed frames between two budgeted relevancy passes (6 = 96 ms at 62.5 Hz)."));

	TAutoConsoleVariable<float> CVarRelevantDistance(
		TEXT("gstn.Minion.Net.RelevantDistance"), 8000.f,
		TEXT("A pack is a candidate for a connection while one member is within this distance of its viewer, cm."));

	/** No pack toggles faster than this for one connection (1 s; spec 8.1 hysteresis) */
	constexpr int32 RelevancyMinToggleFrames = 63;

	/** A member hit within this many frames makes its pack rank as recently hit */
	constexpr int32 RecentlyHitFrames = 63;

	FAutoConsoleCommandWithWorldAndArgs NetDumpCommand(
		TEXT("gstn.Minion.Net.Dump"),
		TEXT("Logs the minion replication state of this world: on the authority every pack item, the grants per remote player and a summary;\n")
		TEXT("on a client the received items and the registry check. Lines start with 'MinionNet'."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& /*Args*/, UWorld* World)
		{
			if (const UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(World))
			{
				Minions->DumpNetToLog();
			}
			else if (const UGSTN_MinionClientRegistry* Registry = World ? World->GetSubsystem<UGSTN_MinionClientRegistry>() : nullptr)
			{
				Registry->DumpToLog();
			}
		}));

	/** The viewer location of a remote controller: its pawn, else its view target */
	bool ViewerLocation(const APlayerController& Controller, FVector& OutLocation)
	{
		if (const APawn* Pawn = Controller.GetPawn())
		{
			OutLocation = Pawn->GetActorLocation();
			return true;
		}
		if (const AActor* ViewTarget = Controller.GetViewTarget())
		{
			OutLocation = ViewTarget->GetActorLocation();
			return true;
		}
		return false;
	}
}

void UGSTN_MinionCombatSubsystem::NetActivate()
{
	const int32 Capacity = Store.Capacity();
	SlotSpawnGroup.Init(0, Capacity);
	SlotPackId.Init(INDEX_NONE, Capacity);
	SlotItemId.Init(INDEX_NONE, Capacity);
	SlotNetUpdates.Init(0, Capacity);
	SlotViewerDistance.Init(0.f, Capacity);
	Packs.Reset();
	ViewerRelevancy.Reset();
	LastRelevancyFrame = INDEX_NONE;
	NetStats = FGSTN_MinionNetStats();

	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		NetManager = World->SpawnActor<AGSTN_MinionNetManager>(Params);
	}
	NetOnRegistryChanged();
}

void UGSTN_MinionCombatSubsystem::NetDeinitialize()
{
	// The world destroys the actors; only the references and the per-connection state go here
	Packs.Reset();
	NetManager = nullptr;
	ViewerRelevancy.Reset();
	NetTraceForTests = nullptr;
}

void UGSTN_MinionCombatSubsystem::NetTrace(const TCHAR* Step)
{
	if (NetTraceForTests)
	{
		NetTraceForTests->Add(FName(Step));
	}
}

void UGSTN_MinionCombatSubsystem::SetNetWorldOrigin(const FVector& Origin)
{
	NetWorldOrigin = FVector3f(Origin);
	NetOnRegistryChanged();
}

int32 UGSTN_MinionCombatSubsystem::GetNumPacks() const
{
	int32 Count = 0;
	for (const TObjectPtr<AGSTN_MinionPack>& Pack : Packs)
	{
		Count += Pack ? 1 : 0;
	}
	return Count;
}

void UGSTN_MinionCombatSubsystem::NetOnRegistryChanged()
{
	FGSTN_MinionNetRegistryInfo Info;
	Info.TypePaths.Reserve(Definitions.Num());
	TBitArray<> ExtTypes(false, Definitions.Num());
	for (int32 Index = 0; Index < Definitions.Num(); ++Index)
	{
		const UGSTN_MinionDefinition* Definition = Definitions[Index];
		Info.TypePaths.Add(Definition ? Definition->GetPathName() : FString());
		ExtTypes[Index] = Definition && Definition->bHasNetExt;
	}
	// The item codec needs the same table the clients build from the same definitions (G10 ExtByte seam)
	AGSTN_MinionPack::SetNetExtTypes(ExtTypes);
	NetStats.RegistryHash = FGSTN_MinionRegistryHash::Compute(Info.TypePaths);
	Info.Hash = static_cast<int32>(NetStats.RegistryHash);
	Info.WorldOrigin = FVector(NetWorldOrigin);
	if (NetManager)
	{
		NetManager->SetRegistryInfo(Info);
	}
}

void UGSTN_MinionCombatSubsystem::MutatePack(AGSTN_MinionPack& Pack, TFunctionRef<void(FGSTN_MinionNetArray&, FGSTN_MinionPackMutation&)> Mutate)
{
	// 1. Dormancy first. A pack that will send again wakes; one that stays all-T3 (a member removed while asleep) is flushed once
	if (Pack.NetDormancy > DORM_Awake)
	{
		if (Pack.CeilingHz > 0)
		{
			NetTrace(TEXT("Wake"));
			Pack.SetNetDormancy(DORM_Awake);
			++NetStats.Wakes;
		}
		else
		{
			NetTrace(TEXT("Flush"));
			Pack.FlushNetDormancy();
			++NetStats.Flushes;
		}
	}

	// 2. The change itself
	NetTrace(TEXT("Mutate"));
	FGSTN_MinionPackMutation Mutation;
	FGSTN_MinionNetArray& Array = Pack.GetMutableNetArray();
	Mutate(Array, Mutation);

	// 3. FastArray marks, then the push-model property mark (without it a push-model build never sends the change)
	for (const int32 Index : Mutation.ChangedItems)
	{
		if (Array.Items.IsValidIndex(Index))
		{
			NetTrace(TEXT("MarkItem"));
			Array.MarkItemDirty(Array.Items[Index]);
		}
	}
	if (Mutation.bStructureChanged)
	{
		Array.MarkArrayDirty();
	}
	NetTrace(TEXT("MarkProperty"));
	Pack.MarkItemsPropertyDirty();
}

FGSTN_MinionNetWireItem UGSTN_MinionCombatSubsystem::NetBuildWire(int32 Slot, int32 Frame) const
{
	FGSTN_MinionNetState State;
	State.Position = Store.Position[Slot];
	State.YawDegrees = Store.YawDegrees[Slot];
	State.ActionState = Store.ActionState[Slot];
	State.MoveId = Store.MoveId[Slot];
	State.MoveStartFrame = State.MoveId != 0 ? Store.MoveStartFrame[Slot] : INDEX_NONE;
	State.FrameStamp = Frame;
	State.HitCounter = Store.HitCounter[Slot];
	State.HealthFraction = Store.MaxHealth[Slot] > 0.f ? FMath::Clamp(Store.Health[Slot] / Store.MaxHealth[Slot], 0.f, 1.f) : 0.f;
	State.Speed = Store.Speed[Slot];
	State.Flags = Store.Flags[Slot];

	FGSTN_MinionNetQuantization Quant;
	Quant.WorldOrigin = NetWorldOrigin;
	bool bTypeHasExt = false;
	if (const UGSTN_MinionDefinition* Definition = GetDefinition(Store.TypeIndex[Slot]))
	{
		Quant.MaxSpeed = Definition->Stats.RunSpeed;
		bTypeHasExt = Definition->bHasNetExt;
	}

	FGSTN_MinionNetWireItem Wire;
	Wire.Id = static_cast<uint8>(SlotItemId.IsValidIndex(Slot) ? FMath::Max(SlotItemId[Slot], 0) : 0);
	// Type on every update in v1: see FGSTN_MinionNetItem
	Wire.bHasType = true;
	Wire.TypeIndex = Store.TypeIndex[Slot];
	FGSTN_MinionNetCodec::Quantize(State, Quant, Wire);
	// Only a type that owns the byte sends it; for any other the field stays 0 and is not on the wire (the rate limiter compares it, so a
	// change of the byte alone makes the item dirty)
	Wire.ExtByte = bTypeHasExt ? Store.ExtByte[Slot] : 0;
	return Wire;
}

AGSTN_MinionPack* UGSTN_MinionCombatSubsystem::NetSpawnPack(int32 SpawnGroup)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	// Lowest free pack id: ids stay small (global id = PackId << 4 | ItemId) over a long session of waves
	int32 PackId = Packs.IndexOfByPredicate([](const TObjectPtr<AGSTN_MinionPack>& Pack) { return Pack == nullptr; });
	if (PackId == INDEX_NONE)
	{
		PackId = Packs.Add(nullptr);
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	AGSTN_MinionPack* Pack = World->SpawnActor<AGSTN_MinionPack>(Params);
	if (!Pack)
	{
		return nullptr;
	}
	Pack->InitPack(PackId, SpawnGroup);
	Pack->FinishSpawning(FTransform::Identity);
	Packs[PackId] = Pack;
	return Pack;
}

void UGSTN_MinionCombatSubsystem::NetAssignSlot(int32 Slot, int32 Frame)
{
	// Re-packing happens only here, at spawn (spec 8.1 default): a wave spawns into whole packs of its group
	const int32 SpawnGroup = SlotSpawnGroup[Slot];
	AGSTN_MinionPack* Pack = nullptr;
	for (const TObjectPtr<AGSTN_MinionPack>& Candidate : Packs)
	{
		if (Candidate && Candidate->SpawnGroup == SpawnGroup && Candidate->NumMembers < AGSTN_MinionPack::MaxItems)
		{
			Pack = Candidate;
			break;
		}
	}
	if (!Pack)
	{
		Pack = NetSpawnPack(SpawnGroup);
		if (!Pack)
		{
			return;
		}
	}

	int32 ItemId = INDEX_NONE;
	for (int32 Id = 0; Id < AGSTN_MinionPack::MaxItems; ++Id)
	{
		if (Pack->MemberSlot[Id] == INDEX_NONE)
		{
			ItemId = Id;
			break;
		}
	}
	check(ItemId != INDEX_NONE);
	Pack->MemberSlot[ItemId] = Slot;
	++Pack->NumMembers;
	SlotPackId[Slot] = Pack->GetPackId();
	SlotItemId[Slot] = ItemId;

	FGSTN_MinionNetWireItem Wire = NetBuildWire(Slot, Frame);
	// The first sample of a minion: no interpolation from wherever a puppet of this id was before
	Wire.Flags |= GSTNMinion::NetFlags::Teleport;
	MutatePack(*Pack, [&Wire](FGSTN_MinionNetArray& Array, FGSTN_MinionPackMutation& Mutation)
	{
		const int32 Index = Array.Items.AddDefaulted();
		Array.Items[Index].Wire = Wire;
		Mutation.ChangedItems.Add(Index);
		Mutation.bStructureChanged = true;
	});
	// An add is a send: the item's next update respects its rate from here
	Pack->ItemLimiters[ItemId].Reset();
	Pack->ItemLimiters[ItemId].OnSent(Frame, GSTNMinionNetRate::RateHz(static_cast<EGSTN_MinionLODTier>(Store.LODTier[Slot])));
	++SlotNetUpdates[Slot];
	++NetStats.ItemAdds;
}

void UGSTN_MinionCombatSubsystem::NetReleaseSlot(int32 Slot)
{
	if (!SlotPackId.IsValidIndex(Slot) || SlotPackId[Slot] == INDEX_NONE)
	{
		return;
	}
	const int32 PackId = SlotPackId[Slot];
	const int32 ItemId = SlotItemId[Slot];
	SlotPackId[Slot] = INDEX_NONE;
	SlotItemId[Slot] = INDEX_NONE;
	SlotNetUpdates[Slot] = 0;

	AGSTN_MinionPack* Pack = GetPack(PackId);
	if (!Pack || ItemId < 0 || ItemId >= AGSTN_MinionPack::MaxItems)
	{
		return;
	}
	Pack->MemberSlot[ItemId] = INDEX_NONE;
	Pack->NumMembers = FMath::Max(Pack->NumMembers - 1, 0);
	++NetStats.ItemRemoves;

	if (Pack->NumMembers == 0)
	{
		// An empty pack is destroyed: the clients close its channel and drop its items with it
		Packs[PackId] = nullptr;
		Pack->Destroy();
		return;
	}
	const int32 Index = Pack->FindItemIndex(static_cast<uint8>(ItemId));
	if (Index != INDEX_NONE)
	{
		MutatePack(*Pack, [Index](FGSTN_MinionNetArray& Array, FGSTN_MinionPackMutation& Mutation)
		{
			Array.Items.RemoveAtSwap(Index);
			Mutation.bStructureChanged = true;
		});
	}
}

void UGSTN_MinionCombatSubsystem::NetPublish(int32 Frame)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GSTN_MinionNetPublish);

	// Packs for the minions the spawn drain activated this frame (ascending slot order, so pack membership is deterministic)
	for (int32 Slot = Store.NextAliveSlot(0); Slot != INDEX_NONE; Slot = Store.NextAliveSlot(Slot + 1))
	{
		if (PendingActivation[Slot])
		{
			continue;
		}
		// Platform seam (G10): a flagged slot has no item. One that already has one loses it exactly like a despawn does (NetReleaseSlot:
		// the id is freed, an emptied pack is destroyed); clearing the flag finds SlotPackId empty and re-adds it with a Teleport sample
		if (Store.ExtFlags[Slot] & GSTNMinion::ExtFlag::PlatformActor)
		{
			if (SlotPackId[Slot] != INDEX_NONE)
			{
				NetReleaseSlot(Slot);
			}
		}
		else if (SlotPackId[Slot] == INDEX_NONE)
		{
			NetAssignSlot(Slot, Frame);
		}
	}

	uint8 Tiers[AGSTN_MinionPack::MaxItems];
	bool Dirty[AGSTN_MinionPack::MaxItems];
	FGSTN_MinionNetWireItem Candidates[AGSTN_MinionPack::MaxItems];
	for (const TObjectPtr<AGSTN_MinionPack>& PackPtr : Packs)
	{
		AGSTN_MinionPack* Pack = PackPtr;
		if (!Pack)
		{
			continue;
		}
		uint8 MemberTiers[AGSTN_MinionPack::MaxItems];
		int32 NumMemberTiers = 0;
		for (int32 Id = 0; Id < AGSTN_MinionPack::MaxItems; ++Id)
		{
			const int32 Slot = Pack->MemberSlot[Id];
			Tiers[Id] = Slot != INDEX_NONE ? Store.LODTier[Slot] : static_cast<uint8>(EGSTN_MinionLODTier::Dormant);
			Dirty[Id] = false;
			if (Slot != INDEX_NONE)
			{
				MemberTiers[NumMemberTiers++] = Tiers[Id];
			}
		}

		const int32 Ceiling = GSTNMinionNetRate::PackCeilingHz(TConstArrayView<uint8>(MemberTiers, NumMemberTiers));
		if (Ceiling != Pack->CeilingHz)
		{
			Pack->CeilingHz = Ceiling;
			Pack->SetNetUpdateFrequency(static_cast<float>(FMath::Max(Ceiling, 1)));
		}
		if (Ceiling > 0 && Pack->NetDormancy > DORM_Awake)
		{
			// Woken before any item of this frame is written (the MutatePack order, here for the tier change itself)
			NetTrace(TEXT("Wake"));
			Pack->SetNetDormancy(DORM_Awake);
			++NetStats.Wakes;
		}

		if (Ceiling > 0 && Pack->PackLimiter.CanSend(Frame, Ceiling))
		{
			const TArray<FGSTN_MinionNetItem>& Items = Pack->GetNetArray().Items;
			for (int32 Id = 0; Id < AGSTN_MinionPack::MaxItems; ++Id)
			{
				const int32 Slot = Pack->MemberSlot[Id];
				if (Slot == INDEX_NONE)
				{
					continue;
				}
				const int32 Index = Pack->FindItemIndex(static_cast<uint8>(Id));
				if (Index == INDEX_NONE)
				{
					continue;
				}
				Candidates[Id] = NetBuildWire(Slot, Frame);
				Dirty[Id] = GSTNMinionNetRate::IsSignificantChange(Items[Index].Wire, Candidates[Id]);
			}

			NetSentScratch.Reset();
			if (GSTNMinionNetRate::SelectPackSends(Frame, Pack->PackLimiter, Ceiling, TConstArrayView<uint8>(Tiers, AGSTN_MinionPack::MaxItems),
				TConstArrayView<bool>(Dirty, AGSTN_MinionPack::MaxItems), TArrayView<FGSTN_MinionNetRateLimiter>(Pack->ItemLimiters, AGSTN_MinionPack::MaxItems),
				NetSentScratch) > 0)
			{
				MutatePack(*Pack, [this, Pack, &Candidates](FGSTN_MinionNetArray& Array, FGSTN_MinionPackMutation& Mutation)
				{
					for (const int32 Id : NetSentScratch)
					{
						const int32 Index = Pack->FindItemIndex(static_cast<uint8>(Id));
						if (Index != INDEX_NONE)
						{
							Array.Items[Index].Wire = Candidates[Id];
							Mutation.ChangedItems.Add(Index);
						}
					}
				});
				for (const int32 Id : NetSentScratch)
				{
					const int32 Slot = Pack->MemberSlot[Id];
					SlotNetUpdates[Slot] = static_cast<uint16>(FMath::Min<int32>(SlotNetUpdates[Slot] + 1, MAX_uint16));
				}
				NetStats.ItemUpdates += NetSentScratch.Num();
				++NetStats.PackSends;
			}
		}

		if (Ceiling == 0 && Pack->NetDormancy <= DORM_Awake)
		{
			// All T3: after this frame's marks, so the last change still goes out before the channel goes dormant
			NetTrace(TEXT("Sleep"));
			Pack->SetNetDormancy(DORM_DormantAll);
			++NetStats.Sleeps;
		}
	}

	const int32 Interval = FMath::Max(CVarRelevancyFrames.GetValueOnGameThread(), 1);
	if (LastRelevancyFrame == INDEX_NONE || Frame - LastRelevancyFrame >= Interval)
	{
		UpdateNetRelevancy(Frame);
	}
}

void UGSTN_MinionCombatSubsystem::UpdateNetRelevancy(int32 Frame)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GSTN_MinionNetRelevancy);
	LastRelevancyFrame = Frame;
	++NetStats.RelevancyPasses;

	// Remote viewers only: the host's own controller has no connection to replicate to
	TArray<const APlayerController*, TInlineAllocator<8>> Remote;
	if (const UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* Controller = It->Get();
			if (Controller && !Controller->IsLocalController())
			{
				Remote.Add(Controller);
			}
		}
	}
	ViewerRelevancy.RemoveAll([&Remote](const FViewerRelevancy& Entry)
	{
		const AActor* Viewer = Entry.Viewer.Get();
		return !Viewer || !Remote.Contains(Viewer);
	});

	FGSTN_MinionRelevancyParams Params;
	Params.MaxItems = FMath::Max(CVarMaxRelevantPerClient.GetValueOnGameThread(), 0);
	Params.BudgetBytesPerSecond = FMath::Max(CVarHostUploadBudgetKBps.GetValueOnGameThread(), 0) * 1000 / FMath::Max(Remote.Num(), 1);
	Params.RelevantDistance = CVarRelevantDistance.GetValueOnGameThread();
	Params.MinToggleFrames = RelevancyMinToggleFrames;

	NetStats.LastGranted = 0;
	NetStats.LastDeniedByBudget = 0;
	NetStats.LastOutOfRange = 0;
	NetStats.LastGrantedItems = 0;
	for (const APlayerController* Controller : Remote)
	{
		FVector Viewer;
		if (!ViewerLocation(*Controller, Viewer))
		{
			continue;
		}
		FViewerRelevancy* Entry = ViewerRelevancy.FindByPredicate([Controller](const FViewerRelevancy& E) { return E.Viewer.Get() == Controller; });
		if (!Entry)
		{
			Entry = &ViewerRelevancy.AddDefaulted_GetRef();
			Entry->Viewer = Controller;
		}

		RelevancyCandidates.Reset();
		for (const TObjectPtr<AGSTN_MinionPack>& PackPtr : Packs)
		{
			const AGSTN_MinionPack* Pack = PackPtr;
			if (!Pack || Pack->NumMembers == 0)
			{
				continue;
			}
			FGSTN_MinionPackCandidate& Candidate = RelevancyCandidates.AddDefaulted_GetRef();
			Candidate.PackId = Pack->GetPackId();
			for (int32 Id = 0; Id < AGSTN_MinionPack::MaxItems; ++Id)
			{
				const int32 Slot = Pack->MemberSlot[Id];
				if (Slot == INDEX_NONE)
				{
					continue;
				}
				const float Distance = static_cast<float>(FVector::Dist(FVector(Store.Position[Slot]), Viewer));
				SlotViewerDistance[Slot] = Distance;
				++Candidate.ItemCount;
				Candidate.PlannedBytesPerSecond += GSTNMinionNetRate::PlannedBytesPerSecond(static_cast<EGSTN_MinionLODTier>(Store.LODTier[Slot]));
				Candidate.NearestDistance = FMath::Min(Candidate.NearestDistance, Distance);
				// A running move holds the attack token (G5): the token is taken before the move starts and goes back when it ends
				Candidate.bAttacking |= Store.MoveId[Slot] >= 1 && Store.MoveId[Slot] <= GSTNMinion::MaxMoves;
				Candidate.bRecentlyHit |= HitstopEndFrame[Slot] != INDEX_NONE && HitstopEndFrame[Slot] >= Frame - RecentlyHitFrames;
			}
		}

		const FGSTN_MinionRelevancyResult Result = FGSTN_MinionRelevancyMath::Evaluate(Params, RelevancyCandidates, Frame, Entry->State);
		NetStats.LastGranted += Result.Granted;
		NetStats.LastDeniedByBudget += Result.DeniedByBudget;
		NetStats.LastOutOfRange += Result.OutOfRange;
		NetStats.LastGrantedItems += Result.GrantedItems;

		// Over-inclusion: the item updates of the last interval this connection received for members beyond the relevant distance
		for (const FGSTN_MinionPackCandidate& Candidate : RelevancyCandidates)
		{
			const AGSTN_MinionPack* Pack = GetPack(Candidate.PackId);
			if (!Pack || !Entry->State.IsGranted(Candidate.PackId))
			{
				continue;
			}
			for (int32 Id = 0; Id < AGSTN_MinionPack::MaxItems; ++Id)
			{
				const int32 Slot = Pack->MemberSlot[Id];
				if (Slot != INDEX_NONE && SlotNetUpdates[Slot] > 0)
				{
					NetStats.OverInclusion.Add(SlotNetUpdates[Slot], SlotViewerDistance[Slot] > Params.RelevantDistance);
				}
			}
		}
	}

	for (uint16& Updates : SlotNetUpdates)
	{
		Updates = 0;
	}
}

bool UGSTN_MinionCombatSubsystem::IsPackNetRelevantFor(const AGSTN_MinionPack& Pack, const AActor* RealViewer, const FVector& SrcLocation) const
{
	if (Pack.NumMembers == 0)
	{
		return false;
	}
	for (const FViewerRelevancy& Entry : ViewerRelevancy)
	{
		if (Entry.Viewer.Get() == RealViewer && Entry.State.HasEvaluated())
		{
			return Entry.State.IsGranted(Pack.GetPackId());
		}
	}

	// Not evaluated yet (a connection that joined since the last pass): the plain distance rule until the next pass, under 100 ms
	const float RelevantDistance = CVarRelevantDistance.GetValueOnGameThread();
	const double MaxSquared = static_cast<double>(RelevantDistance) * RelevantDistance;
	for (int32 Id = 0; Id < AGSTN_MinionPack::MaxItems; ++Id)
	{
		const int32 Slot = Pack.MemberSlot[Id];
		if (Slot != INDEX_NONE && FVector::DistSquared(FVector(Store.Position[Slot]), SrcLocation) <= MaxSquared)
		{
			return true;
		}
	}
	return false;
}

void UGSTN_MinionCombatSubsystem::DumpNetToLog() const
{
	int32 Items = 0;
	int32 Dormant = 0;
	for (const TObjectPtr<AGSTN_MinionPack>& PackPtr : Packs)
	{
		const AGSTN_MinionPack* Pack = PackPtr;
		if (!Pack)
		{
			continue;
		}
		Dormant += Pack->NetDormancy > DORM_Awake ? 1 : 0;
		for (const FGSTN_MinionNetItem& Item : Pack->GetNetArray().Items)
		{
			++Items;
			UE_LOG(LogGSTNMinion, Log, TEXT("MinionNet server pack=%d id=%d type=%d pos=%u,%u,%u yaw=%u state=%d ceiling=%d dormant=%d"),
				Pack->GetPackId(), Item.Wire.Id, Item.Wire.TypeIndex, Item.Wire.PositionQ[0], Item.Wire.PositionQ[1], Item.Wire.PositionQ[2],
				Item.Wire.YawQ, Item.Wire.ActionState, Pack->CeilingHz, Pack->NetDormancy > DORM_Awake ? 1 : 0);
		}
	}
	for (const FViewerRelevancy& Entry : ViewerRelevancy)
	{
		const APlayerController* Controller = Cast<APlayerController>(Entry.Viewer.Get());
		const int32 PlayerId = Controller && Controller->PlayerState ? Controller->PlayerState->GetPlayerId() : INDEX_NONE;
		FString Granted;
		for (const FGSTN_MinionPackGrant& Grant : Entry.State.Grants)
		{
			if (Grant.bGranted)
			{
				Granted += FString::Printf(TEXT("%s%d"), Granted.IsEmpty() ? TEXT("") : TEXT(","), Grant.PackId);
			}
		}
		UE_LOG(LogGSTNMinion, Log, TEXT("MinionNet grants player=%d packs=%s"), PlayerId, *Granted);
	}
	UE_LOG(LogGSTNMinion, Log, TEXT("MinionNet server summary items=%d packs=%d dormant=%d adds=%lld updates=%lld removes=%lld packSends=%lld wakes=%lld flushes=%lld sleeps=%lld relevancyPasses=%lld granted=%d deniedBudget=%d outOfRange=%d overInclusion=%.3f sent=%lld beyond=%lld registryHash=%08x"),
		Items, GetNumPacks(), Dormant, NetStats.ItemAdds, NetStats.ItemUpdates, NetStats.ItemRemoves, NetStats.PackSends, NetStats.Wakes, NetStats.Flushes,
		NetStats.Sleeps, NetStats.RelevancyPasses, NetStats.LastGranted, NetStats.LastDeniedByBudget, NetStats.LastOutOfRange, NetStats.OverInclusion.Ratio(),
		NetStats.OverInclusion.ItemsSent, NetStats.OverInclusion.ItemsBeyond, NetStats.RegistryHash);
}

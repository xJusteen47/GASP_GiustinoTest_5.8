// Minion runtime (authority batch, M8 G3; spike 3 scenario, course and robustness counters G4; debug draw G12).

#include "Debug/GSTN_MinionDebugDraw.h"

#include "Brain/GSTN_MinionBrainSubsystem.h"
#include "Data/GSTN_MinionDefinition.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Intent/GSTN_MinionIntent.h"
#include "LagComp/GSTN_MinionHistory.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "Store/GSTN_MinionStore.h"

#if !UE_BUILD_SHIPPING

namespace
{
	int32 GDebugCapsules = 0;
	int32 GDebugHitboxes = 0;
	int32 GDebugLOD = 0;
	int32 GDebugHistory = 0;
	int32 GDebugIntent = 0;

	/** Mirror of "any variable on", refreshed when a variable changes, so the tick check is one read */
	bool GDebugAny = false;
	int32 GDebugPasses = 0;

	void RefreshAny(IConsoleVariable* /*Changed*/)
	{
		GDebugAny = GDebugCapsules > 0 || GDebugHitboxes > 0 || GDebugLOD > 0 || GDebugHistory > 0 || GDebugIntent > 0;
	}

	FAutoConsoleVariableRef CVarDebugCapsules(TEXT("gstn.Minion.Debug.Capsules"), GDebugCapsules,
		TEXT("1 = draw a capsule per live minion on the host, coloured by LOD tier (not in Shipping)."), FConsoleVariableDelegate::CreateStatic(&RefreshAny), ECVF_Default);
	FAutoConsoleVariableRef CVarDebugHitboxes(TEXT("gstn.Minion.Debug.Hitboxes"), GDebugHitboxes,
		TEXT("1 = draw the reach sphere (MaxRange) of the move each attacking minion plays (not in Shipping)."), FConsoleVariableDelegate::CreateStatic(&RefreshAny), ECVF_Default);
	FAutoConsoleVariableRef CVarDebugLOD(TEXT("gstn.Minion.Debug.LOD"), GDebugLOD,
		TEXT("1 = draw the LOD tier as text above each minion (not in Shipping)."), FConsoleVariableDelegate::CreateStatic(&RefreshAny), ECVF_Default);
	FAutoConsoleVariableRef CVarDebugHistory(TEXT("gstn.Minion.Debug.History"), GDebugHistory,
		TEXT("N = draw the last N history poses (lag compensation) of each minion as points, N up to the ring depth (not in Shipping)."), FConsoleVariableDelegate::CreateStatic(&RefreshAny), ECVF_Default);
	FAutoConsoleVariableRef CVarDebugIntent(TEXT("gstn.Minion.Debug.Intent"), GDebugIntent,
		TEXT("1 = draw each minion's intent (direction and speed) and brain waypoint as lines (not in Shipping)."), FConsoleVariableDelegate::CreateStatic(&RefreshAny), ECVF_Default);

	/** By EGSTN_MinionLODTier: Combat, Near, Far, Dormant */
	const FColor TierColors[] = { FColor::Red, FColor::Orange, FColor::Yellow, FColor(110, 110, 110) };
	const TCHAR* const TierNames[] = { TEXT("T0 Combat"), TEXT("T1 Near"), TEXT("T2 Far"), TEXT("T3 Dormant") };

	uint8 ClampTier(uint8 Tier)
	{
		return FMath::Min<uint8>(Tier, static_cast<uint8>(UE_ARRAY_COUNT(TierColors) - 1));
	}
}

bool GSTNMinionDebug::IsAnyEnabled()
{
	return GDebugAny;
}

int32 GSTNMinionDebug::GetPassCount()
{
	return GDebugPasses;
}

#else

bool GSTNMinionDebug::IsAnyEnabled()
{
	return false;
}

int32 GSTNMinionDebug::GetPassCount()
{
	return 0;
}

#endif

bool UGSTN_MinionDebugSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if !UE_BUILD_SHIPPING
	return Super::ShouldCreateSubsystem(Outer);
#else
	return false;
#endif
}

bool UGSTN_MinionDebugSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UGSTN_MinionDebugSubsystem::IsTickable() const
{
	return GSTNMinionDebug::IsAnyEnabled() && !IsTemplate();
}

TStatId UGSTN_MinionDebugSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGSTN_MinionDebugSubsystem, STATGROUP_Tickables);
}

void UGSTN_MinionDebugSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
#if ENABLE_DRAW_DEBUG && !UE_BUILD_SHIPPING
	if (!GDebugAny)
	{
		return;
	}
	UWorld* World = GetWorld();
	const UGSTN_MinionCombatSubsystem* Combat = UGSTN_MinionCombatSubsystem::GetActive(World);
	if (!World || !Combat)
	{
		return;
	}
	++GDebugPasses;

	const FGSTN_MinionStore& Store = Combat->GetStore();
	const FGSTN_MinionHistory& History = Combat->GetHistory();
	const UGSTN_MinionBrainSubsystem* Brain = GDebugIntent > 0 ? UGSTN_MinionBrainSubsystem::Get(World) : nullptr;
	const int32 Frame = Combat->GetLastBatchFrame();
	const int32 HistoryPoses = FMath::Clamp(GDebugHistory, 0, FGSTN_MinionHistory::DefaultDepth);

	for (int32 Slot = Store.NextAliveSlot(0); Slot != INDEX_NONE; Slot = Store.NextAliveSlot(Slot + 1))
	{
		const EGSTN_MinionActionState State = static_cast<EGSTN_MinionActionState>(Store.ActionState[Slot]);
		if (State == EGSTN_MinionActionState::Dead)
		{
			continue;
		}
		const UGSTN_MinionDefinition* Definition = Combat->GetDefinition(Store.TypeIndex[Slot]);
		const uint8 Tier = ClampTier(Store.LODTier[Slot]);
		const FColor Color = TierColors[Tier];
		const FVector Feet(Store.Position[Slot]);
		const float Radius = Definition ? Definition->Stats.CapsuleRadius : 34.f;
		const float HalfHeight = Definition ? Definition->Stats.CapsuleHalfHeight : 88.f;
		const FVector Center = Feet + FVector(0.0, 0.0, HalfHeight);

		if (GDebugCapsules > 0)
		{
			DrawDebugCapsule(World, Center, HalfHeight, Radius, FQuat::Identity, Color, false, 0.f);
		}

		if (GDebugLOD > 0)
		{
			DrawDebugString(World, Feet + FVector(0.0, 0.0, HalfHeight * 2.0 + 25.0), TierNames[Tier], nullptr, Color, 0.f, true);
		}

		if (GDebugHitboxes > 0 && Definition && State == EGSTN_MinionActionState::Move)
		{
			const int32 MoveIndex = static_cast<int32>(Store.MoveId[Slot]) - 1;
			if (Definition->Moves.IsValidIndex(MoveIndex))
			{
				// The reach sphere covers [0, MaxRange] in front of the minion: centred at half the range, radius half the range
				const float Reach = Definition->Moves[MoveIndex].MaxRange;
				const FVector Forward = FRotator(0.0, Store.YawDegrees[Slot], 0.0).Vector();
				DrawDebugSphere(World, Center + Forward * (0.5 * Reach), 0.5f * Reach, 12, FColor::Magenta, false, 0.f);
			}
		}

		if (HistoryPoses > 0)
		{
			for (int32 Age = 0; Age < HistoryPoses; ++Age)
			{
				const FGSTN_MinionHistoryRecord* Record = History.Find(Slot, Frame - Age);
				// Find answers with the latest record at or before the frame; a gap would draw one pose several times
				if (Record && Record->Frame == Frame - Age)
				{
					const uint8 Fade = static_cast<uint8>(255 - (Age * 200) / HistoryPoses);
					DrawDebugPoint(World, FVector(Record->Position) + FVector(0.0, 0.0, HalfHeight), 6.f, FColor(Fade, Fade, 255), false, 0.f);
				}
			}
		}

		if (GDebugIntent > 0)
		{
			const FGSTN_MinionIntent& Intent = Store.Intent[Slot];
			// A stale intent is ignored by the batch, so it is not drawn either
			if (Intent.StampFrame != INDEX_NONE && Frame - Intent.StampFrame <= GSTNMinion::IntentStaleFrames && Intent.DesiredSpeed > 0.f)
			{
				const FVector Direction(Intent.MoveDir.X, Intent.MoveDir.Y, 0.0);
				DrawDebugDirectionalArrow(World, Center, Center + Direction * (0.5 * Intent.DesiredSpeed), 30.f, FColor::Green, false, 0.f);
			}
			FVector Waypoint;
			if (Brain && Brain->GetSlotWaypoint(Slot, Waypoint))
			{
				DrawDebugLine(World, Center, Waypoint + FVector(0.0, 0.0, HalfHeight), FColor::Cyan, false, 0.f);
			}
		}
	}
#endif
}

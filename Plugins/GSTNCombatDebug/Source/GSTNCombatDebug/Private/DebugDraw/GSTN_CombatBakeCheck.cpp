#include "DebugDraw/GSTN_CombatBakeCheck.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GSTNCombatDebugModule.h"
#include "HAL/IConsoleManager.h"
#include "Simulation/GSTN_CombatActionTypes.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTableSet.h"

namespace
{
	TAutoConsoleVariable<int32> CVarCombatDebugBakeCheck(TEXT("gstn.Combat.Debug.BakeCheck"), 0,
		TEXT("Draw each baked socket of the running timeline next to the live mesh socket, on the machines that simulate the pawn. 0 = off, 1 = draw, 2 = draw and log the gap per socket and frame."),
		ECVF_Default);

	const FColor BakedColor = FColor::Cyan;
	const FColor LiveColor = FColor::Magenta;
}

FGSTN_BakeCheckResult GSTNCombatBakeCheck::Compare(const FTransform& BakedRootRelative, const FTransform& Root, const FTransform& LiveWorld)
{
	FGSTN_BakeCheckResult Result;
	Result.BakedWorld = BakedRootRelative * Root;
	Result.DistanceCm = FVector::Dist(Result.BakedWorld.GetLocation(), LiveWorld.GetLocation());
	Result.AngleDeg = FMath::RadiansToDegrees(Result.BakedWorld.GetRotation().AngularDistance(LiveWorld.GetRotation()));
	return Result;
}

bool UGSTN_CombatBakeCheck::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UGSTN_CombatBakeCheck::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGSTN_CombatBakeCheck, STATGROUP_Tickables);
}

ETickableTickType UGSTN_CombatBakeCheck::GetTickableTickType() const
{
	return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Always;
}

void UGSTN_CombatBakeCheck::Tick(float DeltaTime)
{
	const int32 Mode = CVarCombatDebugBakeCheck.GetValueOnGameThread();
	UWorld* World = GetWorld();
	if (Mode <= 0 || !World)
	{
		return;
	}
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		const UGSTN_CombatSimComponent* Combat = Pawn->FindComponentByClass<UGSTN_CombatSimComponent>();
		const FGSTN_CombatSyncState* Sync = Combat && Combat->TicksOnThisMachine() ? Combat->GetSyncState() : nullptr;
		if (!Sync || Sync->Core.ActionState != EGSTN_ActionState::InTimeline || Sync->Core.TimelineStartFrame == INDEX_NONE)
		{
			continue;
		}
		const FGSTN_CombatSyncCore& Core = Sync->Core;
		const UGSTN_CombatTableSet* Set = Combat->FindTableSet(Core.TableRevision);
		if (!Set || !Set->TimelineRegistry.IsValidIndex(Core.ActiveTimeline) || !Set->TimelineRegistry[Core.ActiveTimeline])
		{
			continue;
		}
		const FGSTN_CombatTimelineData& Data = *Set->GetTimelineData(Core.ActiveTimeline);
		if (Data.BakedSockets.IsEmpty())
		{
			continue;
		}
		const int32 Frame = FMath::Max(Core.Input.History.GetLatestFrame() - Core.TimelineStartFrame - Core.TimelinePausedFrames, 0);
		// The attacker step's root: the pawn's pose of the simulated frame (the actor follows it on a machine that simulates), unscaled
		FTransform Root = Pawn->GetActorTransform();
		Root.SetScale3D(FVector::OneVector);
		const USkeletalMeshComponent* Mesh = Pawn->FindComponentByClass<USkeletalMeshComponent>();

		for (const TPair<FName, FGSTN_BakedSocketFrames>& Pair : Data.BakedSockets)
		{
			FTransform Baked;
			if (!Data.FindBakedSocket(Pair.Key, Frame, Baked))
			{
				continue;
			}
			const bool bHasLive = Mesh && Mesh->DoesSocketExist(Pair.Key);
			const FTransform Live = bHasLive ? Mesh->GetSocketTransform(Pair.Key) : FTransform::Identity;
			const FGSTN_BakeCheckResult Result = GSTNCombatBakeCheck::Compare(Baked, Root, Live);

			DrawDebugCoordinateSystem(World, Result.BakedWorld.GetLocation(), Result.BakedWorld.Rotator(), 12.f, false, -1.f, SDPG_Foreground, 0.5f);
			DrawDebugSphere(World, Result.BakedWorld.GetLocation(), 3.f, 8, BakedColor, false, -1.f, SDPG_Foreground);
			FString Label = FString::Printf(TEXT("%s f%d"), *Pair.Key.ToString(), Frame);
			if (bHasLive)
			{
				DrawDebugSphere(World, Live.GetLocation(), 3.f, 8, LiveColor, false, -1.f, SDPG_Foreground);
				DrawDebugLine(World, Result.BakedWorld.GetLocation(), Live.GetLocation(), FColor::White, false, -1.f, SDPG_Foreground, 0.5f);
				Label += FString::Printf(TEXT("  %.1f cm  %.0f deg"), Result.DistanceCm, Result.AngleDeg);
			}
			else
			{
				Label += TEXT("  (no live socket)");
			}
			DrawDebugString(World, Result.BakedWorld.GetLocation() + FVector(0.0, 0.0, 8.0), Label, nullptr, BakedColor, 0.f, true, 1.f);
			if (Mode >= 2)
			{
				UE_LOG(LogGSTNCombatDebug, Display, TEXT("BakeCheck %s T%d f%d %s live=%d d=%.1f a=%.1f"), *Pawn->GetName(), Core.ActiveTimeline, Frame,
					*Pair.Key.ToString(), bHasLive ? 1 : 0, Result.DistanceCm, Result.AngleDeg);
			}
		}
	}
}

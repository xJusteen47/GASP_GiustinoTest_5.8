// Defence, Network Prediction half.

#include "Debug/GSTN_DefenceDebugDraw.h"

#include "AbilitySystemGlobals.h"
#include "Data/GSTN_DefenceRules.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Math/GSTN_DefenceMath.h"
#include "Sim/GSTN_DefenceAttach.h"
#include "Sim/GSTN_DefenceFragment.h"
#include "Sim/GSTN_DefenceSnapshotContributor.h"
#include "Sim/GSTN_DefenceTickExtension.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "World/GSTN_CombatWorldSubsystem.h"

namespace
{
	int32 GShieldDebugDraw = 0;
	int32 GTechDebugDraw = 0;
	FAutoConsoleVariableRef CVarShieldDebugDraw(TEXT("gstn.Combat.Debug.Shield"), GShieldDebugDraw,
		TEXT("1 draws the shield of every pawn with defence on this machine: bubble sphere, value bar, parry windows (cyan: perfect shield, magenta: a move's window, white: just parried)."),
		ECVF_Cheat);
	FAutoConsoleVariableRef CVarTechDebugDraw(TEXT("gstn.Combat.Debug.Tech"), GTechDebugDraw,
		TEXT("1 draws the tech state of every pawn with defence on this machine: lockout, the last press's window, the tech and the downed state, as bars with their remaining frames."),
		ECVF_Cheat);

	FDelegateHandle GPostActorTickHandle;

	/** Frames a parried hit stays shown in white */
	constexpr int32 ParryFlashFrames = 20;
	constexpr float BarWidth = 100.f;

#if ENABLE_DRAW_DEBUG
	/** A horizontal bar over the pawn, facing the world's +Y so it reads the same from every camera: grey track, coloured fill */
	void DrawBar(const UWorld* World, const FVector& Left, float Fraction, const FColor& Color, const FString& Label)
	{
		const FVector Right = Left + FVector(0.f, BarWidth, 0.f);
		DrawDebugLine(World, Left, Right, FColor(60, 60, 60), false, -1.f, 0, 4.f);
		const float Clamped = FMath::Clamp(Fraction, 0.f, 1.f);
		if (Clamped > 0.f)
		{
			DrawDebugLine(World, Left, Left + FVector(0.f, BarWidth * Clamped, 0.f), Color, false, -1.f, 0, 6.f);
		}
		DrawDebugString(World, Right + FVector(0.f, 8.f, 0.f), Label, nullptr, Color, 0.f, true);
	}

	/** Remaining frames of [.., EndFrame) at Frame, 0 when past or unset */
	int32 Remaining(int32 EndFrame, int32 Frame)
	{
		return EndFrame == INDEX_NONE ? 0 : FMath::Max(EndFrame - Frame, 0);
	}
#endif
}

bool FGSTN_DefenceDebugDraw::IsShieldEnabled()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return GShieldDebugDraw != 0;
#endif
}

bool FGSTN_DefenceDebugDraw::IsTechEnabled()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return GTechDebugDraw != 0;
#endif
}

void FGSTN_DefenceDebugDraw::Register()
{
#if ENABLE_DRAW_DEBUG && !UE_BUILD_SHIPPING
	if (!GPostActorTickHandle.IsValid())
	{
		GPostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddLambda([](UWorld* World, ELevelTick, float) { DrawWorld(World); });
	}
#endif
}

void FGSTN_DefenceDebugDraw::Unregister()
{
	if (GPostActorTickHandle.IsValid())
	{
		FWorldDelegates::OnWorldPostActorTick.Remove(GPostActorTickHandle);
		GPostActorTickHandle.Reset();
	}
}

void FGSTN_DefenceDebugDraw::DrawWorld(UWorld* World)
{
#if ENABLE_DRAW_DEBUG
	if ((!IsShieldEnabled() && !IsTechEnabled()) || !World || !World->IsGameWorld())
	{
		return;
	}
	const UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(World);
	if (!Combat)
	{
		return;
	}
	TArray<int32> Ids;
	Combat->GetRegisteredEntityIds(Ids);
	for (const int32 Id : Ids)
	{
		DrawEntity(Combat->FindOwnerActor(Id));
	}
#endif
}

void FGSTN_DefenceDebugDraw::DrawEntity(const AActor* Owner)
{
#if ENABLE_DRAW_DEBUG
	const FGSTN_DefenceEntityData* Data = Owner ? FGSTN_DefenceAttach::Find(Owner) : nullptr;
	const UGSTN_CombatSimComponent* Combat = Owner ? Owner->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	const FGSTN_CombatSyncState* Sync = Combat ? Combat->GetSyncState() : nullptr;
	if (!Data || !Data->Rules || !Sync)
	{
		return;
	}
	const UGSTN_DefenceRules& Rules = *Data->Rules;
	const UWorld* World = Owner->GetWorld();
	const FGSTN_DefenceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Sync->Fragments);
	const FGSTN_DefenceFragment Defaults;
	const FGSTN_DefenceFragment& Frag = Fragment ? *Fragment : Defaults;
	// The frame of the state shown here, not this machine's newest simulated frame: an interpolated pawn is drawn at its own frame
	const int32 Frame = Sync->Core.Input.History.GetLatestFrame();
	const FVector Location = Owner->GetActorLocation();
	FVector BarLeft = Location + FVector(0.f, -BarWidth * 0.5f, 125.f);

	if (IsShieldEnabled())
	{
		const bool bAvailable = UGSTN_DefenceMath::IsShieldAvailable(&Rules, Data->bKitHasShield);
		if (bAvailable)
		{
			const FGSTN_ShieldRates Rates = FGSTN_DefenceTickExtension::ReadRates(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner), Rules);
			const double Value = UGSTN_DefenceMath::ShieldValueAt(Frag.MakeShieldView(Rates), FMath::Max(Frame, 0), Rates);
			const float Fraction = Rates.Max > 0.0 ? static_cast<float>(Value / Rates.Max) : 0.f;
			const FColor ValueColor = Frag.IsBreaking() ? FColor::Red : FColor::MakeRedToGreenColorFromScalar(Fraction);
			DrawBar(World, BarLeft, Frag.IsBreaking() ? 0.f : Fraction, ValueColor, FString::Printf(TEXT("shield %.1f%s%s%s"), Value,
				Frag.IsRaised() ? TEXT(" raised") : TEXT(""), Frag.IsBreaking() ? *FString::Printf(TEXT(" broken %d"), Remaining(Frag.BreakEndFrame, Frame)) : TEXT(""),
				Frag.IsShieldStunned(Frame) ? *FString::Printf(TEXT(" stun %d"), Remaining(Frag.ShieldStunEndFrame, Frame)) : TEXT("")));
			BarLeft.Z += 18.f;

			if (!Frag.IsBreaking() && FGSTN_DefenceSnapshotContributor::BubbleExistsAt(Frag, Frame, Rules.Shield.ReleaseFrames))
			{
				// The bubble the snapshot contributor builds for this frame; a hit inside the perfect shield window would parry
				const FGSTN_ShieldBubble Bubble = UGSTN_DefenceMath::MakeShieldBubble(Rules.Shield, Owner->GetActorTransform());
				const bool bPerfect = UGSTN_DefenceMath::EvaluateParryWindow(Frag.ShieldPressFrame, Frame, Frag.ShieldReleaseFrame, Rules.Parry);
				DrawDebugSphere(World, Bubble.Centre, static_cast<float>(Bubble.Radius), 20, bPerfect ? FColor::Cyan : FColor(70, 140, 255), false, -1.f, 0,
					bPerfect ? 2.5f : 1.f);
			}
		}
		if (Frag.HasMoveParryWindow() && (Frag.IsMoveParryWindowOpen(Frame) || Frame < Frag.MoveParryStartFrame))
		{
			const bool bOpen = Frag.IsMoveParryWindowOpen(Frame);
			const int32 Length = FMath::Max(Frag.MoveParryEndFrame - Frag.MoveParryStartFrame, 1);
			DrawBar(World, BarLeft, bOpen ? static_cast<float>(Frag.MoveParryEndFrame - Frame) / Length : 0.f, FColor::Magenta,
				FString::Printf(TEXT("parry window %s %d"), bOpen ? TEXT("open") : TEXT("in"), bOpen ? Frag.MoveParryEndFrame - Frame : Frag.MoveParryStartFrame - Frame));
			BarLeft.Z += 18.f;
			if (bOpen)
			{
				DrawDebugCapsule(World, Location, 95.f, 45.f, Owner->GetActorQuat(), FColor::Magenta, false, -1.f, 0, 1.5f);
			}
		}
		if (Frag.LastParryFrame != INDEX_NONE && Frame >= Frag.LastParryFrame && Frame < Frag.LastParryFrame + ParryFlashFrames)
		{
			DrawDebugCapsule(World, Location, 100.f, 50.f, Owner->GetActorQuat(), FColor::White, false, -1.f, 0, 3.f);
			DrawDebugString(World, Location + FVector(0.f, 0.f, 100.f), FString::Printf(TEXT("parried @%d"), Frag.LastParryFrame), nullptr, FColor::White, 0.f, true);
		}
		if (Frag.IsPunished(Frame))
		{
			DrawDebugString(World, Location + FVector(0.f, 0.f, 90.f), FString::Printf(TEXT("punished %d"), Remaining(Frag.PunishEndFrame, Frame)), nullptr, FColor::Orange, 0.f, true);
		}
	}

	if (IsTechEnabled())
	{
		const FGSTN_TechRules& Tech = Rules.Tech;
		if (Frag.IsTechLockedOut(Frame))
		{
			DrawBar(World, BarLeft, static_cast<float>(Remaining(Frag.TechLockoutEndFrame, Frame)) / FMath::Max(Tech.LockoutFrames, 1), FColor::Red,
				FString::Printf(TEXT("tech lockout %d"), Remaining(Frag.TechLockoutEndFrame, Frame)));
			BarLeft.Z += 18.f;
		}
		// A press counts for a contact up to WindowFrames later (m6 spec 6): what is left of the last press's window
		const int32 PressLeft = Frag.LastTechPressFrame == INDEX_NONE ? 0 : Remaining(Frag.LastTechPressFrame + Tech.WindowFrames, Frame);
		if (PressLeft > 0)
		{
			DrawBar(World, BarLeft, static_cast<float>(PressLeft) / FMath::Max(Tech.WindowFrames, 1), FColor::Green, FString::Printf(TEXT("tech window %d"), PressLeft));
			BarLeft.Z += 18.f;
		}
		if (Frag.IsTeching(Frame))
		{
			DrawDebugString(World, BarLeft, FString::Printf(TEXT("teching type %d, %d left"), static_cast<int32>(Frag.TechType), Remaining(Frag.TechEndFrame, Frame)), nullptr,
				FColor::Green, 0.f, true);
			BarLeft.Z += 18.f;
		}
		if (Frag.IsKnockedDown(Frame))
		{
			DrawDebugString(World, BarLeft, Frag.KnockdownEndFrame == MAX_int32 ? FString(TEXT("downed until a get-up"))
				: FString::Printf(TEXT("downed %d"), Remaining(Frag.KnockdownEndFrame, Frame)), nullptr, FColor::Yellow, 0.f, true);
		}
	}
#endif
}

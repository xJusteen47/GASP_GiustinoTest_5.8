// Defence, Network Prediction half.

#include "Sim/GSTN_SDIDrift.h"

#include "AbilitySystemComponent.h"
#include "Data/GSTN_DefenceRules.h"
#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"
#include "GSTN_CombatNPTags.h"
#include "Math/GSTN_SDIMath.h"
#include "Movement/GSTN_LayeredMove_Freeze.h"
#include "Sim/GSTN_DefenceAttach.h"
#include "Sim/GSTN_DefenceFragment.h"
#include "Sim/GSTN_DIWiring.h"
#include "Sim/GSTN_TumbleLanding.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTickExtension.h"

bool FGSTN_SDIDrift::IsDriftFrame(const FGSTN_CombatSyncCore& Core, int32 Frame)
{
	const bool bStaggered = Core.ActionState == EGSTN_ActionState::Hitstun || Core.ActionState == EGSTN_ActionState::Tumble;
	return bStaggered && Core.IsInHitstop(Frame) && Frame + 1 < Core.HitstopEndFrame;
}

FGSTN_SDIStep FGSTN_SDIDrift::Step(FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_SDIRules& Rules, uint8 HitFlags, const FVector& StickOnPlane, const FGameplayTagContainer& VictimTags)
{
	if (!IsDriftFrame(Core, Frame))
	{
		return FGSTN_SDIStep();
	}
	const double Used = static_cast<double>(Core.Hit.SDIDistanceUsed) / FGSTN_HitSyncState::SDIUnitsPerCm;
	const bool bBlocked = (HitFlags & GSTNDefenceHitFlags::SDIBlocked) != 0;
	const int32 HitstopFrames = FMath::Max(Core.HitstopEndFrame - Core.HitstopStartFrame, 1);
	// The blocked hit is passed as a throw: NextDisplacement refuses it the same way, the reason (throw, armor, tier) was decided at the hit
	const FGSTN_SDIStep Drift = UGSTN_SDIMath::NextDisplacement(Rules, StickOnPlane, Used, false, false, bBlocked, VictimTags, HitstopFrames, FrameSeconds);
	if (Drift.DistanceAdded > 0.0)
	{
		// Rounded up: the stored distance never under-counts, so the quantised total can never let the drift pass the cap
		const int64 Units = static_cast<int64>(Core.Hit.SDIDistanceUsed) + FMath::CeilToInt64(Drift.DistanceAdded * FGSTN_HitSyncState::SDIUnitsPerCm - 1e-6);
		Core.Hit.SDIDistanceUsed = static_cast<uint16>(FMath::Clamp<int64>(Units, 0, MAX_uint16));
	}
	return Drift;
}

void FGSTN_SDIDrift::Run(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data)
{
	FGSTN_CombatSyncCore& Core = Context.Out.Core;
	if (!Data.Rules || !IsDriftFrame(Core, Context.Frame) || Context.StickOnPlane.IsNearlyZero())
	{
		return;
	}
	const FGSTN_DefenceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Context.Out.Fragments);
	const FGSTN_SDIStep Drift = Step(Core, Context.Frame, Data.Rules->SDI, Fragment ? Fragment->HitFlags : 0, Context.StickOnPlane, FGSTN_DIWiring::ReadVictimTags(Context));
	if (!Drift.DriftVelocity.IsNearlyZero())
	{
		ApplyDrift(Context.AbilitySystem, Drift.DriftVelocity);
	}
}

void FGSTN_SDIDrift::ApplyDrift(UAbilitySystemComponent* AbilitySystem, const FVector& Velocity)
{
	if (!FGSTN_TumbleLanding::CanQueueMoves(AbilitySystem))
	{
		return;
	}
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UGSTN_GE_SDIDrift::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityX, static_cast<float>(Velocity.X));
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityY, static_cast<float>(Velocity.Y));
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityZ, static_cast<float>(Velocity.Z));
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_FreezeSeconds, FrameSeconds);
	AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

UGSTN_GE_SDIDrift::UGSTN_GE_SDIDrift()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	UGSTN_FreezeMoveCreator* Creator = CreateDefaultSubobject<UGSTN_FreezeMoveCreator>(TEXT("DriftMoveCreator"));
	Creator->bDriftFromSetByCaller = true;
	Creator->LayeredMoveDefaultParams.CopyDurationFromEffect = false;
	ApplyMove->MoveProcessor = Creator;
	GEComponents.Add(ApplyMove);
}

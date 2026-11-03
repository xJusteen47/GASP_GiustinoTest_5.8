// Destructible parts, ability-system half (no Network Prediction, no Mover).

#include "Effects/GSTN_PartBreakEffectApplier.h"

#include "AbilitySystemComponent.h"
#include "Data/GSTN_PartBreakEffects.h"
#include "Data/GSTN_PartsLimits.h"
#include "GameplayEffect.h"

namespace
{
	/** Index of the lowest set bit, which it then clears */
	int32 PopLowestBit(uint16& Mask)
	{
		const int32 Index = FMath::CountTrailingZeros(static_cast<uint32>(Mask));
		Mask = static_cast<uint16>(Mask & (Mask - 1));
		return Index;
	}
}

FGSTN_PartLaunchRequest FGSTN_PartBreakEffectApplier::MakeLaunchRequest(const FGSTN_PartBreakEffect_Launch& Effect, const FVector& Up, const FVector& HitDirection, int32 Frames)
{
	FGSTN_PartLaunchRequest Request;
	const FVector UpDir = Up.GetSafeNormal();
	if (UpDir.IsZero() || !(Effect.Speed > 0.f))
	{
		return Request;
	}
	const float UpFraction = FMath::Clamp(Effect.UpFraction, 0.f, 1.f);
	const FVector Horizontal = (HitDirection - UpDir * FVector::DotProduct(HitDirection, UpDir)).GetSafeNormal();
	Request.Velocity = Horizontal * (Effect.Speed * (1.f - UpFraction)) + UpDir * (Effect.Speed * UpFraction);
	Request.Frames = FMath::Max(0, Frames);
	return Request;
}

int32 FGSTN_PartBreakEffectApplier::GetMinimumStaggerFrames(TConstArrayView<FGSTN_PartDefinition> Parts, uint16 Mask)
{
	int32 Longest = 0;
	while (Mask != 0)
	{
		const int32 Index = PopLowestBit(Mask);
		if (Parts.IsValidIndex(Index))
		{
			for (const FInstancedStruct& Entry : Parts[Index].OnBreak)
			{
				if (const FGSTN_PartBreakEffect_Stagger* Stagger = Entry.GetPtr<FGSTN_PartBreakEffect_Stagger>())
				{
					Longest = FMath::Max(Longest, Stagger->Frames);
				}
			}
		}
	}
	return Longest;
}

void FGSTN_PartBreakEffectApplier::Collect(TConstArrayView<FGSTN_PartDefinition> Parts, uint16 NewlyBrokenMask, const FVector& Up, const FVector& HitDirection,
	int32 LaunchFrames, FGSTN_PartBreakRequests& Out)
{
	Out.StatsParts.Reset();
	Out.Launch = FGSTN_PartLaunchRequest();
	Out.MinimumHitstunFrames = GetMinimumStaggerFrames(Parts, NewlyBrokenMask);

	float StrongestSpeed = 0.f;
	uint16 Mask = NewlyBrokenMask;
	while (Mask != 0)
	{
		const int32 Index = PopLowestBit(Mask);
		if (!Parts.IsValidIndex(Index))
		{
			continue;
		}
		bool bHasStats = false;
		for (const FInstancedStruct& Entry : Parts[Index].OnBreak)
		{
			if (Entry.GetPtr<FGSTN_PartBreakEffect_Stats>())
			{
				bHasStats = true;
			}
			else if (const FGSTN_PartBreakEffect_Launch* Launch = Entry.GetPtr<FGSTN_PartBreakEffect_Launch>())
			{
				// Strict greater: on a tie the lower part index, which comes first, keeps the move
				if (Launch->Speed > StrongestSpeed)
				{
					const FGSTN_PartLaunchRequest Request = MakeLaunchRequest(*Launch, Up, HitDirection, LaunchFrames);
					if (Request.IsSet())
					{
						StrongestSpeed = Launch->Speed;
						Out.Launch = Request;
					}
				}
			}
		}
		if (bHasStats)
		{
			Out.StatsParts.Add(Index);
		}
	}
}

void FGSTN_PartBreakEffectApplier::CollectStatValues(const FGSTN_PartDefinition& Part, TArray<FGSTN_StatValue>& Out)
{
	Out.Reset();
	for (const FInstancedStruct& Entry : Part.OnBreak)
	{
		const FGSTN_PartBreakEffect_Stats* Stats = Entry.GetPtr<FGSTN_PartBreakEffect_Stats>();
		if (!Stats)
		{
			continue;
		}
		for (const FGSTN_PartStatMagnitude& Magnitude : Stats->Magnitudes)
		{
			if (!Magnitude.Stat.IsValid())
			{
				continue;
			}
			FGSTN_StatValue* Existing = Out.FindByPredicate([&Magnitude](const FGSTN_StatValue& Value) { return Value.Stat == Magnitude.Stat; });
			if (Existing)
			{
				Existing->Value += Magnitude.Magnitude;
			}
			else
			{
				FGSTN_StatValue& Added = Out.AddDefaulted_GetRef();
				Added.Stat = Magnitude.Stat;
				Added.Value = Magnitude.Magnitude;
			}
		}
	}
}

int32 FGSTN_PartBreakEffectApplier::ApplyStats(UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_PartStats> EffectClass, TConstArrayView<FGSTN_PartDefinition> Parts, uint16 Mask)
{
	int32 Applied = 0;
	TArray<FGSTN_StatValue> Values;
	while (Mask != 0)
	{
		const int32 Index = PopLowestBit(Mask);
		if (!Parts.IsValidIndex(Index))
		{
			continue;
		}
		CollectStatValues(Parts[Index], Values);
		if (Values.IsEmpty() || UGSTN_GE_PartStats::IsApplied(ASC, EffectClass, Index))
		{
			continue;
		}
		Applied += UGSTN_GE_PartStats::Apply(ASC, EffectClass, Index, Values).IsValid() ? 1 : 0;
	}
	return Applied;
}

int32 FGSTN_PartBreakEffectApplier::RemoveStats(UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_PartStats> EffectClass, uint16 Mask)
{
	int32 Removed = 0;
	while (Mask != 0)
	{
		Removed += UGSTN_GE_PartStats::Remove(ASC, EffectClass, PopLowestBit(Mask));
	}
	return Removed;
}

FGameplayEffectSpecHandle FGSTN_PartBreakEffectApplier::MakeLaunchSpec(UAbilitySystemComponent& ASC, TSubclassOf<UGameplayEffect> EffectClass,
	const FGSTN_PartLaunchRequest& Request, float StepMs, const FGSTN_PartLaunchKeys& Keys)
{
	if (EffectClass == nullptr || !Request.IsSet())
	{
		return FGameplayEffectSpecHandle();
	}
	FGameplayEffectSpecHandle Spec = ASC.MakeOutgoingSpec(EffectClass, 1.f, ASC.MakeEffectContext());
	if (!Spec.IsValid())
	{
		return Spec;
	}
	const auto SetKey = [&Spec](const FGameplayTag& Key, float Value)
	{
		if (Key.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(Key, Value);
		}
	};
	SetKey(Keys.VelocityX, static_cast<float>(Request.Velocity.X));
	SetKey(Keys.VelocityY, static_cast<float>(Request.Velocity.Y));
	SetKey(Keys.VelocityZ, static_cast<float>(Request.Velocity.Z));
	SetKey(Keys.DecayPerFrame, Request.DecayPerFrame);
	SetKey(Keys.GravityScale, Request.GravityScale);
	SetKey(Keys.Seconds, Request.Frames * StepMs * 0.001f);
	return Spec;
}

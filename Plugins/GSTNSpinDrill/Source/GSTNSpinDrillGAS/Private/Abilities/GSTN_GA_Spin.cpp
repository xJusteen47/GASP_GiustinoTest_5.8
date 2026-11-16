// Spin and drill, ability-system half.

#include "Abilities/GSTN_GA_Spin.h"

#include "Data/GSTN_SpinTimelines.h"
#include "Effects/GSTN_SpinDrillEffects.h"
#include "GameplayEffect.h"
#include "GSTN_SpinDrillTags.h"

UGSTN_GA_Spin::UGSTN_GA_Spin(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HopEffect = UGSTN_GE_AirSpinHop::StaticClass();

	// The native tag, not a request by name: a class default is built when the module loads, possibly before the tag manager has its tree
	// Added to the base's tags, never replacing them: Ability.Timeline (UGSTN_GA_BaseCombat) is how the loadout compile knows a timeline
	// ability; without it the command activates the ability as a plain one and the combat tick never adopts it (SD-05)
	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(GSTNSpinDrillTags::Ability_Movement_Spin.GetTag());
	SetAssetTags(Tags);
}

const FGSTN_SpinTrack* UGSTN_GA_Spin::FindSpinTrack() const
{
	return FGSTN_SpinTrack::Find(GetTimelineData());
}

void UGSTN_GA_Spin::OnTimelineBegan(int32 StateStartFrame)
{
	Super::OnTimelineBegan(StateStartFrame);

	// The hop only: the low gravity and the air spin count are the tick extension's, from the same track, in the same frame
	const FGSTN_SpinTrack* Spin = FindSpinTrack();
	if (Spin && Spin->bAirSpin && HopEffect)
	{
		ApplyGameplayEffectToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), HopEffect.GetDefaultObject(), GetAbilityLevel());
	}
}

void UGSTN_GA_Spin::SetDefaultSpin(EGSTN_SpinSense Sense, bool bAirSpin)
{
	// No ActivationBlockedTags on Combat.State.AirSpinSpent: the combat tick adopts the timeline and spends the air spin before the ability
	// system activates the ability in the same frame, so that block refused every air spin (SD-07). The once-per-airtime gate is the kit
	// binding's IgnoreTags (checked at the command match, on the previous frame's tags) and the tick extension's CanAirSpin
	Timeline = FGSTN_SpinTimelines::Make(bAirSpin ? FGSTN_SpinTimelines::AirParams() : FGSTN_SpinTimelines::GroundParams(), Sense, bAirSpin);
}

UGSTN_GA_SpinCancel::UGSTN_GA_SpinCancel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Added to the base's tags, never replacing them: Ability.Timeline (UGSTN_GA_BaseCombat) is how the loadout compile knows a timeline
	// ability; without it the command activates the ability as a plain one and the combat tick never adopts it (SD-05)
	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(GSTNSpinDrillTags::Ability_Movement_SpinCancel.GetTag());
	SetAssetTags(Tags);
}

const FGSTN_SpinCancelTrack* UGSTN_GA_SpinCancel::FindCancelTrack() const
{
	return FGSTN_SpinCancelTrack::Find(GetTimelineData());
}

FGameplayTag UGSTN_GA_SpinCancel::RequiredSpinTag(EGSTN_SpinSense CancellingSense)
{
	return (CancellingSense == EGSTN_SpinSense::Clockwise ? GSTNSpinDrillTags::State_Spinning_CounterClockwise : GSTNSpinDrillTags::State_Spinning_Clockwise).GetTag();
}

void UGSTN_GA_SpinCancel::SetDefaultCancel(EGSTN_SpinSense Sense)
{
	Timeline = FGSTN_SpinTimelines::MakeCancel(FGSTN_SpinCancelTimelineParams(), Sense);
}

UGSTN_GA_SpinCancelClockwise::UGSTN_GA_SpinCancelClockwise(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetDefaultCancel(EGSTN_SpinSense::Clockwise);
}

UGSTN_GA_SpinCancelCounterClockwise::UGSTN_GA_SpinCancelCounterClockwise(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetDefaultCancel(EGSTN_SpinSense::CounterClockwise);
}

UGSTN_GA_SpinClockwise::UGSTN_GA_SpinClockwise(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetDefaultSpin(EGSTN_SpinSense::Clockwise, false);
}

UGSTN_GA_SpinCounterClockwise::UGSTN_GA_SpinCounterClockwise(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetDefaultSpin(EGSTN_SpinSense::CounterClockwise, false);
}

UGSTN_GA_AirSpinClockwise::UGSTN_GA_AirSpinClockwise(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetDefaultSpin(EGSTN_SpinSense::Clockwise, true);
}

UGSTN_GA_AirSpinCounterClockwise::UGSTN_GA_AirSpinCounterClockwise(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetDefaultSpin(EGSTN_SpinSense::CounterClockwise, true);
}

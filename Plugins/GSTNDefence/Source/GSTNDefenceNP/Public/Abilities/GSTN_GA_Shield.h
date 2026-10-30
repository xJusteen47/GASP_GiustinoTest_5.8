// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Abilities/GSTN_ShieldAbilityData.h"
#include "GSTN_GA_Shield.generated.h"

/**
 * The shield ability (m6 spec 4, 5.3): a timeline ability whose frame data is the shield timeline of its FGSTN_ShieldAbilitySpec
 * (startup, ONE hold window on the shield button that stalls the timeline while the button is held and ends by itself when the value
 * decays to zero, the release frames, one cancel window per out-of-shield option). It is thin on purpose: the model does the work.
 * The combat tick starts it like any timeline from Command.Defence.Shield; its hold window track is what the defence hook stalls on,
 * and the tick extension turns the hook's stamps into the raise and the release of the sync state. The ability never writes sync
 * state and never reads a component for simulation facts.
 *
 * The timeline is built on the class default object (the combat component reads frame data from the CDO, never from an instance)
 * from StartupFrames and ReleaseFrames below, which a Blueprint child sets to the numbers of the rules asset its kit plays (the
 * defaults are the rules' defaults). A kit whose rules differ names a child with the matching frames (deviation from the spec's
 * "built from the rules at grant", recorded in F7's deltas).
 *
 * CanActivateAbility adds the shield's own refusals (UGSTN_DefenceLibrary::CanExecuteShield): the shield is available for the entity,
 * its value is above zero, it is not breaking, the entity is grounded. The values come from the combat sync state of the frame and the
 * attached rules, so the answer is the same on every machine.
 */
UCLASS(Blueprintable)
class GSTNDEFENCENP_API UGSTN_GA_Shield : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_Shield(const FObjectInitializer& ObjectInitializer);

	/** The shield timeline as data: button, hold frames, out-of-shield options, montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defence|Shield")
	FGSTN_ShieldAbilitySpec ShieldSpec;

	/** Frames from the press until the bubble blocks; the rules asset's StartupFrames */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defence|Shield", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 StartupFrames = 1;

	/** Frames the bubble outlasts the release; the rules asset's ReleaseFrames */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defence|Shield", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 ReleaseFrames = 6;

	/** Rebuilds the inline timeline from the spec and the two frame counts (what the combat component reads) */
	void RebuildTimeline();

	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
};

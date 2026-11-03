// Defence, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_DefenceTracks.generated.h"

class UGSTN_DefenceRules;

/**
 * Hold window: while the timeline frame is inside the window and the button is held (input history Hold), the timeline stalls
 * (TimelinePausedFrames++); the combat clock does NOT pause, unlike hitstop. Used by the shield and reusable by any held move
 * (m6 spec 3, Timeline tracks). It is a defence track kind registered through the combat extension surface, which finds it by
 * its struct (the core's track kind enum is closed and stays untouched), so GSTNCombat knows nothing about the shield.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Hold Window"))
struct GSTNDEFENCECORE_API FGSTN_HoldWindowTrack : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	/** The button that keeps the timeline stalled while held */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hold", meta = (Categories = "Input.Button"))
	FGameplayTag HoldButton;

	/** The window ends when the shield value reaches zero from decay (a forced drop, never a break) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hold")
	bool bEndOnZeroShield = false;

	/** Appends one message per problem: an empty or inverted frame range, no button */
	void Validate(const FString& Label, TArray<FString>& OutErrors) const;
};

/**
 * Parry window: any timeline ability can carry it. While the window is open (combat frames from the frame the track starts) the
 * move parries incoming hits through the same resolution as the shield's perfect shield (m6 spec 5.4, source Move). It works in
 * every mode, with or without a shield.
 *
 * ProfileName is baked to an index into UGSTN_DefenceRules::ParryProfiles by Bake, before the simulation uses the track, so the
 * simulation reads an integer and never a name (empty name = profile 0). The two Affected* fields select hurtboxes exactly as the
 * hurtbox modifier track does; both empty = any hurtbox, so a counter can parry only the hits on its blade by naming it.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Parry Window"))
struct GSTNDEFENCECORE_API FGSTN_ParryWindowTrack : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	/** The parry profile this window gives, by FGSTN_ParryProfile::Name. Empty = profile 0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parry")
	FName ProfileName = NAME_None;

	/** Hurtboxes by FGSTN_Hurtbox::Name that the window covers, in union with AffectedTags. Both empty = any hurtbox */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parry")
	TArray<FName> AffectedHurtboxes;

	/** Hurtboxes whose Tags match any of these, in union with the names */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parry", meta = (Categories = "Combat.Hurtbox"))
	FGameplayTagContainer AffectedTags;

	/** The window ends the frame after the first parried hit (hits of that same frame are parried together) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parry")
	bool bCloseOnParry = true;

	/** ProfileName baked to an index by Bake; INDEX_NONE until then and when the name is unknown. Not saved: derived at load */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Parry")
	int32 ProfileIndex = INDEX_NONE;

	/** Bakes ProfileName against the rules. Returns false (and sets INDEX_NONE) when the name names no profile */
	bool Bake(const UGSTN_DefenceRules& Rules);

	/** Whether a hurtbox is one the window covers: any when the selector is empty, else a name or a tag match */
	bool Affects(FName HurtboxName, const FGameplayTagContainer& HurtboxTags) const;

	/** Appends one message per problem: an empty or inverted frame range, a profile name that is not baked to a profile */
	void Validate(const FString& Label, TArray<FString>& OutErrors) const;
};

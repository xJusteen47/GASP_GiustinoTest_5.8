// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitExtensions.h"
#include "Math/GSTN_DefenceViews.h"
#include "GSTN_DefenceHitOutcome.generated.h"

class UGSTN_DefenceRules;
struct FGSTN_HitVictimView;
struct FGSTN_DefenceSnapshotExt;

/** What the defence steps decided about a hit */
UENUM()
enum class EGSTN_DefenceHitResult : uint8
{
	None,
	/** Nullified by a parry (either source) */
	Parried,
	/** Caught by the bubble; the shield took damage and held */
	Blocked,
	/** Caught by the bubble; the damage emptied the shield: it breaks */
	Broken
};

/**
 * What the Parry and Shield steps hand to the defence outcome applier (FGSTN_DefenceOutcomeApplier), one per hit they stopped. The steps
 * are pure and run on the attacker's prediction too; this struct is the only way their decision reaches the defence fragment, which the
 * applier writes at the hit's frame plus one on the victim's simulation. Frames are absolute simulation frames, numbers are integers or
 * quantised, so a re-simulated hit hands over the same bits.
 */
USTRUCT()
struct GSTNDEFENCENP_API FGSTN_DefenceHitOutcome : public FGSTN_HitExtension
{
	GENERATED_BODY()

	UPROPERTY()
	EGSTN_DefenceHitResult Result = EGSTN_DefenceHitResult::None;

	/** The parry's source (Parried only) */
	UPROPERTY()
	EGSTN_ParrySource Source = EGSTN_ParrySource::None;

	/** Index into the rules' ParryProfiles (Parried only) */
	UPROPERTY()
	uint8 ProfileIndex = 0;

	/** The hit's stamped frame (the overlap's frame, never the resolution frame) */
	UPROPERTY()
	int32 HitFrame = INDEX_NONE;

	/** The victim's hitstop of this hit; its end (HitFrame + this) anchors the stuns and the pushback */
	UPROPERTY()
	int32 HitstopFrames = 0;

	/** Blocked/Broken: the shield value after the damage, quarter points, stamped at HitFrame (the bubble keeps its size) */
	UPROPERTY()
	int16 ShieldValueAfterQ = 0;

	/** Blocked: shield stun frames after the hitstop. Broken: the break stun frames from the hit */
	UPROPERTY()
	int32 StunFrames = 0;

	/** Blocked: the push back of the victim along the hit, cm/s, over PushbackFrames (a slide, no tumble). Broken: the launch, up */
	UPROPERTY()
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY()
	int32 PushbackFrames = 0;

	/** Parried, source Shield: frames after the hitstop until the defender's shield timeline ends early (its advantage) */
	UPROPERTY()
	int32 DefenderRecoveryFrames = 0;

	/** Parried, source Move: the window's new end (MoveWindowEndAfterParry); INDEX_NONE = unchanged */
	UPROPERTY()
	int32 MoveWindowEndFrame = INDEX_NONE;

	int32 GetHitstopEndFrame() const { return HitFrame + FMath::Max(HitstopFrames, 0); }
};

/** Helpers both defence steps use: the extension and rules of a view, the move window's packed profile byte */
struct GSTNDEFENCENP_API FGSTN_DefenceStepUtils
{
	/** Bit of FGSTN_DefenceFragment::MoveParryProfile (and the snapshot's copy): the track closes its window on a parry (bCloseOnParry) */
	static constexpr uint8 CloseOnParryBit = 0x80;

	/** The defence extension of the victim view and the rules it names; false when either is missing (the entity has no defence) */
	static bool Find(const FGSTN_HitVictimView& Victim, const FGSTN_DefenceSnapshotExt*& OutExt, const UGSTN_DefenceRules*& OutRules);

	/** The relationships the shield and the parry act on: Enemy and Neutral (an ally hit resolves by its variant) */
	static bool IsDefendedRelationship(FGameplayTag Relationship);

	/** The hit type is (or is under) the core's Combat.HitType.Projectile */
	static bool IsProjectileHitType(FGameplayTag HitType);
};

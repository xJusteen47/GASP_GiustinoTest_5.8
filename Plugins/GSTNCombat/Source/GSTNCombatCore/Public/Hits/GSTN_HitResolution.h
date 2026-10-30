// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Hits/GSTN_HitData.h"
#include "Hits/GSTN_HitVictimView.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Hits/GSTN_ReactionTypes.h"
#include "Rules/GSTN_CombatStep.h"
#include "Sync/GSTN_InboxEntry.h"
#include "GSTN_HitResolution.generated.h"

class UGSTN_CombatRules;

/** What a hit resolution step decided about the hit */
UENUM(BlueprintType)
enum class EGSTN_StepResult : uint8
{
	/** The next step runs */
	Continue,
	/** No later step runs; what was written so far is the outcome */
	Stop
};

/** What an effect request asks the layer above to do (the pure core only records it) */
UENUM(BlueprintType)
enum class EGSTN_HitRequestKind : uint8
{
	/** Apply the rules' DamageEffect with Magnitude as the damage */
	Damage,
	/** Apply the rules' MeterGainEffect with Magnitude as the amount */
	MeterGain,
	/** Apply the rules' KnockbackEffect */
	Knockback,
	/** Freeze the target for Magnitude frames (the rules' FreezeEffect) */
	Freeze
};

UENUM(BlueprintType)
enum class EGSTN_HitRequestTarget : uint8
{
	Victim,
	Attacker
};

/** A gameplay effect the outcome asks for. The core records the class and the number; the GAS layer applies it */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_HitRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	EGSTN_HitRequestKind Kind = EGSTN_HitRequestKind::Damage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	EGSTN_HitRequestTarget Target = EGSTN_HitRequestTarget::Victim;

	/** Soft because Core has no GameplayAbilities dependency; the GAS layer rejects anything that is not a gameplay effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	TSoftClassPtr<UObject> Effect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Request")
	float Magnitude = 0.f;
};

/** Meter the attacker gains from a hit (a super meter...). The meaning is the tag's */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_MeterGain
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter")
	FGameplayTag Meter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter")
	float Amount = 0.f;
};

/**
 * The hit as the pipeline receives it: the authored data after the relationship variant, and the geometry the launch
 * direction needs. Plain values, so the same hit gives the same bits on every machine.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_ResolvedHit
{
	GENERATED_BODY()

	/** The hit after the relationship variant (UGSTN_HitDataUtils::ApplyVariant) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	FGSTN_HitData Hit;

	/** The simulation frame the overlap was made on (the inbox entry's stamp; the attacker's own frame in its prediction): what a window of the victim is tested against, never the resolution frame. INDEX_NONE when unknown */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	int32 Frame = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	int32 AttackerEntityId = INDEX_NONE;

	/** 255 = no team */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	uint8 AttackerTeamId = 255;

	/** Combat.Relationship.* of the victim for this attacker; invalid = the TeamFilter step resolves it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	FGameplayTag Relationship;

	/** The victim's local up at the frame of the hit (minus its gravity direction), never world Z */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	FVector Up = FVector::UpVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	FVector AttackerFacing = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	FVector HitboxToVictim = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	FVector HitboxVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	bool bVictimGrounded = false;

	/**
	 * The ATTACKER's snapshot extension PODs (M6 F10: the link request reads the initiator's link state and kit through them), from the same
	 * snapshot of the end of the previous frame as the victim's: the victim step finds the attacker's entry by AttackerEntityId, the attacker's
	 * prediction reads its own. Empty when the attacker has no entry on this machine (then a step that needs them decides without). Not
	 * reflected, not owned: valid for the run only, like FGSTN_HitVictimView::SnapshotExtensions
	 */
	FGSTN_HitVictimView::FSnapshotExtensionRef AttackerExtensions[FGSTN_HitVictimView::MaxSnapshotExtensions];

	/** The attacker's POD of a plugin by its feature id, or null */
	const void* FindAttackerExtension(uint8 TypeId) const
	{
		for (const FGSTN_HitVictimView::FSnapshotExtensionRef& Ref : AttackerExtensions)
		{
			if (Ref.TypeId == TypeId && Ref.Data)
			{
				return Ref.Data;
			}
		}
		return nullptr;
	}
};

/**
 * What the pipeline decided. The defaults are the identity of an empty pipeline: the hit lands and nothing else happens.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_HitOutcome
{
	GENERATED_BODY()

	/** False when a step let the hit pass through (team filter, intangible) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
	bool bHit = true;

	/** Blocked, parried and clank are written by the shield and parry steps of M6; nothing in M4 sets them */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
	bool bBlocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
	bool bParried = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
	bool bClank = false;

	/** Damage in the unit of the damage mode (percent points or health). Added to the victim by the layer above, not here */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
	float Damage = 0.f;

	/** True once ComputeKnockback ran; false = no model, Knockback is the default */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
	bool bHasKnockback = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
	FGSTN_KnockbackResult Knockback;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
	TArray<FGSTN_MeterGain> MeterGains;

	/** Events the layer above raises (tags, never stored in sync state) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
	TArray<FGameplayTag> Events;

	/** Extra gameplay effect classes for the victim (the hit's ExtraEffects) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
	TArray<TSoftClassPtr<UObject>> Effects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
	TArray<FGSTN_HitRequest> Requests;
};

/**
 * Everything the pipeline reads and builds for one hit on one victim (combat-m4-hits 3). Steps never see this struct: they get
 * a const FGSTN_HitContextView and write through an FGSTN_StepResult, which the runner merges, so a Blueprint step cannot
 * touch the context by accident. Plain C++ struct, not reflected.
 */
struct GSTNCOMBATCORE_API FGSTN_HitResolutionContext
{
	FGSTN_ResolvedHit Incoming;

	/** The victim at the end of the previous frame, plus the outcomes of earlier entries of the same drain (FGSTN_HitPipelineRunner::AdvanceVictim) */
	FGSTN_HitVictimView Victim;

	/** The modifiers the steps build; the knockback model reads them exactly once */
	FGSTN_HitReactionModifiers Reaction;

	/** Which hurtbox the overlap picked; 255 = none */
	uint8 HurtboxIndex = 255;

	/** How the hurtbox that was hit takes hits at this frame (the hurtbox modifier tracks of the victim's timeline), and the Armor window's threshold */
	EGSTN_HurtState HurtState = EGSTN_HurtState::Normal;
	FGSTN_StaggerThreshold ArmorThreshold;

	/** The hit lands but does no damage (an Invincible window); written by the HurtState step */
	bool bNoDamage = false;

	/** The victim's hurtboxes, resolved by the caller outside the simulation. Null = a victim with no set (the rules decide alone) */
	const UGSTN_HurtboxSetDefinition* HurtboxSet = nullptr;

	FGSTN_HitOutcome Outcome;

	/** Data of optional plugins the outcome carries to the appliers (FGSTN_HitExtension structs); steps of those plugins append */
	TArray<FInstancedStruct> OutcomeExtensions;

	/**
	 * Messages the hit sends back to its attacker (a parry's punish, M6 F2): appended by the steps, posted by the victim step after the pipeline
	 * through PostToInbox once the hit stands. The victim step stamps Frame and SenderId, so a step leaves them unset. Only the victim's run
	 * delivers them; the attacker's prediction of the same hit ignores them (a reverse delivery exists on a machine that ticks the attacker)
	 */
	TArray<FGSTN_InboxEntry> OutgoingEntries;
};

/**
 * The const, Blueprint readable face of the context that a step receives. A copy: a step cannot change the context through it.
 * The two pointers are not reflected (a Blueprint step reads the rules through the fields it needs, which the view mirrors where
 * a game asked for them) and are valid for the duration of the run only.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_HitContextView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	FGSTN_ResolvedHit Incoming;

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	FGSTN_HitVictimView Victim;

	/** The modifiers as the steps before this one left them */
	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	FGSTN_HitReactionModifiers Reaction;

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	int32 HurtboxIndex = 255;

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	EGSTN_HurtState HurtState = EGSTN_HurtState::Normal;

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	FGSTN_StaggerThreshold ArmorThreshold;

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	bool bNoDamage = false;

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	FGSTN_HitOutcome Outcome;

	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	TArray<FInstancedStruct> OutcomeExtensions;

	const UGSTN_CombatRules* Rules = nullptr;
	const UGSTN_HurtboxSetDefinition* HurtboxSet = nullptr;
};

/**
 * What one step writes. Every field is a delta that the runner merges into the context, so the order of two steps that write
 * different things does not matter, and two that write the same thing combine in a defined way: scales multiply, a threshold
 * set by a later step replaces the earlier, the reaction cap takes the lower, flags only go from false to true, lists append.
 * The defaults are the identity: a step that writes nothing changes nothing.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_StepResult
{
	GENERATED_BODY()

	// --- Modifiers of the knockback model

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Modifiers", meta = (ClampMin = "0.0"))
	float WeightScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Modifiers", meta = (ClampMin = "0.0"))
	float KnockbackScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Modifiers", meta = (ClampMin = "0.0"))
	float HitstunScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Modifiers", meta = (ClampMin = "0.0"))
	float HitstopScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Modifiers", meta = (ClampMin = "0.0"))
	float StaggerScale = 1.f;

	/** Replaces the context's threshold when set (a negative, finite one is "unset" and changes nothing) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Modifiers")
	FGSTN_StaggerThreshold StaggerThreshold;

	/** The lower of this and the context's cap wins */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Modifiers")
	EGSTN_HitReaction MaxReaction = EGSTN_HitReaction::Launch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Modifiers")
	bool bNoFlinch = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Modifiers")
	bool bNoDamage = false;

	// --- Outcome

	/** Set to false to say the hit did not land. It cannot turn a missed hit back into a hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Outcome")
	bool bHit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Outcome")
	bool bBlocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Outcome")
	bool bParried = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Outcome")
	bool bClank = false;

	/** Added to the outcome's damage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Outcome")
	float DamageAdd = 0.f;

	/** The relationship the step resolved; written into the incoming hit when valid */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Outcome")
	FGameplayTag Relationship;

	/** Replaces the outcome's knockback when bHasKnockback */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Outcome")
	bool bHasKnockback = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Outcome")
	FGSTN_KnockbackResult Knockback;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Outcome")
	TArray<FGSTN_MeterGain> MeterGains;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Outcome")
	TArray<FGameplayTag> Events;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Outcome")
	TArray<TSoftClassPtr<UObject>> Effects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Outcome")
	TArray<FGSTN_HitRequest> Requests;

	/** Appended to the context's OutgoingEntries. C++ steps only: an inbox entry is not a reflected type, so a Blueprint step cannot write it */
	TArray<FGSTN_InboxEntry> OutgoingEntries;

	/** Appended to the context's OutcomeExtensions (FGSTN_HitExtension structs for the appliers) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step|Outcome")
	TArray<FInstancedStruct> OutcomeExtensions;
};

/**
 * One step of the hit resolution pipeline of UGSTN_CombatRules (combat-m4-hits 3). It derives from the activation step class so
 * the rules asset, the validator and Blueprint authoring treat both the same way, but the runner calls Resolve, never Evaluate.
 *
 * A step runs inside the simulation on every machine and again on every re-simulation, so it is a pure function of its view:
 * no world, actor or component reads, no latent nodes, no state kept between calls (the object is the class default object,
 * shared by every pawn), seeded random only. It writes only through Out; the runner merges it. A Blueprint step that breaks
 * this desynchronises the match, and so does a C++ one.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ShowWorldContextPin = "false"))
class GSTNCOMBATCORE_API UGSTN_HitResolutionStep : public UGSTN_CombatStep
{
	GENERATED_BODY()

public:
	/** The default writes nothing and continues */
	UFUNCTION(BlueprintNativeEvent, Category = "Combat|Step")
	EGSTN_StepResult Resolve(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const;
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const;
};

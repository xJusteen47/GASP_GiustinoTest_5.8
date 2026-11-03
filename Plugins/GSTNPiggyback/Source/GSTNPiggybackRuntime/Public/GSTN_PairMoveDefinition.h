// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Actions/GSTN_LinkActionProvider.h"
#include "Engine/DataAsset.h"
#include "GSTN_MountTypes.h"
#include "GSTN_PairMoveTypes.h"
#include "GSTN_PairMoveDefinition.generated.h"

struct FGSTN_CombatTimelineData;

/**
 * One pair move of the mount (combat-m7-piggyback.md 4 and 6.5): two timeline abilities, one per role, started together through GSTNLink's
 * partner moves (M7 G2). Content only: nothing here runs in the simulation. The initiator's timeline carries a Link Outcome track with a
 * PartnerMove key naming PairMoveId; GSTNLink resolves that id through the mount type's action provider (UGSTN_PairMoveRegistry), which turns
 * this definition into an FGSTN_LinkActionInfo.
 *
 * Deviation from the spec (m7-doc-deltas.md, G8): the wire identity is the authored PairMoveId, not an index a kit compiler assigns. GSTNLink's
 * provider is one per link type and the initiator's outcome key names the id as data, so the id must be known when the timeline is authored and
 * must not depend on which kits a machine happened to compile. The spec's InitiatorTimelineOverride / PartnerTimelineOverride are not built: a
 * Blueprint subclass of the ability is the per-kit timing (GSTNLink's action info has no timeline field).
 */
UCLASS(BlueprintType)
class GSTNPIGGYBACKRUNTIME_API UGSTN_PairMoveDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The primary asset type of every definition */
	static const FPrimaryAssetType PrimaryAssetType;

	/** Names the pair move for designers, events and debug (Combat.PairMove.*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair Move", meta = (Categories = "Combat.PairMove"))
	FGameplayTag PairTag;

	/** The wire id: the PartnerMoveId of the initiator's PartnerMove key. Unique among every pair move of the mount type (1..32767) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair Move", meta = (ClampMin = "1", ClampMax = "32767"))
	int32 PairMoveId = 1;

	/** The side that starts the pair move; the partner is the other side of the mount */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair Move|Initiator")
	EGSTN_PairRole InitiatorRole = EGSTN_PairRole::Carrier;

	/** The initiator's move (usually a UGSTN_GA_PairMove child): its timeline posts the partner move */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair Move|Initiator")
	TSubclassOf<UGSTN_GA_BaseCombat> InitiatorAbility;

	/** What the initiator's own state must hold for its binding to fire (read by the kit's binding; not checked by GSTNLink) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair Move|Initiator")
	FGSTN_TagRequirements InitiatorRequirements;

	/** The partner's move, started by the link tick (source Forced). Must be in the partner kit's timeline registry */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair Move|Partner")
	TSubclassOf<UGSTN_GA_BaseCombat> PartnerAbility;

	/** Frames after the initiator's PartnerMove key at which the partner's part starts. 0 behaves as 1: the partner drains its inbox one frame later */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair Move|Partner", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 PartnerStartOffsetFrames = 0;

	/** The partner's request tags (its snapshot entry) must satisfy these when its part starts; otherwise its part is dropped and the initiator plays alone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair Move|Partner")
	FGSTN_TagRequirements PartnerRequirements;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair Move|Partner")
	EGSTN_PairPartnerMode PartnerMode = EGSTN_PairPartnerMode::Cooperative;

	/** The partner starts no command while its part plays */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair Move|Partner")
	bool bPartnerLocksCommands = true;

	/** What happens to the mount at OutcomeFrame. SwapRoles is reserved (GSTNLink does not build it): refused by validation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair Move|Outcome")
	EGSTN_LinkActionOutcome Outcome = EGSTN_LinkActionOutcome::Break;

	/** The initiator's timeline frame at which Outcome applies (not before its PartnerMove key). INDEX_NONE = none (KeepLink only) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair Move|Outcome", meta = (ClampMin = "-1", ForceUnits = "Frames"))
	int32 OutcomeFrame = INDEX_NONE;

	/** Where the rider leaves on a Break: the mount placer's mode */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair Move|Outcome")
	EGSTN_ExitMode ExitMode = EGSTN_ExitMode::AtAnchor;

	/** Ring distance of the exit candidates from the carrier's root */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair Move|Outcome", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ExitDistanceCm = 90.f;

	/** Who is credited for the pair move's hits (combat-m7-piggyback.md 6.7) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair Move|Credit")
	EGSTN_PairCredit Credit = EGSTN_PairCredit::OwnerOnly;

	/** The partner's share of a Split credit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair Move|Credit", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "Credit == EGSTN_PairCredit::Split"))
	float PartnerShare = 0.5f;

	/** Fail-safe: a part still playing this many frames after it started aborts and the mount breaks with the default exit. 0 = never */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair Move|Outcome", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 MaxDurationFrames = 120;

	/** The role tag of each side (Combat.Link.Role.Carrier / .Rider) */
	FGameplayTag GetInitiatorRoleTag() const;
	FGameplayTag GetPartnerRoleTag() const;

	/** Every problem of this definition against its abilities' class-default timelines, one message each */
	void Validate(TArray<FString>& OutErrors) const;

	/**
	 * The same checks on timelines given by the caller (tests, tools); null = that ability is missing. Checks: id in range, pair tag set, both
	 * abilities present, the initiator timeline has a PartnerMove key naming PairMoveId, the partner's start (key + max(offset, 1)) inside the
	 * initiator's duration, OutcomeFrame inside the initiator timeline and not before the key, an outcome frame for Break, no SwapRoles, the roles
	 * (the initiator's requirements do not require the partner's role nor ignore its own; the partner's the reverse), a non-empty partner timeline
	 */
	void ValidateWithTimelines(const FGSTN_CombatTimelineData* InitiatorTimeline, const FGSTN_CombatTimelineData* PartnerTimeline, TArray<FString>& OutErrors) const;

	/** The initiator's frame of the first PartnerMove key naming Id in Timeline (track start + key frame), INDEX_NONE if none */
	static int32 FindPartnerMoveKeyFrame(const FGSTN_CombatTimelineData& Timeline, int32 Id);

	/** The action GSTNLink plays for this definition. Placer is the exit of a Break (kept alive by the caller) */
	void MakeLinkActionInfo(const UGSTN_LinkExitPlacer* Placer, FGSTN_LinkActionInfo& OutInfo) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/** The pair moves a kit offers (combat-m7-piggyback.md 4): named by the kit's mount data, added to the registry when the kit is compiled */
UCLASS(BlueprintType)
class GSTNPIGGYBACKRUNTIME_API UGSTN_PairMoveSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pair Move")
	TArray<TObjectPtr<UGSTN_PairMoveDefinition>> PairMoves;

#if WITH_EDITOR
	/** Every definition valid, and no two with the same id */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/**
 * Base of a pair move's initiator ability (combat-m7-piggyback.md 4). It adds nothing at runtime: PairMove names the definition so validation
 * can check the two agree; the timeline (inline or asset, as every combat move) carries the PartnerMove key. Designers subclass it in Blueprint.
 */
UCLASS(Abstract, Blueprintable)
class GSTNPIGGYBACKRUNTIME_API UGSTN_GA_PairMove : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Pair Move")
	TObjectPtr<const UGSTN_PairMoveDefinition> PairMove;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WallMovement/Transitions/GSTN_WallEntryTransition.h"
#include "GSTN_WallReentryUtils.generated.h"

struct FGSTN_WallMovementSyncState;
class UPrimitiveComponent;

/** The re-entry settings of an entry transition (UGSTN_WallEntryTransition, "Wall Entry|Re-entry"), as the pure rules read them */
struct FGSTN_WallReentryRules
{
	EGSTN_SameWallPolicy SameWallPolicy = EGSTN_SameWallPolicy::HeightCap;
	float SameWallAngle = 30.f;
	float HeightCapTolerance = 10.f;
	float ReentryCooldown = 0.25f;
	TArrayView<const FName> MemoryResetModes;
	/** UGSTN_WallEntryTransition::bInferModeChangeLostToRollback */
	bool bInferModeChangeLostToRollback = false;
	/** UGSTN_WallEntryTransition::bBaseRelativeHeightCap */
	bool bBaseRelativeHeightCap = false;
};

/**
 * Everything one entry check reads for the re-entry rules: the start state's wall memory, Mover's last mode change record (rollback blackboard), the
 * mode of the start sync state, the reset tags, and the wall found now. Filled by UGSTN_WallEntryTransition::FindEntryWall; a plain struct so the
 * rules are a pure function that tests (and a server and a client copy of the same frame) can evaluate
 */
struct FGSTN_WallReentryQuery
{
	const FGSTN_WallMovementSyncState* Memory = nullptr;

	/** Mover's CommonBlackboard::LastModeChangeRecord, if the blackboard holds one */
	bool bHasModeChange = false;
	FName ChangePrevMode = NAME_None;
	FName ChangeMode = NAME_None;
	double ChangeTimeMs = 0.0;
	/** The movement mode of the start sync state (what the authority's state says the character is in) */
	FName CurrentMode = NAME_None;

	/** One of the transition's MemoryResetTags is present */
	bool bResetTagPresent = false;

	double NowMs = 0.0;
	FVector GravityUp = FVector::UpVector;
	/** The face normal of the wall found now */
	FVector WallNormal = FVector::ZeroVector;
	/** The capsule's location along gravity-up */
	float Height = 0.f;
	/** The wall found now is a moving one (UGSTN_WallReentryUtils::GetMovingWallHeight), and the height of its origin along gravity-up */
	bool bWallMoving = false;
	float WallBaseHeight = 0.f;
};

/** Why the wall memory does not apply */
enum class EGSTN_WallMemoryReset : uint8
{
	None,
	/** Mover's last mode change left one of MemoryResetModes after the wall (e.g. landed and took off again) */
	ModeChange,
	/** The same, read from a record a rollback left behind (bInferModeChangeLostToRollback) */
	InferredModeChange,
	/** FGSTN_ResetWallMemoryEffect, in the sync state */
	Manual,
	/** A MemoryResetTags tag is present */
	Tag,
};

enum class EGSTN_WallReentryVerdict : uint8
{
	Allowed,
	Cooldown,
	Denied,
	HeightCap,
};

struct FGSTN_WallReentryResult
{
	EGSTN_WallReentryVerdict Verdict = EGSTN_WallReentryVerdict::Allowed;
	EGSTN_WallMemoryReset Reset = EGSTN_WallMemoryReset::None;
	/** The wall found faces the way of the remembered one */
	bool bSameWall = false;
	/** The height cap was judged in the moving wall's own frame */
	bool bBaseRelative = false;
	/** Moving wall, memory without a wall frame to compare in: the height cap was skipped (the cooldown still applied) */
	bool bHeightCapSkipped = false;
	/** The heights the cap compared (log) */
	float ComparedHeight = 0.f;
	float ComparedCap = 0.f;

	bool IsAllowed() const { return Verdict == EGSTN_WallReentryVerdict::Allowed; }
};

/**
 * The entry transition's re-entry rules (wall memory, same-wall policy, height cap), as pure functions of FGSTN_WallReentryQuery.
 *
 * Two additions over the original rules, both off unless the rules ask for them (the legacy transitions keep the old answer bit for bit), both from
 * traversal T2 (re-grabbing a moving enemy shape reconciled):
 *
 *  - bInferModeChangeLostToRollback. "The memory is reset by going through a MemoryResetModes mode after the wall" reads Mover's last mode change record,
 *    a rollback blackboard entry (InvalidatedOnRollback). A correction that rolls back to a frame after the authority's take-off but before the client's own
 *    erases the client's record of that take-off, and the re-simulation does not write it again (the authority's state is already in the air): the client
 *    is left with the record before it (the landing), the server with the take-off. The server sees "came from Walking after the wall" and lets the wall
 *    be grabbed again, the client applies the height cap and refuses it: a mode, ability and position reconcile at every re-grab after a correction near
 *    a take-off. Since the start sync state's mode is the authority's, a record whose mode is not the current one is a stale record: the change into the
 *    current mode is missing, and it came from the record's mode at a later time. So a stale record whose mode is a reset mode, entered after the wall,
 *    gives the answer the lost record would have given. Not recovered: two lost changes (a landing and a take-off inside one rollback window)
 *
 *  - bBaseRelativeHeightCap. On a moving wall the height cap compares heights in the wall's own frame: the capsule's height minus the wall origin's,
 *    now, against the same difference when the wall was left (FGSTN_WallMovementSyncState::LastWallBaseHeight, recorded by the v2 modes). A memory
 *    without a wall frame (left a static wall, or a legacy mode) cannot be compared with a moving wall: the cap is skipped and only the cooldown applies,
 *    the traversal plan's original proposal. A static wall keeps the world-height cap
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_WallReentryUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Why the memory does not apply (None = it does). Checked in the original order: mode change, manual reset, tag */
	static EGSTN_WallMemoryReset FindMemoryReset(const FGSTN_WallReentryRules& Rules, const FGSTN_WallReentryQuery& Query);

	/** The re-entry rules: memory reset, same wall, cooldown, deny, height cap (in that order, as the transition always applied them) */
	static FGSTN_WallReentryResult Judge(const FGSTN_WallReentryRules& Rules, const FGSTN_WallReentryQuery& Query);

	/**
	 * Whether a wall is a moving one for the wall memory (a dynamic movement base: Movable, the same test the base riding uses), and the height of its
	 * origin along GravityUp. The pose is the one the component holds now, i.e. the frame's pose for a wall posed from the simulation time
	 */
	static bool GetMovingWallHeight(const UPrimitiveComponent* Wall, const FVector& GravityUp, float& OutHeight);

	static const TCHAR* LexReset(EGSTN_WallMemoryReset Reset);
};

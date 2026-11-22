// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_CommandBindings.h"
#include "UObject/Interface.h"
#include "GSTN_LinkActionProvider.generated.h"

class UGSTN_LinkExitPlacer;

/** What a partner move does with the link when the initiator's move reaches its outcome frame (M7 G2) */
UENUM(BlueprintType)
enum class EGSTN_LinkActionOutcome : uint8
{
	/** The link stays; the move is over for the initiator at its outcome frame */
	KeepLink,
	/** The link ends through the watchdog, both sides on the same frame, with the action's reason and exit */
	Break,
	/** The two entities swap roles. Reserved: not built (logged once and treated as KeepLink) */
	SwapRoles
};

/** The two sides of a partner move, as FGSTN_LinkActions::FActive::Side holds them */
namespace GSTNLinkActionSide
{
	constexpr uint8 None = 0;
	/** The entity whose move posted the partner move (its link outcome key) */
	constexpr uint8 Initiator = 1;
	/** The entity the move was posted to: its ability was started by the link tick (source Forced) */
	constexpr uint8 Partner = 2;
}

/**
 * One partner move as a link type's action provider resolves it from its wire id (M7 G2, M7 spec 6.5): what the partner plays, when, what it
 * needs, and what happens to the link. Content, the same on every machine; the simulation carries only the int16 id (FGSTN_LinkActions).
 * Plain data, not reflected: the provider builds it from its own assets (M7's pair-move definitions) or by code.
 */
struct GSTNLINKCORE_API FGSTN_LinkActionInfo
{
	/** The partner's ability (a combat timeline ability of its kit: it must be in the partner's timeline registry). Null = the partner plays nothing */
	const UClass* PartnerAbility = nullptr;

	/** The partner's own state tags (the request tags of its snapshot entry) must satisfy these when its part starts; otherwise its part is dropped */
	FGSTN_TagRequirements PartnerRequirements;

	/** Frames after the initiator's key at which the partner's part starts. At least 1: the entry reaches the partner the frame after it is posted */
	int32 PartnerStartOffsetFrames = 1;

	/** The initiator's timeline frame (from the move's frame 0, hitstop-aware) at which Outcome applies. INDEX_NONE = no outcome frame */
	int32 OutcomeFrame = INDEX_NONE;

	EGSTN_LinkActionOutcome Outcome = EGSTN_LinkActionOutcome::KeepLink;

	/** The break reason of Outcome Break (a Combat.Link.BreakReason tag resolved through the registry's reason table). Empty = Release */
	FGameplayTag BreakReason;

	/** The exit of Outcome Break, instead of the link type's own placer. Null = the type's placer. Content: kept alive by the provider */
	const UGSTN_LinkExitPlacer* ExitPlacer = nullptr;

	/** The partner starts no command while its part plays (cooperative partner, M7.14) */
	bool bPartnerLocksCommands = true;

	/** Either side whose part still plays this many frames after it started aborts it: the link breaks with the default exit. 0 = never */
	int32 MaxDurationFrames = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGSTN_LinkActionProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * Resolves the partner moves of one link type (M7 G2): registered per type with UGSTN_LinkRegistry::RegisterActionProvider, asked by the link
 * tick and the watchdog with the id the link actions carry. Implemented by the plugin that owns the type (M7's pair-move registry) or by a
 * plain table (UGSTN_LinkActionTable). It must be pure content: the same answer for the same id on every machine and every frame of a match,
 * so a resimulation resolves what the first pass resolved. C++ only: it hands out class and placer pointers.
 */
class GSTNLINKCORE_API IGSTN_LinkActionProvider
{
	GENERATED_BODY()

public:
	/** Fills OutInfo for Id (1..32767) and returns true, or false for an id this provider does not know */
	virtual bool ResolveLinkAction(int16 Id, FGSTN_LinkActionInfo& OutInfo) const = 0;
};

/** The simplest provider: a table of actions filled by code (tests, a plugin that builds its moves at startup) */
UCLASS()
class GSTNLINKCORE_API UGSTN_LinkActionTable : public UObject, public IGSTN_LinkActionProvider
{
	GENERATED_BODY()

public:
	/** Adds or replaces the action of Id (1..32767); false for an id out of range */
	bool SetAction(int16 Id, const FGSTN_LinkActionInfo& Info);

	virtual bool ResolveLinkAction(int16 Id, FGSTN_LinkActionInfo& OutInfo) const override;

private:
	TMap<int16, FGSTN_LinkActionInfo> Actions;
};

// Links, ability-system half (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "GameplayTagContainer.h"
#include "Subsystems/EngineSubsystem.h"
#include "GSTN_LinkRegistry.generated.h"

/**
 * One row of the compiled link type table: the type's wire index and what the simulation reads of it without touching a tag name.
 * The row keeps the definition alive while the table refers to it.
 */
USTRUCT(BlueprintType)
struct GSTNLINKGAS_API FGSTN_LinkTypeEntry
{
	GENERATED_BODY()

	/** The wire index of the type (FGSTN_LinkState::LinkTypeIndex), 1-based: 0 means no link */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Link")
	int32 Index = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Link")
	FGameplayTag LinkType;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Link")
	EGSTN_LinkChannel Channel = EGSTN_LinkChannel::Body;

	/** Role 1's tag (the initiator) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Link")
	FGameplayTag InitiatorRoleTag;

	/** Role 2's tag (the target) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Link")
	FGameplayTag TargetRoleTag;

	/** 1 or 2 for the parked role, 0 when the type parks nobody */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Link")
	int32 ParkedRoleIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Link")
	TObjectPtr<UGSTN_LinkTypeDefinition> Definition;

	/** The tag of role 1 or 2; an empty tag for any other index */
	FGameplayTag GetRoleTag(int32 RoleIndex) const
	{
		return RoleIndex == 1 ? InitiatorRoleTag : (RoleIndex == 2 ? TargetRoleTag : FGameplayTag());
	}
};

/**
 * The break reason table (m6 spec 7.0 item 3, 7.5): reason index <-> reason tag, so the link state and the events carry a byte and
 * never a tag. The generic reasons keep the indices of EGSTN_LinkBreakReason (UGSTN_LinkMath::BreakReasonTag names them); a plugin's
 * own reasons (M7: Dismount, Drop) take the indices from EGSTN_LinkBreakReason::FirstPlugin on, in the order of their tag names, so
 * two machines with the same plugins get the same indices whatever order the plugins registered in.
 *
 * Appending re-sorts the plugin part, so an index read before an append may change: the table is completed while the registry is
 * built, before a match starts, and never during one.
 */
struct GSTNLINKGAS_API FGSTN_LinkReasonTable
{
	/** Adds a plugin reason. Returns its index after the append, the existing index for a reason already known (a generic one
	 * included), or 0 for an invalid tag or a full table */
	uint8 Append(const FGameplayTag& Reason);

	/** The index of a reason tag, generic or plugin; 0 when unknown */
	uint8 IndexOf(const FGameplayTag& Reason) const;

	/** The tag of an index; an empty tag for 0, a gap below FirstPlugin or an index past the end */
	FGameplayTag TagOf(uint8 Index) const;

	int32 NumPluginReasons() const { return PluginReasons.Num(); }

	/** Drops the plugin reasons (the generic part is fixed) */
	void ResetPluginReasons() { PluginReasons.Reset(); }

private:
	/** Sorted by tag name */
	TArray<FGameplayTag> PluginReasons;
};

/**
 * The open link type registry (m6 spec 7.0 item 3): every UGSTN_LinkTypeDefinition the asset manager finds, whichever plugin ships it,
 * plus definitions added by code (a test module, a plugin that builds a type at startup), compiled into one table.
 *
 * Wire index: the table is sorted by the type tag's name, and the index is the 1-based row. It depends only on the SET of types, never
 * on load or registration order, so every machine with the same plugins derives the same indices (the content signature tells two
 * machines apart when they do not). A link type therefore needs no id file and no code here: a definition asset plus its tags.
 *
 * A definition that fails UGSTN_LinkTypeDefinition::Validate (the same check its IsDataValid reports) is skipped with a warning and
 * listed in GetBuildErrors: typically its tags are not registered because the plugin that declares them is disabled. Two definitions
 * of one type tag keep the one whose path sorts first. At most GSTNLinkLimits::MaxLinkTypes types fit the wire byte.
 *
 * An engine subsystem rather than a world one: the table is content, the same for every world of the process, so a listen host and
 * its PIE clients in one process share it. It is built when the asset manager completes its first scan and again on demand; it must
 * not change during a match, like any other content the simulation reads.
 */
UCLASS()
class GSTNLINKGAS_API UGSTN_LinkRegistry : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** The process's registry, built on first use. Null without an engine (a commandlet before engine start) */
	static UGSTN_LinkRegistry* Get();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Reloads every definition the asset manager lists under GSTNLinkAssetTypes::LinkType, keeps the code-added ones, rebuilds */
	void RebuildFromAssetManager();

	/** Adds a definition made by code and rebuilds; a definition already present is not added twice */
	void AddDefinition(UGSTN_LinkTypeDefinition* Definition);

	/** Removes a code-added definition and rebuilds */
	void RemoveDefinition(UGSTN_LinkTypeDefinition* Definition);

	/**
	 * Replaces the table with exactly these definitions (no asset manager, no code-added ones): the pure entry point the tests and the
	 * rebuilds use. Problems are logged, kept in GetBuildErrors and appended to OutErrors when given. The plugin reasons registered
	 * through RegisterPluginReason are appended to the reason table (appending is idempotent, so a rebuild changes nothing there)
	 */
	void Build(TConstArrayView<UGSTN_LinkTypeDefinition*> Definitions, TArray<FString>* OutErrors = nullptr);

	/** Whether a build ran since the registry was created */
	bool IsBuilt() const { return bBuilt; }

	// ---- Link types ----

	/** Number of link types in the table (the highest index) */
	int32 Num() const { return Entries.Num(); }

	/** The wire index of a type tag; 0 when the type is not registered */
	uint8 GetTypeIndex(const FGameplayTag& LinkType) const;

	/** The row of an index; null for 0 and past the end */
	const FGSTN_LinkTypeEntry* GetEntry(uint8 TypeIndex) const;

	/** The definition of an index; null for 0 and past the end */
	const UGSTN_LinkTypeDefinition* GetDefinition(uint8 TypeIndex) const;

	/** The definition of a type tag; null when the type is not registered */
	const UGSTN_LinkTypeDefinition* FindDefinition(const FGameplayTag& LinkType) const { return GetDefinition(GetTypeIndex(LinkType)); }

	/** The type tag of an index; empty for 0 and past the end */
	FGameplayTag GetLinkType(uint8 TypeIndex) const;

	/** The tag of role 1 or 2 of a type; empty for an unknown type or role */
	FGameplayTag GetRoleTag(uint8 TypeIndex, int32 RoleIndex) const;

	/** 1 or 2 for a role tag of a type, 0 when the type has no such role */
	int32 FindRoleIndex(uint8 TypeIndex, const FGameplayTag& RoleTag) const;

	/** Every row, index order (row i has index i + 1) */
	TConstArrayView<FGSTN_LinkTypeEntry> GetEntries() const { return Entries; }

	/**
	 * Hash of the table (type tags, channels, role tags, parked roles, in index order) and of the plugin reasons: two machines whose
	 * signatures differ disagree on the wire indices
	 */
	uint32 GetSignature() const { return Signature; }

	/** The problems of the last build, one line each */
	const TArray<FString>& GetBuildErrors() const { return BuildErrors; }

	// ---- Break reasons ----

	const FGSTN_LinkReasonTable& GetReasons() const { return Reasons; }

	/** Appends a plugin reason to this registry's table (see FGSTN_LinkReasonTable::Append). Build-time only */
	uint8 AppendReason(const FGameplayTag& Reason);

	/**
	 * Registers a plugin's break reason by tag name, from the plugin's module startup, before any registry exists. Every later build
	 * resolves the name (RequestGameplayTag, no error when missing: a name no loaded plugin declares is skipped) and appends it
	 */
	static void RegisterPluginReason(FName ReasonTagName);
	static void UnregisterPluginReason(FName ReasonTagName);

private:
	/** Asset definitions plus code-added ones, then Build */
	void RebuildFromSources();

	/** Recomputes Signature from Entries and Reasons */
	void UpdateSignature();

	UPROPERTY(Transient)
	TArray<FGSTN_LinkTypeEntry> Entries;

	/** The definitions the asset manager listed at the last rebuild */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UGSTN_LinkTypeDefinition>> AssetDefinitions;

	/** The definitions added by code */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UGSTN_LinkTypeDefinition>> CodeDefinitions;

	TMap<FGameplayTag, uint8> IndexByType;
	FGSTN_LinkReasonTable Reasons;
	TArray<FString> BuildErrors;
	uint32 Signature = 0;
	bool bBuilt = false;
};

// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Events/GSTN_SimEvent.h"
#include "Hits/GSTN_HitResolution.h"
#include "Sync/GSTN_SyncFragment.h"
#include "GSTN_HitExtensions.generated.h"

/**
 * Base of the data an optional plugin attaches to a hit (FGSTN_HitData::ExtensionData, modularity-and-tags 5b). Core never
 * reads an extension: it carries it, lets a relationship variant replace one by its struct type
 * (FGSTN_HitDataOverride::ExtensionOverrides), and hands the ones a step copied into the outcome to the appliers below.
 */
USTRUCT(BlueprintType, meta = (Hidden))
struct GSTNCOMBATCORE_API FGSTN_HitExtension
{
	GENERATED_BODY()

	virtual ~FGSTN_HitExtension() = default;
};

/**
 * A set of hit groups, one bit per group (a hitbox's HitGroup is a byte). Plain data. The "before candidates" hook point of the
 * combat tick lets an extension set bits to mask the entity's own active hitbox groups for that frame (a grabbed fighter cannot
 * swing, a held weapon is blocked): a set bit means "this group does not hit". Empty by default, which masks nothing.
 */
struct GSTNCOMBATCORE_API FGSTN_HitGroupMask
{
	uint64 Words[4] = { 0, 0, 0, 0 };

	void Set(uint8 Group) { Words[Group >> 6] |= (uint64(1) << (Group & 63)); }
	bool Has(uint8 Group) const { return (Words[Group >> 6] & (uint64(1) << (Group & 63))) != 0; }
	bool IsEmpty() const { return (Words[0] | Words[1] | Words[2] | Words[3]) == 0; }

	/** Adds the bits of Other (two extensions that both mask groups: the union is masked) */
	void Merge(const FGSTN_HitGroupMask& Other)
	{
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Words[Index] |= Other.Words[Index];
		}
	}
};

/** What an applier produced besides the state it wrote: tags to raise and effect classes to apply, merged into the outcome by the caller */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_OutcomeApplyResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
	TArray<FGameplayTag> Events;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
	TArray<TSoftClassPtr<UObject>> Effects;
};

/** What a native applier reads and writes besides the view */
struct FGSTN_OutcomeApplyContext
{
	/** The victim's sync fragments at F+1 (the state the combat tick is writing); null when the caller has none. Appliers change only their own fragment */
	FGSTN_SyncFragmentArray* VictimFragments = nullptr;

	FGSTN_OutcomeApplyResult Result;
};

/**
 * Applies the outcome extensions of one struct type when the combat tick applies a hit's outcome (combat-m4-hits 3b, item 3).
 * C++ only: it runs inside the simulation for every hit and again on every re-simulation, so it follows the rules of any
 * simulation code (deterministic, its state in its own sync fragment, no world reads). State written here is state at F+1, so
 * rollback restores it with the combat state. Blueprint designers use UGSTN_OutcomeApplier instead.
 */
class GSTNCOMBATCORE_API IGSTN_OutcomeExtensionApplier
{
public:
	virtual ~IGSTN_OutcomeExtensionApplier() = default;

	/** The extension struct this applier owns. Fixed for the lifetime of the registration */
	virtual const UScriptStruct* GetExtensionStruct() const = 0;

	/** Called once for each OutcomeExtensions entry of that struct, in the order of the entries */
	virtual void Apply(const FInstancedStruct& Extension, const FGSTN_HitContextView& View, FGSTN_OutcomeApplyContext& Context) const = 0;

	/**
	 * Appends the events this extension stands for to Outbox, with the hit's event as their envelope (tags of the move, the hit type, the
	 * relationship; frame, instigator and target). Called once per hit by the victim step after the hit events, for each entry of that struct,
	 * on every machine and every re-simulation; the authority publishes the outbox once. The default appends nothing
	 */
	virtual void AppendEvents(const FInstancedStruct& Extension, const FGSTN_SimEvent& Envelope, FGSTN_SimEventOutbox& Outbox) const {}
};

/**
 * Blueprint form of an outcome applier, instanced in the rules asset (OutcomeAppliers). Pure by contract: everything it may read
 * is in the view and the extension, it touches no world, actor or component, and what it returns (events, effects) is merged
 * by the caller. It cannot write a sync fragment: state that must be simulated belongs to a C++ applier.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ShowWorldContextPin = "false"))
class GSTNCOMBATCORE_API UGSTN_OutcomeApplier : public UObject
{
	GENERATED_BODY()

public:
	/** The extension struct this applier handles; None applies to nothing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applier", meta = (BaseStruct = "/Script/GSTNCombatCore.GSTN_HitExtension", ExcludeBaseStruct))
	TObjectPtr<UScriptStruct> ExtensionStruct;

	/** The default does nothing */
	UFUNCTION(BlueprintNativeEvent, Category = "Combat|Hits")
	FGSTN_OutcomeApplyResult Apply(const FGSTN_HitContextView& View, const FInstancedStruct& Extension) const;
	virtual FGSTN_OutcomeApplyResult Apply_Implementation(const FGSTN_HitContextView& View, const FInstancedStruct& Extension) const;
};

/**
 * The native appliers by struct type. Registration order does not matter: they run in the order of a hash of the struct's
 * path name (ties by the path), identical on every machine whatever the plugin load order. Only the game thread uses the
 * shared instance; a module registers its applier when it starts, before any simulation tick, and unregisters at shutdown.
 * Tests create their own instance.
 */
class GSTNCOMBATCORE_API FGSTN_OutcomeApplierRegistry
{
public:
	static FGSTN_OutcomeApplierRegistry& Get();

	/** False when Applier is null or already registered; two appliers for one struct both run */
	bool Register(IGSTN_OutcomeExtensionApplier* Applier);
	void Unregister(IGSTN_OutcomeExtensionApplier* Applier);

	int32 Num() const { return Entries.Num(); }

	/**
	 * Calls every native applier for the entries of Extensions it owns (in registry order, entries in array order), then the
	 * Blueprint appliers of Rules in their array order. An entry with no applier, or an empty one (its plugin is not loaded), is
	 * ignored. Rules may be null.
	 */
	void ApplyAll(const UGSTN_CombatRules* Rules, const TArray<FInstancedStruct>& Extensions, const FGSTN_HitContextView& View, FGSTN_OutcomeApplyContext& Context) const;

	/** Calls AppendEvents of every native applier for the entries of Extensions it owns, in registry order. No Blueprint applier makes events */
	void AppendEvents(const TArray<FInstancedStruct>& Extensions, const FGSTN_SimEvent& Envelope, FGSTN_SimEventOutbox& Outbox) const;

private:
	struct FEntry
	{
		uint32 Hash = 0;
		FString Path;
		IGSTN_OutcomeExtensionApplier* Applier = nullptr;
	};

	TArray<FEntry> Entries;
};

/** Whether the attacker's hit landed as usual or bounced */
UENUM(BlueprintType)
enum class EGSTN_AttackerHitOutcome : uint8
{
	Normal,
	/** The hit does nothing at all and the attacker recoils (a hardness band too hard for the attack) */
	Ricochet
};

/**
 * Decides, on the attacker's side, whether a hit bounces (combat-m4-hits 3b, item 3c). Called with the snapshot's victim reaction
 * state, so the attacker's prediction and the host agree. The core alone has none registered and the answer is always Normal.
 * C++ only; Blueprint designers use UGSTN_AttackerOutcome.
 */
class GSTNCOMBATCORE_API IGSTN_AttackerOutcomeExtension
{
public:
	virtual ~IGSTN_AttackerOutcomeExtension() = default;

	/** Names the extension; fixed for the lifetime of the registration. Orders the evaluation */
	virtual FName GetName() const = 0;

	virtual EGSTN_AttackerHitOutcome Evaluate(const FGSTN_HitContextView& View) const = 0;
};

/** Blueprint form of the attacker outcome, instanced in the rules asset (AttackerOutcomes). Pure by contract, like every hook of the pipeline */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ShowWorldContextPin = "false"))
class GSTNCOMBATCORE_API UGSTN_AttackerOutcome : public UObject
{
	GENERATED_BODY()

public:
	/** The default is Normal */
	UFUNCTION(BlueprintNativeEvent, Category = "Combat|Hits")
	EGSTN_AttackerHitOutcome Evaluate(const FGSTN_HitContextView& View) const;
	virtual EGSTN_AttackerHitOutcome Evaluate_Implementation(const FGSTN_HitContextView& View) const;
};

/** The native attacker outcome extensions, evaluated in the order of a hash of their names. Same threading rules as the applier registry */
class GSTNCOMBATCORE_API FGSTN_AttackerOutcomeRegistry
{
public:
	static FGSTN_AttackerOutcomeRegistry& Get();

	bool Register(IGSTN_AttackerOutcomeExtension* Extension);
	void Unregister(IGSTN_AttackerOutcomeExtension* Extension);

	int32 Num() const { return Entries.Num(); }

	/**
	 * The first answer other than Normal, asking the native extensions in registry order and then the Blueprint ones of Rules in
	 * their array order; Normal when none has an opinion (and always when nothing is registered). Rules may be null.
	 */
	EGSTN_AttackerHitOutcome Evaluate(const UGSTN_CombatRules* Rules, const FGSTN_HitContextView& View) const;

private:
	struct FEntry
	{
		uint32 Hash = 0;
		FString Name;
		IGSTN_AttackerOutcomeExtension* Extension = nullptr;
	};

	TArray<FEntry> Entries;
};

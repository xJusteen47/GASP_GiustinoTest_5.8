// Backpacks, ability layer: what the backpack does to a loadout and to the pawn's look (combat-loadout-mods.md §10, §11).

#pragma once

#include "CoreMinimal.h"
#include "Cosmetics/GSTN_AttachmentProvider.h"

class UAbilitySystemComponent;
class UGSTN_BackpackDefinition;
struct FGSTN_CompileInput;
struct FGSTN_LoadoutData;
struct FGSTN_LoadoutExtensionContext;
struct FGSTN_LoadoutRulesView;

/**
 * The loadout extension of the backpack plugin (registered as "GSTN.Backpack", priority 200: after the mods at 100, so a mod never
 * removes or reshapes a backpack verb; mods on backpacks are not asked, spec §8). Game thread, outside the simulation; loads the
 * backpack asset it reads.
 */
struct GSTNBACKPACKGAS_API FGSTN_BackpackLoadout
{
	static const FName ExtensionId;
	static const FName AttachmentProviderId;
	static constexpr int32 ExtensionPriority = 200;

	/** The loadout's backpack asset, loaded; null when it carries none. A path that is not a backpack asset adds a warning */
	static const UGSTN_BackpackDefinition* ResolveBackpack(const FGSTN_LoadoutData& Loadout, TArray<FString>* OutWarnings = nullptr);

	/**
	 * Adds the backpack's bindings and passives to the kit's (spec §10: its verbs stay bound whatever weapon is active, which is what the
	 * kit source already means; a separate entry source waits for a plugin that needs one, L1). Same input on every machine
	 */
	static void ContributeCompileInput(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input);

	/** Warnings only: a path that is not a backpack, a feeding backpack with no carried support weapon it feeds */
	static void ContributeRulesView(const FGSTN_LoadoutExtensionContext& Context, FGSTN_LoadoutRulesView& View);

	/** Applies the backpack's Effects on the authority */
	static void OnGranted(const FGSTN_LoadoutExtensionContext& Context, UAbilitySystemComponent& AbilitySystem);

	/** Removes what OnGranted applied */
	static void OnCleared(UAbilitySystemComponent& AbilitySystem);

	/**
	 * The backpack's look as attachment requests. A part with neither Socket nor ParentSlot goes on the kit implant's connector
	 * (Implant.Back / Connector_Backpack); without an implant the skin component falls back to the pawn's mesh
	 */
	static void GatherVisual(const UGSTN_BackpackDefinition& Backpack, TArray<FGSTN_AttachmentRequest>& OutRequests);
};

/** Puts the carried backpack on the pawn's look, after the kit implant it attaches to (registered with priority 100) */
class GSTNBACKPACKGAS_API FGSTN_BackpackAttachmentProvider : public IGSTN_AttachmentProvider
{
public:
	virtual void GatherAttachments(const FGSTN_AttachmentContext& Context, TArray<FGSTN_AttachmentRequest>& OutRequests) const override;
};

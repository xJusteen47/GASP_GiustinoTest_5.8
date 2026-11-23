// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"

class UGSTN_MinionDefinition;
class UGSTN_MinionWaveDefinition;

/**
 * The cross-asset rules of the minion data (M8 G12). They are plain functions over loaded objects so the editor validators
 * (GSTNMinionEditor) and the automation tests run the same code, headless. The asset's own IsDataValid keeps what one asset can check
 * alone; these are the checks that need another asset, a loaded reference, or the capacity of the store. Messages are appended, never
 * cleared. Loading is synchronous: call from the editor or a test, never from a simulation tick.
 */
namespace GSTNMinionValidation
{
	/** Most types the registry holds: TypeIndex is a byte in the store and on the wire */
	constexpr int32 MaxRegistryTypes = 256;

	/**
	 * The type table (registry order = the order of Types). Errors: a null entry, the same definition twice, two definitions with the same
	 * TypeTag (the clients tell types apart by index and the designer by tag), more than MaxRegistryTypes entries. The order itself
	 * cannot be wrong here: the registry is filled in first-spawn order and the clients check its hash (G6), so only what breaks
	 * that identity is reported
	 */
	GSTNMINIONCORE_API void CheckRegistry(TConstArrayView<const UGSTN_MinionDefinition*> Types, TArray<FString>& OutErrors);

	/**
	 * What a definition needs beyond its own IsDataValid. Errors: a soft reference that is set but does not load (puppet class, mesh,
	 * anim class, brain, promotion fighter class), a hurtbox set with no hurtbox. Warnings: no moves (a brain cannot make it attack),
	 * a move with MaxRange of 0 or less, non-positive walk or run speed
	 */
	GSTNMINIONCORE_API void CheckDefinitionReferences(const UGSTN_MinionDefinition& Definition, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);

	/**
	 * What a wave needs beyond its own IsDataValid. Errors: a total above Capacity (gstn.Minion.Max, so a wave can never fill the
	 * store), a definition path that is set but does not load, and the registry rules over the distinct definitions it names
	 */
	GSTNMINIONCORE_API void CheckWaveCrossAsset(const UGSTN_MinionWaveDefinition& Wave, int32 Capacity, TArray<FString>& OutErrors);
}

// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPath.h"
#include "GSTN_MinionWaveDefinition.generated.h"

class UGSTN_MinionDefinition;

namespace GSTNMinion
{
	/** Most minions one entry may spawn */
	constexpr int32 MaxWaveEntryCount = 64;

	/** Most minions one wave may spawn in total (the store holds 128 by default and the fighters need room) */
	constexpr int32 MaxWaveTotal = 128;
}

/** One line of a wave: Count minions of one type at one kind of spawn point, DelayFrames after the wave starts */
USTRUCT(BlueprintType)
struct GSTNMINIONCORE_API FGSTN_MinionWaveEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Wave")
	TSoftObjectPtr<UGSTN_MinionDefinition> Definition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Wave", meta = (ClampMin = "1", ClampMax = "64"))
	int32 Count = 1;

	/**
	 * Actor tag of the spawn points (any actor in the level carrying it). The minions are dealt round robin over the actors found.
	 * None, or no actor found, = the transform of the spawner actor
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Wave")
	FName SpawnPointTag;

	/** Fixed frames after the wave starts at which this entry spawns (62.5 frames = 1 s). The spawn queue then spreads it over a few frames */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Wave", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 DelayFrames = 0;

	/** Minions that land on one spawn point are spread on a circle of this radius around it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Wave", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float RingRadius = 300.f;
};

/**
 * A wave of minions as data (combat-m8-minions.md 10): which types, how many, where and when. A UGSTN_MinionSpawnerActor placed in the
 * level plays it. Pure data: no world, no spawn code (that is the Runtime module's spawner).
 *
 * IsDataValid reports (errors): an entry with no definition, an entry count outside 1..64, a negative delay, a total above 128.
 */
UCLASS(BlueprintType)
class GSTNMINIONCORE_API UGSTN_MinionWaveDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minion|Wave")
	TArray<FGSTN_MinionWaveEntry> Entries;

	/** Sum of the entries' counts */
	int32 GetTotalCount() const;

	/** Appends the path of every entry's definition, once each. Loading these first is what makes GetAssetsToPreload complete */
	void GetDefinitionPaths(TArray<FSoftObjectPath>& Out) const;

	/**
	 * Appends what the wave needs in memory before it spawns: every definition and, for each definition that is already loaded, its soft
	 * references (UGSTN_MinionDefinition::GetSoftDependencies: the Brain tree, puppet class, mesh, anim class, hurtbox set, timelines,
	 * promotion fighter class). Never loads anything itself: a spawner loads the definitions (GetDefinitionPaths), then calls this
	 * again for the rest. bForClient leaves out what a client never loads (the brain)
	 */
	void GetAssetsToPreload(TArray<FSoftObjectPath>& Out, bool bForClient = false) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

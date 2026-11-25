// Test cubes: fixed targets of different hardness for playtesting penetration (owner request 2026-11-25).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_PenetrationTable.h"
#include "Present/GSTN_MinionPuppet.h"
#include "Store/GSTN_MinionStore.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_TestCubes.generated.h"

class UGSTN_HurtboxSetDefinition;
class UGSTN_MinionDefinition;
class UGSTN_PartSetDefinition;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTextRenderComponent;
struct FGSTN_PartHitOutcome;

/**
 * What a test cube has taken since it was spawned. Kept on the authority, read by the cube's puppet on the same machine (standalone,
 * listen host) and by gstn.TestCubes.Report
 */
struct FGSTN_TestCubeRecord
{
	FGSTN_MinionHandle Handle;
	TWeakObjectPtr<const UGSTN_MinionDefinition> Definition;
	int32 Hardness = 0;
	int32 Hits = 0;
	int32 Ricochets = 0;
	float TotalDamage = 0.f;
	/** The last hit, as the label shows it */
	FString LastHit;
};

/**
 * Fixed cubes the player can attack, one per hardness, to feel the penetration rule in a playtest (combat-parts.md 3.4: the hit's
 * penetration against the part's hardness decides full damage, the reduced band or a ricochet).
 *
 * A cube is a minion (GSTNMinion) of a transient type with one part (GSTNParts through this bridge), so it goes through the same hit
 * pipeline as any enemy and needs no new entity kind: no new sync state, no change to the combat core. What
 * keeps it fixed is data only: speeds 0, no brain, no moves, body collision None (never pushed by a fighter), the part's and the body's
 * MaxReaction StaggerInPlace and KnockbackScale 0 (no displacement), health high enough never to die in a session, the part unbreakable.
 *
 * The penetration table is the parts settings' default table when one is set, else the built-in three bands, copied onto the cube's
 * set at spawn so the label can name the band the hit fell in. A rules extension table (FGSTN_PartsRulesExtension) is not used here.
 *
 * Authority only (standalone, listen host, the server of PIE). The types are transient objects of this machine: a remote client cannot
 * resolve them, so on a remote client the cubes are not drawn (the hits still resolve on the host). Not a gameplay object: test tooling.
 */
UCLASS()
class GSTNPARTSMINION_API UGSTN_TestCubesSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Clears the cubes of this world, then spawns one cube per entry of Hardnesses (0..15) in a row Distance cm in front of the first
	 * local player, Spacing cm apart, on the floor below. Returns the cubes queued; 0 on a client or without a player pawn
	 */
	int32 SpawnCubes(const TArray<int32>& Hardnesses, double Distance, double Spacing);

	/** Despawns every cube of this world */
	void ClearCubes();

	/** Logs one TestCubes line per cube (hardness, hits, ricochets, total damage, last hit) */
	void Report() const;

	/** The record of the cube of a type, null for any other type (the puppet asks with its definition) */
	const FGSTN_TestCubeRecord* FindByDefinition(const UGSTN_MinionDefinition* Definition) const;

	/** The text the cube's label shows */
	static FString MakeLabel(const FGSTN_TestCubeRecord& Record);

private:
	void HandlePartOutcome(const FGSTN_MinionHandle& Handle, const FGSTN_PartHitOutcome& Outcome, int32 Frame);
	UGSTN_MinionDefinition* GetOrCreateDefinition(int32 Hardness);
	FString DescribeBand(int32 BandIndex) const;

	FDelegateHandle OutcomeHandle;

	/** One type per hardness, reused across spawns: a type's wire format is fixed at its first spawn, a new object per spawn would add types */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UGSTN_MinionDefinition>> Definitions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGSTN_PartSetDefinition>> PartSets;

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_HurtboxSetDefinition> Hurtboxes;

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_PenetrationTable> Table;

	TArray<FGSTN_TestCubeRecord> Cubes;
};

/**
 * The look of a test cube: the engine's 1 m cube coloured by hardness (green soft, red hard), a white flash on the puppet's hit flash, and
 * a label above it with the hardness and the last hit. Pooled and posed by the minion presentation like any puppet; it only adds the
 * cube, the label and an actor tick that refreshes them.
 */
UCLASS(NotPlaceable, Transient)
class GSTNPARTSMINION_API AGSTN_TestCubePuppet : public AGSTN_MinionPuppet
{
	GENERATED_BODY()

public:
	AGSTN_TestCubePuppet();

	virtual void Tick(float DeltaSeconds) override;

	/** Edge of the cube, cm; the hurtbox and the minion capsule use the same size */
	static constexpr double CubeSize = 100.0;

private:
	UPROPERTY(VisibleAnywhere, Category = "TestCube")
	TObjectPtr<UStaticMeshComponent> Cube;

	UPROPERTY(VisibleAnywhere, Category = "TestCube")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CubeMaterial;

	TWeakObjectPtr<const UGSTN_MinionDefinition> ShownDefinition;
	FLinearColor BaseColor = FLinearColor::White;
	FString ShownText;
	float ShownFlash = -1.f;
};

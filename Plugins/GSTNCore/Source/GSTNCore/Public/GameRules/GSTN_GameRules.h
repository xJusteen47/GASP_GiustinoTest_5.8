// Game rules of a mode or level (GR-1): the gameplay factors that used to live in Project Settings, one asset per mode/level.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Info.h"
#include "Subsystems/WorldSubsystem.h"

#include "GSTN_GameRules.generated.h"

class AGameModeBase;

/**
 * One plugin's share of the game rules (owner decision 2026-11-11: one GameRules asset, a piece per plugin). A plugin derives its own piece
 * (UGSTN_HeldRules for GSTNHeld) and reads it with GetActivePiece<T>(). The class default object is the plugin's default rules, so a mode
 * without an asset, or an asset without that piece, plays exactly as before. Exceptions per kit or per object stay in the kit or object.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class GSTNCORE_API UGSTN_GameRulesPiece : public UObject
{
	GENERATED_BODY()
};

/**
 * The rules of a mode or a level. The server picks one when the world begins play (UGSTN_GameRulesSubsystem) and replicates the choice
 * (AGSTN_GameRulesInfo, always relevant), so every machine reads the same values. The simulation reads them, so they must never change
 * during a match.
 */
UCLASS(BlueprintType)
class GSTNCORE_API UGSTN_GameRules : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The plugins' pieces, at most one per class; a missing piece = that plugin's defaults */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Rules")
	TArray<TObjectPtr<UGSTN_GameRulesPiece>> Pieces;

	/** Levels that use these rules when the URL names none (GameRules=/Game/...). A level listed here wins over a game mode below */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection")
	TArray<TSoftObjectPtr<UWorld>> Levels;

	/** Game modes (the class or a parent) that use these rules when neither the URL nor a level picks one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection")
	TArray<TSoftClassPtr<AGameModeBase>> GameModes;

	/** The piece of class PieceClass (exact or child), or null */
	UGSTN_GameRulesPiece* FindPiece(const TSubclassOf<UGSTN_GameRulesPiece> PieceClass) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("GSTN_GameRules"), GetFName()); }
};

/**
 * The rules in force in this process. One pointer for the process, not per world, because the plugins read their factors from code that has
 * no world (as the old Project Settings). In single-process PIE every world plays the same level, so they all agree; across processes each
 * machine sets its own from the server's choice.
 */
struct GSTNCORE_API FGSTN_ActiveGameRules
{
	static const UGSTN_GameRules* Get();
	static void Set(const UGSTN_GameRules* Rules);

	/** The active asset's piece of class T, else T's class default object (the plugin's defaults) */
	template <typename T>
	static const T& GetPiece()
	{
		if (const UGSTN_GameRules* Rules = Get())
		{
			if (const T* Piece = Cast<T>(Rules->FindPiece(T::StaticClass())))
			{
				return *Piece;
			}
		}
		return *GetDefault<T>();
	}

	/** Dev only (console commands, tests): the same object GetPiece returns, writable. Never during a networked match across processes */
	template <typename T>
	static T& GetMutablePieceForDev() { return const_cast<T&>(GetPiece<T>()); }
};

/** Carries the server's choice to the clients (always relevant, sent with the first replication of the world, before any pawn) */
UCLASS(NotPlaceable, Transient)
class GSTNCORE_API AGSTN_GameRulesInfo : public AInfo
{
	GENERATED_BODY()

public:
	AGSTN_GameRulesInfo();

	UPROPERTY(ReplicatedUsing = OnRep_Rules)
	TObjectPtr<const UGSTN_GameRules> Rules;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_Rules();
};

/**
 * Picks the rules on the server when the world begins play: the URL option GameRules=<asset path>, else an asset that lists this level, else
 * one that lists the game mode, else none (every plugin's defaults). Clients take the server's choice from AGSTN_GameRulesInfo.
 */
UCLASS()
class GSTNCORE_API UGSTN_GameRulesSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** The rules this world chose or received (null = defaults) */
	const UGSTN_GameRules* GetRules() const { return Rules; }

	/** Called on clients when the server's choice arrives */
	void ApplyRules(const UGSTN_GameRules* InRules);

	/** The server's choice for World (null = defaults); public for tests */
	static const UGSTN_GameRules* ChooseRules(const UWorld& World);

private:
	UPROPERTY(Transient)
	TObjectPtr<const UGSTN_GameRules> Rules;
};

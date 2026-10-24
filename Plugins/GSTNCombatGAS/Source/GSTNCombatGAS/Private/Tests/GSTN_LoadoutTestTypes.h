// Abilities and test doubles of the loadout component and subsystem used only by the Automation tests.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "Loadout/GSTN_LoadoutSubsystem.h"
#include "GSTN_LoadoutTestTypes.generated.h"

/** Three distinct ability classes to tell grants apart. */
UCLASS(Hidden)
class UGSTN_TestGrantAbilityA : public UGameplayAbility
{
	GENERATED_BODY()
};

UCLASS(Hidden)
class UGSTN_TestGrantAbilityB : public UGameplayAbility
{
	GENERATED_BODY()
};

UCLASS(Hidden)
class UGSTN_TestGrantAbilityC : public UGameplayAbility
{
	GENERATED_BODY()
};

/** Counts the applied broadcasts of a loadout component. */
UCLASS(Hidden)
class UGSTN_TestLoadoutListener : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void OnApplied(UGSTN_LoadoutComponent* Component, int32 Revision)
	{
		++Count;
		LastRevision = Revision;
	}

	int32 Count = 0;
	int32 LastRevision = 0;
};

/**
 * A loadout component that answers asset ids from a table and grants on a chosen ability system, so the server apply path runs
 * on a transient world with neither the Asset Manager nor a PlayerState nor a pawn.
 */
UCLASS(Hidden)
class UGSTN_TestLoadoutComponent : public UGSTN_LoadoutComponent
{
	GENERATED_BODY()

public:
	TMap<FPrimaryAssetId, UObject*> Assets;
	UAbilitySystemComponent* Target = nullptr;

protected:
	virtual UObject* ResolveAsset(const FPrimaryAssetId& Id) const override;
	virtual UAbilitySystemComponent* GetTargetAbilitySystem() const override;
};

/** A loadout subsystem that keeps its save slot in memory and knows the asset ids it is told. */
UCLASS(Hidden)
class UGSTN_TestLoadoutSubsystem : public UGSTN_LoadoutSubsystem
{
	GENERATED_BODY()

public:
	/** The bytes of the "save slot" */
	mutable TArray<uint8> Storage;

	bool bEverythingKnown = true;
	TSet<FPrimaryAssetId> Known;

protected:
	virtual bool IsAssetIdKnown(const FPrimaryAssetId& Id) const override;
	virtual bool ReadStorage(TArray<uint8>& OutBytes) const override;
	virtual bool WriteStorage(const TArray<uint8>& Bytes) const override;
};

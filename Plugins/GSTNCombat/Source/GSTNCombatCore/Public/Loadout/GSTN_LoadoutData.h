// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "GSTN_LoadoutData.generated.h"

class UPackageMap;

/** A number a player chose for a designer-defined loadout setting (a tuning slider, a toggle as 0 or 1). */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_LoadoutValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	float Value = 0.f;

	bool operator==(const FGSTN_LoadoutValue& Other) const { return Value == Other.Value; }
};

/** The swap of the ability a weapon binds to a command for one of the weapon's allowed alternatives. */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_SlotCustomisation
{
	GENERATED_BODY()

	/** Index in the loadout's weapon order: 0 = the kit's primary weapon, 1.. = AdditionalWeapons */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout", meta = (ClampMin = "0"))
	int32 WeaponIndex = 0;

	/** The command whose ability is swapped */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout", meta = (Categories = "Command"))
	FGameplayTag Command;

	/**
	 * The ability class to bind instead. Typed as a plain class because this module cannot name the ability base class
	 * (that belongs to the ability layer); the loadout rules check that it is one of the weapon's allowed alternatives.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	TSoftClassPtr<UObject> AlternativeAbility;

	bool operator==(const FGSTN_SlotCustomisation& Other) const
	{
		return WeaponIndex == Other.WeaponIndex && Command == Other.Command && AlternativeAbility == Other.AlternativeAbility;
	}
};

/**
 * What a player brings into a match (combat-m3 8): ids only, never assets, so it is small enough to send to the server and to
 * keep in a save game, and a content change cannot break a saved loadout (unknown ids are dropped by whoever resolves them).
 * Whether the combination is allowed is the loadout rules' job; this struct only carries it.
 *
 * It has two serializers. NetSerialize (the request to the server and the applied copy) has no version and Serialize (save
 * games, presets) leads with one, so it survives tag and content changes between builds. Both write tags and ids as names
 * (independent of the project's tag replication mode; a loadout is sent once per change), and a tag that no longer exists is
 * dropped on load. Both put the settings map in tag-name order so the same loadout always produces the same bytes. Counts are
 * bounded: a malformed or hostile stream fails instead of allocating.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_LoadoutData
{
	GENERATED_BODY()

	/** Version written by Serialize (2 added the extension block; 1 is still read) */
	static constexpr int32 SaveVersion = 2;

	/** Version of the extension block layout written by this build. A block of a newer layout is carried unread */
	static constexpr uint8 ExtensionLayoutVersion = 0;

	/** Largest extension payload a stream may carry */
	static constexpr int32 MaxExtensionBytes = 1024;

	/** Largest list or map a stream may carry */
	static constexpr int32 MaxEntries = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout", meta = (AllowedTypes = "GSTN_PrimaryKit"))
	FPrimaryAssetId PrimaryKit;

	/** Weapons beyond the kit's primary one, in loadout order */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout", meta = (AllowedTypes = "GSTN_Weapon"))
	TArray<FPrimaryAssetId> AdditionalWeapons;

	/** One entry per super slot of the kit; an invalid id is an empty slot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout", meta = (AllowedTypes = "GSTN_Super"))
	TArray<FPrimaryAssetId> Supers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	TArray<FGSTN_SlotCustomisation> SlotCustomisations;

	/**
	 * Settings a game defines for its own loadout screen. The key is a tag under the loadout root; code never names one.
	 * NotReplicated only silences UHT's "no maps in RPCs" check: the map travels through the struct's own NetSerialize
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout", NotReplicated)
	TMap<FGameplayTag, FGSTN_LoadoutValue> DesignerValues;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout", meta = (AllowedTypes = "GSTN_Skin"))
	FPrimaryAssetId Skin;

	/**
	 * Reserved for typed extensions of optional plugins (M11): the payload that follows the extension layout version at the end of
	 * both serializers. Empty today and written as such; a stream that carries bytes (a newer build) is read into this array and
	 * written back unchanged, so a relay or a save/load cycle never drops them. Not part of the equality of two loadouts.
	 */
	TArray<uint8> ReservedExtension;

	/** Wire format of a request or an applied loadout */
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

	/** Save game and preset format (versioned text) */
	bool Serialize(FArchive& Ar);

	bool operator==(const FGSTN_LoadoutData& Other) const;
	bool operator!=(const FGSTN_LoadoutData& Other) const { return !(*this == Other); }

private:
	bool SerializeInternal(FArchive& Ar, bool bWithExtension);
};

template <>
struct TStructOpsTypeTraits<FGSTN_LoadoutData> : public TStructOpsTypeTraitsBase2<FGSTN_LoadoutData>
{
	enum
	{
		WithNetSerializer = true,
		WithSerializer = true,
		WithIdenticalViaEquality = true,
	};
};

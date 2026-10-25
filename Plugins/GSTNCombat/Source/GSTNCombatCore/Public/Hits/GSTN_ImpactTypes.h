// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GSTN_ImpactTypes.generated.h"

/** What a launched victim undergoes on a surface (combat-m4-hits 7b) */
UENUM(BlueprintType)
enum class EGSTN_ImpactOutcome : uint8
{
	/** Slide or land as without impacts */
	None,
	/** Pinned to the surface for a number of frames */
	Splat,
	/** Reflected about the contact normal */
	Bounce,
	/** The surface object is destroyed, the victim continues */
	BreakSurface
};

/**
 * The attack's say in a surface impact: a finisher that forces a slam, a jab that never splats. The victim and surface layers
 * are FGSTN_ImpactResponse and FGSTN_SurfaceImpactRow; UGSTN_ImpactMath::Resolve combines the three.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_ImpactProfile
{
	GENERATED_BODY()

	/** Outcome the attack asks for; None = no preference */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	EGSTN_ImpactOutcome Force = EGSTN_ImpactOutcome::None;

	/** With a Force: it applies even to a surface that allows no outcome at all (a finisher that breaks the unbreakable) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	bool bForceIgnoresSurface = false;

	/** Outcomes this attack never causes (an enum, since outcomes are not tags) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	TArray<EGSTN_ImpactOutcome> Forbid;

	/** Scales the impact speed the contact is judged with */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact", meta = (ClampMin = "0.0"))
	float SpeedScale = 1.f;
};

/** Which outcomes a surface or a victim lets through (None is always allowed) */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_ImpactAllowed
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	bool bSplat = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	bool bBounce = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	bool bBreakSurface = true;

	bool Allows(EGSTN_ImpactOutcome Outcome) const
	{
		switch (Outcome)
		{
		case EGSTN_ImpactOutcome::Splat: return bSplat;
		case EGSTN_ImpactOutcome::Bounce: return bBounce;
		case EGSTN_ImpactOutcome::BreakSurface: return bBreakSurface;
		default: return true;
		}
	}

	/** True when at least one real outcome is allowed */
	bool AllowsAnything() const { return bSplat || bBounce || bBreakSurface; }
};

/** Layer 3: what one surface type does to whoever hits it (combat-m4-hits 7b) */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_SurfaceImpactRow
{
	GENERATED_BODY()

	/** The surface type; a contact on a child tag (Surface.Type.Wall.Brick) uses the nearest parent row */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (Categories = "Surface.Type"))
	FGameplayTag Surface;

	/** What happens when nothing above asks for something else */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	EGSTN_ImpactOutcome DefaultOutcome = EGSTN_ImpactOutcome::None;

	/** What the surface can do at all: an outcome it does not allow can neither be a default nor be asked by a victim */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	FGSTN_ImpactAllowed Allowed;

	/**
	 * The physical truth of the surface: DefaultOutcome happens whatever the attack or the victim prefer (a trampoline always
	 * bounces). The impact speed threshold still applies.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	bool bForceDefault = false;

	/** The impact speed along the normal must exceed this, else None */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float MinImpactSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0.0"))
	float Restitution = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 SplatFrames = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0.0"))
	float ImpactDamageScale = 1.f;
};

/**
 * The surface layer of the impact rules: rows by surface type tag. A contact carries the GSTNSurface type index of its surface
 * (UGSTN_SurfaceUtils::GetSurfaceTypeIndex, 0 = no type) and no tag is ever stored in a sync state or snapshot. The table compiles
 * a lookup from every type index to its row (exact tag, else the nearest parent tag) at load, and again when the surface type
 * list changes, so the order of Rows does not matter.
 */
UCLASS(BlueprintType)
class GSTNCOMBATCORE_API UGSTN_SurfaceImpactTable : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Rows by surface type tag, in any order. A tag with two rows uses the first */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surfaces")
	TArray<FGSTN_SurfaceImpactRow> Rows;

	/** Builds the tag and surface type index lookups. Called at load and after an edit; call it after filling Rows from code */
	void Compile();

	/** The row index of a surface type (exact tag, else the nearest parent), or INDEX_NONE */
	int32 FindRowIndex(const FGameplayTag& Surface) const;

	/** The row of a contact's surface type index (GSTNSurface), or null for no type, an unknown index or a type without a row */
	const FGSTN_SurfaceImpactRow* GetRow(int32 SurfaceTypeIndex) const;

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	TMap<FGameplayTag, int32> IndexByTag;

	/** Row index by surface type index, INDEX_NONE for none; sized to the surface type list when compiled */
	mutable TArray<int32> RowBySurfaceIndex;

	/** Checksum of the surface type list RowBySurfaceIndex was built for; a different one means the indices moved */
	mutable uint32 CompiledTypeChecksum = 0;
	mutable bool bCompiledOnce = false;

	void BuildSurfaceIndexLookup() const;
};

/** Layer 2 for one surface type: what this victim can undergo there. Negative numbers mean "the surface's value" */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_ImpactResponseEntry
{
	GENERATED_BODY()

	/** Empty in FGSTN_ImpactResponse::Default */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response", meta = (Categories = "Surface.Type"))
	FGameplayTag Surface;

	/** Restricts the surface's default (a heavy enemy never splats) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response")
	FGSTN_ImpactAllowed Allowed;

	/** At or above 0: the larger of this and the surface's threshold applies */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response", meta = (ClampMin = "-1.0", ForceUnits = "cm/s"))
	float MinImpactSpeed = -1.f;

	/** At or above 0: replaces the surface's */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response", meta = (ClampMin = "-1.0"))
	float Restitution = -1.f;

	/** At or above 0: replaces the surface's */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response", meta = (ClampMin = "-1", ForceUnits = "Frames"))
	int32 SplatFrames = -1;

	/** Multiplies the surface's */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response", meta = (ClampMin = "0.0"))
	float ImpactDamageScale = 1.f;
};

/** The victim's say in a surface impact (from its kit or definition), by surface type with a default for the rest */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_ImpactResponse
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response")
	FGSTN_ImpactResponseEntry Default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response")
	TArray<FGSTN_ImpactResponseEntry> PerSurface;

	/** The entry for a surface type: the first whose tag the surface matches (hierarchically), else Default */
	const FGSTN_ImpactResponseEntry& Find(const FGameplayTag& Surface) const;
};

/**
 * One contact as the decision reads it: quantised so every machine feeds the same integers. Plain data (a 16 bit field is not a
 * Blueprint type).
 */
struct GSTNCOMBATCORE_API FGSTN_ImpactInput
{
	/** GSTNSurface type index of the surface (UGSTN_SurfaceUtils::GetSurfaceTypeIndex); 0 = no surface type */
	uint8 SurfaceTypeIndex = 0;

	/** Impact speed along the contact normal in whole cm/s */
	uint16 ImpactSpeedQ = 0;

	static FGSTN_ImpactInput Make(uint8 InSurfaceTypeIndex, double ImpactSpeed)
	{
		FGSTN_ImpactInput Input;
		Input.SurfaceTypeIndex = InSurfaceTypeIndex;
		Input.ImpactSpeedQ = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(ImpactSpeed), 0, 65535));
		return Input;
	}
};

/** What a contact resolves to */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_ImpactResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	EGSTN_ImpactOutcome Outcome = EGSTN_ImpactOutcome::None;

	/** The speed the contact was judged with (after the attack's SpeedScale) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	float JudgedSpeed = 0.f;

	/** For Bounce */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	float Restitution = 0.f;

	/** For Splat */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	int32 SplatFrames = 0;

	/** Surface scale times the victim's */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	float ImpactDamageScale = 1.f;
};

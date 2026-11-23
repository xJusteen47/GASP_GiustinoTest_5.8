// Modular cosmetics, pure layer.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_CosmeticTypes.generated.h"

class UStaticMesh;

/**
 * Collision of a cosmetic piece, an explicit field the validator checks (the Lyra character part pattern, rewritten). Cosmetics never
 * collide: hitboxes, hurtboxes and the capsule belong to the kit and are baked on the game skeleton, so a piece that blocked or
 * overlapped would make the look lie about the game. The only legal value is NoCollision; the other one exists so an asset that kept
 * its mesh's collision is caught by the validator instead of silently stripped.
 */
UENUM(BlueprintType)
enum class EGSTN_CosmeticCollision : uint8
{
	NoCollision,
	/** Invalid for cosmetics: reported by the validator, and the assembler builds the piece with NoCollision anyway */
	KeepMeshCollision
};

/** How a piece is scaled into a slot's envelope */
UENUM(BlueprintType)
enum class EGSTN_CosmeticFit : uint8
{
	/** One factor on every axis, the largest that fits: the piece keeps its shape */
	Uniform,
	/** Each axis scaled on its own to fill the envelope: bands and rings that must hug the item */
	PerAxis,
	/** Native size; the validator reports a piece larger than the envelope */
	None
};

/**
 * The box a slot offers to a piece, in the slot's space, in cm. A piece is scaled to stay inside it: this is what makes one piece valid
 * on very different items, and what keeps a decoration from changing the reach the player reads.
 */
USTRUCT(BlueprintType)
struct GSTNCOSMETICSCORE_API FGSTN_CosmeticEnvelope
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (ClampMin = "0.1", ForceUnits = "cm"))
	FVector Size = FVector(5.0);
};

/** A decoration point of an item (host): a socket of its mesh, the class of decoration it accepts and how much room it has */
USTRUCT(BlueprintType)
struct GSTNCOSMETICSCORE_API FGSTN_CosmeticSlot
{
	GENERATED_BODY()

	/** Socket on the host mesh, named SLOT_<Class>_<n> by convention */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FName Socket;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (Categories = "Cosmetic.Slot"))
	FGameplayTag SlotClass;

	/**
	 * The socket's transform in mesh space (pivot at the main grip, X along the blade). A copy, filled from the socket by the editor tool
	 * (phase 2) or by hand now; the silhouette check and the plan need it without loading the mesh
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FTransform MeshTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FGSTN_CosmeticEnvelope Envelope;

	/** The left one of a pair: pieces that ask for it are mirrored on Y */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	bool bMirrored = false;
};

/** A decorative curve of an item, in mesh space, along which a theme repeats a module (filigree on the spine, a band on the grip) */
USTRUCT(BlueprintType)
struct GSTNCOSMETICSCORE_API FGSTN_CosmeticSpline
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (Categories = "Cosmetic.Spline"))
	FGameplayTag SplineClass;

	/** Control points in mesh space, cm. The length is measured on the polyline */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	TArray<FVector> Points;

	/** A ring (Band, Rim): the last point joins the first */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	bool bClosed = false;
};

/** A piece of a theme for one slot class, with the rules that adapt it to any item's slot */
USTRUCT(BlueprintType)
struct GSTNCOSMETICSCORE_API FGSTN_CosmeticPiece
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (Categories = "Cosmetic.Slot"))
	FGameplayTag SlotClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Size of the piece's mesh bounds, cm. Copied from the mesh by the editor tool (phase 2) or by hand: the plan never loads a mesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (ClampMin = "0.1", ForceUnits = "cm"))
	FVector NativeSize = FVector(5.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	EGSTN_CosmeticFit Fit = EGSTN_CosmeticFit::Uniform;

	/** The plan never scales the piece below this (it is then dropped: a speck reads as dirt, not decoration) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (ClampMin = "0.01"))
	float MinScale = 0.25f;

	/** Nor above this (a pommel blown up to a shield's slot looks like a different piece) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (ClampMin = "0.01"))
	float MaxScale = 2.f;

	/** Mirror on a mirrored slot (left shoulder, left guard) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	bool bMirrorOnMirroredSlot = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	EGSTN_CosmeticCollision Collision = EGSTN_CosmeticCollision::NoCollision;

	/**
	 * Tags the piece brings to its wearer when placed (the Lyra "tags from parts" pattern): an Anim.Trait.Heavy shoulder pad picks the
	 * heavy visual variant of a move through the skin's cosmetic tags. Also what a style option hides pieces by
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FGameplayTagContainer Tags;
};

/** A module a theme repeats along a spline class */
USTRUCT(BlueprintType)
struct GSTNCOSMETICSCORE_API FGSTN_CosmeticSplineModule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (Categories = "Cosmetic.Spline"))
	FGameplayTag SplineClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Length of one module along the curve, cm; the plan stretches it a little so a whole number fits */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (ClampMin = "0.5", ForceUnits = "cm"))
	float ModuleLength = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	EGSTN_CosmeticCollision Collision = EGSTN_CosmeticCollision::NoCollision;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FGameplayTagContainer Tags;
};

/**
 * The colour channels every item shares, the Warframe-like palette, mapped to the item's vertex colours: R primary structure, G
 * secondary, B accents, A emissive or energy areas. Same meaning on every item, so a theme's palette reads the same on all of them.
 */
USTRUCT(BlueprintType)
struct GSTNCOSMETICSCORE_API FGSTN_CosmeticPalette
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FLinearColor Primary = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FLinearColor Secondary = FLinearColor::Gray;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FLinearColor Accent = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FLinearColor Emissive = FLinearColor::Black;

	bool operator==(const FGSTN_CosmeticPalette& Other) const
	{
		return Primary.Equals(Other.Primary) && Secondary.Equals(Other.Secondary) && Accent.Equals(Other.Accent) && Emissive.Equals(Other.Emissive);
	}
};

/** One option of a style channel (Fortnite-like styles): a palette, pieces to hide, an emissive strength */
USTRUCT(BlueprintType)
struct GSTNCOSMETICSCORE_API FGSTN_CosmeticStyleOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	bool bOverridePalette = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (EditCondition = "bOverridePalette"))
	FGSTN_CosmeticPalette Palette;

	/** Pieces and modules carrying any of these tags are left out */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FGameplayTagContainer HidePiecesWithTags;

	/** Multiplies the emissive channel; 1 = as authored */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (ClampMin = "0"))
	float EmissiveScale = 1.f;
};

/** A style channel of a theme: its options, the first one is the default */
USTRUCT(BlueprintType)
struct GSTNCOSMETICSCORE_API FGSTN_CosmeticStyleChannel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (Categories = "Cosmetic.Style"))
	FGameplayTag Channel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	TArray<FGSTN_CosmeticStyleOption> Options;
};

/** A player's choice on one style channel. Travels as data of the loadout (IDs only, never meshes) */
USTRUCT(BlueprintType)
struct GSTNCOSMETICSCORE_API FGSTN_CosmeticStyleChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (Categories = "Cosmetic.Style"))
	FGameplayTag Channel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (ClampMin = "0"))
	int32 Option = 0;
};

/** A piece one item uses instead of the theme's for one slot class, only where a theme reads badly on it: the exception, not the rule */
USTRUCT(BlueprintType)
struct GSTNCOSMETICSCORE_API FGSTN_CosmeticPieceOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (Categories = "Cosmetic.Theme"))
	FGameplayTag Theme;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FGSTN_CosmeticPiece Piece;
};

/**
 * Per-item budget and the silhouette rule (skin-cosmetici-modulari.md §2b point 4, §3 rule 8). Starting values, to tune with the owner
 * on a captured video: the hitbox is the truth, so a decoration may not visibly lengthen the item beyond MaxLengthGrowth along X
 * (the reach axis) nor widen it beyond MaxWidthGrowth on Y and Z.
 */
USTRUCT(BlueprintType)
struct GSTNCOSMETICSCORE_API FGSTN_CosmeticLimits
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (ClampMin = "0"))
	int32 MaxPieces = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (ClampMin = "0"))
	int32 MaxSplines = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (ClampMin = "1"))
	int32 MaxSegmentsPerSpline = 8;

	/** Fraction of the item's length (X extent of its bounds) a decoration may add at either end */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (ClampMin = "0"))
	float MaxLengthGrowth = 0.05f;

	/** Fraction of the item's width or height (Y, Z extents) a decoration may add on each side */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (ClampMin = "0"))
	float MaxWidthGrowth = 0.15f;
};

/**
 * Support level of an item (skin-cosmetici-modulari.md §3 rule 9): what any theme can do on it. Every item is at least Material: the
 * theme's material always applies.
 */
UENUM(BlueprintType)
enum class EGSTN_CosmeticTier : uint8
{
	Material,
	Slots,
	Splines
};

/** A theme piece placed on one slot of an item */
USTRUCT(BlueprintType)
struct GSTNCOSMETICSCORE_API FGSTN_CosmeticPlacement
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	FName Socket;

	/** The piece chosen (theme piece or the item's override), copied so the plan stands on its own */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	FGSTN_CosmeticPiece Piece;

	/** True when the piece came from the item's override for this theme */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	bool bFromOverride = false;

	/** Scale in the slot's space; Y is negative when mirrored */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	FVector Scale = FVector::OneVector;
};

/** A theme module repeated along one spline of an item */
USTRUCT(BlueprintType)
struct GSTNCOSMETICSCORE_API FGSTN_CosmeticSplineRun
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	int32 SplineIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	FGSTN_CosmeticSplineModule Module;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	int32 Segments = 0;

	/** Stretch of each module along the curve so Segments fit exactly (1 = native length) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	float Stretch = 1.f;
};

/**
 * What decorating one item with one theme and one style choice gives: the input of the assembler (phase 2), and of the coverage report.
 * Deterministic: the same item, theme and style always give the same plan on every machine, so every client builds the same mesh from
 * IDs alone.
 */
USTRUCT(BlueprintType)
struct GSTNCOSMETICSCORE_API FGSTN_CosmeticPlan
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	TArray<FGSTN_CosmeticPlacement> Placements;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	TArray<FGSTN_CosmeticSplineRun> SplineRuns;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	FGSTN_CosmeticPalette Palette;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	float EmissiveScale = 1.f;

	/** Union of the Tags of every placed piece and module */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	FGameplayTagContainer Tags;

	/** Fraction of the item's slots that got a piece (0-1; 1 when the item has no slot) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	float Coverage = 1.f;

	/** Why something was left out (budget, a piece scaled under its minimum, a silhouette violation). Readable lines */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetics")
	TArray<FString> Notes;
};

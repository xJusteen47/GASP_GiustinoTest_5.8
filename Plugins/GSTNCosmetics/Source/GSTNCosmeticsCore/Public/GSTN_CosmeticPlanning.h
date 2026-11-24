// Modular cosmetics, pure layer.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_CosmeticTypes.h"

class UGSTN_CosmeticHostData;
class UGSTN_CosmeticThemeData;

/**
 * Stateless planning and validation of a theme on an item. No mesh is loaded and no world is touched: everything works on the authored
 * numbers, so the same inputs give the same plan on every machine (remote players' items are built locally from IDs) and the whole
 * thing is tested without assets. The assembler that turns a plan into one fused mesh is phase 2.
 */
struct GSTNCOSMETICSCORE_API FGSTN_CosmeticPlanning
{
	/** Bumped when the plan or the assembler changes what an item looks like: part of every cache key */
	static constexpr int32 AssemblerVersion = 1;

	/**
	 * The piece for a slot class: the item's override for this theme first, then the theme's piece of the exact class, then of each
	 * parent class up to (not including) the Cosmetic.Slot root. Null when none
	 */
	static const FGSTN_CosmeticPiece* FindPiece(const UGSTN_CosmeticThemeData& Theme, const UGSTN_CosmeticHostData* Host, const FGameplayTag& SlotClass, bool& bOutFromOverride);

	/** The theme's module for a spline class, exact then parents. Null when none */
	static const FGSTN_CosmeticSplineModule* FindModule(const UGSTN_CosmeticThemeData& Theme, const FGameplayTag& SplineClass);

	/** Scale that puts the piece inside the envelope per its fit rule. False (and Why) when it would go under MinScale or, for Fit None, does not fit */
	static bool FitScale(const FGSTN_CosmeticPiece& Piece, const FGSTN_CosmeticEnvelope& Envelope, FVector& OutScale, FString& OutWhy);

	/** Polyline length of a spline, cm (closed: back to the first point) */
	static float SplineLength(const FGSTN_CosmeticSpline& Spline);

	/** Whole modules along a length: the nearest count, 1 to MaxSegments, and the stretch that makes them fit exactly */
	static int32 SegmentCount(float Length, float ModuleLength, int32 MaxSegments, float& OutStretch);

	/** What any theme can do on the item */
	static EGSTN_CosmeticTier GetTier(const UGSTN_CosmeticHostData& Host);

	/**
	 * The box a decoration may reach: the item's bounds grown by MaxLengthGrowth of its length on X at both ends and MaxWidthGrowth of
	 * its width and height on Y and Z
	 */
	static FBox AllowedBounds(const FBox& BaseBounds, const FGSTN_CosmeticLimits& Limits);

	/**
	 * Whether a placed piece stays inside the allowed bounds. The piece box is its NativeSize scaled, centred on the socket and turned
	 * by the socket's mesh transform (a conservative box: an axis-aligned box around the turned one)
	 */
	static bool FitsSilhouette(const FBox& BaseBounds, const FGSTN_CosmeticSlot& Slot, const FVector& NativeSize, const FVector& Scale, const FGSTN_CosmeticLimits& Limits);

	/** Palette, emissive and hidden tags of a style choice. A missing channel or an out of range option uses the channel's first option */
	static void ResolveStyle(const UGSTN_CosmeticThemeData& Theme, const TArray<FGSTN_CosmeticStyleChoice>& Choices, FGSTN_CosmeticPalette& OutPalette, float& OutEmissiveScale, FGameplayTagContainer& OutHiddenTags);

	/** The plan of Theme on Host with a style choice, within Limits. See FGSTN_CosmeticPlan */
	static FGSTN_CosmeticPlan BuildPlan(const UGSTN_CosmeticHostData& Host, const UGSTN_CosmeticThemeData& Theme, const TArray<FGSTN_CosmeticStyleChoice>& Choices, const FGSTN_CosmeticLimits& Limits);

	/**
	 * Key of a built item in the locker cache and the in-memory cache: item and version, theme and version, the style choice (order of
	 * the choices does not matter) and AssemblerVersion. A canonical string, the same on every machine and every run (no FName hashes)
	 */
	static FString MakeCacheKey(const FPrimaryAssetId& HostId, int32 HostVersion, const FPrimaryAssetId& ThemeId, int32 ThemeVersion, const TArray<FGSTN_CosmeticStyleChoice>& Choices);

	/** Readable errors (the asset is unusable) and warnings of a theme */
	static void ValidateTheme(const UGSTN_CosmeticThemeData& Theme, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);

	/** Readable errors and warnings of an item, including its overrides' pieces */
	static void ValidateHost(const UGSTN_CosmeticHostData& Host, const FGSTN_CosmeticLimits& Limits, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);
};

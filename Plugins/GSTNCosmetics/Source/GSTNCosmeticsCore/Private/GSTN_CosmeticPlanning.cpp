// Modular cosmetics, pure layer.

#include "GSTN_CosmeticPlanning.h"
#include "GSTN_CosmeticData.h"
#include "GSTN_CosmeticTags.h"

namespace
{
	/** Exact class first, then each parent, stopping before the root (a piece for the bare root would go everywhere) */
	template <typename TItem>
	const TItem* FindByClass(const TArray<TItem>& Items, const FGameplayTag& Class, const FGameplayTag& Root, FGameplayTag TItem::* Member)
	{
		FGameplayTag Current = Class;
		while (Current.IsValid() && Current != Root)
		{
			if (const TItem* Found = Items.FindByPredicate([&Current, Member](const TItem& Item) { return Item.*Member == Current; }))
			{
				return Found;
			}
			Current = Current.RequestDirectParent();
		}
		return nullptr;
	}

	bool IsHidden(const FGameplayTagContainer& ItemTags, const FGameplayTagContainer& Hidden)
	{
		return !Hidden.IsEmpty() && ItemTags.HasAny(Hidden);
	}

	void ValidatePiece(const FGSTN_CosmeticPiece& Piece, const FString& Where, TArray<FString>& OutErrors)
	{
		if (!Piece.SlotClass.MatchesTag(GSTNCosmeticTags::Slot.GetTag()) || Piece.SlotClass == GSTNCosmeticTags::Slot.GetTag())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: slot class must be a leaf under Cosmetic.Slot"), *Where));
		}
		if (Piece.Mesh.IsNull())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: no mesh"), *Where));
		}
		if (Piece.NativeSize.GetMin() <= 0.0)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: native size must be positive on every axis"), *Where));
		}
		if (Piece.MinScale > Piece.MaxScale)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: min scale %.2f above max scale %.2f"), *Where, Piece.MinScale, Piece.MaxScale));
		}
		if (Piece.Collision != EGSTN_CosmeticCollision::NoCollision)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: cosmetics never collide (hitboxes belong to the kit): set Collision to NoCollision"), *Where));
		}
	}
}

const FGSTN_CosmeticPiece* FGSTN_CosmeticPlanning::FindPiece(const UGSTN_CosmeticThemeData& Theme, const UGSTN_CosmeticHostData* Host, const FGameplayTag& SlotClass, bool& bOutFromOverride)
{
	bOutFromOverride = false;
	if (Host)
	{
		FGameplayTag Current = SlotClass;
		while (Current.IsValid() && Current != GSTNCosmeticTags::Slot.GetTag())
		{
			const FGSTN_CosmeticPieceOverride* Override = Host->Overrides.FindByPredicate([&Theme, &Current](const FGSTN_CosmeticPieceOverride& Item)
			{
				return Item.Theme == Theme.ThemeTag && Item.Piece.SlotClass == Current;
			});
			if (Override)
			{
				bOutFromOverride = true;
				return &Override->Piece;
			}
			Current = Current.RequestDirectParent();
		}
	}
	return FindByClass(Theme.Pieces, SlotClass, GSTNCosmeticTags::Slot.GetTag(), &FGSTN_CosmeticPiece::SlotClass);
}

const FGSTN_CosmeticSplineModule* FGSTN_CosmeticPlanning::FindModule(const UGSTN_CosmeticThemeData& Theme, const FGameplayTag& SplineClass)
{
	return FindByClass(Theme.SplineModules, SplineClass, GSTNCosmeticTags::Spline.GetTag(), &FGSTN_CosmeticSplineModule::SplineClass);
}

bool FGSTN_CosmeticPlanning::FitScale(const FGSTN_CosmeticPiece& Piece, const FGSTN_CosmeticEnvelope& Envelope, FVector& OutScale, FString& OutWhy)
{
	const FVector Native = Piece.NativeSize.ComponentMax(FVector(UE_KINDA_SMALL_NUMBER));
	const FVector Ratios = Envelope.Size / Native;
	switch (Piece.Fit)
	{
	case EGSTN_CosmeticFit::None:
		OutScale = FVector::OneVector;
		if (Ratios.GetMin() < 1.0)
		{
			OutWhy = TEXT("larger than the slot and set not to scale");
			return false;
		}
		return true;
	case EGSTN_CosmeticFit::PerAxis:
		OutScale = FVector(
			FMath::Min<double>(Ratios.X, Piece.MaxScale),
			FMath::Min<double>(Ratios.Y, Piece.MaxScale),
			FMath::Min<double>(Ratios.Z, Piece.MaxScale));
		break;
	case EGSTN_CosmeticFit::Uniform:
	default:
		OutScale = FVector(FMath::Min<double>(Ratios.GetMin(), Piece.MaxScale));
		break;
	}
	if (OutScale.GetMin() < Piece.MinScale)
	{
		OutWhy = FString::Printf(TEXT("would be scaled to %.2f, under its minimum %.2f"), OutScale.GetMin(), Piece.MinScale);
		return false;
	}
	return true;
}

float FGSTN_CosmeticPlanning::SplineLength(const FGSTN_CosmeticSpline& Spline)
{
	double Length = 0.0;
	for (int32 Index = 1; Index < Spline.Points.Num(); ++Index)
	{
		Length += FVector::Distance(Spline.Points[Index - 1], Spline.Points[Index]);
	}
	if (Spline.bClosed && Spline.Points.Num() > 2)
	{
		Length += FVector::Distance(Spline.Points.Last(), Spline.Points[0]);
	}
	return static_cast<float>(Length);
}

int32 FGSTN_CosmeticPlanning::SegmentCount(float Length, float ModuleLength, int32 MaxSegments, float& OutStretch)
{
	OutStretch = 1.f;
	if (Length <= UE_KINDA_SMALL_NUMBER || ModuleLength <= UE_KINDA_SMALL_NUMBER)
	{
		return 0;
	}
	const int32 Count = FMath::Clamp(FMath::RoundToInt(Length / ModuleLength), 1, FMath::Max(1, MaxSegments));
	OutStretch = Length / (Count * ModuleLength);
	return Count;
}

EGSTN_CosmeticTier FGSTN_CosmeticPlanning::GetTier(const UGSTN_CosmeticHostData& Host)
{
	if (Host.Slots.Num() > 0 && Host.Splines.Num() > 0)
	{
		return EGSTN_CosmeticTier::Splines;
	}
	return Host.Slots.Num() > 0 ? EGSTN_CosmeticTier::Slots : EGSTN_CosmeticTier::Material;
}

FBox FGSTN_CosmeticPlanning::AllowedBounds(const FBox& BaseBounds, const FGSTN_CosmeticLimits& Limits)
{
	const FVector Size = BaseBounds.GetSize();
	const FVector Grow(Size.X * Limits.MaxLengthGrowth, Size.Y * Limits.MaxWidthGrowth, Size.Z * Limits.MaxWidthGrowth);
	return FBox(BaseBounds.Min - Grow, BaseBounds.Max + Grow);
}

bool FGSTN_CosmeticPlanning::FitsSilhouette(const FBox& BaseBounds, const FGSTN_CosmeticSlot& Slot, const FVector& NativeSize, const FVector& Scale, const FGSTN_CosmeticLimits& Limits)
{
	const FVector Half = (NativeSize * Scale.GetAbs()) * 0.5;
	const FBox Placed = FBox(-Half, Half).TransformBy(Slot.MeshTransform);
	const FBox Allowed = AllowedBounds(BaseBounds, Limits);
	return Allowed.IsInsideOrOn(Placed.Min) && Allowed.IsInsideOrOn(Placed.Max);
}

void FGSTN_CosmeticPlanning::ResolveStyle(const UGSTN_CosmeticThemeData& Theme, const TArray<FGSTN_CosmeticStyleChoice>& Choices, FGSTN_CosmeticPalette& OutPalette, float& OutEmissiveScale, FGameplayTagContainer& OutHiddenTags)
{
	OutPalette = Theme.Palette;
	OutEmissiveScale = 1.f;
	OutHiddenTags.Reset();
	// Channels in the theme's order, so two channels that both override the palette resolve the same way everywhere
	for (const FGSTN_CosmeticStyleChannel& Channel : Theme.StyleChannels)
	{
		if (Channel.Options.Num() == 0)
		{
			continue;
		}
		const FGSTN_CosmeticStyleChoice* Choice = Choices.FindByPredicate([&Channel](const FGSTN_CosmeticStyleChoice& Item) { return Item.Channel == Channel.Channel; });
		const int32 Index = Choice && Channel.Options.IsValidIndex(Choice->Option) ? Choice->Option : 0;
		const FGSTN_CosmeticStyleOption& Option = Channel.Options[Index];
		if (Option.bOverridePalette)
		{
			OutPalette = Option.Palette;
		}
		OutEmissiveScale *= Option.EmissiveScale;
		OutHiddenTags.AppendTags(Option.HidePiecesWithTags);
	}
}

FGSTN_CosmeticPlan FGSTN_CosmeticPlanning::BuildPlan(const UGSTN_CosmeticHostData& Host, const UGSTN_CosmeticThemeData& Theme, const TArray<FGSTN_CosmeticStyleChoice>& Choices, const FGSTN_CosmeticLimits& Limits)
{
	FGSTN_CosmeticPlan Plan;
	FGameplayTagContainer Hidden;
	ResolveStyle(Theme, Choices, Plan.Palette, Plan.EmissiveScale, Hidden);

	// Slots in authored order: when the budget runs out, the first slots of the item (its most important ones) keep their piece
	int32 Covered = 0;
	for (int32 SlotIndex = 0; SlotIndex < Host.Slots.Num(); ++SlotIndex)
	{
		const FGSTN_CosmeticSlot& Slot = Host.Slots[SlotIndex];
		bool bFromOverride = false;
		const FGSTN_CosmeticPiece* Piece = FindPiece(Theme, &Host, Slot.SlotClass, bFromOverride);
		if (!Piece)
		{
			continue;
		}
		if (IsHidden(Piece->Tags, Hidden))
		{
			// Left out by the player's style: the slot is still covered by the theme
			++Covered;
			continue;
		}
		if (Plan.Placements.Num() >= Limits.MaxPieces)
		{
			Plan.Notes.Add(FString::Printf(TEXT("slot %s: over the budget of %d pieces"), *Slot.Socket.ToString(), Limits.MaxPieces));
			continue;
		}
		FVector Scale;
		FString Why;
		if (!FitScale(*Piece, Slot.Envelope, Scale, Why))
		{
			Plan.Notes.Add(FString::Printf(TEXT("slot %s: piece %s"), *Slot.Socket.ToString(), *Why));
			continue;
		}
		if (!FitsSilhouette(Host.BaseBounds, Slot, Piece->NativeSize, Scale, Limits))
		{
			Plan.Notes.Add(FString::Printf(TEXT("slot %s: piece would change the item's silhouette beyond the limits"), *Slot.Socket.ToString()));
			continue;
		}
		if (Slot.bMirrored && Piece->bMirrorOnMirroredSlot)
		{
			Scale.Y = -Scale.Y;
		}
		FGSTN_CosmeticPlacement& Placement = Plan.Placements.AddDefaulted_GetRef();
		Placement.SlotIndex = SlotIndex;
		Placement.Socket = Slot.Socket;
		Placement.Piece = *Piece;
		// Whatever the asset says, a placed piece never collides; the validator reports the asset
		Placement.Piece.Collision = EGSTN_CosmeticCollision::NoCollision;
		Placement.bFromOverride = bFromOverride;
		Placement.Scale = Scale;
		Plan.Tags.AppendTags(Piece->Tags);
		++Covered;
	}
	Plan.Coverage = Host.Slots.Num() > 0 ? static_cast<float>(Covered) / Host.Slots.Num() : 1.f;

	for (int32 SplineIndex = 0; SplineIndex < Host.Splines.Num(); ++SplineIndex)
	{
		const FGSTN_CosmeticSpline& Spline = Host.Splines[SplineIndex];
		const FGSTN_CosmeticSplineModule* Module = FindModule(Theme, Spline.SplineClass);
		if (!Module || IsHidden(Module->Tags, Hidden))
		{
			continue;
		}
		if (Plan.SplineRuns.Num() >= Limits.MaxSplines)
		{
			Plan.Notes.Add(FString::Printf(TEXT("spline %d: over the budget of %d splines"), SplineIndex, Limits.MaxSplines));
			continue;
		}
		float Stretch = 1.f;
		const int32 Segments = SegmentCount(SplineLength(Spline), Module->ModuleLength, Limits.MaxSegmentsPerSpline, Stretch);
		if (Segments <= 0)
		{
			Plan.Notes.Add(FString::Printf(TEXT("spline %d: too short for a module"), SplineIndex));
			continue;
		}
		FGSTN_CosmeticSplineRun& Run = Plan.SplineRuns.AddDefaulted_GetRef();
		Run.SplineIndex = SplineIndex;
		Run.Module = *Module;
		Run.Module.Collision = EGSTN_CosmeticCollision::NoCollision;
		Run.Segments = Segments;
		Run.Stretch = Stretch;
		Plan.Tags.AppendTags(Module->Tags);
	}
	return Plan;
}

FString FGSTN_CosmeticPlanning::MakeCacheKey(const FPrimaryAssetId& HostId, int32 HostVersion, const FPrimaryAssetId& ThemeId, int32 ThemeVersion, const TArray<FGSTN_CosmeticStyleChoice>& Choices)
{
	TArray<FString> Parts;
	for (const FGSTN_CosmeticStyleChoice& Choice : Choices)
	{
		Parts.Add(FString::Printf(TEXT("%s=%d"), *Choice.Channel.ToString(), Choice.Option));
	}
	Parts.Sort();
	return FString::Printf(TEXT("%s@%d|%s@%d|%s|a%d"), *HostId.ToString(), HostVersion, *ThemeId.ToString(), ThemeVersion, *FString::Join(Parts, TEXT(",")), AssemblerVersion);
}

void FGSTN_CosmeticPlanning::ValidateTheme(const UGSTN_CosmeticThemeData& Theme, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	if (!Theme.ThemeTag.MatchesTag(GSTNCosmeticTags::Theme.GetTag()) || Theme.ThemeTag == GSTNCosmeticTags::Theme.GetTag())
	{
		OutErrors.Add(TEXT("theme tag must be a leaf under Cosmetic.Theme"));
	}
	if (Theme.LayerMaterial.IsNull())
	{
		OutErrors.Add(TEXT("no layer material: the material is what every item gets, even one with no slot"));
	}
	TSet<FGameplayTag> Seen;
	for (int32 Index = 0; Index < Theme.Pieces.Num(); ++Index)
	{
		const FGSTN_CosmeticPiece& Piece = Theme.Pieces[Index];
		ValidatePiece(Piece, FString::Printf(TEXT("piece %d (%s)"), Index, *Piece.SlotClass.ToString()), OutErrors);
		bool bDuplicate = false;
		Seen.Add(Piece.SlotClass, &bDuplicate);
		if (bDuplicate)
		{
			OutErrors.Add(FString::Printf(TEXT("piece %d: a second piece for %s (one per slot class)"), Index, *Piece.SlotClass.ToString()));
		}
	}
	Seen.Reset();
	for (int32 Index = 0; Index < Theme.SplineModules.Num(); ++Index)
	{
		const FGSTN_CosmeticSplineModule& Module = Theme.SplineModules[Index];
		const FString Where = FString::Printf(TEXT("spline module %d (%s)"), Index, *Module.SplineClass.ToString());
		if (!Module.SplineClass.MatchesTag(GSTNCosmeticTags::Spline.GetTag()) || Module.SplineClass == GSTNCosmeticTags::Spline.GetTag())
		{
			OutErrors.Add(Where + TEXT(": spline class must be a leaf under Cosmetic.Spline"));
		}
		if (Module.Mesh.IsNull())
		{
			OutErrors.Add(Where + TEXT(": no mesh"));
		}
		if (Module.ModuleLength <= 0.f)
		{
			OutErrors.Add(Where + TEXT(": module length must be positive"));
		}
		if (Module.Collision != EGSTN_CosmeticCollision::NoCollision)
		{
			OutErrors.Add(Where + TEXT(": cosmetics never collide (hitboxes belong to the kit): set Collision to NoCollision"));
		}
		bool bDuplicate = false;
		Seen.Add(Module.SplineClass, &bDuplicate);
		if (bDuplicate)
		{
			OutErrors.Add(Where + TEXT(": a second module for the same spline class"));
		}
	}
	Seen.Reset();
	for (const FGSTN_CosmeticStyleChannel& Channel : Theme.StyleChannels)
	{
		if (!Channel.Channel.IsValid())
		{
			OutErrors.Add(TEXT("a style channel has no channel tag"));
		}
		if (Channel.Options.Num() == 0)
		{
			OutWarnings.Add(FString::Printf(TEXT("style channel %s has no option and is ignored"), *Channel.Channel.ToString()));
		}
		bool bDuplicate = false;
		Seen.Add(Channel.Channel, &bDuplicate);
		if (bDuplicate)
		{
			OutErrors.Add(FString::Printf(TEXT("style channel %s appears twice"), *Channel.Channel.ToString()));
		}
	}
	if (Theme.Pieces.Num() == 0 && Theme.SplineModules.Num() == 0)
	{
		OutWarnings.Add(TEXT("no piece and no spline module: the theme is material only"));
	}
}

void FGSTN_CosmeticPlanning::ValidateHost(const UGSTN_CosmeticHostData& Host, const FGSTN_CosmeticLimits& Limits, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	if (Host.BaseMesh.IsNull())
	{
		OutErrors.Add(TEXT("no base mesh"));
	}
	if (!Host.BaseBounds.IsValid || Host.BaseBounds.GetSize().GetMin() <= 0.0)
	{
		OutErrors.Add(TEXT("base bounds are empty: copy them from the mesh"));
	}
	TSet<FName> Sockets;
	for (int32 Index = 0; Index < Host.Slots.Num(); ++Index)
	{
		const FGSTN_CosmeticSlot& Slot = Host.Slots[Index];
		const FString Where = FString::Printf(TEXT("slot %d (%s)"), Index, *Slot.Socket.ToString());
		if (Slot.Socket.IsNone())
		{
			OutErrors.Add(Where + TEXT(": no socket"));
		}
		bool bDuplicate = false;
		Sockets.Add(Slot.Socket, &bDuplicate);
		if (bDuplicate)
		{
			OutErrors.Add(Where + TEXT(": socket used by two slots"));
		}
		if (!Slot.SlotClass.MatchesTag(GSTNCosmeticTags::Slot.GetTag()) || Slot.SlotClass == GSTNCosmeticTags::Slot.GetTag())
		{
			OutErrors.Add(Where + TEXT(": slot class must be a leaf under Cosmetic.Slot"));
		}
		if (Slot.Envelope.Size.GetMin() <= 0.0)
		{
			OutErrors.Add(Where + TEXT(": envelope must be positive on every axis"));
		}
		if (Host.BaseBounds.IsValid && !AllowedBounds(Host.BaseBounds, Limits).IsInsideOrOn(Slot.MeshTransform.GetLocation()))
		{
			OutWarnings.Add(Where + TEXT(": the socket is outside the item's allowed silhouette, no piece will ever fit there"));
		}
	}
	if (Host.Slots.Num() > Limits.MaxPieces)
	{
		OutWarnings.Add(FString::Printf(TEXT("%d slots, over the budget of %d pieces: the last slots stay bare"), Host.Slots.Num(), Limits.MaxPieces));
	}
	if (Host.Splines.Num() > Limits.MaxSplines)
	{
		OutErrors.Add(FString::Printf(TEXT("%d splines, the limit is %d"), Host.Splines.Num(), Limits.MaxSplines));
	}
	for (int32 Index = 0; Index < Host.Splines.Num(); ++Index)
	{
		const FGSTN_CosmeticSpline& Spline = Host.Splines[Index];
		const FString Where = FString::Printf(TEXT("spline %d (%s)"), Index, *Spline.SplineClass.ToString());
		if (!Spline.SplineClass.MatchesTag(GSTNCosmeticTags::Spline.GetTag()) || Spline.SplineClass == GSTNCosmeticTags::Spline.GetTag())
		{
			OutErrors.Add(Where + TEXT(": spline class must be a leaf under Cosmetic.Spline"));
		}
		if (Spline.Points.Num() < 2)
		{
			OutErrors.Add(Where + TEXT(": needs at least 2 points"));
		}
	}
	int32 OverridesPerTheme = 0;
	for (int32 Index = 0; Index < Host.Overrides.Num(); ++Index)
	{
		const FGSTN_CosmeticPieceOverride& Override = Host.Overrides[Index];
		if (!Override.Theme.MatchesTag(GSTNCosmeticTags::Theme.GetTag()) || Override.Theme == GSTNCosmeticTags::Theme.GetTag())
		{
			OutErrors.Add(FString::Printf(TEXT("override %d: theme must be a leaf under Cosmetic.Theme"), Index));
		}
		ValidatePiece(Override.Piece, FString::Printf(TEXT("override %d"), Index), OutErrors);
		OverridesPerTheme = FMath::Max(OverridesPerTheme, static_cast<int32>(Host.Overrides.FilterByPredicate([&Override](const FGSTN_CosmeticPieceOverride& Item) { return Item.Theme == Override.Theme; }).Num()));
	}
	// The scale test of the approach (phase 2 goal): more than one override in ten slots for a theme means the slots are wrong
	if (Host.Slots.Num() > 0 && OverridesPerTheme * 10 > Host.Slots.Num())
	{
		OutWarnings.Add(FString::Printf(TEXT("%d overrides for one theme on %d slots: more than one in ten, fix the slots instead"), OverridesPerTheme, Host.Slots.Num()));
	}
}

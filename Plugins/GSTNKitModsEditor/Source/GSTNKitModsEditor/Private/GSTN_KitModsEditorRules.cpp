// Editor tooling of kit mods (M11 L8). Editor only.

#include "GSTN_KitModsEditorRules.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "Cosmetics/GSTN_AttachmentProvider.h"
#include "Cosmetics/GSTN_EquipmentVisual.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GSTN_KitModBaking.h"
#include "GSTN_KitModDefinition.h"
#include "GSTN_KitModsEditorSettings.h"
#include "Hits/GSTN_HitReachUtils.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "Loadout/GSTN_LoadoutExtensionData.h"
#include "Misc/DataValidation.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "UObject/UnrealType.h"
#include "Validation/GSTN_ValidationExtensions.h"
#include "Validation/GSTN_ValidationReport.h"

#define LOCTEXT_NAMESPACE "GSTNKitModsEditor"

const FName FGSTN_KitModsEditorRules::ExtensionName(TEXT("GSTN.KitMods"));

FName GSTNKitModsCodes::Data() { return TEXT("Mod.Data"); }
FName GSTNKitModsCodes::NoKit() { return TEXT("Mod.NoKit"); }
FName GSTNKitModsCodes::NoMove() { return TEXT("Mod.NoMove"); }
FName GSTNKitModsCodes::Clamped() { return TEXT("Mod.Clamped"); }
FName GSTNKitModsCodes::Derived() { return TEXT("Mod.Derived"); }
FName GSTNKitModsCodes::ReachMesh() { return TEXT("Mod.ReachMesh"); }
FName GSTNKitModsCodes::ReachNoMesh() { return TEXT("Mod.ReachNoMesh"); }
FName GSTNKitModsCodes::SlotsData() { return TEXT("Mod.Slots.Data"); }
FName GSTNKitModsCodes::SlotsCap() { return TEXT("Mod.Slots.Cap"); }
FName GSTNKitModsCodes::SlotsNoMod() { return TEXT("Mod.Slots.NoMod"); }
FName GSTNKitModsCodes::Loadout() { return TEXT("Mod.Loadout"); }

namespace
{
	const FGSTN_KitModSlots* FindSlots(const UGSTN_PrimaryKitDefinition& Kit)
	{
		return Kit.Extensions.Find<FGSTN_KitModSlots>();
	}

	/** The FGSTN_LoadoutData values the object's class holds (the combat loadout preset validator looks the same way) */
	void GatherLoadouts(const UObject& Object, TArray<const FGSTN_LoadoutData*>& Out)
	{
		for (TFieldIterator<FStructProperty> Property(Object.GetClass()); Property; ++Property)
		{
			if (Property->Struct == FGSTN_LoadoutData::StaticStruct())
			{
				Out.Add(Property->ContainerPtrToValuePtr<FGSTN_LoadoutData>(&Object));
			}
		}
	}

	bool HoldsModChoice(const UObject& Object)
	{
		TArray<const FGSTN_LoadoutData*> Loadouts;
		GatherLoadouts(Object, Loadouts);
		for (const FGSTN_LoadoutData* Loadout : Loadouts)
		{
			if (GSTNLoadoutExtension::Find<FGSTN_LoadoutModsExt>(Loadout->Extensions))
			{
				return true;
			}
		}
		return false;
	}

	FString MoveLabel(const UGSTN_PrimaryKitDefinition& Kit, const FGSTN_ModMovePreview& Move)
	{
		const FString Source = Move.WeaponIndex == INDEX_NONE ? FString(TEXT("kit")) : FString::Printf(TEXT("weapon %d"), Move.WeaponIndex);
		return FString::Printf(TEXT("%s, %s (%s)"), *Kit.GetName(), *Move.Command.ToString(), *Source);
	}

	/** How many findings of each (code, severity) a report has: what the derived rule compares between base and derived */
	TMap<TPair<FName, uint8>, int32> CountFindings(const FGSTN_ValidationReport& Report)
	{
		TMap<TPair<FName, uint8>, int32> Counts;
		for (const FGSTN_ValidationFinding& Finding : Report.Findings)
		{
			++Counts.FindOrAdd(TPair<FName, uint8>(Finding.Code, static_cast<uint8>(Finding.Severity)));
		}
		return Counts;
	}

	bool ChangesReach(const UGSTN_KitModDefinition& Mod)
	{
		return Mod.MoveModifiers.ContainsByPredicate([](const FGSTN_MoveModifier& Modifier) { return !FMath::IsNearlyEqual(Modifier.ReachScale, 1.f); });
	}
}

void FGSTN_KitModsEditorRules::Register()
{
	FGSTN_ValidationExtension Extension;
	Extension.Name = ExtensionName;
	Extension.AssetClasses.Add(UGSTN_KitModDefinition::StaticClass());
	Extension.AppliesTo = &FGSTN_KitModsEditorRules::AppliesTo;
	Extension.Rule = &FGSTN_KitModsEditorRules::Run;
	FGSTN_ValidationExtensions::Register(MoveTemp(Extension));
}

void FGSTN_KitModsEditorRules::Unregister()
{
	FGSTN_ValidationExtensions::Unregister(ExtensionName);
}

bool FGSTN_KitModsEditorRules::AppliesTo(const UObject& Asset)
{
	if (Asset.IsA<UGSTN_KitModDefinition>())
	{
		return true;
	}
	if (const UGSTN_PrimaryKitDefinition* Kit = Cast<UGSTN_PrimaryKitDefinition>(&Asset))
	{
		return FindSlots(*Kit) != nullptr;
	}
	return HoldsModChoice(Asset);
}

void FGSTN_KitModsEditorRules::Run(UObject& Asset, FGSTN_ValidationReport& Report)
{
	if (const UGSTN_KitModDefinition* Mod = Cast<UGSTN_KitModDefinition>(&Asset))
	{
		TArray<const UGSTN_PrimaryKitDefinition*> Kits;
		GatherKits(Kits);
		CheckMod(*Mod, Kits, UGSTN_KitModsEditorSettings::GetReachMeshTolerance(), Report);
		return;
	}
	if (const UGSTN_PrimaryKitDefinition* Kit = Cast<UGSTN_PrimaryKitDefinition>(&Asset))
	{
		TArray<const UGSTN_KitModDefinition*> Mods;
		GatherMods(Mods);
		CheckKitSlots(*Kit, Mods, Report);
		return;
	}
	TArray<const FGSTN_LoadoutData*> Loadouts;
	GatherLoadouts(Asset, Loadouts);
	if (!Loadouts.IsEmpty())
	{
		TArray<const UGSTN_PrimaryKitDefinition*> Kits;
		GatherKits(Kits);
		for (const FGSTN_LoadoutData* Loadout : Loadouts)
		{
			CheckLoadout(*Loadout, &Asset, Kits, Report);
		}
	}
}

bool FGSTN_KitModsEditorRules::ModFitsKit(const UGSTN_KitModDefinition& Mod, const UGSTN_PrimaryKitDefinition& Kit)
{
	const FGSTN_KitModSlots* Slots = FindSlots(Kit);
	if (!Slots || !Slots->Slots.Contains(Mod.Slot))
	{
		return false;
	}
	if (Mod.CompatibleKits.IsEmpty())
	{
		return true;
	}
	// The same tags ResolveMods matches: the kit's plus its primary weapon's
	FGameplayTagContainer Tags = Kit.KitTags;
	if (const UGSTN_WeaponDefinition* Weapon = Kit.PrimaryWeapon.LoadSynchronous())
	{
		Tags.AppendTags(Weapon->WeaponTags);
	}
	return Mod.CompatibleKits.Matches(Tags);
}

void FGSTN_KitModsEditorRules::PreviewModsOnKit(const UGSTN_PrimaryKitDefinition& Kit, TConstArrayView<const UGSTN_KitModDefinition*> Mods, FGSTN_KitModPreview& Out)
{
	FGSTN_ResolvedLoadout Resolved;
	Resolved.Kit = &Kit;
	Resolved.Loadout.PrimaryKit = Kit.GetPrimaryAssetId();
	if (const UGSTN_WeaponDefinition* Weapon = Kit.PrimaryWeapon.LoadSynchronous())
	{
		Resolved.Weapons.Add(Weapon);
	}

	// The input the compile starts from, without mods (the other extensions still contribute, as in a match)
	FGSTN_CompileInput BaseInput;
	FGSTN_LoadoutAssembly::BuildCompileInput(Resolved, 0, BaseInput);

	// The mods as a player would equip them: the wildcard configuration, each in its own slot type, in the given order
	FGSTN_LoadoutModsExt Ext;
	FGSTN_KitModConfig& Config = Ext.Configs.AddDefaulted_GetRef();
	for (const UGSTN_KitModDefinition* Mod : Mods)
	{
		if (Mod)
		{
			Config.Mods.Add({ Mod->Slot, FSoftObjectPath(Mod) });
		}
	}
	GSTNLoadoutExtension::Set(Resolved.Loadout.Extensions, Ext);
	FGSTN_KitModLoadout::BuildPreview(Resolved, BaseInput, Out);
}

void FGSTN_KitModsEditorRules::CheckMod(const UGSTN_KitModDefinition& Mod, TConstArrayView<const UGSTN_PrimaryKitDefinition*> Kits, float ReachTolerance,
	FGSTN_ValidationReport& Report)
{
	UObject* Asset = const_cast<UGSTN_KitModDefinition*>(&Mod);

	// 1. The asset's own checks, so the panel lists them with the rest
	FDataValidationContext Own;
	Mod.IsDataValid(Own);
	for (const FDataValidationContext::FIssue& Issue : Own.GetIssues())
	{
		const EGSTN_Severity Severity = Issue.Severity == EMessageSeverity::Error ? EGSTN_Severity::Error
			: Issue.Severity == EMessageSeverity::Warning || Issue.Severity == EMessageSeverity::PerformanceWarning ? EGSTN_Severity::Warning : EGSTN_Severity::Info;
		Report.Add(Severity, GSTNKitModsCodes::Data(), Issue.Message, Asset);
	}

	// 2. Where it fits
	const double MeshReach = EstimateVisualReach(Mod.Visual);
	if (ChangesReach(Mod) && MeshReach <= 0.0)
	{
		Report.Add(EGSTN_Severity::Info, GSTNKitModsCodes::ReachNoMesh(),
			LOCTEXT("ReachNoMesh", "The mod changes reach but its visual has no mesh: nothing shows the longer (or shorter) weapon"), Asset);
	}
	int32 Fitting = 0;
	for (const UGSTN_PrimaryKitDefinition* Kit : Kits)
	{
		if (!Kit || !ModFitsKit(Mod, *Kit))
		{
			continue;
		}
		++Fitting;

		// 3. What it does to each move of that kit, as the compile bakes it
		FGSTN_KitModPreview Preview;
		PreviewModsOnKit(*Kit, { &Mod }, Preview);
		if (!Mod.MoveModifiers.IsEmpty() && Preview.Moves.IsEmpty())
		{
			Report.Add(EGSTN_Severity::Warning, GSTNKitModsCodes::NoMove(), FText::Format(
				LOCTEXT("NoMove", "On kit {0} no move filter selects a move with a timeline: the move modifiers do nothing there"),
				FText::FromString(Kit->GetName())), Asset);
		}
		for (const FGSTN_ModMovePreview& Move : Preview.Moves)
		{
			const FText Label = FText::FromString(MoveLabel(*Kit, Move));
			if (Move.bClamped)
			{
				Report.Add(EGSTN_Severity::Info, GSTNKitModsCodes::Clamped(), FText::Format(
					LOCTEXT("Clamped", "{0}: the kit's mod limits cut the change back (time x{1}, reach x{2})"),
					Label, FText::AsNumber(Move.Modifier.TimeScale), FText::AsNumber(Move.Modifier.ReachScale)), Asset);
			}

			// The M5 timeline rules on base and derived: only what the mod adds is the mod's problem
			FGSTN_ValidationReport BaseReport;
			FGSTN_ValidationReport DerivedReport;
			FGSTN_ValidationExtensions::ValidateTimelineData(Move.Base, Asset, BaseReport, false);
			FGSTN_ValidationExtensions::ValidateTimelineData(Move.Derived, Asset, DerivedReport, false);
			TMap<TPair<FName, uint8>, int32> Remaining = CountFindings(BaseReport);
			for (const FGSTN_ValidationFinding& Finding : DerivedReport.Findings)
			{
				int32& Left = Remaining.FindOrAdd(TPair<FName, uint8>(Finding.Code, static_cast<uint8>(Finding.Severity)));
				if (Left > 0)
				{
					--Left;
					continue;
				}
				Report.Add(Finding.Severity, GSTNKitModsCodes::Derived(), FText::Format(LOCTEXT("Derived", "{0}: the modded move breaks [{1}] {2}"),
					Label, FText::FromName(Finding.Code), Finding.Message), Asset);
			}

			// The longer blade: the scaled hitboxes should end about where the mod's mesh does
			if (!FMath::IsNearlyEqual(Move.Modifier.ReachScale, 1.f) && MeshReach > 0.0)
			{
				const double Tip = EstimateReachTip(Move.Derived);
				if (Tip > 0.0 && FMath::Abs(Tip - MeshReach) > MeshReach * ReachTolerance)
				{
					Report.Add(EGSTN_Severity::Warning, GSTNKitModsCodes::ReachMesh(), FText::Format(
						LOCTEXT("ReachMesh", "{0}: the scaled hitboxes end at {1} cm, the mod's mesh at about {2} cm"),
						Label, FText::AsNumber(FMath::RoundToInt(Tip)), FText::AsNumber(FMath::RoundToInt(MeshReach))), Asset);
				}
			}
		}
	}
	if (Fitting == 0)
	{
		Report.Add(EGSTN_Severity::Warning, GSTNKitModsCodes::NoKit(), FText::Format(
			LOCTEXT("NoKit", "No kit in the project has a {0} slot that this mod's CompatibleKits accepts: no player can equip it"),
			FText::FromString(Mod.Slot.ToString())), Asset);
	}
}

void FGSTN_KitModsEditorRules::CheckKitSlots(const UGSTN_PrimaryKitDefinition& Kit, TConstArrayView<const UGSTN_KitModDefinition*> Mods, FGSTN_ValidationReport& Report)
{
	const FGSTN_KitModSlots* Slots = FindSlots(Kit);
	if (!Slots)
	{
		return;
	}
	UObject* Asset = const_cast<UGSTN_PrimaryKitDefinition*>(&Kit);
	if (Slots->Slots.IsEmpty())
	{
		Report.Add(EGSTN_Severity::Warning, GSTNKitModsCodes::SlotsData(), LOCTEXT("NoSlot", "The Kit mod slots extension lists no slot: no mod fits this kit"), Asset);
	}
	for (const FGameplayTag& Slot : Slots->Slots)
	{
		if (!Slot.IsValid() || !Slot.MatchesTag(GSTNKitModsTags::Mod_Slot))
		{
			Report.Add(EGSTN_Severity::Error, GSTNKitModsCodes::SlotsData(), FText::Format(
				LOCTEXT("SlotTag", "Mod slot {0} is not a tag under Mod.Slot"), FText::FromString(Slot.ToString())), Asset);
		}
	}
	const FGSTN_KitModLimits& Limits = Slots->Limits;
	if (Limits.MinTimeScale > 1.f || Limits.MaxTimeScale < 1.f || Limits.MaxReachScale < 1.f || Limits.MinTimeScale <= 0.f)
	{
		Report.Add(EGSTN_Severity::Error, GSTNKitModsCodes::SlotsData(),
			LOCTEXT("Limits", "The mod limits must include 1 (MinTimeScale <= 1 <= MaxTimeScale, MaxReachScale >= 1): an unmodded move would be changed"), Asset);
	}

	// The cap against the moves mods could change: every kit binding and every primary weapon binding
	int32 Moves = Kit.KitBindings.Num();
	if (const UGSTN_WeaponDefinition* Weapon = Kit.PrimaryWeapon.LoadSynchronous())
	{
		TArray<FGSTN_AbilityBinding> Bindings;
		Weapon->GetEffectiveBindings(Bindings);
		Moves += Bindings.Num();
	}
	if (Slots->MaxDerivedTimelines < Moves)
	{
		Report.Add(EGSTN_Severity::Warning, GSTNKitModsCodes::SlotsCap(), FText::Format(
			LOCTEXT("Cap", "MaxDerivedTimelines is {0} but the kit and its weapon bind {1} moves: with mods on every move, {2} keep their base timeline"),
			FText::AsNumber(Slots->MaxDerivedTimelines), FText::AsNumber(Moves), FText::AsNumber(Moves - Slots->MaxDerivedTimelines)), Asset);
	}

	// Slot types no mod of the project fits
	TSet<FGameplayTag> Reported;
	for (const FGameplayTag& Slot : Slots->Slots)
	{
		if (Reported.Contains(Slot))
		{
			continue;
		}
		Reported.Add(Slot);
		const bool bAnyMod = Mods.ContainsByPredicate([&Kit, &Slot](const UGSTN_KitModDefinition* Mod)
		{
			return Mod && Mod->Slot == Slot && FGSTN_KitModsEditorRules::ModFitsKit(*Mod, Kit);
		});
		if (!bAnyMod)
		{
			Report.Add(EGSTN_Severity::Info, GSTNKitModsCodes::SlotsNoMod(), FText::Format(
				LOCTEXT("SlotNoMod", "No mod in the project fits the {0} slot of this kit yet"), FText::FromString(Slot.ToString())), Asset);
		}
	}
}

void FGSTN_KitModsEditorRules::CheckLoadout(const FGSTN_LoadoutData& Loadout, UObject* Asset, TConstArrayView<const UGSTN_PrimaryKitDefinition*> Kits,
	FGSTN_ValidationReport& Report)
{
	const FGSTN_LoadoutModsExt* Ext = GSTNLoadoutExtension::Find<FGSTN_LoadoutModsExt>(Loadout.Extensions);
	if (!Ext)
	{
		return;
	}
	for (const FGSTN_KitModConfig& Config : Ext->Configs)
	{
		for (const FGSTN_EquippedMod& Equipped : Config.Mods)
		{
			if (!Cast<UGSTN_KitModDefinition>(Equipped.Mod.TryLoad()))
			{
				Report.Add(EGSTN_Severity::Warning, GSTNKitModsCodes::Loadout(), FText::Format(
					LOCTEXT("LoadoutMissing", "Mod choice {0} is not a kit mod asset: the game skips it"), FText::FromString(Equipped.Mod.ToString())), Asset);
			}
		}

		// Each kit the configuration is for: what ResolveMods would skip there. A wildcard configuration is checked only on the loadout's own kit
		for (const UGSTN_PrimaryKitDefinition* Kit : Kits)
		{
			const bool bForKit = Kit && (Config.Kit.IsValid() ? Kit->KitTags.HasTagExact(Config.Kit) : Kit->GetPrimaryAssetId() == Loadout.PrimaryKit);
			if (!bForKit)
			{
				continue;
			}
			FGSTN_ResolvedLoadout Resolved;
			Resolved.Kit = Kit;
			Resolved.Loadout.PrimaryKit = Kit->GetPrimaryAssetId();
			if (const UGSTN_WeaponDefinition* Weapon = Kit->PrimaryWeapon.LoadSynchronous())
			{
				Resolved.Weapons.Add(Weapon);
			}
			FGSTN_LoadoutModsExt Single;
			Single.Configs.Add(Config);
			GSTNLoadoutExtension::Set(Resolved.Loadout.Extensions, Single);
			FGSTN_ResolvedMods Mods;
			FGSTN_KitModLoadout::ResolveMods(Resolved, Mods);
			for (const FString& Warning : Mods.Warnings)
			{
				Report.Add(EGSTN_Severity::Warning, GSTNKitModsCodes::Loadout(), FText::FromString(Warning), Asset);
			}
		}
	}
}

double FGSTN_KitModsEditorRules::EstimateReachTip(const FGSTN_CombatTimelineData& Data)
{
	double Tip = 0.0;
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Instance : Data.Tracks)
	{
		const FGSTN_TimelineTrack_Hitbox* Track = Instance.GetPtr<FGSTN_TimelineTrack_Hitbox>();
		if (!Track)
		{
			continue;
		}
		for (const FGSTN_HitboxSpec& Spec : Track->Hitboxes)
		{
			if (!UGSTN_HitReachUtils::IsReachScalable(Spec))
			{
				continue;
			}
			const FVector Axis = UGSTN_HitReachUtils::AxisVector(Spec.ReachAxis);
			const double Extent = Spec.Shape.Extent.GetAbsMax();
			if (Spec.Keys.IsEmpty())
			{
				const double Along = FMath::Abs(FVector::DotProduct(UGSTN_HitReachUtils::GetCentreInSpace(Spec, INDEX_NONE), Axis));
				Tip = FMath::Max(Tip, Along + Extent * Spec.Shape.Local.GetScale3D().GetAbsMax());
				continue;
			}
			for (int32 KeyIndex = 0; KeyIndex < Spec.Keys.Num(); ++KeyIndex)
			{
				const double Along = FMath::Abs(FVector::DotProduct(UGSTN_HitReachUtils::GetCentreInSpace(Spec, KeyIndex), Axis));
				const double Scale = Spec.Keys[KeyIndex].Scale.GetAbsMax() * Spec.Shape.Local.GetScale3D().GetAbsMax();
				Tip = FMath::Max(Tip, Along + Extent * Scale);
			}
		}
	}
	return Tip;
}

double FGSTN_KitModsEditorRules::EstimateVisualReach(const FGSTN_EquipmentVisual& Visual)
{
	double Reach = 0.0;
	for (const FGSTN_AttachmentRequest& Part : Visual.Parts)
	{
		const UObject* Mesh = Part.Mesh.LoadSynchronous();
		FBoxSphereBounds Bounds;
		if (const UStaticMesh* Static = Cast<UStaticMesh>(Mesh))
		{
			Bounds = Static->GetBounds();
		}
		else if (const USkeletalMesh* Skeletal = Cast<USkeletalMesh>(Mesh))
		{
			Bounds = Skeletal->GetBounds();
		}
		else
		{
			continue;
		}
		// The farthest point of the box from the pivot, on the box's longest axis: where the blade ends when it is attached at its pivot
		const FVector Far = Bounds.Origin.GetAbs() + Bounds.BoxExtent;
		Reach = FMath::Max(Reach, Far.GetMax() * Part.Offset.GetScale3D().GetAbsMax());
	}
	return Reach;
}

void FGSTN_KitModsEditorRules::GatherKits(TArray<const UGSTN_PrimaryKitDefinition*>& OutKits)
{
	TArray<FAssetData> Assets;
	IAssetRegistry::GetChecked().GetAssetsByClass(UGSTN_PrimaryKitDefinition::StaticClass()->GetClassPathName(), Assets, true);
	for (const FAssetData& Data : Assets)
	{
		if (const UGSTN_PrimaryKitDefinition* Kit = Cast<UGSTN_PrimaryKitDefinition>(Data.GetAsset()))
		{
			OutKits.Add(Kit);
		}
	}
}

void FGSTN_KitModsEditorRules::GatherMods(TArray<const UGSTN_KitModDefinition*>& OutMods)
{
	TArray<FAssetData> Assets;
	IAssetRegistry::GetChecked().GetAssetsByClass(UGSTN_KitModDefinition::StaticClass()->GetClassPathName(), Assets, true);
	for (const FAssetData& Data : Assets)
	{
		if (const UGSTN_KitModDefinition* Mod = Cast<UGSTN_KitModDefinition>(Data.GetAsset()))
		{
			OutMods.Add(Mod);
		}
	}
}

#undef LOCTEXT_NAMESPACE

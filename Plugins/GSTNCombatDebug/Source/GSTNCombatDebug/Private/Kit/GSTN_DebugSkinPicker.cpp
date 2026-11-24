// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#include "Kit/GSTN_DebugSkinPicker.h"

#include "Cosmetics/GSTN_SkinComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kit/GSTN_DebugKitPicker.h"
#include "Kits/GSTN_CombatAssetTypes.h"
#include "Kits/GSTN_SkinDefinition.h"

namespace
{
	/** The pawn's skin component that applies skins, else its first one, else a new one that only takes forced skins */
	UGSTN_SkinComponent* FindOrAddSkinComponent(APawn& Pawn)
	{
		TArray<UGSTN_SkinComponent*> Components;
		Pawn.GetComponents(Components);
		for (UGSTN_SkinComponent* Component : Components)
		{
			if (Component->bApplySkin)
			{
				return Component;
			}
		}
		if (Components.Num() > 0)
		{
			return Components[0];
		}
		UGSTN_SkinComponent* Added = NewObject<UGSTN_SkinComponent>(&Pawn, TEXT("GSTN_DebugSkin"));
		Added->bAutoBind = false;
		Added->bApplySkin = false;
		Added->bIncludeLoadoutVisuals = false;
		Added->RegisterComponent();
		return Added;
	}

	FAutoConsoleCommand SkinCommand(TEXT("gstn.Debug.Skin"),
		TEXT("gstn.Debug.Skin <SkinName|base|off> [all]: forces a skin on the first local player's pawn (or every pawn with \"all\"), on this machine only. base = the original look, off = back to the loadout."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			APlayerController* Controller = FGSTN_DebugKitPicker::FindPlayController();
			const bool bAll = Args.Num() > 1 && Args[1].Equals(TEXT("all"), ESearchCase::IgnoreCase);
			FString Error;
			if (Args.Num() < 1 || !Controller || !FGSTN_DebugSkinPicker::ForceSkin(*Controller, Args[0], bAll, Error))
			{
				UE_LOG(LogTemp, Warning, TEXT("gstn.Debug.Skin: %s"), Args.Num() < 1 ? TEXT("give a skin name, base or off") : !Controller ? TEXT("no play session") : *Error);
			}
		}));
}

TArray<FString> FGSTN_DebugSkinPicker::GetSkinNames()
{
	TArray<FString> Names;
	if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
	{
		TArray<FPrimaryAssetId> Ids;
		Manager->GetPrimaryAssetIdList(GSTNCombatAssetTypes::Skin(), Ids);
		for (const FPrimaryAssetId& Id : Ids)
		{
			Names.Add(Id.PrimaryAssetName.ToString());
		}
	}
	Names.Sort();
	return Names;
}

bool FGSTN_DebugSkinPicker::ForceSkin(APlayerController& Controller, const FString& SkinName, bool bAllPawns, FString& Error)
{
	const bool bOff = SkinName.Equals(TEXT("off"), ESearchCase::IgnoreCase);
	const bool bBase = SkinName.Equals(TEXT("base"), ESearchCase::IgnoreCase);
	UGSTN_SkinDefinition* Skin = nullptr;
	if (!bOff && !bBase)
	{
		UAssetManager* Manager = UAssetManager::GetIfInitialized();
		const FSoftObjectPath Path = Manager ? Manager->GetPrimaryAssetPath(FPrimaryAssetId(GSTNCombatAssetTypes::Skin(), FName(*SkinName))) : FSoftObjectPath();
		// Debug tooling: a synchronous load is fine here, the skin component itself loads the skin's content synchronously too
		Skin = Path.IsValid() ? Cast<UGSTN_SkinDefinition>(Path.TryLoad()) : nullptr;
		if (!Skin)
		{
			Error = FString::Printf(TEXT("unknown skin %s"), *SkinName);
			return false;
		}
	}

	TArray<APawn*> Pawns;
	if (bAllPawns)
	{
		if (UWorld* World = Controller.GetWorld())
		{
			for (TActorIterator<APawn> It(World); It; ++It)
			{
				Pawns.Add(*It);
			}
		}
	}
	else if (APawn* Pawn = Controller.GetPawn())
	{
		Pawns.Add(Pawn);
	}
	if (Pawns.Num() == 0)
	{
		Error = TEXT("no pawn");
		return false;
	}

	for (APawn* Pawn : Pawns)
	{
		if (bOff)
		{
			// Releasing never adds a component: a pawn without one was never forced
			TArray<UGSTN_SkinComponent*> Components;
			Pawn->GetComponents(Components);
			for (UGSTN_SkinComponent* Component : Components)
			{
				Component->ClearForcedSkin();
			}
		}
		else
		{
			FindOrAddSkinComponent(*Pawn)->SetForcedSkin(Skin);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("gstn.Debug.Skin: %s on %d pawn(s)"), *SkinName, Pawns.Num());
	return true;
}

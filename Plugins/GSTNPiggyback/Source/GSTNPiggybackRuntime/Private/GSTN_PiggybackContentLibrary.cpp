// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#include "GSTN_PiggybackContentLibrary.h"

#include "Engine/AssetManager.h"
#include "GSTN_HitStep_MountHit.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_MountTypes.h"
#include "GSTN_PairMoveDefinition.h"
#include "GSTN_PairMoveTestContent.h"
#include "GSTN_PiggybackTags.h"
#include "Kits/GSTN_CombatAssetTypes.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Hits/GSTN_HitSteps.h"
#include "Misc/PackageName.h"
#include "Rules/GSTN_CombatRules.h"

bool UGSTN_PiggybackContentLibrary::WriteMountKitData(UGSTN_PrimaryKitDefinition* Kit, float RiderWeight, float CarryCapacity)
{
	if (!Kit)
	{
		return false;
	}
	FGSTN_MountKitExtension Data;
	Data.Rider.Weight = RiderWeight;
	Data.Carrier.CarryCapacity = CarryCapacity;

	TArray<FInstancedStruct>& Entries = Kit->Extensions.ExtensionData;
	for (FInstancedStruct& Entry : Entries)
	{
		if (const FGSTN_MountKitExtension* Existing = Entry.GetPtr<FGSTN_MountKitExtension>())
		{
			if (Existing->Rider.Weight == RiderWeight && Existing->Carrier.CarryCapacity == CarryCapacity)
			{
				return true;
			}
			Kit->Modify();
			// The rest of the entry (pair moves, profiles a script wrote) stays: only the weights change
			FGSTN_MountKitExtension Changed = *Existing;
			Changed.Rider.Weight = RiderWeight;
			Changed.Carrier.CarryCapacity = CarryCapacity;
			Entry = FInstancedStruct::Make(Changed);
			return true;
		}
	}
	Kit->Modify();
	Entries.Add(FInstancedStruct::Make(Data));
	return true;
}

bool UGSTN_PiggybackContentLibrary::GetMountKitData(const UGSTN_PrimaryKitDefinition* Kit, float& RiderWeight, float& CarryCapacity)
{
	const FGSTN_MountKitExtension* Data = Kit ? Kit->Extensions.Find<FGSTN_MountKitExtension>() : nullptr;
	RiderWeight = Data ? Data->Rider.Weight : 0.f;
	CarryCapacity = Data ? Data->Carrier.CarryCapacity : 0.f;
	return Data != nullptr;
}

bool UGSTN_PiggybackContentLibrary::ScanPluginKits()
{
	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager || !FPackageName::MountPointExists(FStringView(TEXT("/GSTNPiggyback/"))))
	{
		return false;
	}
	// The same base class and flags as the project's own scan of the type, or the asset manager reports a mismatch
	AssetManager->ScanPathsForPrimaryAssets(GSTNCombatAssetTypes::PrimaryKit(), { TEXT("/GSTNPiggyback") }, UGSTN_PrimaryKitDefinition::StaticClass(), false, false, true);
	return true;
}

int32 UGSTN_PiggybackContentLibrary::GetMountTypeIndex()
{
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	return Registry ? Registry->GetTypeIndex(GSTNPiggybackTags::Type_Mount) : 0;
}

bool UGSTN_PiggybackContentLibrary::AddMountStepToRules(UGSTN_CombatRules* Rules)
{
	if (!Rules)
	{
		return false;
	}
	TArray<TSoftClassPtr<UGSTN_HitResolutionStep>>& Pipeline = Rules->HitPipeline;
	const FSoftObjectPath MountPath(UGSTN_HitStep_MountHit::StaticClass());
	const FSoftObjectPath KnockbackPath(UGSTN_HitStep_ComputeKnockback::StaticClass());
	const int32 Knockback = Pipeline.IndexOfByPredicate([&](const TSoftClassPtr<UGSTN_HitResolutionStep>& Step) { return Step.ToSoftObjectPath() == KnockbackPath; });
	if (Knockback == INDEX_NONE)
	{
		return false;
	}
	if (Pipeline.IsValidIndex(Knockback + 1) && Pipeline[Knockback + 1].ToSoftObjectPath() == MountPath
		&& Pipeline.FilterByPredicate([&](const TSoftClassPtr<UGSTN_HitResolutionStep>& Step) { return Step.ToSoftObjectPath() == MountPath; }).Num() == 1)
	{
		return true;
	}
	RemoveMountStepFromRules(Rules);
	const int32 At = Pipeline.IndexOfByPredicate([&](const TSoftClassPtr<UGSTN_HitResolutionStep>& Step) { return Step.ToSoftObjectPath() == KnockbackPath; }) + 1;
	Pipeline.Insert(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_MountHit::StaticClass()), At);
	Rules->ResolveHitPipeline();
	return true;
}

bool UGSTN_PiggybackContentLibrary::RemoveMountStepFromRules(UGSTN_CombatRules* Rules)
{
	if (!Rules)
	{
		return false;
	}
	const FSoftObjectPath MountPath(UGSTN_HitStep_MountHit::StaticClass());
	const int32 Removed = Rules->HitPipeline.RemoveAll([&](const TSoftClassPtr<UGSTN_HitResolutionStep>& Step) { return Step.ToSoftObjectPath() == MountPath; });
	if (Removed > 0)
	{
		Rules->ResolveHitPipeline();
	}
	return Removed > 0;
}

bool UGSTN_PiggybackContentLibrary::FillReferencePairMove(UGSTN_PairMoveDefinition* Definition, FGameplayTag PairTag)
{
	if (!Definition)
	{
		return false;
	}
	const bool bThrow = PairTag == GSTNPiggybackTags::PairMove_ThrowRider.GetTag();
	if (!bThrow && PairTag != GSTNPiggybackTags::PairMove_BoostJump.GetTag())
	{
		return false;
	}
	Definition->Modify();
	Definition->PairTag = PairTag;
	Definition->Outcome = EGSTN_LinkActionOutcome::Break;
	Definition->ExitMode = EGSTN_ExitMode::AtAnchor;
	Definition->PartnerMode = EGSTN_PairPartnerMode::Cooperative;
	Definition->bPartnerLocksCommands = true;
	Definition->MaxDurationFrames = 120;
	Definition->InitiatorRequirements = FGSTN_TagRequirements();
	Definition->PartnerRequirements = FGSTN_TagRequirements();
	Definition->ExitLaunch = FGSTN_PairExitLaunch();
	if (bThrow)
	{
		// Spec 6.6: the carrier throws along its forward, pitched up; the rider keeps the carrier's velocity. The test kit has no
		// Command.Special.Directional slot (spec 7): its neutral special is the trigger
		Definition->PairMoveId = 1;
		Definition->InitiatorRole = EGSTN_PairRole::Carrier;
		Definition->InitiatorAbility = UGSTN_GA_Test_CarrierThrowRider::StaticClass();
		Definition->TriggerCommand = FGameplayTag::RequestGameplayTag(TEXT("Command.Special.Neutral"), false);
		Definition->PartnerAbility = UGSTN_GA_Test_RiderThrown::StaticClass();
		Definition->PartnerStartOffsetFrames = UGSTN_GA_Test_CarrierThrowRider::ReleaseFrame - 1 - UGSTN_GA_Test_CarrierThrowRider::KeyFrame;
		Definition->OutcomeFrame = UGSTN_GA_Test_CarrierThrowRider::ReleaseFrame;
		Definition->ExitLaunch.Speed = 1400.f;
		Definition->ExitLaunch.PitchDeg = 25.f;
		Definition->ExitLaunch.InheritPartnerVelocity = 1.f;
		Definition->Credit = EGSTN_PairCredit::InitiatorAndOwner;
	}
	else
	{
		// Spec 6.6: straight up at twice a 650 cm/s jump, plus the carrier's velocity on the movement plane
		Definition->PairMoveId = 2;
		Definition->InitiatorRole = EGSTN_PairRole::Rider;
		Definition->InitiatorAbility = UGSTN_GA_Test_RiderBoostJump::StaticClass();
		Definition->TriggerCommand = FGameplayTag::RequestGameplayTag(TEXT("Command.Movement.Jump"), false);
		Definition->PartnerAbility = UGSTN_GA_Test_CarrierBraced::StaticClass();
		Definition->PartnerStartOffsetFrames = 0;
		Definition->OutcomeFrame = UGSTN_GA_Test_RiderBoostJump::LaunchFrame;
		Definition->ExitLaunch.UpSpeed = 1300.f;
		Definition->ExitLaunch.InheritPartnerVelocity = 1.f;
		Definition->ExitLaunch.bInheritHorizontalOnly = true;
		Definition->Credit = EGSTN_PairCredit::OwnerOnly;
	}
	return true;
}

bool UGSTN_PiggybackContentLibrary::WritePairMoveSet(UGSTN_PairMoveSet* Set, const TArray<UGSTN_PairMoveDefinition*>& Definitions)
{
	if (!Set)
	{
		return false;
	}
	TArray<TObjectPtr<UGSTN_PairMoveDefinition>> Moves;
	for (UGSTN_PairMoveDefinition* Definition : Definitions)
	{
		if (Definition)
		{
			Moves.Add(Definition);
		}
	}
	if (Moves != Set->PairMoves)
	{
		Set->Modify();
		Set->PairMoves = MoveTemp(Moves);
	}
	return true;
}

bool UGSTN_PiggybackContentLibrary::WriteMountKitPairMoves(UGSTN_PrimaryKitDefinition* Kit, const UGSTN_PairMoveSet* Set)
{
	if (!Kit)
	{
		return false;
	}
	const FSoftObjectPath Path(Set);
	for (FInstancedStruct& Entry : Kit->Extensions.ExtensionData)
	{
		if (FGSTN_MountKitExtension* Existing = Entry.GetMutablePtr<FGSTN_MountKitExtension>())
		{
			if (Existing->PairMoves != Path)
			{
				Kit->Modify();
				Existing->PairMoves = Path;
			}
			return true;
		}
	}
	return false;
}

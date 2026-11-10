#include "GASNPPTest/GSTN_TestStrikeKinds.h"

#include "Abilities/GameplayAbility.h"
#include "Kits/GSTN_AbilityBinding.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "StructUtils/InstancedStruct.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelineTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_TestStrikeKinds)

UGSTN_GA_TestStrikeKinds::UGSTN_GA_TestStrikeKinds(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The asset the setup script writes; until it exists the inline data (empty) runs and the move does nothing
	TimelineAsset = TSoftObjectPtr<UGSTN_CombatTimeline>(FSoftObjectPath(TEXT("/Game/GASNPPTest/Combat/TL_Test_StrikeKinds.TL_Test_StrikeKinds")));
}

namespace
{
	FGSTN_CombatTimelineData MakeStrikeKindsData()
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 40;
		Data.FirstActionableFrame = 34;
		Data.OverrideFrames = 0;
		// The move itself stays Unspecified with no recoil: each group says what it is, and nothing above the kind default answers a ricochet
		Data.bHasRicochetResponse = false;

		auto MakeHit = []()
		{
			FGSTN_HitData Hit;
			Hit.Damage = 5.f;
			Hit.BaseKnockback = 300.f;
			Hit.AngleDeg = 30.f;
			Hit.HitstopMode = EGSTN_HitstopMode::Override;
			Hit.HitstopFramesOverride = 6;
			Hit.AttackerHitstopFramesOverride = 4;
			return Hit;
		};
		auto MakeGroup = [&MakeHit](int32 Start, int32 End, uint8 Group, double Radius, const FVector& From, const FVector& To, EGSTN_StrikeKind Kind)
		{
			FGSTN_TimelineTrack_Hitbox Track;
			Track.StartFrame = Start;
			Track.EndFrame = End;
			Track.bOverrideStrike = true;
			Track.StrikeKind = Kind;
			Track.HardSurfaceBehaviour = EGSTN_HardSurfaceBehaviour::Auto;
			Track.SweepDeflection.bEnabled = Kind == EGSTN_StrikeKind::Swing;
			FGSTN_HitboxSpec Spec;
			Spec.Shape.Type = EGSTN_ShapeType::Sphere;
			Spec.Shape.Extent = FVector(Radius);
			Spec.HitGroup = Group;
			Spec.Space = EGSTN_HitboxSpace::Root;
			FGSTN_HitboxKey KeyFrom;
			KeyFrom.Frame = 0;
			KeyFrom.Offset = From;
			FGSTN_HitboxKey KeyTo;
			KeyTo.Frame = End - Start - 1;
			KeyTo.Offset = To;
			Spec.Keys = { KeyFrom, KeyTo };
			Spec.bSweepBetweenFrames = true;
			Spec.Data = MakeHit();
			Track.Hitboxes.Add(Spec);
			return TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track);
		};
		Data.Tracks.Add(MakeGroup(8, 15, 0, 40.0, FVector(40.0, 140.0, 20.0), FVector(140.0, 40.0, 20.0), EGSTN_StrikeKind::Swing));
		Data.Tracks.Add(MakeGroup(22, 27, 1, 30.0, FVector(60.0, 0.0, 20.0), FVector(170.0, 0.0, 20.0), EGSTN_StrikeKind::Stab));
		return Data;
	}
}

bool UGSTN_TestStrikeKindsLibrary::FillStrikeKindsTimeline(UGSTN_CombatTimeline* Timeline)
{
	if (!Timeline)
	{
		return false;
	}
	FGSTN_CombatTimelineData Data = MakeStrikeKindsData();
	// Compared as exported text so a rerun on an up-to-date asset changes nothing (the setup script then saves nothing)
	FString Old;
	FString New;
	FGSTN_CombatTimelineData::StaticStruct()->ExportText(Old, &Timeline->Data, nullptr, nullptr, PPF_None, nullptr);
	FGSTN_CombatTimelineData::StaticStruct()->ExportText(New, &Data, nullptr, nullptr, PPF_None, nullptr);
	if (Old == New)
	{
		return false;
	}
	Timeline->Modify();
	Timeline->Data = MoveTemp(Data);
	Timeline->MarkPackageDirty();
	return true;
}

bool UGSTN_TestStrikeKindsLibrary::BindKitCommand(UGSTN_PrimaryKitDefinition* Kit, FGameplayTag Command, TSubclassOf<UGameplayAbility> Ability)
{
	if (!Kit || !Command.IsValid() || !Ability)
	{
		return false;
	}
	const TSoftClassPtr<UGameplayAbility> Wanted(Ability.Get());
	FGSTN_AbilityBinding* Binding = Kit->KitBindings.FindByPredicate([&Command](const FGSTN_AbilityBinding& Entry) { return Entry.Command == Command; });
	if (Binding && Binding->Ability == Wanted)
	{
		return false;
	}
	Kit->Modify();
	if (!Binding)
	{
		Binding = &Kit->KitBindings.AddDefaulted_GetRef();
		Binding->Command = Command;
	}
	Binding->Ability = Wanted;
	Kit->MarkPackageDirty();
	return true;
}

bool UGSTN_TestStrikeKindsLibrary::ClearKitRicochetFallback(UGSTN_PrimaryKitDefinition* Kit)
{
	if (!Kit || !Kit->bHasRicochetFallback)
	{
		return false;
	}
	Kit->Modify();
	Kit->bHasRicochetFallback = false;
	Kit->MarkPackageDirty();
	return true;
}

FString UGSTN_TestStrikeKindsLibrary::DescribeRecoilChain(const UGSTN_PrimaryKitDefinition* Kit)
{
	if (!Kit)
	{
		return TEXT("no kit");
	}
	const UGSTN_WeaponDefinition* Weapon = Kit->PrimaryWeapon.LoadSynchronous();
	return FString::Printf(TEXT("kit fallback %s, weapon %s fallback %s"), Kit->GetRicochetFallback() ? TEXT("ON") : TEXT("off"), *GetNameSafe(Weapon),
		Weapon && Weapon->GetRicochetFallback() ? TEXT("ON (hides the kind default)") : TEXT("off"));
}

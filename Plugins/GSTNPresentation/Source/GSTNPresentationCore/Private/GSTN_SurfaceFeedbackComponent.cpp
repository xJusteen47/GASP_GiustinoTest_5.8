// Surface audio and haptics (docs/notes/surface-feedback.md).

#include "GSTN_SurfaceFeedbackComponent.h"

#include "GSTN_PresentationTags.h"
#include "GSTN_PresentationTypes.h"
#include "CollisionQueryParams.h"
#include "Components/AudioComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/ForceFeedbackEffect.h"
#include "GameFramework/ForceFeedbackParameters.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_SurfaceFeedbackComponent)

// Playtest switches (owner rule 2026-11-21: every cvar also in GSTN Debug Options, one control each). On/off helps never name a value of
// 2 or more, so the window gives them a check box; the others list their levels and get a number.
namespace GSTNSurfaceFeedbackCVars
{
	static TAutoConsoleVariable<int32> Audio(TEXT("gstn.Audio.Surface"), 1,
		TEXT("1 = footsteps and landings play the footwear x floor sound of the surface response table where a row has one (stock GASP foley elsewhere); 0 = stock GASP foley only"));
	static TAutoConsoleVariable<int32> FootwearOverride(TEXT("gstn.Audio.FootwearOverride"), -1,
		TEXT("Footwear for every character: -1 = the skin's (default), 0 = Clay (stock), 1 = Metal, 2 = Barefoot, 3 = Shoe sole"));
	static TAutoConsoleVariable<int32> LayerMode(TEXT("gstn.Audio.LayerMode"), 0,
		TEXT("Layered footstep MetaSound: 0 = both layers, 1 = footwear layer only, 2 = floor layer only"));
	static TAutoConsoleVariable<int32> Debug(TEXT("gstn.Audio.SurfaceDebug"), 0,
		TEXT("Surface feedback debug: 0 = off, 1 = print event, footwear, floor and row, 2 = also draw the floor trace"));
	static TAutoConsoleVariable<int32> Haptics(TEXT("gstn.Haptics.Enable"), 1,
		TEXT("1 = the pad rumbles on steps, landings and hits from the surface response table (local player only); 0 = no rumble"));
	static TAutoConsoleVariable<int32> HapticLevel(TEXT("gstn.Haptics.Intensity"), 2,
		TEXT("Rumble strength: 0 = off, 1 = low, 2 = normal, 3 = high"));
}

DEFINE_LOG_CATEGORY_STATIC(LogGSTNSurfaceFeedback, Log, All);

namespace
{
	// MetaSound inputs of the layered sounds (docs/notes/surface-feedback.md §MetaSound): a sound without one ignores it
	const FName ParamFootwear(TEXT("Footwear"));
	const FName ParamFloor(TEXT("Floor"));
	const FName ParamSide(TEXT("Side"));
	const FName ParamLayerMode(TEXT("LayerMode"));
	const FName ParamSpeed(TEXT("Speed"));
	const FName ParamWeight(TEXT("Weight"));
	const FName ParamIntensity(TEXT("Intensity"));
	const FName ParamFootwearGain(TEXT("FootwearGain"));
	const FName ParamFloorGain(TEXT("FloorGain"));
}

UGSTN_SurfaceFeedbackComponent::UGSTN_SurfaceFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

UGSTN_SurfaceFeedbackComponent* UGSTN_SurfaceFeedbackComponent::FindOrAdd(AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}
	if (UGSTN_SurfaceFeedbackComponent* Existing = Owner->FindComponentByClass<UGSTN_SurfaceFeedbackComponent>())
	{
		return Existing;
	}
	UGSTN_SurfaceFeedbackComponent* Added = NewObject<UGSTN_SurfaceFeedbackComponent>(Owner, TEXT("GSTN_SurfaceFeedback"));
	Added->RegisterComponent();
	return Added;
}

void UGSTN_SurfaceFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Table && !ProjectDefaultTable.IsNull())
	{
		ProjectDefaultTable.LoadSynchronous();  // here, not on the first step
	}
}

const UGSTN_SurfaceResponseTable* UGSTN_SurfaceFeedbackComponent::GetTable() const
{
	return Table ? Table.Get() : ProjectDefaultTable.Get();
}

void UGSTN_SurfaceFeedbackComponent::SetSkinFootwear(FGameplayTag Footwear)
{
	SkinFootwear = Footwear;
}

FGameplayTag UGSTN_SurfaceFeedbackComponent::GetFootwear() const
{
	const int32 Forced = GSTNSurfaceFeedbackCVars::FootwearOverride.GetValueOnGameThread();
	const TArray<FGameplayTag>& Order = UGSTN_SurfaceResponseTable::DefaultFootwearOrder();
	if (Order.IsValidIndex(Forced))
	{
		return Order[Forced];
	}
	if (FootwearOverride.IsValid())
	{
		return FootwearOverride;
	}
	return SkinFootwear.IsValid() ? SkinFootwear : FGameplayTag(GSTNPresentationTags::Footwear_Clay);
}

TEnumAsByte<EPhysicalSurface> UGSTN_SurfaceFeedbackComponent::GetFloorAt(FVector Location) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return SurfaceType_Default;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTN_SurfaceFeedbackFloor), /*bTraceComplex*/ true, GetOwner());
	Params.bReturnPhysicalMaterial = true;
	const FVector Start = Location + FVector(0.0, 0.0, TraceUp);
	const FVector End = Location - FVector(0.0, 0.0, TraceDown);
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, Params);
	if (GSTNSurfaceFeedbackCVars::Debug.GetValueOnGameThread() >= 2)
	{
		DrawDebugLine(World, Start, bHit ? Hit.ImpactPoint : End, bHit ? FColor::Green : FColor::Red, false, 1.f, 0, 1.5f);
	}
	return bHit ? UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get()) : SurfaceType_Default;
}

bool UGSTN_SurfaceFeedbackComponent::HandleFoleyEventOn(AActor* Owner, FGameplayTag FoleyEvent, FVector Location, EGSTN_FoleySide Side)
{
	UGSTN_SurfaceFeedbackComponent* Component = FindOrAdd(Owner);
	return Component ? Component->HandleFoleyEvent(FoleyEvent, Location, Side) : false;
}

bool UGSTN_SurfaceFeedbackComponent::HandleFoleyEvent(FGameplayTag FoleyEvent, FVector Location, EGSTN_FoleySide Side)
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	double& Last = LastFootTime[static_cast<int32>(Side)];
	if (Side != EGSTN_FoleySide::None && MinFootInterval > 0.f && Last >= 0.0 && Now - Last < MinFootInterval)
	{
		// The same foot again within the window (blended poses fire the same notify): the stock sound is skipped too, so the stock
		// double at a blend goes away with ours. Returned true only when the table would answer this event with a sound
		const UGSTN_SurfaceResponseTable* Asked = GetTable();
		const FGSTN_SurfaceResponseRow* Row = Asked ? Asked->FindRow(FoleyEvent, GetFootwear(), SurfaceType_Default) : nullptr;
		return Row && Row->HasSound() && GSTNSurfaceFeedbackCVars::Audio.GetValueOnGameThread() != 0;
	}
	Last = Now;
	if (Location.IsZero())
	{
		Location = Owner->GetActorLocation() - FVector(0.0, 0.0, Owner->GetSimpleCollisionHalfHeight());
	}
	return PlaySurfaceResponse(FoleyEvent, Location, Owner->GetVelocity().Size2D(), Side);
}

void UGSTN_SurfaceFeedbackComponent::HandlePresentationEvent(const FGSTN_PresentationEvent& Event)
{
	const AActor* Owner = GetOwner();
	FVector Location = Event.Location;
	if (Owner && (Location.IsZero() || Location.Equals(Owner->GetActorLocation(), 1.0)))
	{
		Location = Owner->GetActorLocation() - FVector(0.0, 0.0, Owner->GetSimpleCollisionHalfHeight());  // the floor under the feet
	}
	PlaySurfaceResponse(Event.EventTag, Location, Event.Magnitude, EGSTN_FoleySide::None);
}

bool UGSTN_SurfaceFeedbackComponent::PlaySurfaceResponse(FGameplayTag EventTag, FVector Location, float Magnitude, EGSTN_FoleySide Side)
{
	const UGSTN_SurfaceResponseTable* Asked = GetTable();
	if (!Asked || !EventTag.IsValid())
	{
		return false;
	}
	const FGameplayTag Footwear = GetFootwear();
	// The trace only when a row of this event names a floor: most steps on an unmarked level need none
	const EPhysicalSurface Floor = Asked->NeedsFloor(EventTag) || GSTNSurfaceFeedbackCVars::Debug.GetValueOnGameThread() > 0
		? GetFloorAt(Location).GetValue() : SurfaceType_Default;
	const FGSTN_SurfaceResponseRow* Row = Asked->FindRow(EventTag, Footwear, Floor);
	if (!Row || Magnitude < Row->MinMagnitude)
	{
		if (GSTNSurfaceFeedbackCVars::Debug.GetValueOnGameThread() > 0)
		{
			UE_LOG(LogGSTNSurfaceFeedback, Log, TEXT("SURF %s %s footwear=%s floor=%d mag=%.0f: no row (stock)"), *GetNameSafe(GetOwner()),
				*EventTag.ToString(), *Footwear.ToString(), static_cast<int32>(Floor), Magnitude);
		}
		return false;
	}
	const float Intensity = FMath::Max(Row->Scale.Evaluate(Magnitude), 0.f);
	return PlayRow(*Row, *Asked, Footwear, Floor, Location, Intensity, Magnitude, Side);
}

bool UGSTN_SurfaceFeedbackComponent::PlayRow(const FGSTN_SurfaceResponseRow& Row, const UGSTN_SurfaceResponseTable& Asked,
	const FGameplayTag& Footwear, EPhysicalSurface Floor, const FVector& Location, float Intensity, float Magnitude, EGSTN_FoleySide Side)
{
	UWorld* World = GetWorld();
	const bool bPresent = World && GetNetMode() != NM_DedicatedServer;
	bool bPlayedSound = false;
	if (bPresent && Row.HasSound() && GSTNSurfaceFeedbackCVars::Audio.GetValueOnGameThread() != 0)
	{
		// A component made by hand, not SpawnSoundAtLocation: that one plays at once, and a MetaSound reads its inputs at play
		UAudioComponent* Audio = NewObject<UAudioComponent>(GetOwner());
		Audio->bAutoActivate = false;
		Audio->bAutoDestroy = true;
		Audio->SetSound(Row.Sound);
		Audio->SetWorldLocation(Location);
		if (Row.Attenuation)
		{
			Audio->AttenuationSettings = Row.Attenuation;
		}
		if (Row.Concurrency)
		{
			Audio->ConcurrencySet.Add(Row.Concurrency);
		}
		Audio->RegisterComponent();
		const float Volume = Row.Volume * (Row.bScaleVolume ? Intensity : 1.f);
		Audio->SetVolumeMultiplier(Volume);
		Audio->SetPitchMultiplier(FMath::FRandRange(static_cast<float>(Row.PitchRange.X), static_cast<float>(Row.PitchRange.Y)));
		const int32 Mode = FMath::Clamp(GSTNSurfaceFeedbackCVars::LayerMode.GetValueOnGameThread(), 0, 2);
		Audio->SetIntParameter(ParamFootwear, Asked.GetFootwearParam(Footwear));
		Audio->SetIntParameter(ParamFloor, static_cast<int32>(Floor));
		Audio->SetIntParameter(ParamSide, static_cast<int32>(Side));
		Audio->SetIntParameter(ParamLayerMode, Mode);
		Audio->SetFloatParameter(ParamSpeed, Magnitude);
		Audio->SetFloatParameter(ParamWeight, Weight);
		Audio->SetFloatParameter(ParamIntensity, Intensity);
		Audio->SetFloatParameter(ParamFootwearGain, Mode == 2 ? 0.f : Row.FootwearLayerGain);
		Audio->SetFloatParameter(ParamFloorGain, Mode == 1 ? 0.f : Row.FloorLayerGain);
		Audio->Play();
		bPlayedSound = true;
	}
	if (bPresent && Row.HasHaptic())
	{
		PlayHaptic(Row, Intensity, Side);
	}
	if (GSTNSurfaceFeedbackCVars::Debug.GetValueOnGameThread() > 0)
	{
		const FString Line = FString::Printf(TEXT("SURF %s %s footwear=%s floor=%d mag=%.0f int=%.2f sound=%s rumble=%.2f"),
			*GetNameSafe(GetOwner()), *Row.EventTag.ToString(), *Footwear.ToString(), static_cast<int32>(Floor), Magnitude, Intensity,
			bPlayedSound ? *GetNameSafe(Row.Sound) : TEXT("-"), Row.HapticIntensity);
		UE_LOG(LogGSTNSurfaceFeedback, Log, TEXT("%s"), *Line);
		if (GEngine && bPresent)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, Line);
		}
	}
	OnSurfaceResponse.Broadcast(Row.EventTag, Footwear, Floor, Intensity, bPlayedSound);
	return bPlayedSound;
}

float UGSTN_SurfaceFeedbackComponent::HapticLevelScale(int32 Level)
{
	static const float Scales[] = {0.f, 0.5f, 1.f, 1.5f};
	return Scales[FMath::Clamp(Level, 0, 3)];
}

APlayerController* UGSTN_SurfaceFeedbackComponent::GetLocalController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return nullptr;  // a simulated proxy or an AI never rumbles the local pad
	}
	return Cast<APlayerController>(Pawn->GetController());
}

void UGSTN_SurfaceFeedbackComponent::PlayHaptic(const FGSTN_SurfaceResponseRow& Row, float Intensity, EGSTN_FoleySide Side) const
{
	if (GSTNSurfaceFeedbackCVars::Haptics.GetValueOnGameThread() == 0)
	{
		return;
	}
	const float LevelScale = HapticLevelScale(GSTNSurfaceFeedbackCVars::HapticLevel.GetValueOnGameThread());
	APlayerController* Controller = LevelScale > 0.f ? GetLocalController() : nullptr;
	if (!Controller)
	{
		return;
	}
	if (Row.HapticIntensity > 0.f && Row.HapticDuration > 0.f)
	{
		const float Strength = FMath::Clamp(Row.HapticIntensity * (Row.bScaleHaptic ? Intensity : 1.f) * LevelScale, 0.f, 1.f);
		const bool bLarge = Row.HapticMotors != EGSTN_HapticMotors::Small;
		const bool bSmall = Row.HapticMotors != EGSTN_HapticMotors::Large;
		const bool bLeft = !Row.bHapticFollowsFoot || Side != EGSTN_FoleySide::Right;
		const bool bRight = !Row.bHapticFollowsFoot || Side != EGSTN_FoleySide::Left;
		// Native overload (not the latent Blueprint node): a fire-and-forget pulse that stops by itself after the duration
		Controller->PlayDynamicForceFeedback(Strength, Row.HapticDuration, bLeft && bLarge, bLeft && bSmall, bRight && bLarge, bRight && bSmall);
	}
	if (Row.HapticEffect)
	{
		FForceFeedbackParameters Params;
		Params.Tag = Row.EventTag.GetTagName();  // the same event replaces its previous rumble instead of stacking
		Controller->ClientPlayForceFeedback(Row.HapticEffect, Params);
	}
}

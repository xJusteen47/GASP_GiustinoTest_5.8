#include "GASNPPTest/GSTN_PlaytestCamera.h"

#include "Camera/GSTN_CameraAnchor.h"
#include "GameFramework/Pawn.h"

#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace GSTNPlaytestCamera
{
	static TAutoConsoleVariable<float> CVarExtraDistance(TEXT("gstn.Camera.PlaytestExtraDistance"), 0.f,
		TEXT("Extra camera distance (cm) behind the play view, pushed back along the view's forward. 0 = off. Presentation only"));
	static TAutoConsoleVariable<float> CVarExtraHeight(TEXT("gstn.Camera.PlaytestExtraHeight"), 0.f,
		TEXT("Extra camera height (cm) on top of gstn.Camera.PlaytestExtraDistance"));
	static constexpr float ProbeRadius = 12.f;
	/** Shared camera anchor tuning, optional (C++ defaults when missing). Created by Tools/GASNPPTest/camera_anchor_setup.py */
	static const TCHAR* AnchorSettingsPath = TEXT("/Game/Blueprints/Data/DA_CameraAnchor_Default.DA_CameraAnchor_Default");
}

FVector UGSTN_PlaytestCameraModifier::PushBack(const FVector& ViewLocation, const FVector& ViewForward, float ExtraDistance, float ExtraHeight)
{
	return ViewLocation - ViewForward.GetSafeNormal() * FMath::Max(ExtraDistance, 0.f) + FVector::UpVector * ExtraHeight;
}

bool UGSTN_PlaytestCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);
	// Camera anchor in ShiftView mode (or no camera component found): the view moves by the anchor's correction first
	const AActor* ViewTarget = CameraOwner ? CameraOwner->GetViewTarget() : nullptr;
	if (const UGSTN_CameraAnchorComponent* Anchor = ViewTarget ? ViewTarget->FindComponentByClass<UGSTN_CameraAnchorComponent>() : nullptr)
	{
		InOutPOV.Location += Anchor->GetViewShift();
	}
	const float Extra = GSTNPlaytestCamera::CVarExtraDistance.GetValueOnGameThread();
	const float Height = GSTNPlaytestCamera::CVarExtraHeight.GetValueOnGameThread();
	if (Extra <= 0.f && FMath::IsNearlyZero(Height))
	{
		return false;
	}
	const FVector Start = InOutPOV.Location;
	FVector End = PushBack(Start, InOutPOV.Rotation.Vector(), Extra, Height);
	// Stop short of a wall: the sweep starts at the GASP view, which its own rig already keeps clear
	UWorld* World = CameraOwner ? CameraOwner->GetWorld() : nullptr;
	if (World)
	{
		FCollisionQueryParams Query(SCENE_QUERY_STAT(GSTNPlaytestCamera), false, CameraOwner->GetViewTarget());
		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(GSTNPlaytestCamera::ProbeRadius), Query))
		{
			End = Hit.Location;
		}
	}
	InOutPOV.Location = End;
	return false;
}

bool UGSTN_PlaytestCameraSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer;
}

void UGSTN_PlaytestCameraSubsystem::Tick(float DeltaTime)
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		APlayerCameraManager* Manager = Controller && Controller->IsLocalController() ? Controller->PlayerCameraManager.Get() : nullptr;
		if (Manager && !Manager->FindCameraModifierByClass(UGSTN_PlaytestCameraModifier::StaticClass()))
		{
			Manager->AddNewCameraModifier(UGSTN_PlaytestCameraModifier::StaticClass());
		}
		// The viewed pawn gets a camera anchor unless its Blueprint placed one (per character settings)
		APawn* Viewed = Controller && Controller->IsLocalController() ? Cast<APawn>(Controller->GetViewTarget()) : nullptr;
		if (Viewed && !Viewed->FindComponentByClass<UGSTN_CameraAnchorComponent>())
		{
			if (!bAnchorSettingsLoaded)
			{
				bAnchorSettingsLoaded = true;
				AnchorSettings = LoadObject<UGSTN_CameraAnchorSettingsAsset>(nullptr, GSTNPlaytestCamera::AnchorSettingsPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
			}
			UGSTN_CameraAnchorComponent* Anchor = NewObject<UGSTN_CameraAnchorComponent>(Viewed, TEXT("GSTN_CameraAnchor"), RF_Transient);
			Anchor->SettingsAsset = AnchorSettings;
			Anchor->RegisterComponent();
		}
	}
}

TStatId UGSTN_PlaytestCameraSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGSTN_PlaytestCameraSubsystem, STATGROUP_Tickables);
}

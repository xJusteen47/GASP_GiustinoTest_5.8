#include "GASNPPTest/GSTN_PlaytestCamera.h"

#include "Camera/GSTN_CameraAnchor.h"
#include "GameFramework/Pawn.h"

#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace GSTNPlaytestCamera
{
	static TAutoConsoleVariable<float> CVarExtraDistance(TEXT("gstn.Camera.PlaytestExtraDistance"), -1.f,
		TEXT("Extra camera distance (cm) behind the play view, pushed back along the view's forward. -1 = the camera settings' ExtraDistance "
			 "(DA_CameraAnchor_Default), 0 = off. Presentation only"));
	static TAutoConsoleVariable<float> CVarExtraHeight(TEXT("gstn.Camera.PlaytestExtraHeight"), -1.f,
		TEXT("Extra camera height (cm) on top of the extra distance. -1 = the camera settings' ExtraHeight"));
	static TAutoConsoleVariable<int32> CVarTiltShift(TEXT("gstn.Camera.TiltShift"), -1,
		TEXT("Tilt-shift toy look: -1 = camera settings (DA_CameraAnchor_Default TiltShift), 0 = off, 1 = Cinematic DOF, 2 = cheap screen band"));
	static TAutoConsoleVariable<float> CVarTiltShiftIntensity(TEXT("gstn.Camera.TiltShift.Intensity"), -1.f,
		TEXT("Tilt-shift intensity override (-1 = camera settings)"));
	static TAutoConsoleVariable<int32> CVarTrace(TEXT("gstn.Camera.Trace"), 0,
		TEXT("1 = write one CSV line per rendered frame to Saved/Logs/gstn_camera_trace.csv: frame time, the view GASP's rig produced, the anchor's ")
		TEXT("values, the push back sweep and the final view. Diagnostics for camera jitter (2026-11-20), debug only"));
	static constexpr float ProbeRadius = 12.f;
	/** Shared camera anchor tuning, optional (C++ defaults when missing). Created by Tools/GASNPPTest/camera_anchor_setup.py */
	static const TCHAR* AnchorSettingsPath = TEXT("/Game/Blueprints/Data/DA_CameraAnchor_Default.DA_CameraAnchor_Default");
}

FVector UGSTN_PlaytestCameraModifier::PushBack(const FVector& ViewLocation, const FVector& ViewForward, float ExtraDistance, float ExtraHeight)
{
	return ViewLocation - ViewForward.GetSafeNormal() * FMath::Max(ExtraDistance, 0.f) + FVector::UpVector * ExtraHeight;
}

float UGSTN_PlaytestCameraModifier::ScreenV(const FMinimalViewInfo& View, const FVector& WorldPoint)
{
	const FVector Local = View.Rotation.UnrotateVector(WorldPoint - View.Location);  // X forward, Y right, Z up
	const float Aspect = View.AspectRatio > KINDA_SMALL_NUMBER ? View.AspectRatio : 16.f / 9.f;
	const float TanHalfVertical = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(View.FOV, 1.f, 170.f)) * 0.5f) / Aspect;
	if (Local.X <= KINDA_SMALL_NUMBER || TanHalfVertical <= KINDA_SMALL_NUMBER)
	{
		return 0.5f;
	}
	return 0.5f - 0.5f * float(Local.Z / Local.X) / TanHalfVertical;
}

float UGSTN_PlaytestCameraModifier::TiltShiftFstop(const FGSTN_CameraTiltShiftSettings& TiltShift)
{
	return FMath::Max(TiltShift.Fstop / FMath::Max(TiltShift.Intensity, 0.01f), 0.7f);
}

void UGSTN_PlaytestCameraModifier::ApplyTiltShift(const FGSTN_CameraTiltShiftSettings& InTiltShift, const FMinimalViewInfo& POV, const FVector& Focus)
{
	FGSTN_CameraTiltShiftSettings TiltShift = InTiltShift;
	const int32 Forced = GSTNPlaytestCamera::CVarTiltShift.GetValueOnGameThread();
	if (Forced >= 0)
	{
		TiltShift.bEnabled = Forced > 0;
		TiltShift.Mode = Forced == 2 ? EGSTN_TiltShiftMode::ScreenBand : EGSTN_TiltShiftMode::CinematicDOF;
	}
	const float ForcedIntensity = GSTNPlaytestCamera::CVarTiltShiftIntensity.GetValueOnGameThread();
	if (ForcedIntensity >= 0.f)
	{
		TiltShift.Intensity = ForcedIntensity;
	}
	if (!TiltShift.bEnabled || TiltShift.Intensity <= KINDA_SMALL_NUMBER || !CameraOwner)
	{
		return;
	}

	// The cheap band: needs its material (made by Tools/GASNPPTest/camera_tiltshift_setup.py); without it, falls back to the DOF
	if (TiltShift.Mode == EGSTN_TiltShiftMode::ScreenBand)
	{
		if (!bBandMaterialTried)
		{
			bBandMaterialTried = true;
			if (UMaterialInterface* Material = TiltShift.BandMaterial.LoadSynchronous())
			{
				BandMaterial = UMaterialInstanceDynamic::Create(Material, this);
			}
		}
		if (BandMaterial)
		{
			BandMaterial->SetScalarParameterValue(TEXT("BandCenter"), FMath::Clamp(ScreenV(POV, Focus), 0.f, 1.f));
			BandMaterial->SetScalarParameterValue(TEXT("BandHalfHeight"), TiltShift.BandHalfHeight);
			BandMaterial->SetScalarParameterValue(TEXT("BandFeather"), TiltShift.BandFeather);
			BandMaterial->SetScalarParameterValue(TEXT("Radius"), TiltShift.BandMaxRadius * TiltShift.Intensity);
			FPostProcessSettings Settings;
			Settings.AddBlendable(BandMaterial.Get(), 1.f);
			CameraOwner->AddCachedPPBlend(Settings, 1.f);
			return;
		}
	}

	// Cinematic DOF focused on the character: a big virtual sensor at the same field of view = a long lens = a thin sharp slice (the
	// macro-lens look that makes the world read as a model). The ground in front and the background blur, more the farther from the slice
	FPostProcessSettings Settings;
	Settings.bOverride_DepthOfFieldFocalDistance = true;
	Settings.DepthOfFieldFocalDistance = FMath::Max(float(FVector::Dist(POV.Location, Focus)) + TiltShift.FocusOffset, 10.f);
	Settings.bOverride_DepthOfFieldFstop = true;
	Settings.DepthOfFieldFstop = TiltShiftFstop(TiltShift);
	Settings.bOverride_DepthOfFieldMinFstop = true;
	Settings.DepthOfFieldMinFstop = FMath::Min(Settings.DepthOfFieldFstop, 1.2f);
	Settings.bOverride_DepthOfFieldSensorWidth = true;
	Settings.DepthOfFieldSensorWidth = TiltShift.SensorWidth;
	CameraOwner->AddCachedPPBlend(Settings, 1.f);
}

bool UGSTN_PlaytestCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);
	const FMinimalViewInfo TraceIn = InOutPOV;
	bool bSweepHit = false;
	// Camera anchor in ShiftView mode (or no camera component found): the view moves by the anchor's correction first
	const AActor* ViewTarget = CameraOwner ? CameraOwner->GetViewTarget() : nullptr;
	const UGSTN_CameraAnchorComponent* Anchor = ViewTarget ? ViewTarget->FindComponentByClass<UGSTN_CameraAnchorComponent>() : nullptr;
	if (Anchor)
	{
		InOutPOV.Location += Anchor->GetViewShift();
	}
	// Framing and tilt shift come from the camera settings (C++ defaults when the pawn has no anchor yet); console values >= 0 override
	static const FGSTN_CameraAnchorSettings DefaultSettings;
	const FGSTN_CameraAnchorSettings& Tuning = Anchor ? Anchor->GetSettings() : DefaultSettings;
	const float ForcedExtra = GSTNPlaytestCamera::CVarExtraDistance.GetValueOnGameThread();
	const float ForcedHeight = GSTNPlaytestCamera::CVarExtraHeight.GetValueOnGameThread();
	const float Extra = ForcedExtra >= 0.f ? ForcedExtra : Tuning.ExtraDistance;
	const float Height = ForcedHeight >= 0.f ? ForcedHeight : Tuning.ExtraHeight;
	if (Extra > 0.f || !FMath::IsNearlyZero(Height))
	{
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
				bSweepHit = true;
			}
		}
		InOutPOV.Location = End;
	}
	// Focus on the character: the damped anchor (where the camera aims) when it runs, else the view target
	if (ViewTarget)
	{
		const bool bAnchorRuns = Anchor && Anchor->IsActiveForOwner();
		ApplyTiltShift(Tuning.TiltShift, InOutPOV, bAnchorRuns ? Anchor->GetDampedAnchor() : ViewTarget->GetActorLocation());
	}
	if (GSTNPlaytestCamera::CVarTrace.GetValueOnGameThread() != 0)
	{
		Trace(DeltaTime, TraceIn, InOutPOV, bSweepHit, Anchor);
	}
	else if (TraceLines.Num() > 0)
	{
		Trace(DeltaTime, TraceIn, InOutPOV, bSweepHit, nullptr);  // flushes the tail
	}
	return false;
}

void UGSTN_PlaytestCameraModifier::Trace(float DeltaTime, const FMinimalViewInfo& In, const FMinimalViewInfo& Out, bool bSweepHit, const UGSTN_CameraAnchorComponent* Anchor)
{
	// One file per PIE instance (3 players in one process would interleave their rows and fake zig-zags): _0 = listen host
	const UWorld* TraceWorld = CameraOwner ? CameraOwner->GetWorld() : nullptr;
	const int32 Instance = TraceWorld && TraceWorld->GetOutermost() ? FMath::Max(0, TraceWorld->GetOutermost()->GetPIEInstanceID()) : 0;
	const FString Path = FPaths::ProjectLogDir() / (Instance == 0 ? FString(TEXT("gstn_camera_trace.csv")) : FString::Printf(TEXT("gstn_camera_trace_%d.csv"), Instance));
	const bool bOn = GSTNPlaytestCamera::CVarTrace.GetValueOnGameThread() != 0;
	if (bOn)
	{
		if (!bTraceHeaderWritten)
		{
			bTraceHeaderWritten = true;
			FFileHelper::SaveStringToFile(TEXT("frame,time,dt,in_x,in_y,in_z,in_pitch,in_yaw,out_x,out_y,out_z,out_pitch,out_yaw,sweep_hit,")
				TEXT("anchor_on,in_air,root_x,root_y,root_z,stand_x,stand_y,stand_z,damped_z,corr_x,corr_y,corr_z,cam_comp_x,cam_comp_y,cam_comp_z\n"), *Path);
		}
		const AActor* Target = CameraOwner ? CameraOwner->GetViewTarget() : nullptr;
		const FVector Root = Target ? Target->GetActorLocation() : FVector::ZeroVector;
		const bool bAnchorOn = Anchor && Anchor->IsActiveForOwner();
		const FVector Stand = Anchor ? Anchor->GetStandingRoot() : FVector::ZeroVector;
		const FVector Corr = Anchor ? Anchor->GetLastCorrection() : FVector::ZeroVector;
		const USceneComponent* Moved = Anchor ? Anchor->GetMovedCameraComponent() : nullptr;
		const FVector CamComp = Moved ? Moved->GetComponentLocation() : FVector::ZeroVector;
		const double Now = CameraOwner && CameraOwner->GetWorld() ? CameraOwner->GetWorld()->GetTimeSeconds() : 0.0;
		TraceLines.Add(FString::Printf(TEXT("%llu,%.4f,%.4f,%.2f,%.2f,%.2f,%.3f,%.3f,%.2f,%.2f,%.2f,%.3f,%.3f,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f"),
			(unsigned long long)GFrameCounter, Now, DeltaTime, In.Location.X, In.Location.Y, In.Location.Z, In.Rotation.Pitch, In.Rotation.Yaw,
			Out.Location.X, Out.Location.Y, Out.Location.Z, Out.Rotation.Pitch, Out.Rotation.Yaw, bSweepHit ? 1 : 0, bAnchorOn ? 1 : 0,
			Anchor && Anchor->WasInAirLastTick() ? 1 : 0, Root.X, Root.Y, Root.Z, Stand.X, Stand.Y, Stand.Z, Anchor ? Anchor->GetDampedAnchor().Z : 0.0,
			Corr.X, Corr.Y, Corr.Z, CamComp.X, CamComp.Y, CamComp.Z));
	}
	if (TraceLines.Num() >= 60 || (!bOn && TraceLines.Num() > 0))
	{
		FFileHelper::SaveStringToFile(FString::Join(TraceLines, TEXT("\n")) + TEXT("\n"), *Path, FFileHelper::EEncodingOptions::AutoDetect,
			&IFileManager::Get(), FILEWRITE_Append);
		TraceLines.Reset();
	}
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

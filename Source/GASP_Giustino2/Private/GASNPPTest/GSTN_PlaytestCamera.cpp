#include "GASNPPTest/GSTN_PlaytestCamera.h"

#include "Camera/GSTN_CameraAnchor.h"
#include "GameFramework/Pawn.h"
#include "GSTN_CameraComponent.h"
#include "GSTN_CameraProfile.h"
#include "GSTN_CameraRuntimeTypes.h"
#include "MoverTypes.h"

#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Components/CapsuleComponent.h"
#include "GameplayTagContainer.h"
#include "MoverComponent.h"
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
	static TAutoConsoleVariable<int32> CVarTiltShift(TEXT("gstn.Camera.TiltShift"), 0,
		TEXT("Tilt (focus, blur) toy look, default 0 = off (owner 2026-11-21: no blur). -1 = camera settings (DA_CameraAnchor_Default TiltShift), 0 = off, 1 = Cinematic DOF, 2 = cheap screen band, 3 = tilted plane"));
	static TAutoConsoleVariable<int32> CVarShift(TEXT("gstn.Camera.Shift"), -1,
		TEXT("Lens shift (off-axis framing, independent from the tilt): -1 = camera settings (DA_CameraAnchor_Default Shift), 0 = off, 1 = on"));
	static TAutoConsoleVariable<float> CVarShiftStraighten(TEXT("gstn.Camera.Shift.Straighten"), -1.f,
		TEXT("Lens shift: share of the camera pitch turned into shift, 0..1 (1 = camera level, verticals parallel). -1 = camera settings (Shift.StraightenVerticals, 0.5)"));
	static TAutoConsoleVariable<float> CVarTiltShiftIntensity(TEXT("gstn.Camera.TiltShift.Intensity"), -1.f,
		TEXT("Tilt-shift intensity override (-1 = camera settings)"));
	static TAutoConsoleVariable<int32> CVarTrace(TEXT("gstn.Camera.Trace"), 0,
		TEXT("1 = write one CSV line per rendered frame to Saved/Logs/gstn_camera_trace.csv: frame time, the view GASP's rig produced, the anchor's ")
		TEXT("values, the push back sweep and the final view. Diagnostics for camera jitter (2026-11-20), debug only"));
	static TAutoConsoleVariable<int32> CVarP1a(TEXT("gstn.Camera.P1a"), 1,
		TEXT("1 = the playtest view is the new GSTN free platform camera (plugin GSTNCamera, P1a; scripted run PASS, stick to be tried by hand), 0 = GASP's own camera. Playtest switch"));
	static TAutoConsoleVariable<int32> CVarP1aLookFeed(TEXT("gstn.Camera.P1a.LookFeed"), 1,
		TEXT("1 = GASP's look input (the control rotation change since the P1a camera's own write) steers its orbit, 0 = no look input (diagnostics)"));
	static constexpr float ProbeRadius = 12.f;
	/** Shared camera anchor tuning, optional (C++ defaults when missing). Created by Tools/GASNPPTest/camera_anchor_setup.py */
	static const TCHAR* AnchorSettingsPath = TEXT("/Game/GSTN/Data/Camera/DA_CameraAnchor_Default.DA_CameraAnchor_Default");

	/** The presented subject of a Mover pawn for the P1a camera: the anchor's capsule-free standing root (undamped: the camera damps), else the actor */
	static FGSTN_CameraSubjectState MakeSubject(const APawn& Viewed, const UGSTN_CameraAnchorComponent* Anchor)
	{
		FGSTN_CameraSubjectState Subject;
		Subject.bValid = true;
		Subject.Pivot = Anchor && Anchor->IsActiveForOwner() ? Anchor->GetStandingRoot() : Viewed.GetActorLocation();
		Subject.Velocity = Viewed.GetVelocity();
		const FVector Flat(Subject.Velocity.X, Subject.Velocity.Y, 0.0);
		Subject.Facing = Flat.SizeSquared() > 25.0 ? Flat.GetSafeNormal() : Viewed.GetActorForwardVector();
		const UMoverComponent* Mover = Viewed.FindComponentByClass<UMoverComponent>();
		Subject.bGrounded = Mover ? Mover->HasGameplayTag(Mover_IsOnGround, true) : FMath::Abs(Subject.Velocity.Z) < 30.0;
		return Subject;
	}
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

float UGSTN_PlaytestCameraModifier::TelephotoDistanceScale(float FromFOV, float ToFOV)
{
	const float From = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(FromFOV, 1.f, 170.f)) * 0.5f);
	const float To = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(ToFOV, 1.f, 170.f)) * 0.5f);
	return From / To;
}

float UGSTN_PlaytestCameraModifier::FOVForDistanceScale(float FromFOV, float DistanceScale)
{
	const float From = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(FromFOV, 1.f, 170.f)) * 0.5f);
	return FMath::RadiansToDegrees(2.f * FMath::Atan(From / FMath::Max(DistanceScale, 0.01f)));
}

FVector2D UGSTN_PlaytestCameraModifier::ProjectNDC(const FMinimalViewInfo& View, const FVector& WorldPoint)
{
	// UE's perspective with the horizontal FOV kept: x_ndc = (right / forward) / tan(FOV/2), y_ndc = (up / forward) * aspect / tan(FOV/2), then
	// FMinimalViewInfo's off-center projection subtracts its offset (projection matrix M[2][0] = -Offset.X, M[2][1] = -Offset.Y)
	const FVector Local = View.Rotation.UnrotateVector(WorldPoint - View.Location);  // X forward, Y right, Z up
	const float Aspect = View.AspectRatio > KINDA_SMALL_NUMBER ? View.AspectRatio : 16.f / 9.f;
	const float TanHalf = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(View.FOV, 1.f, 170.f)) * 0.5f);
	const double Forward = FMath::Max(Local.X, 1.0);
	return FVector2D(Local.Y / Forward / TanHalf, Local.Z / Forward * Aspect / TanHalf) - FVector2D(View.OffCenterProjectionOffset.X, View.OffCenterProjectionOffset.Y);
}

FVector2D UGSTN_PlaytestCameraModifier::StraightenWithShift(FMinimalViewInfo& InOutView, const FVector& WorldPoint, float Straighten)
{
	const FVector2D Before = ProjectNDC(InOutView, WorldPoint);
	FMinimalViewInfo Levelled = InOutView;
	Levelled.Rotation.Pitch *= 1.f - FMath::Clamp(Straighten, 0.f, 1.f);
	Levelled.OffCenterProjectionOffset = decltype(Levelled.OffCenterProjectionOffset)(0, 0);
	// ndc = projected - offset: the offset that keeps the point where it was is the difference of the two projections
	const FVector2D After = ProjectNDC(Levelled, WorldPoint);
	InOutView.Rotation = Levelled.Rotation;
	return After - Before;
}

FVector UGSTN_PlaytestCameraModifier::TiltedPlaneNormal(const FRotator& ViewRotation, float TiltAngle)
{
	// Plane parallel to the sensor: normal = forward. Tilting it by A around the right axis moves its "up" to Up cos A + Forward sin A (top away
	// from the camera for A > 0), so its normal becomes Forward cos A - Up sin A
	const FRotationMatrix Axes(ViewRotation);
	const FVector Forward = Axes.GetUnitAxis(EAxis::X);
	const FVector Up = Axes.GetUnitAxis(EAxis::Z);
	const float A = FMath::DegreesToRadians(TiltAngle);
	return (Forward * FMath::Cos(A) - Up * FMath::Sin(A)).GetSafeNormal();
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
		TiltShift.Mode = Forced == 3 ? EGSTN_TiltShiftMode::TiltedPlane : (Forced == 2 ? EGSTN_TiltShiftMode::ScreenBand : EGSTN_TiltShiftMode::CinematicDOF);
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

	// The real tilt: focal plane through the character, rotated around the camera's right axis (Scheimpflug); blur from each pixel's distance
	// to it. Camera relative vectors: the material works in camera relative space (no large world coordinates in the shader)
	if (TiltShift.Mode == EGSTN_TiltShiftMode::TiltedPlane)
	{
		if (!bPlaneMaterialTried)
		{
			bPlaneMaterialTried = true;
			if (UMaterialInterface* Material = TiltShift.PlaneMaterial.LoadSynchronous())
			{
				PlaneMaterial = UMaterialInstanceDynamic::Create(Material, this);
			}
		}
		if (PlaneMaterial)
		{
			const FVector Point = Focus - POV.Location;
			const FVector Normal = TiltedPlaneNormal(POV.Rotation, TiltShift.TiltAngle);
			PlaneMaterial->SetVectorParameterValue(TEXT("PlanePoint"), FLinearColor(Point.X, Point.Y, Point.Z, 0.f));
			PlaneMaterial->SetVectorParameterValue(TEXT("PlaneNormal"), FLinearColor(Normal.X, Normal.Y, Normal.Z, 0.f));
			PlaneMaterial->SetScalarParameterValue(TEXT("BlurScale"), TiltShift.PlaneBlurScale * TiltShift.Intensity);
			PlaneMaterial->SetScalarParameterValue(TEXT("Radius"), TiltShift.PlaneMaxRadius * TiltShift.Intensity);
			FPostProcessSettings Settings;
			Settings.AddBlendable(PlaneMaterial.Get(), 1.f);
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
	// P1a: the GSTN camera component of the viewed pawn replaces GASP's view (its distance is its own, so the push back is skipped)
	UGSTN_CameraComponent* P1a = nullptr;
	if (CameraOwner && GSTNPlaytestCamera::CVarP1a.GetValueOnGameThread() != 0)
	{
		const AActor* Target = CameraOwner->GetViewTarget();
		P1a = Target ? Target->FindComponentByClass<UGSTN_CameraComponent>() : nullptr;
		if (P1a && P1a->IsActive())
		{
			P1a->GetCameraView(DeltaTime, InOutPOV);
		}
		else
		{
			P1a = nullptr;
		}
	}
	// Camera anchor in ShiftView mode (or no camera component found): the view moves by the anchor's correction first
	const AActor* ViewTarget = CameraOwner ? CameraOwner->GetViewTarget() : nullptr;
	const UGSTN_CameraAnchorComponent* Anchor = ViewTarget ? ViewTarget->FindComponentByClass<UGSTN_CameraAnchorComponent>() : nullptr;
	if (Anchor && !P1a)
	{
		InOutPOV.Location += Anchor->GetViewShift();
	}
	// Framing and tilt shift come from the camera settings (C++ defaults when the pawn has no anchor yet); console values >= 0 override
	static const FGSTN_CameraAnchorSettings DefaultSettings;
	const FGSTN_CameraAnchorSettings& Tuning = Anchor ? Anchor->GetSettings() : DefaultSettings;
	const float ForcedExtra = GSTNPlaytestCamera::CVarExtraDistance.GetValueOnGameThread();
	const float ForcedHeight = GSTNPlaytestCamera::CVarExtraHeight.GetValueOnGameThread();
	const float Extra = P1a ? 0.f : (ForcedExtra >= 0.f ? ForcedExtra : Tuning.ExtraDistance);
	const float Height = P1a ? 0.f : (ForcedHeight >= 0.f ? ForcedHeight : Tuning.ExtraHeight);
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
	if (!ViewTarget)
	{
		return false;
	}
	// Focus on the character: the damped anchor (where the camera aims) when it runs, else the view target
	const bool bAnchorRuns = Anchor && Anchor->IsActiveForOwner();
	const FVector Focus = bAnchorRuns ? Anchor->GetDampedAnchor() : ViewTarget->GetActorLocation();

	// Telephoto framing: scaling the camera position about the character by k and the FOV's tangent by 1/k keeps the character at the same
	// size and place on screen (exact for the focus point); only the perspective flattens. Not with P1a: its profile owns distance and FOV
	if (!P1a && Tuning.TelephotoFOV > 1.f && Tuning.TelephotoFOV < InOutPOV.FOV)
	{
		const float Scale = TelephotoDistanceScale(InOutPOV.FOV, Tuning.TelephotoFOV);
		const FVector Start = InOutPOV.Location;
		FVector End = Focus + (Start - Focus) * Scale;
		float FOV = Tuning.TelephotoFOV;
		if (UWorld* World = CameraOwner ? CameraOwner->GetWorld() : nullptr)
		{
			FCollisionQueryParams Query(SCENE_QUERY_STAT(GSTNPlaytestCameraTele), false, ViewTarget);
			FHitResult Hit;
			if (World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(GSTNPlaytestCamera::ProbeRadius), Query))
			{
				// Stopped short by a wall: open the FOV to the scale actually reached, so the character keeps its size
				End = Hit.Location;
				const double Base = FVector::Dist(Start, Focus);
				const float Reached = Base > 1.0 ? float(FVector::Dist(End, Focus) / Base) : 1.f;
				FOV = FOVForDistanceScale(InOutPOV.FOV, FMath::Max(Reached, 1.f));
			}
		}
		InOutPOV.Location = End;
		InOutPOV.FOV = FOV;
	}
	// Shift (independent from the tilt): level part of the pitch and slide the lens so the character stays put on screen, plus a manual offset
	const int32 ForcedShift = GSTNPlaytestCamera::CVarShift.GetValueOnGameThread();
	const bool bShift = ForcedShift >= 0 ? ForcedShift > 0 : Tuning.Shift.bEnabled;
	if (bShift)
	{
		// Works on P1a's view as well: it only turns part of the final pitch into lens shift, P1a's orbit and distance are untouched
		const float ForcedStraighten = GSTNPlaytestCamera::CVarShiftStraighten.GetValueOnGameThread();
		const float Straighten = ForcedStraighten >= 0.f ? ForcedStraighten : Tuning.Shift.StraightenVerticals;
		FVector2D Offset = StraightenWithShift(InOutPOV, Focus, Straighten) + Tuning.Shift.Offset;
		Offset.X = FMath::Clamp(Offset.X, -1.5, 1.5);
		Offset.Y = FMath::Clamp(Offset.Y, -1.5, 1.5);
		using FOffset = decltype(InOutPOV.OffCenterProjectionOffset);
		InOutPOV.OffCenterProjectionOffset = FOffset(InOutPOV.OffCenterProjectionOffset.X + Offset.X, InOutPOV.OffCenterProjectionOffset.Y + Offset.Y);
	}
	ApplyTiltShift(Tuning.TiltShift, InOutPOV, Focus);
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
				TEXT("anchor_on,in_air,root_x,root_y,root_z,stand_x,stand_y,stand_z,damped_z,corr_x,corr_y,corr_z,cam_comp_x,cam_comp_y,cam_comp_z,mesh_stand_z,half_height,crouch\n"), *Path);
		}
		const AActor* Target = CameraOwner ? CameraOwner->GetViewTarget() : nullptr;
		const FVector Root = Target ? Target->GetActorLocation() : FVector::ZeroVector;
		const bool bAnchorOn = Anchor && Anchor->IsActiveForOwner();
		const FVector Stand = Anchor ? Anchor->GetStandingRoot() : FVector::ZeroVector;
		const FVector Corr = Anchor ? Anchor->GetLastCorrection() : FVector::ZeroVector;
		const USceneComponent* Moved = Anchor ? Anchor->GetMovedCameraComponent() : nullptr;
		const FVector CamComp = Moved ? Moved->GetComponentLocation() : FVector::ZeroVector;
		const double Now = CameraOwner && CameraOwner->GetWorld() ? CameraOwner->GetWorld()->GetTimeSeconds() : 0.0;
		// Capsule columns: a crouch, aerial or landing capsule change shows in half_height; mesh_stand_z is the first version's (DrawnMesh) height
		const UCapsuleComponent* Capsule = Target ? Cast<UCapsuleComponent>(Target->GetRootComponent()) : nullptr;
		const UMoverComponent* TargetMover = Target ? Target->FindComponentByClass<UMoverComponent>() : nullptr;
		static const FGameplayTag CrouchTag = FGameplayTag::RequestGameplayTag(TEXT("Mover.IsCrouching"), false);
		const bool bCrouch = TargetMover && CrouchTag.IsValid() && TargetMover->HasGameplayTag(CrouchTag, true);
		TraceLines.Add(FString::Printf(TEXT("%llu,%.4f,%.4f,%.2f,%.2f,%.2f,%.3f,%.3f,%.2f,%.2f,%.2f,%.3f,%.3f,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d"),
			(unsigned long long)GFrameCounter, Now, DeltaTime, In.Location.X, In.Location.Y, In.Location.Z, In.Rotation.Pitch, In.Rotation.Yaw,
			Out.Location.X, Out.Location.Y, Out.Location.Z, Out.Rotation.Pitch, Out.Rotation.Yaw, bSweepHit ? 1 : 0, bAnchorOn ? 1 : 0,
			Anchor && Anchor->WasInAirLastTick() ? 1 : 0, Root.X, Root.Y, Root.Z, Stand.X, Stand.Y, Stand.Z, Anchor ? Anchor->GetDampedAnchor().Z : 0.0,
			Corr.X, Corr.Y, Corr.Z, CamComp.X, CamComp.Y, CamComp.Z, Anchor ? Anchor->GetMeshStandingRoot().Z : 0.0,
			Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.0, bCrouch ? 1 : 0));
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
		// P1a: the viewed pawn gets the GSTN free camera; GASP's look input (control rotation change) steers its orbit
		if (Viewed && Controller->IsLocalController())
		{
			UGSTN_CameraComponent* Cam = Viewed->FindComponentByClass<UGSTN_CameraComponent>();
			const bool bWanted = GSTNPlaytestCamera::CVarP1a.GetValueOnGameThread() != 0;
			if (bWanted && !Cam && Viewed->GetRootComponent())
			{
				Cam = NewObject<UGSTN_CameraComponent>(Viewed, TEXT("GSTN_P1aCamera"), RF_Transient);
				Cam->SetupAttachment(Viewed->GetRootComponent());
				Cam->bAutoActivate = false;
				// Own copy of the code default profile (never the shared default object): GASP's look already turns the controller, so the camera
				// reads the control rotation change (LookInputSource ControlRotation) inside its own frame, before its write back
				Cam->Profile = NewObject<UGSTN_CameraProfile>(Cam, TEXT("GSTN_P1aProfile"), RF_Transient);
				Cam->RegisterComponent();
			}
			if (Cam)
			{
				if (bWanted && !Cam->IsActive())
				{
					Cam->Activate();
				}
				else if (!bWanted && Cam->IsActive())
				{
					Cam->Deactivate();
				}
				if (bWanted && Cam->IsActive())
				{
					// The Mover pawn has no adapter in the camera plugin: hand it the presented subject (anchor's capsule-free standing root). Here only
					// until the anchor runs (first frames, anchor off): the anchor's own update hands it over the same frame, before the camera evaluates
					UGSTN_CameraAnchorComponent* Anchor = Viewed->FindComponentByClass<UGSTN_CameraAnchorComponent>();
					if (Anchor && SubjectAnchor.Get() != Anchor)
					{
						if (UGSTN_CameraAnchorComponent* Old = SubjectAnchor.Get())
						{
							Old->OnAnchorUpdated.RemoveAll(this);
						}
						Anchor->OnAnchorUpdated.AddUObject(this, &UGSTN_PlaytestCameraSubsystem::OnSubjectAnchorUpdated);
						SubjectAnchor = Anchor;
					}
					// The camera evaluates in its own tick: after the anchor (whatever order the components were made in), when the anchor ticks before the view
					if (Anchor && Anchor->PrimaryComponentTick.TickGroup <= UGSTN_CameraAnchorComponent::EarlyTickGroup)
					{
						Cam->PrimaryComponentTick.AddPrerequisite(Anchor, Anchor->PrimaryComponentTick);
					}
					if (!Anchor || !Anchor->IsActiveForOwner())
					{
						Cam->SetSubjectOverride(GSTNPlaytestCamera::MakeSubject(*Viewed, nullptr));
					}
				}
				// Look input (2026-11-21 fix of the pitch lock): the camera reads the control rotation change itself, BEFORE it writes the control
				// rotation back. The first wiring measured it here, after the write back, so the camera's own motion came back as input every frame
				// (an integrator: the pitch ran to the Free clamp, -70 on the ground, +20 after a jump) and the stick was lost
				if (bWanted && Cam->IsActive() && (!Cam->Profile || Cam->Profile->GetOuter() != Cam))
				{
					// A component the character Blueprint already has: give it an own copy of its profile (or of the code default)
					Cam->Profile = Cam->Profile ? DuplicateObject<UGSTN_CameraProfile>(Cam->Profile, Cam) : NewObject<UGSTN_CameraProfile>(Cam, TEXT("GSTN_P1aProfile"), RF_Transient);
				}
				if (Cam->Profile && Cam->Profile->GetOuter() == Cam)
				{
					Cam->Profile->LookInputSource = GSTNPlaytestCamera::CVarP1aLookFeed.GetValueOnGameThread() != 0
						? EGSTN_CameraLookInputSource::ControlRotation : EGSTN_CameraLookInputSource::Plugin;
				}
			}
		}
	}
}

void UGSTN_PlaytestCameraSubsystem::OnSubjectAnchorUpdated(const UGSTN_CameraAnchorComponent& Anchor)
{
	APawn* Viewed = Cast<APawn>(Anchor.GetOwner());
	UGSTN_CameraComponent* Cam = Viewed ? Viewed->FindComponentByClass<UGSTN_CameraComponent>() : nullptr;
	if (Cam && Cam->IsActive() && GSTNPlaytestCamera::CVarP1a.GetValueOnGameThread() != 0)
	{
		Cam->SetSubjectOverride(GSTNPlaytestCamera::MakeSubject(*Viewed, &Anchor));
	}
}

TStatId UGSTN_PlaytestCameraSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGSTN_PlaytestCameraSubsystem, STATGROUP_Tickables);
}

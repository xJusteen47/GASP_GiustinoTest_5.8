// Offscreen world snapshots of the toy depth marker (GSTN.Visual.DepthMarker.Toy*): a floor, a wall, capsule "characters" carrying the real marker
// component, rendered by a scene capture with the real project materials, saved to Saved/Notes/ui-snapshots/depth-marker-<name>.png.
// They need a real RHI (-RenderOffscreen -AllowCommandletRendering); under -nullrhi, or without the toy style asset, they pass without drawing.
// The snapshots are for LOOKING at (iterate on the look); the assertions only check that the pipeline produced a marker.

#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GSTN_DepthMarkerComponent.h"
#include "GSTN_DepthMarkerSettings.h"
#include "GSTN_DepthMarkerStyle.h"
#include "GSTN_DepthMarkerSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AssetCompilingManager.h"
#include "ImageUtils.h"
#include "ShaderCompiler.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "RenderingThread.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNDepthMarkerVisualTest
{
	constexpr EAutomationTestFlags VisualFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	static const FLinearColor Red(0.9f, 0.1f, 0.08f);
	static const FLinearColor Blue(0.12f, 0.38f, 1.f);
	static const FLinearColor Yellow(1.f, 0.82f, 0.05f);
	static const FLinearColor Green(0.15f, 0.8f, 0.22f);

	struct FVisualScene
	{
		UWorld* World = nullptr;
		UGSTN_DepthMarkerSubsystem* Subsystem = nullptr;
		UGSTN_DepthMarkerStyle* Style = nullptr;
		UMaterialInterface* GridMaterial = nullptr;
		TArray<ACharacter*> Characters;

		FVisualScene()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNDepthMarkerVisualWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Subsystem = UGSTN_DepthMarkerSubsystem::Get(World);
			Style = UGSTN_DepthMarkerSettings::Get().ToyStyle.LoadSynchronous();
			GridMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
			// A key light with shadows plus soft fills from every side, so walls and bodies read whichever way the camera looks
			struct FLightSpec { FRotator Rotation; float Intensity; bool bShadows; };
			for (const FLightSpec& Spec : { FLightSpec{ FRotator(-48.0, 25.0, 0.0), 6.f, true }, FLightSpec{ FRotator(-25.0, 180.0, 0.0), 2.5f, false },
				FLightSpec{ FRotator(-25.0, 90.0, 0.0), 2.f, false }, FLightSpec{ FRotator(-25.0, 270.0, 0.0), 2.f, false },
				FLightSpec{ FRotator(-25.0, 0.0, 0.0), 2.f, false } })
			{
				ADirectionalLight* Light = World->SpawnActor<ADirectionalLight>(FVector::ZeroVector, Spec.Rotation);
				Light->SetMobility(EComponentMobility::Movable);
				Light->GetLightComponent()->SetIntensity(Spec.Intensity);
				Light->GetLightComponent()->SetCastShadows(Spec.bShadows);
				Light->SetActorRotation(Spec.Rotation);
				UE_LOG(LogTemp, Display, TEXT("GSTN_VISUAL light forward %s intensity %.1f"), *Light->GetActorForwardVector().ToString(), Light->GetLightComponent()->Intensity);
			}
		}

		~FVisualScene()
		{
			FlushRenderingCommands();
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		bool IsUsable() const { return World && Subsystem && Style && !GUsingNullRHI; }

		AStaticMeshActor* Box(const FVector& Centre, const FVector& HalfExtent)
		{
			AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Centre, FRotator::ZeroRotator);
			UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Mesh->SetWorldScale3D(HalfExtent / 50.0);
			Mesh->SetCollisionProfileName(TEXT("BlockAll"));
			if (GridMaterial)
			{
				Mesh->SetMaterial(0, GridMaterial);
			}
			// Static: a platform that may move does not update the last grounded point (the plane of the air markers)
			Mesh->SetMobility(EComponentMobility::Static);
			return Actor;
		}

		/** A capsule character (the real adapter reads it) with a grey body and the real marker component; Falling or Walking, no gravity */
		ACharacter* Character(const FVector& Feet, const FLinearColor& Tint, bool bFalling = true, UMaterialInterface* BodyOverride = nullptr)
		{
			ACharacter* Actor = World->SpawnActor<ACharacter>(Feet + FVector(0, 0, 88.0), FRotator::ZeroRotator);
			Actor->GetCharacterMovement()->GravityScale = 0.f;
			Actor->GetCharacterMovement()->SetMovementMode(bFalling ? MOVE_Falling : MOVE_Walking);
			UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(Actor, TEXT("Body"));
			Body->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
			Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (BodyOverride)
			{
				Body->SetMaterial(0, BodyOverride);
			}
			else if (UMaterialInterface* Debug = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
			{
				UMaterialInstanceDynamic* BodyMaterial = UMaterialInstanceDynamic::Create(Debug, Actor);
				BodyMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.2f, 0.2f, 0.24f));
				Body->SetMaterial(0, BodyMaterial);
			}
			Body->SetupAttachment(Actor->GetCapsuleComponent());
			Body->SetRelativeScale3D(FVector(0.6, 0.6, 1.7));
			Body->RegisterComponent();
			UGSTN_DepthMarkerComponent* Marker = NewObject<UGSTN_DepthMarkerComponent>(Actor);
			Marker->Style = Style;
			Marker->Audience = EGSTN_DepthAudience::All;
			Marker->bOverrideTint = true;
			Marker->TintOverride = Tint;
			Marker->RegisterComponent();
			Subsystem->RegisterMarker(Marker);
			Characters.Add(Actor);
			return Actor;
		}

		void Place(ACharacter* Actor, const FVector& Feet) const
		{
			Actor->SetActorLocation(Feet + FVector(0, 0, 88.0), false, nullptr, ETeleportType::TeleportPhysics);
		}

		void Tick(int32 Frames)
		{
			for (int32 Frame = 0; Frame < Frames; ++Frame)
			{
				World->Tick(LEVELTICK_All, 1.f / 30.f);
			}
		}

		/** Logs the mesh components the presenters made (the holder actor): visibility, transform and the custom primitive data */
		void DumpPresenterMeshes() const
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->IsA<ACharacter>() || It->IsA<AStaticMeshActor>() || It->IsA<ADirectionalLight>())
				{
					continue;
				}
				TArray<UStaticMeshComponent*> Meshes;
				It->GetComponents(Meshes);
				for (const UStaticMeshComponent* Mesh : Meshes)
				{
					FString Data;
					for (const float Value : Mesh->GetCustomPrimitiveData().Data)
					{
						Data += FString::Printf(TEXT("%.2f "), Value);
					}
					UE_LOG(LogTemp, Display, TEXT("GSTN_VISUAL mesh %s visible=%d scale=%s loc=%s cpd=%s"), *GetNameSafe(Mesh->GetStaticMesh()), Mesh->IsVisible() ? 1 : 0,
						*Mesh->GetComponentScale().ToString(), *Mesh->GetComponentLocation().ToString(), *Data);
				}
			}
		}

		UGSTN_DepthMarkerComponent* MarkerOf(const ACharacter* Actor) const { return Actor->FindComponentByClass<UGSTN_DepthMarkerComponent>(); }

		/** Renders the scene from CamLocation towards LookAt and saves Saved/Notes/ui-snapshots/depth-marker-<Name>.png */
		FString Snapshot(const FString& Name, const FVector& CamLocation, const FVector& LookAt, float Fov, int32 Width = 1280, int32 Height = 720)
		{
			UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
			Target->AddToRoot();
			Target->RenderTargetFormat = RTF_RGBA8;
			Target->ClearColor = FLinearColor(0.02f, 0.02f, 0.03f, 1.f);
			Target->InitAutoFormat(Width, Height);
			Target->UpdateResourceImmediate(true);

			ASceneCapture2D* Capture = World->SpawnActor<ASceneCapture2D>(CamLocation, (LookAt - CamLocation).Rotation());
			USceneCaptureComponent2D* Component = Capture->GetCaptureComponent2D();
			Component->TextureTarget = Target;
			Component->CaptureSource = SCS_FinalColorLDR;
			Component->FOVAngle = Fov;
			Component->bCaptureEveryFrame = false;
			Component->bCaptureOnMovement = false;
			Component->bAlwaysPersistRenderingState = true;
			FPostProcessSettings& Post = Component->PostProcessSettings;
			// Fixed exposure at 1.0 (no eye adaptation, no physical camera): what the materials output is what the snapshot shows
			Post.bOverride_AutoExposureMethod = true;
			Post.AutoExposureMethod = AEM_Histogram;
			Post.bOverride_AutoExposureMinBrightness = true;
			Post.AutoExposureMinBrightness = 1.0f;
			Post.bOverride_AutoExposureMaxBrightness = true;
			Post.AutoExposureMaxBrightness = 1.0f;
			Post.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
			Post.AutoExposureApplyPhysicalCameraExposure = false;
			Post.bOverride_DynamicGlobalIlluminationMethod = true;
			Post.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
			Post.bOverride_ReflectionMethod = true;
			Post.ReflectionMethod = EReflectionMethod::None;
			Post.bOverride_AmbientCubemapIntensity = true;
			Post.AmbientCubemapIntensity = 1.f;
			Component->PostProcessBlendWeight = 1.f;
			Component->MarkRenderStateDirty();

			// A material that is still compiling draws as the default material (or not at all): wait for every shader before the first capture
			if (GShaderCompilingManager)
			{
				GShaderCompilingManager->FinishAllCompilation();
			}
			FAssetCompilingManager::Get().FinishAllCompilation();
			// Two captures: the first lets the render state and the materials settle
			Component->CaptureScene();
			FlushRenderingCommands();
			// Materials compile when they are first drawn: wait for them, then draw again
			if (GShaderCompilingManager)
			{
				GShaderCompilingManager->FinishAllCompilation();
			}
			FAssetCompilingManager::Get().FinishAllCompilation();
			Tick(2);
			Component->CaptureScene();
			FlushRenderingCommands();

			TArray<FColor> Pixels;
			FString Path;
			if (Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels))
			{
				int32 Brightest = 0;
				for (FColor& Pixel : Pixels)
				{
					Brightest = FMath::Max(Brightest, FMath::Max3<int32>(Pixel.R, Pixel.G, Pixel.B));
					Pixel.A = 255;
				}
				UE_LOG(LogTemp, Display, TEXT("GSTN_VISUAL %s: scene %s, brightest channel %d"), *Name, World->Scene ? TEXT("yes") : TEXT("NO"), Brightest);
				TArray64<uint8> Png;
				FImageUtils::PNGCompressImageArray(Width, Height, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
				Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Notes/ui-snapshots") / FString::Printf(TEXT("depth-marker-%s.png"), *Name));
				FFileHelper::SaveArrayToFile(Png, *Path);
			}
			Capture->Destroy();
			Target->RemoveFromRoot();
			return Path;
		}
	};
}

#define GSTN_DMV_TEST(Name) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNDepthMarkerVisualTest_##Name, "GSTN.Visual.DepthMarker." #Name, GSTNDepthMarkerVisualTest::VisualFlags) \
	bool FGSTNDepthMarkerVisualTest_##Name::RunTest(const FString& Parameters)

using namespace GSTNDepthMarkerVisualTest;

/** Five heights over a plain floor, seen from a raised side camera: the spot is the landing, the beam a hint, nothing under the minimum height */
GSTN_DMV_TEST(ToyHeights)
{
	FVisualScene Scene;
	if (!Scene.IsUsable())
	{
		AddInfo(TEXT("Skipped: needs a real RHI and the toy style asset"));
		return true;
	}
	Scene.Box(FVector(0, 0, -5), FVector(1600, 900, 5));
	const float Heights[] = { 20.f, 45.f, 70.f, 120.f, 220.f };
	const FLinearColor Tints[] = { GSTNDepthMarkerVisualTest::Red, GSTNDepthMarkerVisualTest::Blue, GSTNDepthMarkerVisualTest::Yellow, GSTNDepthMarkerVisualTest::Green, GSTNDepthMarkerVisualTest::Red };
	for (int32 Index = 0; Index < 5; ++Index)
	{
		Scene.Character(FVector(0, -400.0 + 200.0 * Index, Heights[Index]), Tints[Index]);
	}
	Scene.Tick(20);
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const FGSTN_DepthResult& Result = Scene.MarkerOf(Scene.Characters[Index])->GetLastResult();
		AddInfo(FString::Printf(TEXT("height %.0f: target %d, height %.1f"), Heights[Index], static_cast<int32>(Result.Below.Kind), Result.Below.Height));
		if (Heights[Index] > 30.f)
		{
			TestTrue(TEXT("a marker target exists under a character in the air"), Result.Below.IsValid());
		}
	}
	const FString Path = Scene.Snapshot(TEXT("toy-heights"), FVector(-900, -150, 330), FVector(0, 0, 120), 55.f);
	AddInfo(Path);
	TestFalse(TEXT("snapshot written"), Path.IsEmpty());
	return true;
}

/** Characters hugging a wall at several heights, one free standing: the beam hides next to geometry, the spot stays */
GSTN_DMV_TEST(ToyWall)
{
	FVisualScene Scene;
	if (!Scene.IsUsable())
	{
		AddInfo(TEXT("Skipped: needs a real RHI and the toy style asset"));
		return true;
	}
	Scene.Box(FVector(0, 0, -5), FVector(1600, 900, 5));
	Scene.Box(FVector(38, 0, 300), FVector(40, 900, 300));
	Scene.Character(FVector(-36, -450, 160), GSTNDepthMarkerVisualTest::Red);
	Scene.Character(FVector(-36, -150, 100), GSTNDepthMarkerVisualTest::Blue);
	Scene.Character(FVector(-140, 150, 160), GSTNDepthMarkerVisualTest::Yellow);
	Scene.Character(FVector(-36, 450, 260), GSTNDepthMarkerVisualTest::Green);
	Scene.Tick(30);
	const FString Path = Scene.Snapshot(TEXT("toy-wall"), FVector(-800, -50, 300), FVector(-20, 0, 120), 60.f);
	AddInfo(Path);
	TestFalse(TEXT("snapshot written"), Path.IsEmpty());
	return true;
}

/** A character that fell off a platform (the air marker: the beam goes UP to the platform plane and ends in the fog puff, same landing size); one still over the platform */
GSTN_DMV_TEST(ToyAir)
{
	FVisualScene Scene;
	if (!Scene.IsUsable())
	{
		AddInfo(TEXT("Skipped: needs a real RHI and the toy style asset"));
		return true;
	}
	Scene.Box(FVector(0, 0, -20), FVector(300, 300, 20));
	ACharacter* Over = Scene.Character(FVector(0, -150, 200), GSTNDepthMarkerVisualTest::Red);
	ACharacter* Off = Scene.Character(FVector(0, 150, 3), GSTNDepthMarkerVisualTest::Blue, false);
	Scene.Tick(15);
	Off->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	Scene.Place(Off, FVector(0, 450, -280));
	Scene.Tick(25);
	for (ACharacter* Character : { Over, Off })
	{
		const FGSTN_DepthResult& Result = Scene.MarkerOf(Character)->GetLastResult();
		AddInfo(FString::Printf(TEXT("target kind %d state %d height %.1f"), static_cast<int32>(Result.Below.Kind), static_cast<int32>(Result.Below.State), Result.Below.Height));
	}
	const FString Path = Scene.Snapshot(TEXT("toy-air"), FVector(-1100, 300, 120), FVector(0, 300, -40), 60.f);
	AddInfo(Path);
	TestFalse(TEXT("snapshot written"), Path.IsEmpty());
	return true;
}

/** Close up, three heights: the spot shape, the ring, the soft shadow and the beam that never leaves the spot */
GSTN_DMV_TEST(ToyCloseup)
{
	FVisualScene Scene;
	if (!Scene.IsUsable())
	{
		AddInfo(TEXT("Skipped: needs a real RHI and the toy style asset"));
		return true;
	}
	Scene.Box(FVector(0, 0, -5), FVector(1200, 900, 5));
	Scene.Character(FVector(0, -150, 60), GSTNDepthMarkerVisualTest::Red);
	Scene.Character(FVector(0, 0, 110), GSTNDepthMarkerVisualTest::Blue);
	Scene.Character(FVector(0, 150, 420), GSTNDepthMarkerVisualTest::Yellow);
	Scene.Tick(20);
	const FString Path = Scene.Snapshot(TEXT("toy-closeup"), FVector(-520, 0, 300), FVector(0, 0, 90), 52.f);
	AddInfo(Path);
	TestFalse(TEXT("snapshot written"), Path.IsEmpty());
	return true;
}

/** The crack glow: a naked crystal body and a plain shell with the glow layer, each grounded and in the air (the glow follows the beam fade) */
GSTN_DMV_TEST(ToyCrystal)
{
	FVisualScene Scene;
	if (!Scene.IsUsable())
	{
		AddInfo(TEXT("Skipped: needs a real RHI and the toy style asset"));
		return true;
	}
	UMaterialInterface* Crystal = UGSTN_DepthMarkerSettings::Get().CrystalPreviewMaterial.LoadSynchronous();
	UMaterialInterface* Shell = UGSTN_DepthMarkerSettings::Get().CrackShellPreviewMaterial.LoadSynchronous();
	if (!Crystal || !Shell)
	{
		AddInfo(TEXT("Skipped: the crystal preview materials are not generated"));
		return true;
	}
	Scene.Box(FVector(0, 0, -5), FVector(1200, 900, 5));
	Scene.Character(FVector(0, -330, 3), GSTNDepthMarkerVisualTest::Red, false, Crystal);
	Scene.Character(FVector(0, -110, 240), GSTNDepthMarkerVisualTest::Red, true, Crystal);
	Scene.Character(FVector(0, 110, 3), GSTNDepthMarkerVisualTest::Blue, false, Shell);
	Scene.Character(FVector(0, 330, 240), GSTNDepthMarkerVisualTest::Blue, true, Shell);
	Scene.Tick(30);
	const FString Path = Scene.Snapshot(TEXT("toy-crystal"), FVector(-1000, 0, 230), FVector(0, 0, 190), 55.f);
	AddInfo(Path);
	TestFalse(TEXT("snapshot written"), Path.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

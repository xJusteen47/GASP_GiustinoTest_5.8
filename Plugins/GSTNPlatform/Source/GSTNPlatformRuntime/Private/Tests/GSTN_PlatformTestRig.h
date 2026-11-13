// Test helpers of the platform runtime tests (not shipped: compiled only with the dev automation tests).

#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GSTN_EnemyPlatformComponent.h"
#include "GSTN_PlatformSubsystem.h"
#include "Misc/AutomationTest.h"

namespace GSTNPlatformTest
{
	constexpr EAutomationTestFlags PlatformTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A throwaway game world with the platform subsystem; nothing ticks, the tests call the pose point themselves */
	struct FRig
	{
		UWorld* World = nullptr;

		FRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNPlatformTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FRig()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		UGSTN_PlatformSubsystem* Subsystem() const { return World->GetSubsystem<UGSTN_PlatformSubsystem>(); }

		/** An actor with a root at Location and a platform component of class TPlatform carrying Spec */
		template <typename TPlatform = UGSTN_EnemyPlatformComponent>
		TPlatform* SpawnPlatform(const FGSTN_PlatformSpec& Spec, const FVector& Location = FVector::ZeroVector)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
			Root->SetMobility(EComponentMobility::Movable);
			Actor->SetRootComponent(Root);
			Root->RegisterComponent();
			Actor->SetActorLocation(Location);
			TPlatform* Platform = NewObject<TPlatform>(Actor, TEXT("Platform"));
			Platform->Spec = Spec;
			Platform->RegisterComponent();
			return Platform;
		}
	};

	inline FGSTN_PlatformShape MakeShape(uint8 Id, EGSTN_PlatformShapeType Type = EGSTN_PlatformShapeType::Box,
		const FVector& Extent = FVector(50.0, 50.0, 10.0), const FVector& LocalLocation = FVector::ZeroVector)
	{
		FGSTN_PlatformShape Shape;
		Shape.ShapeId = Id;
		Shape.Type = Type;
		Shape.Extent = Extent;
		Shape.LocalLocation = LocalLocation;
		return Shape;
	}

	inline FGSTN_PlatformPoseContext MakeContext(int32 Frame, float StepMs = 16.f)
	{
		FGSTN_PlatformPoseContext Context;
		Context.Frame = Frame;
		Context.StepMs = StepMs;
		Context.TimeMs = double(Frame + 1) * StepMs;
		Context.bFromSimulation = true;
		return Context;
	}
}

#endif

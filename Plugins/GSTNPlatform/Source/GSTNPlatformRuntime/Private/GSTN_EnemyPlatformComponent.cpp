// Platform runtime: components, colliders, the pose point (Mover, no GAS, no direct Network Prediction code).

#include "GSTN_EnemyPlatformComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GSTN_PlatformColliders.h"
#include "GSTN_PlatformDefinition.h"
#include "GSTN_PlatformSettings.h"
#include "GSTN_PlatformSubsystem.h"
#include "GSTNPlatformCoreModule.h"
#include "Misc/DataValidation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_EnemyPlatformComponent)

#define LOCTEXT_NAMESPACE "GSTNEnemyPlatform"

UGSTN_EnemyPlatformComponent::UGSTN_EnemyPlatformComponent()
{
	// Ticks only for the presentation fallback (no simulation runs the pose point); before physics, like any kinematic mover
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

const FGSTN_PlatformSpec& UGSTN_EnemyPlatformComponent::GetSpec() const
{
	return Definition ? Definition->Spec : Spec;
}

FTransform UGSTN_EnemyPlatformComponent::ComputeOwnerPose_Implementation(const FGSTN_PlatformPoseContext& Context) const
{
	const AActor* Owner = GetOwner();
	return Owner ? FTransform(Owner->GetActorQuat(), Owner->GetActorLocation()) : FTransform::Identity;
}

void UGSTN_EnemyPlatformComponent::OnRegister()
{
	Super::OnRegister();

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	if (Colliders.Num() == 0)
	{
		CreateColliders();
	}
	if (UGSTN_PlatformSubsystem* Subsystem = World->GetSubsystem<UGSTN_PlatformSubsystem>())
	{
		Subsystem->RegisterPlatform(this);
	}
	ApplyPresentationPose();
}

void UGSTN_EnemyPlatformComponent::OnUnregister()
{
	if (UWorld* World = GetWorld())
	{
		if (UGSTN_PlatformSubsystem* Subsystem = World->GetSubsystem<UGSTN_PlatformSubsystem>())
		{
			Subsystem->UnregisterPlatform(this);
		}
	}
	DestroyColliders();
	Super::OnUnregister();
}

void UGSTN_EnemyPlatformComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UWorld* World = GetWorld();
	const UGSTN_PlatformSubsystem* Subsystem = World ? World->GetSubsystem<UGSTN_PlatformSubsystem>() : nullptr;
	if (!Subsystem || !Subsystem->HasRecentSimulationPose())
	{
		ApplyPresentationPose();
	}
}

void UGSTN_EnemyPlatformComponent::CreateColliders()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	DestroyColliders();
	Colliders.SetNum(GSTNPlatform::MaxShapes);

	const FTransform OwnerPose(Owner->GetActorQuat(), Owner->GetActorLocation());
	for (const FGSTN_PlatformShape& Shape : GetSpec().Shapes)
	{
		if (Shape.ShapeId >= GSTNPlatform::MaxShapes || Colliders[Shape.ShapeId])
		{
			continue;   // the validator reports it; the first shape with an id wins
		}
		const FName Name = GSTNPlatform::GetShapeComponentName(Shape.ShapeId);
		if (StaticFindObjectFast(UObject::StaticClass(), Owner, Name))
		{
			UE_LOG(LogGSTNPlatform, Error, TEXT("%s: the owner already has an object named %s, shape %d is not created"), *GetPathName(), *Name.ToString(), Shape.ShapeId);
			continue;
		}

		UPrimitiveComponent* Collider = nullptr;
		switch (Shape.Type)
		{
		case EGSTN_PlatformShapeType::Sphere:
			{
				UGSTN_PlatformSphereCollider* Sphere = NewObject<UGSTN_PlatformSphereCollider>(Owner, Name, RF_Transient);
				Sphere->Binding.Platform = this;
				Sphere->Binding.ShapeId = Shape.ShapeId;
				Sphere->InitSphereRadius(Shape.Extent.X);
				Collider = Sphere;
			}
			break;
		case EGSTN_PlatformShapeType::Capsule:
			{
				UGSTN_PlatformCapsuleCollider* Capsule = NewObject<UGSTN_PlatformCapsuleCollider>(Owner, Name, RF_Transient);
				Capsule->Binding.Platform = this;
				Capsule->Binding.ShapeId = Shape.ShapeId;
				Capsule->InitCapsuleSize(Shape.Extent.X, Shape.Extent.Z);
				Collider = Capsule;
			}
			break;
		default:
			{
				UGSTN_PlatformBoxCollider* Box = NewObject<UGSTN_PlatformBoxCollider>(Owner, Name, RF_Transient);
				Box->Binding.Platform = this;
				Box->Binding.ShapeId = Shape.ShapeId;
				Box->InitBoxExtent(Shape.Extent);
				Collider = Box;
			}
			break;
		}

		// Unattached and absolute: the pose point is the only thing that moves it, never the owner's tick
		Collider->SetUsingAbsoluteLocation(true);
		Collider->SetUsingAbsoluteRotation(true);
		Collider->SetUsingAbsoluteScale(true);
		Collider->SetMobility(EComponentMobility::Movable);
		Collider->SetSimulatePhysics(false);
		Collider->SetEnableGravity(false);
		Collider->SetCanEverAffectNavigation(false);
		Collider->SetGenerateOverlapEvents(false);
		Collider->SetHiddenInGame(true);
		Collider->CanCharacterStepUpOn = ECB_Yes;
		ConfigureCollision(*Collider, Shape);
		// Mover writes the movement base into its sync state as an object reference: the client must resolve it to its own collider
		Collider->SetNetAddressable();
		Collider->SetWorldTransform(Shape.ComposeWorld(OwnerPose));
		Collider->RegisterComponent();
		Colliders[Shape.ShapeId] = Collider;
	}
}

void UGSTN_EnemyPlatformComponent::DestroyColliders()
{
	for (TObjectPtr<UPrimitiveComponent>& Collider : Colliders)
	{
		if (Collider)
		{
			Collider->DestroyComponent();
			// Frees the stable name at once, so colliders created again (re-registration) can take it before garbage collection
			const FName FreedName = MakeUniqueObjectName(Collider->GetOuter(), Collider->GetClass(), TEXT("DestroyedPlatformCollider"));
			Collider->Rename(*FreedName.ToString(), nullptr, REN_DontCreateRedirectors | REN_DoNotDirty | REN_NonTransactional);
		}
		Collider = nullptr;
	}
	Colliders.Reset();
}

void UGSTN_EnemyPlatformComponent::ConfigureCollision(UPrimitiveComponent& Collider, const FGSTN_PlatformShape& Shape) const
{
	Collider.SetCollisionObjectType(UGSTN_PlatformSettings::Get().GetPlatformObjectChannel());
	Collider.SetCollisionResponseToAllChannels(ECR_Ignore);
	Collider.SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Collider.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

UPrimitiveComponent* UGSTN_EnemyPlatformComponent::GetShapeComponent(int32 ShapeId) const
{
	return Colliders.IsValidIndex(ShapeId) ? Colliders[ShapeId].Get() : nullptr;
}

void UGSTN_EnemyPlatformComponent::ApplyPosePoint(const FGSTN_PlatformPoseContext& Context)
{
	const FTransform OwnerPose = ComputeOwnerPose(Context);
	CurrentPoseFrame = Context.Frame;
	if (Context.Frame != INDEX_NONE)
	{
		LatestPoseFrame = FMath::Max(LatestPoseFrame, Context.Frame);
		PoseHistory.Record(Context.Frame, OwnerPose, Context.StepMs * 0.001);
	}
	PoseColliders(OwnerPose, Context.Frame);
	++PoseApplicationCount;
	OnPoseApplied(Context, OwnerPose);
}

void UGSTN_EnemyPlatformComponent::ApplyPresentationPose()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FGSTN_PlatformPoseContext Context;
	const AGameStateBase* GameState = World->GetGameState();
	Context.TimeMs = (GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds()) * 1000.0;
	const FTransform OwnerPose = ComputeOwnerPose(Context);
	PoseColliders(OwnerPose, INDEX_NONE);
	OnPoseApplied(Context, OwnerPose);
}

void UGSTN_EnemyPlatformComponent::PoseColliders(const FTransform& OwnerPose, int32 Frame)
{
	for (const FGSTN_PlatformShape& Shape : GetSpec().Shapes)
	{
		UPrimitiveComponent* Collider = GetShapeComponent(Shape.ShapeId);
		if (!Collider)
		{
			continue;
		}
		if (Frame != INDEX_NONE)
		{
			const ECollisionEnabled::Type Wanted = IsShapeSolidAt(Shape.ShapeId, Frame) ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision;
			if (Collider->GetCollisionEnabled() != Wanted)
			{
				Collider->SetCollisionEnabled(Wanted);
			}
		}
		const FTransform World = Shape.ComposeWorld(OwnerPose);
		// Teleport: the sweeps of this frame see the new pose at once, and no physics velocity is made up from a pose that a
		// re-simulation may move backwards. The velocity of the surface comes from the pose history
		Collider->SetWorldLocationAndRotation(World.GetLocation(), World.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	}
}

bool UGSTN_EnemyPlatformComponent::CollectFindings(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	return GetSpec().CollectFindings(OutErrors, OutWarnings);
}

#if WITH_EDITOR
EDataValidationResult UGSTN_EnemyPlatformComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TArray<FText> Errors, Warnings;
	CollectFindings(Errors, Warnings);
	for (const FText& Warning : Warnings)
	{
		Context.AddWarning(Warning);
	}
	for (const FText& Error : Errors)
	{
		Context.AddError(Error);
	}
	return Errors.Num() > 0 ? EDataValidationResult::Invalid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE

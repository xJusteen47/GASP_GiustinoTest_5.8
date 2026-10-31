// Platform runtime: components, colliders, the pose point (Mover, no GAS, no direct Network Prediction code).

#include "GSTN_EnemyPlatformComponent.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GSTN_PlatformCollision.h"
#include "GSTN_PlatformColliders.h"
#include "GSTN_PlatformDefinition.h"
#include "GSTN_PlatformSettings.h"
#include "GSTN_PlatformSubsystem.h"
#include "GSTN_PlatformTags.h"
#include "GSTN_SurfaceBehaviourComponent.h"
#include "GSTNPlatformCoreModule.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DataValidation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_EnemyPlatformComponent)

#define LOCTEXT_NAMESPACE "GSTNEnemyPlatform"

namespace GSTNEnemyPlatformPrivate
{
	static TAutoConsoleVariable<int32> CVarDraw(TEXT("gstn.Platform.Draw"), 0,
		TEXT("Draws the platform shapes of every enemy platform: green Active, orange Breaking, grey Inactive (presentation only)"));
}

UGSTN_EnemyPlatformComponent::UGSTN_EnemyPlatformComponent()
{
	// Ticks for the presentation fallback (no simulation runs the pose point) and the debug draw; before physics, like any kinematic mover
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

void UGSTN_EnemyPlatformComponent::OnShapeBreakRequested_Implementation(int32 ShapeId)
{
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

	if (const AActor* Owner = GetOwner(); Owner && Owner->FindComponentByClass<UGSTN_SurfaceBehaviourComponent>())
	{
		UE_LOG(LogGSTNPlatform, Warning, TEXT("%s: the owner has a GSTN Surface Behaviour component, which wins over the platform shapes' surfaces (resolution step 1)"), *GetPathName());
	}
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
	if (GSTNEnemyPlatformPrivate::CVarDraw.GetValueOnGameThread() != 0)
	{
		DrawDebugShapes();
	}
}

// ---------------------------------------------------------------------------------------------------------------- colliders

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

	ShapeStates.Reset(ComputeDesiredMask());
	for (int32 Id = 0; Id < Colliders.Num(); ++Id)
	{
		if (Colliders[Id])
		{
			Colliders[Id]->SetCollisionEnabled(IsShapeSolidAt(Id, MAX_int32) ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
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
	const UGSTN_PlatformSettings& Settings = UGSTN_PlatformSettings::Get();
	FCollisionResponseContainer Responses;
	GSTNPlatformCollision::DeriveResponses(Shape.Behaviour, Settings.GetCollisionRules(), Responses);
	Collider.SetCollisionObjectType(Settings.GetPlatformObjectChannel());
	Collider.SetCollisionResponseToChannels(Responses);
	// Queries only: the characters' movement sweeps; no physics body interacts with a collider that teleports every frame
	Collider.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

UPrimitiveComponent* UGSTN_EnemyPlatformComponent::GetShapeComponent(int32 ShapeId) const
{
	return Colliders.IsValidIndex(ShapeId) ? Colliders[ShapeId].Get() : nullptr;
}

// ---------------------------------------------------------------------------------------------------------------- pose point

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
	// Without a simulated frame nothing can sweep against the collider in a way that matters: show the final state of the requests
	const int32 StateFrame = Frame != INDEX_NONE ? Frame : MAX_int32;
	for (const FGSTN_PlatformShape& Shape : GetSpec().Shapes)
	{
		UPrimitiveComponent* Collider = GetShapeComponent(Shape.ShapeId);
		if (!Collider)
		{
			continue;
		}
		const ECollisionEnabled::Type Wanted = IsShapeSolidAt(Shape.ShapeId, StateFrame) ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision;
		if (Collider->GetCollisionEnabled() != Wanted)
		{
			Collider->SetCollisionEnabled(Wanted);
		}
		const FTransform World = Shape.ComposeWorld(OwnerPose);
		// Teleport: the sweeps of this frame see the new pose at once, and no physics velocity is made up from a pose that a
		// re-simulation may move backwards. The velocity of the surface comes from the pose history
		Collider->SetWorldLocationAndRotation(World.GetLocation(), World.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	}
}

// ---------------------------------------------------------------------------------------------------------------- activation

uint8 UGSTN_EnemyPlatformComponent::ComputeDesiredMask() const
{
	const FGSTN_PlatformSpec& Current = GetSpec();
	return Current.GetShapeIdMask() & ShapeMaskInput & Current.GetActionStateMask(ActionState);
}

void UGSTN_EnemyPlatformComponent::RequestShapeStates(EGSTN_PlatformShapeState DisableAs)
{
	const FGSTN_PlatformSpec& Current = GetSpec();
	int32 Grace[GSTNPlatform::MaxShapes];
	for (int32 Id = 0; Id < GSTNPlatform::MaxShapes; ++Id)
	{
		const int32 Index = Current.FindShapeIndex(Id);
		Grace[Id] = Index != INDEX_NONE ? Current.GetDropGraceFrames(Current.Shapes[Index]) : 0;
	}
	// The next frame the pose point simulates forward: the frames already simulated keep their collision if they are replayed
	const int32 Frame = LatestPoseFrame == INDEX_NONE ? MIN_int32 : LatestPoseFrame + 1;
	ShapeStates.Request(ComputeDesiredMask(), DisableAs, Grace, Frame);
}

void UGSTN_EnemyPlatformComponent::SetShapeMask(uint8 EnabledShapes, EGSTN_PlatformShapeState DisableAs)
{
	ShapeMaskInput = EnabledShapes;
	RequestShapeStates(DisableAs == EGSTN_PlatformShapeState::Breaking ? EGSTN_PlatformShapeState::Breaking : EGSTN_PlatformShapeState::Inactive);
}

void UGSTN_EnemyPlatformComponent::SetActionState(EGSTN_PlatformActionState State)
{
	ActionState = State;
	RequestShapeStates(EGSTN_PlatformShapeState::Breaking);
}

bool UGSTN_EnemyPlatformComponent::IsShapeSolidAt(int32 ShapeId, int32 Frame) const
{
	return GSTNPlatform::IsSolid(ShapeStates.GetState(ShapeId, Frame));
}

// ---------------------------------------------------------------------------------------------------------------- queries

EGSTN_PlatformShapeState UGSTN_EnemyPlatformComponent::GetShapeState(int32 ShapeId) const
{
	if (GetSpec().FindShapeIndex(ShapeId) == INDEX_NONE)
	{
		return EGSTN_PlatformShapeState::Inactive;
	}
	return ShapeStates.GetState(ShapeId, CurrentPoseFrame != INDEX_NONE ? CurrentPoseFrame : MAX_int32);
}

FGameplayTag UGSTN_EnemyPlatformComponent::GetShapeStateTag(int32 ShapeId) const
{
	switch (GetShapeState(ShapeId))
	{
	case EGSTN_PlatformShapeState::Active:
		return GSTNPlatformTags::State_Active;
	case EGSTN_PlatformShapeState::Braced:
		return GSTNPlatformTags::State_Braced;
	case EGSTN_PlatformShapeState::Breaking:
		return GSTNPlatformTags::State_Breaking;
	default:
		return GSTNPlatformTags::State_Inactive;
	}
}

FGSTN_SurfaceBehaviour UGSTN_EnemyPlatformComponent::GetShapeBehaviour(int32 ShapeId) const
{
	const FGSTN_PlatformSpec& Current = GetSpec();
	const int32 Index = Current.FindShapeIndex(ShapeId);
	return Index != INDEX_NONE ? Current.Shapes[Index].Behaviour : FGSTN_SurfaceBehaviour();
}

FVector UGSTN_EnemyPlatformComponent::GetSurfaceVelocityAt(int32 ShapeId, FVector WorldPoint) const
{
	if (CurrentPoseFrame == INDEX_NONE || GetSpec().FindShapeIndex(ShapeId) == INDEX_NONE || !IsShapeSolidAt(ShapeId, CurrentPoseFrame))
	{
		return FVector::ZeroVector;
	}
	// The shapes are rigid in the enemy's frame: every shape moves with the owner pose the history records
	FVector Velocity;
	PoseHistory.GetVelocityAtPoint(CurrentPoseFrame, WorldPoint, Velocity);
	return Velocity;
}

FGSTN_OccupantResponse UGSTN_EnemyPlatformComponent::GetOccupantResponse(int32 ShapeId) const
{
	const FGSTN_PlatformSpec& Current = GetSpec();
	const int32 Index = Current.FindShapeIndex(ShapeId);
	return Index != INDEX_NONE ? Current.GetOccupantResponse(Current.Shapes[Index]) : Current.DefaultOccupantResponse;
}

// ---------------------------------------------------------------------------------------------------------------- debug, validation

void UGSTN_EnemyPlatformComponent::DrawDebugShapes() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	for (int32 Id = 0; Id < Colliders.Num(); ++Id)
	{
		const UPrimitiveComponent* Collider = Colliders[Id];
		if (!Collider)
		{
			continue;
		}
		const EGSTN_PlatformShapeState State = GetShapeState(Id);
		const FColor Color = State == EGSTN_PlatformShapeState::Active ? FColor::Green
			: State == EGSTN_PlatformShapeState::Breaking ? FColor::Orange : FColor(128, 128, 128);
		const FVector Location = Collider->GetComponentLocation();
		const FQuat Rotation = Collider->GetComponentQuat();
		if (const UBoxComponent* Box = Cast<UBoxComponent>(Collider))
		{
			DrawDebugBox(World, Location, Box->GetScaledBoxExtent(), Rotation, Color, false, -1.f, 0, 2.f);
		}
		else if (const USphereComponent* Sphere = Cast<USphereComponent>(Collider))
		{
			DrawDebugSphere(World, Location, Sphere->GetScaledSphereRadius(), 16, Color, false, -1.f, 0, 2.f);
		}
		else if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Collider))
		{
			DrawDebugCapsule(World, Location, Capsule->GetScaledCapsuleHalfHeight(), Capsule->GetScaledCapsuleRadius(), Rotation, Color, false, -1.f, 0, 2.f);
		}
	}
#endif
}

bool UGSTN_EnemyPlatformComponent::CollectFindings(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	const bool bSpecValid = GetSpec().CollectFindings(OutErrors, OutWarnings, UGSTN_PlatformSettings::Get().MakeValidationContext());
	if (const AActor* Owner = GetOwner(); Owner && Owner->FindComponentByClass<UGSTN_SurfaceBehaviourComponent>())
	{
		OutWarnings.Add(LOCTEXT("SurfaceComponentShadows", "The owner has a GSTN Surface Behaviour component: it wins over the surfaces of the platform shapes (resolution step 1). Remove it, or name the shapes' colliders (Surface<Id>) in its PerComponent map on purpose"));
	}
	return bSpecValid;
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

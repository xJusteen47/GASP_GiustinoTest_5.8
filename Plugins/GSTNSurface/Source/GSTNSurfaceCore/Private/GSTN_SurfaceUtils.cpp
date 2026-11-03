// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#include "GSTN_SurfaceUtils.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "GSTN_SurfaceBehaviourComponent.h"
#include "GSTN_SurfaceProvider.h"
#include "GSTN_SurfaceSettings.h"
#include "GSTN_SurfaceTable.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"

namespace GSTNSurfaceUtilsPrivate
{
	/** The provider that answers for this collider: the collider itself first, then its owner */
	UObject* FindProvider(UPrimitiveComponent* Collider)
	{
		if (!Collider)
		{
			return nullptr;
		}
		if (Collider->GetClass()->ImplementsInterface(UGSTN_SurfaceProvider::StaticClass()))
		{
			return Collider;
		}
		AActor* Owner = Collider->GetOwner();
		if (Owner && Owner->GetClass()->ImplementsInterface(UGSTN_SurfaceProvider::StaticClass()))
		{
			return Owner;
		}
		return nullptr;
	}

	const UPhysicalMaterial* GetBodyMaterial(const UPrimitiveComponent* Component, FName BoneName)
	{
		if (!Component)
		{
			return nullptr;
		}
		const FBodyInstance* Body = Component->GetBodyInstance(BoneName);
		return Body ? Body->GetSimplePhysicalMaterial() : nullptr;
	}
}

FGSTN_SurfaceView UGSTN_SurfaceUtils::Resolve(const FHitResult& Hit)
{
	const UGSTN_SurfaceSettings& Settings = UGSTN_SurfaceSettings::Get();
	return ResolveQuery(MakeQuery(Hit), GetHitPhysicalMaterial(Hit), Settings.GetTable(), Settings.Default);
}

FGSTN_SurfaceView UGSTN_SurfaceUtils::ResolveComponent(const UPrimitiveComponent* Component, FName BoneName)
{
	FGSTN_SurfaceQuery Query;
	Query.Component = const_cast<UPrimitiveComponent*>(Component);
	Query.ComponentName = Component ? Component->GetFName() : NAME_None;
	Query.BoneName = BoneName;

	const UGSTN_SurfaceSettings& Settings = UGSTN_SurfaceSettings::Get();
	return ResolveQuery(Query, GSTNSurfaceUtilsPrivate::GetBodyMaterial(Component, BoneName), Settings.GetTable(),
		Settings.Default);
}

FGSTN_SurfaceView UGSTN_SurfaceUtils::ResolveQuery(const FGSTN_SurfaceQuery& Query, const UPhysicalMaterial* Material,
	const FGSTN_SurfaceTable& Table, const FGSTN_SurfaceBehaviour& Default)
{
	UPrimitiveComponent* Collider = Query.Component;
	const AActor* Owner = Collider ? Collider->GetOwner() : nullptr;
	UObject* Provider = GSTNSurfaceUtilsPrivate::FindProvider(Collider);
	const FGSTN_SurfaceBehaviour* MaterialEntry = Table.FindMaterial(Material);

	// The provider's answer must outlive the branch below, so it is held here rather than in the branch
	FGSTN_SurfaceBehaviour ProvidedBehaviour;
	const FGSTN_SurfaceBehaviour* Behaviour = nullptr;
	EGSTN_SurfaceSource Source = EGSTN_SurfaceSource::Default;

	const UGSTN_SurfaceBehaviourComponent* SurfaceComponent = UGSTN_SurfaceBehaviourComponent::FindFor(Owner);
	if (SurfaceComponent)
	{
		Behaviour = &SurfaceComponent->GetBehaviourFor(Collider);
		Source = EGSTN_SurfaceSource::Component;
	}
	else if (Provider)
	{
		ProvidedBehaviour = IGSTN_SurfaceProvider::Execute_GetSurfaceBehaviour(Provider, Query);
		Behaviour = &ProvidedBehaviour;
		Source = EGSTN_SurfaceSource::Provider;
	}
	else if (MaterialEntry)
	{
		Behaviour = MaterialEntry;
		Source = EGSTN_SurfaceSource::PhysicalMaterial;
	}
	else
	{
		Behaviour = &Default;
	}

	FGSTN_SurfaceView View;
	View.Behaviours = Behaviour->Behaviours;
	View.Launch = Behaviour->Launch;
	View.DesignerTags = Behaviour->DesignerTags;
	View.Source = Source;

	View.SurfaceType = Behaviour->SurfaceType;
	if (!View.SurfaceType.IsValid() && MaterialEntry)
	{
		View.SurfaceType = MaterialEntry->SurfaceType;
	}
	if (!View.SurfaceType.IsValid())
	{
		View.SurfaceType = Default.SurfaceType;
	}
	View.SurfaceTypeIndex = Table.GetTypeIndex(View.SurfaceType);

	View.bMoving = Collider && Collider->Mobility == EComponentMobility::Movable;
	if (Provider)
	{
		View.SurfaceVelocity = IGSTN_SurfaceProvider::Execute_GetSurfaceVelocity(Provider, Query);
	}
	return View;
}

FGSTN_SurfaceQuery UGSTN_SurfaceUtils::MakeQuery(const FHitResult& Hit)
{
	FGSTN_SurfaceQuery Query;
	Query.Component = Hit.GetComponent();
	Query.ComponentName = Query.Component ? Query.Component->GetFName() : NAME_None;
	Query.BoneName = Hit.BoneName;
	Query.ContactPoint = Hit.ImpactPoint;
	Query.ContactNormal = Hit.ImpactNormal;
	return Query;
}

const UPhysicalMaterial* UGSTN_SurfaceUtils::GetHitPhysicalMaterial(const FHitResult& Hit)
{
	if (const UPhysicalMaterial* Material = Hit.PhysMaterial.Get())
	{
		return Material;
	}
	return GSTNSurfaceUtilsPrivate::GetBodyMaterial(Hit.GetComponent(), Hit.BoneName);
}

uint8 UGSTN_SurfaceUtils::GetSurfaceTypeIndex(FGameplayTag SurfaceType)
{
	return UGSTN_SurfaceSettings::Get().GetTable().GetTypeIndex(SurfaceType);
}

uint32 UGSTN_SurfaceUtils::GetSurfaceTypeTableChecksum()
{
	return UGSTN_SurfaceSettings::Get().GetTable().GetTypesChecksum();
}

FGameplayTag UGSTN_SurfaceUtils::GetSurfaceTypeFromIndex(uint8 Index)
{
	return UGSTN_SurfaceSettings::Get().GetTable().GetTypeByIndex(Index);
}

FVector UGSTN_SurfaceUtils::ComputeLaunchDirection(const FGSTN_SurfaceLaunchParams& Params, const FTransform& SurfaceTransform,
	const FVector& ContactNormal)
{
	const FVector SurfaceUp = SurfaceTransform.GetRotation().GetUpVector();
	FVector Direction = SurfaceUp;

	switch (Params.DirectionMode)
	{
	case EGSTN_SurfaceLaunchDirection::SurfaceNormal:
		Direction = ContactNormal.IsNearlyZero() ? SurfaceUp : ContactNormal;
		break;

	case EGSTN_SurfaceLaunchDirection::LocalDirection:
		Direction = Params.LocalDirection.IsNearlyZero()
			? SurfaceUp
			: SurfaceTransform.TransformVectorNoScale(Params.LocalDirection);
		break;

	case EGSTN_SurfaceLaunchDirection::Tilt:
	{
		// Spherical angles around the local up: pitch away from it, yaw from local forward towards local right
		const float PitchRad = FMath::DegreesToRadians(FMath::Clamp(Params.TiltPitch, 0.f, 89.f));
		const float YawRad = FMath::DegreesToRadians(Params.TiltYaw);
		float SinPitch, CosPitch, SinYaw, CosYaw;
		FMath::SinCos(&SinPitch, &CosPitch, PitchRad);
		FMath::SinCos(&SinYaw, &CosYaw, YawRad);
		const FVector Local(SinPitch * CosYaw, SinPitch * SinYaw, CosPitch);
		Direction = SurfaceTransform.TransformVectorNoScale(Local);
		break;
	}
	}

	return Direction.GetSafeNormal(UE_SMALL_NUMBER, SurfaceUp);
}

float UGSTN_SurfaceUtils::ComputeLaunchSpeed(const FGSTN_SurfaceLaunchParams& Params, float IncomingSpeed)
{
	if (Params.SpeedMode == EGSTN_SurfaceLaunchSpeed::Fixed)
	{
		return FMath::Max(0.f, Params.Speed);
	}

	const float Scaled = FMath::Max(0.f, IncomingSpeed) * FMath::Max(0.f, Params.Restitution);
	const float MinSpeed = FMath::Max(0.f, Params.MinSpeed);
	const float MaxSpeed = FMath::Max(MinSpeed, Params.MaxSpeed);
	return FMath::Clamp(Scaled, MinSpeed, MaxSpeed);
}

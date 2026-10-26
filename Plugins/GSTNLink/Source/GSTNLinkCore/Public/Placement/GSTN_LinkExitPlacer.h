// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GSTN_LinkExitPlacer.generated.h"

class IGSTN_ExitCollisionQuery;

/**
 * What an exit placer decides from (m6 spec 7.7): values only. The placer puts the EXITING entity (usually the parked one) near the
 * entity that stays.
 */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_ExitQuery
{
	GENERATED_BODY()

	/** Where the entity that stays is, in the world */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit")
	FTransform PartnerTransform = FTransform::Identity;

	/** The local up of the entity, so any gravity direction works */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit")
	FVector Up = FVector::UpVector;

	/** The direction the exit prefers, in the world (a throw's, a type's). Zero = the partner's forward */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit")
	FVector PreferredDirection = FVector::ZeroVector;

	/** Radius of the exiting entity's capsule */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float CapsuleRadius = 34.f;

	/** Half height of the exiting entity's capsule */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float CapsuleHalfHeight = 88.f;

	/** The partner's socket in the world, for a placer that launches from it (a throw) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit")
	FTransform SocketTransform = FTransform::Identity;

	/** The velocity the exit should carry, from the knockback of the throw hit (zero for a plain release) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit")
	FVector KnockbackVelocity = FVector::ZeroVector;

	/** The world, or null for no collision check. Not reflected and not owned: valid for the call only, and a Blueprint placer cannot reach it */
	const IGSTN_ExitCollisionQuery* Collision = nullptr;
};

/** Where and how an entity leaves a link */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_ExitResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit")
	FVector Velocity = FVector::ZeroVector;

	/** A collision-free place was found. False = the transform is the fallback next to the partner, which may overlap geometry */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit")
	bool bValid = false;

	/** Index of the candidate that won, in the placer's own order; INDEX_NONE for the fallback. For logs and tests */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit")
	int32 CandidateIndex = INDEX_NONE;
};

/**
 * Decides where an entity lands when a link ends (m6 spec 3 and 7.7). It is the only exit helper: a mount adds a subclass, and a
 * definition holds the placer it wants, instanced, so tuning (a ring radius) is the definition's data and needs no subclass.
 *
 * ONE Blueprint-implementable entry point, const, deterministic for equal inputs: the candidates come in a fixed order and the world
 * is asked only through the query. Never ends inside geometry when any candidate is free; otherwise the fallback is reported as
 * invalid, so the caller knows.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class GSTNLINKCORE_API UGSTN_LinkExitPlacer : public UObject
{
	GENERATED_BODY()

public:
	/** The default is the partner's position lifted by a body height, reported invalid */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Link")
	FGSTN_ExitResult Place(const FGSTN_ExitQuery& Query) const;

	virtual FGSTN_ExitResult Place_Implementation(const FGSTN_ExitQuery& Query) const;

protected:
	/** Whether the query's capsule fits at Location; true when the query has no world to ask */
	static bool IsFree(const FGSTN_ExitQuery& Query, const FVector& Location, const FQuat& Rotation);

	/** The fallback next to the partner: its position plus Lift along the local up, invalid */
	static FGSTN_ExitResult MakeFallback(const FGSTN_ExitQuery& Query, double Lift);
};

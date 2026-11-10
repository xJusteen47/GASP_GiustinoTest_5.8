// Held objects, world side.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "GSTN_HeldTypes.h"

#include "GSTN_HeldObjectComponent.generated.h"

class AGSTN_Projectile;

/** One kind of object (crate, barrel...): shared tuning, a data asset so many placed objects use one row. Slice 1 subset of the plan's UGSTN_ObjectDefinition */
UCLASS(BlueprintType)
class GSTNHELDRUNTIME_API UGSTN_HeldObjectDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Held", meta = (ShowOnlyInnerProperties))
	FGSTN_HeldObjectTuning Tuning;
};

/**
 * Makes its actor a held object (M10). Put it on any movable actor (or use AGSTN_HeldObjectActor); without a Definition the built-in
 * test tuning applies, so a placed object works with no setup.
 *
 * Authority: the object is the host's (combat-m10 §9 C, server-authoritative by design, not simulation state). Only the host changes
 * State, Holder, Flight and RestLocation, through UGSTN_HeldSubsystem; they replicate as properties, so an object at rest costs no bytes.
 * Every machine places the actor itself from them each frame: at the holder (hold offset in the holder's space) while Held, at its
 * projectile's drawn position while Flying (on a client that is its own predicted frame, as for any projectile), at RestLocation when Idle.
 * Movement replication stays off.
 */
UCLASS(ClassGroup = (GSTN), meta = (BlueprintSpawnableComponent))
class GSTNHELDRUNTIME_API UGSTN_HeldObjectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_HeldObjectComponent();

	/** Optional shared tuning; null = the built-in test tuning */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Held")
	TObjectPtr<UGSTN_HeldObjectDefinition> Definition;

	const FGSTN_HeldObjectTuning& GetTuning() const;

	EGSTN_HeldObjectState GetState() const { return State; }
	AActor* GetHolder() const { return Holder; }
	AGSTN_Projectile* GetFlight() const { return Flight; }
	/** Host-assigned id (registration order), the race tie-break; INDEX_NONE on clients */
	int32 GetObjectId() const { return ObjectId; }

	/** Host only, through UGSTN_HeldSubsystem */
	void SetHeld(AActor* InHolder);
	void SetFlying(AGSTN_Projectile& InFlight);
	void SetIdleAt(const FVector& Location);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	friend class UGSTN_HeldSubsystem;

	UFUNCTION()
	void OnRep_RestLocation();

	/** Places the owner from the replicated state (every machine) */
	void UpdatePlacement();

	UPROPERTY(ReplicatedUsing = OnRep_RestLocation, VisibleInstanceOnly, BlueprintReadOnly, Category = "Held", meta = (AllowPrivateAccess = "true"))
	EGSTN_HeldObjectState State = EGSTN_HeldObjectState::Idle;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Held", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> Holder;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Held", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AGSTN_Projectile> Flight;

	/** Where it rests while Idle (1/10 cm on the wire) */
	UPROPERTY(ReplicatedUsing = OnRep_RestLocation)
	FVector_NetQuantize10 RestLocation;

	int32 ObjectId = INDEX_NONE;

	/** Host: the flight's motion, kept after the projectile actor is gone (hit or expiry) to land where it ended */
	FGSTN_BallisticSegment FlightSegment;
	int32 FlightEndFrame = INDEX_NONE;
};

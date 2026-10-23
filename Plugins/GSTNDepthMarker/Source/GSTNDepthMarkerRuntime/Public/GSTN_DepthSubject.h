// Depth marker subjects: how an actor reports its presented state (depth-marker.md §5).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_DepthMarkerRuntimeTypes.h"
#include "UObject/Interface.h"
#include "GSTN_DepthSubject.generated.h"

/** An actor that reports its own depth subject state (wins over any adapter) */
UINTERFACE(BlueprintType)
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthSubject : public UInterface
{
	GENERATED_BODY()
};

class GSTNDEPTHMARKERRUNTIME_API IGSTN_DepthSubject
{
	GENERATED_BODY()

public:
	/** Fill the state from the PRESENTED transform; return false to skip the marker this frame */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DepthMarker")
	bool GetDepthSubjectState(FGSTN_DepthSubjectState& OutState) const;
};

/** Optional: the pawn gameplay tags for the HideWhen / ShowOnlyWhen queries. The plugin names no tag */
UINTERFACE(BlueprintType)
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthTagSource : public UInterface
{
	GENERATED_BODY()
};

class GSTNDEPTHMARKERRUNTIME_API IGSTN_DepthTagSource
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DepthMarker")
	void GetDepthMarkerTags(FGameplayTagContainer& OutTags) const;
};

/**
 * Reads an actor movement into a subject state. Stateless: the registry uses the class default object, so the project own
 * adapters (a Mover one lives in its own plugin) register a class and a priority and the runtime knows no movement type.
 */
UCLASS(Abstract, Blueprintable)
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthSubjectAdapter : public UObject
{
	GENERATED_BODY()

public:
	/** The highest priority adapter that can handle the actor wins; later registrations win ties */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DepthMarker")
	int32 Priority = 0;

	virtual bool CanHandle(const AActor* Actor) const { return false; }

	/** Fills OutState from the actor; false when it cannot (no feet this frame) */
	virtual bool ReadState(const AActor* Actor, FGSTN_DepthSubjectState& OutState) const { return false; }
};

/** Picks the adapter for an actor */
class GSTNDEPTHMARKERRUNTIME_API FGSTN_DepthAdapterRegistry
{
public:
	static void Register(TSubclassOf<UGSTN_DepthSubjectAdapter> AdapterClass);
	static void Unregister(TSubclassOf<UGSTN_DepthSubjectAdapter> AdapterClass);

	/** The class default object of the winning adapter, or null */
	static const UGSTN_DepthSubjectAdapter* Resolve(const AActor* Actor);

	static int32 Num();
};

/** A character with a UCharacterMovementComponent: feet, local gravity, floor and base from it */
UCLASS()
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthSubjectAdapter_CharacterMovement : public UGSTN_DepthSubjectAdapter
{
	GENERATED_BODY()

public:
	UGSTN_DepthSubjectAdapter_CharacterMovement() { Priority = 100; }

	virtual bool CanHandle(const AActor* Actor) const override;
	virtual bool ReadState(const AActor* Actor, FGSTN_DepthSubjectState& OutState) const override;
};

/** Any actor (props, tests): actor location as the feet, world gravity as Up, airborne from speed along Up */
UCLASS()
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthSubjectAdapter_Actor : public UGSTN_DepthSubjectAdapter
{
	GENERATED_BODY()

public:
	UGSTN_DepthSubjectAdapter_Actor() { Priority = 0; }

	/** Speed along Up above which the actor counts as airborne */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DepthMarker", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float AirborneSpeed = 5.f;

	virtual bool CanHandle(const AActor* Actor) const override { return Actor != nullptr; }
	virtual bool ReadState(const AActor* Actor, FGSTN_DepthSubjectState& OutState) const override;
};

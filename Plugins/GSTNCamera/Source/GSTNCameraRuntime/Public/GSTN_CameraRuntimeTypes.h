// Runtime types of the camera: the log category, the three small interfaces a project pawn may implement, and the subject adapter
// registry (camera.md §8, §10). The plugin knows no GAS, no Mover and no project class: everything it reads about the view target comes
// through these.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_CameraTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "UObject/Object.h"
#include "GSTN_CameraRuntimeTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGSTNCamera, Log, All);

/** Who writes the look input into the orbit (camera.md §4.3) */
UENUM(BlueprintType)
enum class EGSTN_CameraLookInputSource : uint8
{
	/** The plugin reads its Enhanced Input look action (and AddLookInput) */
	Plugin,
	/** The project already feeds the controller; the plugin takes no look input of its own */
	ControlRotation
};

/** Whether the camera writes the player controller's control rotation (camera.md §5) */
UENUM(BlueprintType)
enum class EGSTN_CameraControlRotationPolicy : uint8
{
	/** The camera is the only writer: the input source samples CameraYaw and aim from it */
	WriteBack,
	None
};

/** Presented state of a view target: the presented capsule point, Up, velocity, suppressed (camera.md §8). Implemented by the pawn or one of its components */
UINTERFACE(BlueprintType, MinimalAPI)
class UGSTN_CameraSubject : public UInterface
{
	GENERATED_BODY()
};

class GSTNCAMERARUNTIME_API IGSTN_CameraSubject
{
	GENERATED_BODY()

public:
	/** Fill the presented state. False when the subject has none (the adapters are tried next) */
	UFUNCTION(BlueprintNativeEvent, Category = "Camera")
	bool GetCameraSubjectState(FGSTN_CameraSubjectState& OutState) const;
};

/** The tags the camera rule table reads, supplied by the project from its ability system so the plugin knows no GAS (camera.md §3.2) */
UINTERFACE(BlueprintType, MinimalAPI)
class UGSTN_CameraTagSource : public UInterface
{
	GENERATED_BODY()
};

class GSTNCAMERARUNTIME_API IGSTN_CameraTagSource
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Camera")
	void GetCameraTags(FGameplayTagContainer& OutTags) const;
};

/** Where the character shoots from, presented (a socket; default the pivot). Used by the converged aim (camera.md §6) */
UINTERFACE(BlueprintType, MinimalAPI)
class UGSTN_CameraAimOrigin : public UInterface
{
	GENERATED_BODY()
};

class GSTNCAMERARUNTIME_API IGSTN_CameraAimOrigin
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Camera")
	FVector GetCameraAimOrigin() const;
};

/**
 * Reads the presented state of an actor that does not implement IGSTN_CameraSubject. A movement type registers its own adapter class
 * with FGSTN_CameraAdapterRegistry (a Mover adapter is a separate plugin). The adapter object is the class default object: stateless.
 */
UCLASS(Abstract)
class GSTNCAMERARUNTIME_API UGSTN_CameraSubjectAdapter : public UObject
{
	GENERATED_BODY()

public:
	virtual bool CanRead(const AActor* Actor) const { return false; }

	/** Highest wins; later registration wins ties */
	virtual int32 GetPriority() const { return 0; }

	virtual bool Read(const AActor* Actor, FGSTN_CameraSubjectState& OutState) const { return false; }
};

class GSTNCAMERARUNTIME_API FGSTN_CameraAdapterRegistry
{
public:
	static void Register(UClass* AdapterClass);
	static void Unregister(UClass* AdapterClass);

	/** The adapter that will read this actor, or null */
	static const UGSTN_CameraSubjectAdapter* Find(const AActor* Actor);

	/** The interface first, then the best adapter. False when nobody can read the actor */
	static bool Read(const AActor* Actor, FGSTN_CameraSubjectState& OutState);

private:
	static TArray<TWeakObjectPtr<UClass>>& Classes();
};

// Minion runtime (M8 G8): the designer handle of a minion's brain.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSTN_MinionBrainComponent.generated.h"

/**
 * The designer-facing handle of a minion's brain on its pooled AGSTN_MinionActor. It does not tick and holds no state of the
 * simulation: it only says WHICH StateTree this minion should think with (overriding the type's) and lets Blueprint start or stop
 * the brain. The tree itself runs inside UGSTN_MinionBrainSubsystem, so a crowd pays no ticking component per minion.
 *
 * Authority only. The brain of the type's definition starts by itself when the minion activates; this component is for the
 * minion that needs another tree or none at runtime.
 */
UCLASS(ClassGroup = "GSTN", meta = (BlueprintSpawnableComponent))
class GSTNMINIONRUNTIME_API UGSTN_MinionBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_MinionBrainComponent();

	/** A StateTree to use instead of the definition's brain. Empty = the definition's. Server only */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Brain", meta = (AllowedClasses = "/Script/StateTreeModule.StateTree"))
	TSoftObjectPtr<UObject> BrainOverride;

	/** Starts (or restarts) the brain of this minion: the override, else the definition's, else the native brain. False when pooled */
	UFUNCTION(BlueprintCallable, Category = "Minion|Brain")
	bool StartBrain();

	/** Stops the brain. The minion's last intent goes stale on its own and it idles */
	UFUNCTION(BlueprintCallable, Category = "Minion|Brain")
	bool StopBrain();

	UFUNCTION(BlueprintPure, Category = "Minion|Brain")
	bool IsBrainRunning() const;
};

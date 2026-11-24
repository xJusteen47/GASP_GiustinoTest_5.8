#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSTN_ExperienceGatedSimulation.h"
#include "GSTN_ExperienceGateComponent.generated.h"

class UGSTN_ExperienceManagerComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSTN_OnExperienceGateOpenedDynamic, const UGSTN_ExperienceDefinition*, Experience);
DECLARE_MULTICAST_DELEGATE_OneParam(FGSTN_OnExperienceGateOpenedNative, const UGSTN_ExperienceDefinition* /*Experience*/);

/**
 * Put on a pawn (or any actor that owns a simulation) to keep it from simulating before the Experience is Loaded. It is the
 * ready-made IGSTN_ExperienceGatedSimulation: on BeginPlay it asks the world's manager whether the gate is open, registers
 * if not, and holds until the manager calls it.
 *
 * Usage from a simulation tick (an NP tick extension, a Mover input producer, an ability tick):
 *   if (!Gate->ShouldRunSimulationTick()) { return; }
 *   ... run the tick ...
 *   Gate->ReportSimulationTickRan();   // keeps the spike G4 leak counter honest
 *
 * No fork change: the NP service still ticks the pawn's proxy, and the pawn's own code declines to advance the simulation while
 * held. Whether that is enough for a pawn that is already registered with the NP service is exactly what spike G4 measures
 * (UGSTN_ExperienceManagerComponent::GetLeakedSimTicks).
 */
UCLASS(ClassGroup = "GSTN", meta = (BlueprintSpawnableComponent, DisplayName = "GSTN Experience Gate"))
class GSTNEXPERIENCE_API UGSTN_ExperienceGateComponent : public UActorComponent, public IGSTN_ExperienceGatedSimulation
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	//~ IGSTN_ExperienceGatedSimulation
	virtual void GSTN_OnExperienceGateOpened(const UGSTN_ExperienceDefinition* Experience) override;

	/** True from BeginPlay until the Experience is Loaded (and never, in a world with no Experience assigned). */
	UFUNCTION(BlueprintPure, Category = "GSTN|Experience")
	bool IsSimulationHeld() const { return bHeld; }

	/** Ask before a simulation tick. False = skip this tick; the refusal is counted on the manager. */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Experience")
	bool ShouldRunSimulationTick();

	/** Report a tick that ran, so one that ran while held is counted as a leak. */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Experience")
	void ReportSimulationTickRan();

	/** Hold or release against a given manager: BeginPlay's work, public so tests and late-added gates can use it. */
	void BindToManager(UGSTN_ExperienceManagerComponent* Manager);

	/** Blueprint hook for presentation (spawn-in effect, input enable). Not for simulation data: that was final before this. */
	UPROPERTY(BlueprintAssignable, Category = "GSTN|Experience")
	FGSTN_OnExperienceGateOpenedDynamic OnGateOpened;

	/** C++ twin of OnGateOpened, fired first. */
	FGSTN_OnExperienceGateOpenedNative OnGateOpenedNative;

private:
	bool bHeld = false;
	TWeakObjectPtr<UGSTN_ExperienceManagerComponent> BoundManager;
};

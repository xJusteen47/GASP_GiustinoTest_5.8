// Combat presentation (GAS-NPP fork): the presentation events of a fighter (GSTNPresentation), read from its presented state. Never read by the simulation.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rules/GSTN_KitExtension.h"
#include "GSTN_PresentationLogic.h"
#include "GSTN_CombatPresentationEvents.generated.h"

class UGSTN_PresentationTable;
class UGSTN_PresentationComponent;

/**
 * The kit's presentation table (kit or weapon Extensions): its dust, sparks, sounds and shakes per presentation event (polish M0). A skin
 * puts its own table over it at a higher priority (UGSTN_PresentationComponent::SetTableLayer, source "Skin"); a kit without it uses the
 * component's DefaultTable. Data only, never read by the simulation.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_PresentationKitExtension : public FGSTN_KitExtension
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	TObjectPtr<UGSTN_PresentationTable> Table;
};

/**
 * The fighter's adapter to GSTNPresentation: each frame, after the movement (game thread), it copies what the presentation events need
 * from the presented state into a FGSTN_PresentationSnapshot (Mover velocity and up, the combat state view: air, run stage, reaction,
 * splat, the running move and its start frame, the confirmed last hit; the run skid's phase; the ability system's owned tags) and hands
 * it to the character's UGSTN_PresentationComponent, which derives, dedupes and plays the events. It also keeps the component's "Kit"
 * table layer on the active kit's FGSTN_PresentationKitExtension.
 *
 * Runs on every machine (server included: the Blueprint events fire there too, effects do not) from that machine's own state, so nothing
 * is replicated for it. UGSTN_CombatSimComponent adds it at BeginPlay: no Blueprint wiring.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class GSTNCOMBATNP_API UGSTN_CombatPresentationEvents : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_CombatPresentationEvents();

	/** Adds the component to Owner once (game thread, outside the simulation) */
	static UGSTN_CombatPresentationEvents* FindOrAdd(AActor* Owner);

	/** Off = no snapshot is submitted (events reported by hand still play) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	bool bEnabled = true;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** This frame's snapshot of Owner (false when it has no combat state yet). Reads only */
	static bool ReadSnapshot(const AActor* Owner, FGSTN_PresentationSnapshot& Out);

private:
	UPROPERTY(Transient)
	TObjectPtr<UGSTN_PresentationComponent> Presentation;

	TWeakObjectPtr<const UObject> AppliedKit;
};

// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSTN_SurfaceTypes.h"
#include "GSTN_SurfaceBehaviourComponent.generated.h"

class AActor;
class UPrimitiveComponent;

/**
 * Marks the colliders of an actor as surfaces (traversal-surfaces.md §2.4, step 1 of the resolution order): a level wall that
 * forbids wall slide, a climb-only wall, a trampoline. Add it to the actor, fill Default, and name the colliders that differ in
 * PerComponent (one component per actor: with several, only the one with the lowest name is read, and the validator reports an error).
 *
 * Static configuration: the fields are BlueprintReadOnly and nothing writes them during a match, because the simulation of every
 * machine resolves surfaces from them and must read the same values (CLAUDE.md rule 1).
 */
UCLASS(ClassGroup = (GSTN), meta = (BlueprintSpawnableComponent, DisplayName = "GSTN Surface Behaviour"))
class GSTNSURFACECORE_API UGSTN_SurfaceBehaviourComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_SurfaceBehaviourComponent();

	/** What every collider of the owner offers, unless PerComponent names it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
	FGSTN_SurfaceBehaviour Default;

	/** Overrides by collider name, for an actor whose faces behave differently (a box with one slideable side) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
	TMap<FName, FGSTN_SurfaceBehaviour> PerComponent;

	/** The behaviour of one of the owner's colliders: its PerComponent entry, else Default */
	const FGSTN_SurfaceBehaviour& GetBehaviourFor(const UPrimitiveComponent* Collider) const;

	/** The behaviour of one of the owner's colliders: its PerComponent entry, else Default */
	UFUNCTION(BlueprintPure, Category = "Surface", meta = (DisplayName = "Get Behaviour For"))
	FGSTN_SurfaceBehaviour K2_GetBehaviourFor(const UPrimitiveComponent* Collider) const { return GetBehaviourFor(Collider); }

	/**
	 * The surface component the resolution reads for an actor. With a single one (the supported setup) it is that one; with
	 * several it is the one with the lowest name, so every machine picks the same: the owner's component set has no stable
	 * order, and a pick by iteration order could differ between server and client
	 */
	static const UGSTN_SurfaceBehaviourComponent* FindFor(const AActor* Owner);

	/**
	 * Findings shared by IsDataValid and the tests: invalid launch parameters, PerComponent names that are not primitive
	 * components of the owner (only when an owner is known) and a second surface component on the owner. All are errors; the
	 * warning list is kept for findings that do not change what is simulated and is empty today
	 */
	void CollectFindings(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

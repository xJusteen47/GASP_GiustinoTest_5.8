// Gameplay Ability System setup (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSTN_AbilityInputComponent.generated.h"

class UInputMappingContext;
class UNpAbilitySystemComponent;

/**
 * Connects the local player's Enhanced Input to the pawn's UNpAbilitySystemComponent.
 *
 * The fork activates abilities from their ActivationInputs: the ability system binds the input actions listed in the
 * given mapping contexts (which must also be listed in Project Settings > Ability Simulation Settings) and sends their
 * state to the server with its predicted input. The keys come from whatever mapping context the player already has
 * (e.g. IMC_Sandbox): these contexts only tell the ability system which actions to listen to, so they are not added to
 * the Enhanced Input subsystem unless bAddToInputSubsystem is set.
 *
 * Add it to a pawn that has a UNpAbilitySystemComponent. It waits until the pawn is locally controlled and the player
 * controller has its input component, refreshes the ability actor info, then binds once.
 */
UCLASS(ClassGroup = (AbilitySystem), Config = Game, meta = (BlueprintSpawnableComponent))
class GASP_GIUSTINO2_API UGSTN_AbilityInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_AbilityInputComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "Ability Input")
	bool IsAbilityInputBound() const { return bBound; }

	/** Contexts whose input actions activate abilities. Each must be in AbilitySimulationSettings::AbilitySystemMappingContexts */
	UPROPERTY(EditDefaultsOnly, Category = "Ability Input")
	TArray<TObjectPtr<UInputMappingContext>> AbilityMappingContexts;

	/**
	 * Used when AbilityMappingContexts is empty (a component whose Blueprint lists none): loaded at BeginPlay. Config, so the project
	 * names its ability context in Config/DefaultGame.ini ([/Script/GASP_Giustino2.GSTN_AbilityInputComponent]) and no content path
	 * is in C++. An empty list after this is logged as an error.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Ability Input")
	TSoftObjectPtr<UInputMappingContext> DefaultAbilityMappingContext;

	/** Also add the contexts to the local player's Enhanced Input subsystem (needed only if they bring their own keys) */
	UPROPERTY(EditDefaultsOnly, Category = "Ability Input")
	bool bAddToInputSubsystem = false;

	UPROPERTY(EditDefaultsOnly, Category = "Ability Input", meta = (EditCondition = "bAddToInputSubsystem"))
	int32 InputSubsystemPriority = 5;

private:
	bool TryBind();

	UPROPERTY(Transient)
	TObjectPtr<UNpAbilitySystemComponent> AbilitySystem;

	bool bBound = false;
};

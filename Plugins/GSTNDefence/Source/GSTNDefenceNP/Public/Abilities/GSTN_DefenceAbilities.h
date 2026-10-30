// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "GSTN_DefenceAbilities.generated.h"

/**
 * The tech ability (m6 spec 6, a pluggable kit ability): one class per tech kind in the kit's FGSTN_TechSet, started Forced by the model
 * at a successful tech (F8). A thin timeline ability: its frame data (the tech's intangible window, its movement) is data of the class or
 * its Blueprint child; nothing here reads or writes the simulation.
 */
UCLASS(Blueprintable)
class GSTNDEFENCENP_API UGSTN_GA_Tech : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_Tech(const FObjectInitializer& ObjectInitializer);
};

/**
 * The mini ground bounce of a landing from tumble (m6 spec 6, owner revision): started Forced by the model at the contact (F8), with
 * the stick-steered bounce applied by the model through the reaction seam, never by the ability. Thin: frame data only.
 */
UCLASS(Blueprintable)
class GSTNDEFENCENP_API UGSTN_GA_GroundBounce : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_GroundBounce(const FObjectInitializer& ObjectInitializer);
};

/**
 * The get-up roll of a downed entity (m6 spec 6): started Forced by the model from the stick (F8). Thin: frame data only (the roll's
 * movement track and intangible window are data of the class or its child).
 */
UCLASS(Blueprintable)
class GSTNDEFENCENP_API UGSTN_GA_GetupRoll : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_GetupRoll(const FObjectInitializer& ObjectInitializer);
};

/**
 * The neutral get-up (m6 spec 6): the one get-up a command starts, bound to Command.Movement.Jump while the entity is downed
 * (UGSTN_DefenceLibrary::GetupNeutralRequirements, Combat.State.Knockdown required). Thin: frame data only.
 */
UCLASS(Blueprintable)
class GSTNDEFENCENP_API UGSTN_GA_GetupNeutral : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_GetupNeutral(const FObjectInitializer& ObjectInitializer);
};

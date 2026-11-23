// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "Templates/SubclassOf.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UGSTN_LinkRegistry;
struct FGSTN_LinkFragment;

/**
 * The link effects of the roles (M7 G1, FGSTN_LinkRoleDef::LinkEffect: a carry load on the carrier). A role's effect is applied on the frame
 * its link becomes Active on the entity (the applier's F+1 for the target, the response's F+2 or the optimistic accept for the initiator) with
 * the link's Payload / 255 as its SetByCaller magnitude, and removed by its source effect class on the frame the link ends, whatever ended it
 * (a watchdog break, a throw, a refusal of an optimistic accept, the serial of a corrected link). Run by the link tick extension at
 * BeforeTagProjection, as the exit effect is: from the fragment of the previous frame and the one this frame wrote, so a re-simulated frame
 * computes the same operations, and an apply first removes the class (idempotent when an effect survived a rollback).
 *
 * No stored handle: the effect class is the key, so two links of one entity must not name the same class (one per channel at most anyway).
 */
struct GSTNLINKNP_API FGSTN_LinkEffects
{
	/** One operation of a frame on one channel */
	struct FOp
	{
		/** True = apply the role's effect, false = remove it */
		bool bApply = false;
		EGSTN_LinkChannel Channel = EGSTN_LinkChannel::Body;
		uint8 LinkTypeIndex = 0;
		uint8 Role = 0;
		uint8 Payload = 0;
	};

	/**
	 * The operations between two consecutive fragments (null = no fragment): per channel, a role with an effect that is Active in Out and was not
	 * Active in In with the same type, role and serial is applied; one that was and is not any more is removed (removals first). Pure
	 */
	static void ComputeOps(const FGSTN_LinkFragment* In, const FGSTN_LinkFragment* Out, const UGSTN_LinkRegistry* Registry, TArray<FOp, TInlineAllocator<2>>& OutOps);

	/** The SetByCaller magnitude of a payload: Payload / 255, so 0 -> 0 and 255 -> 1 */
	static float Magnitude(uint8 Payload) { return static_cast<float>(Payload) / 255.f; }

	/** Runs ComputeOps on the entity's ability system. Nothing without one */
	static void Step(UAbilitySystemComponent* AbilitySystem, const FGSTN_LinkFragment* In, const FGSTN_LinkFragment* Out, const UGSTN_LinkRegistry* Registry);

	/** The role's effect class, loaded on first use; null when unset or not a gameplay effect */
	static TSubclassOf<UGameplayEffect> ResolveEffectClass(const FGSTN_LinkEffectDef& Effect);

	/** Removes any effect of the class, then applies it with the payload's magnitude. False without an ability system or a class */
	static bool Apply(UAbilitySystemComponent* AbilitySystem, const FGSTN_LinkEffectDef& Effect, uint8 Payload);

	/** Removes every effect of the class from the entity */
	static void Remove(UAbilitySystemComponent* AbilitySystem, const FGSTN_LinkEffectDef& Effect);
};

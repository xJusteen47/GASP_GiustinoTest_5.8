// Test only (M6 F14): a defensive pipeline step added from outside GSTNDefence, as m6 spec 14 "A new defensive pipeline step" describes it.

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitResolution.h"
#include "NativeGameplayTags.h"
#include "GSTN_ExtTestStep_Brace.generated.h"

/** The brace's tags, declared here and nowhere in GSTNDefence or GSTNCombat */
namespace GSTNExtTestDefenceTags
{
	/** A hit type the brace softens */
	GSTNEXTTESTDEFENCE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HitType_Braceable);

	/** The event a braced hit publishes */
	GSTNEXTTESTDEFENCE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Braced);
}

/**
 * Halves the knockback of a braceable hit and keeps the victim from flinching. It writes only to its step result (the runner merges it into the
 * context), reads only the view, and keeps nothing between calls (the runner calls the class default object), so it is deterministic and
 * predictable by the attacker like every pipeline step. It goes before the Parry step: a parry or a block still decides afterwards.
 */
UCLASS(meta = (DisplayName = "Ext Test Brace"))
class GSTNEXTTESTDEFENCE_API UGSTN_ExtTestStep_Brace : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	/** Knockback scale of a braced hit */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defence|Brace", meta = (ClampMin = "0.0"))
	float BracedKnockbackScale = 0.5f;

	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override;
};

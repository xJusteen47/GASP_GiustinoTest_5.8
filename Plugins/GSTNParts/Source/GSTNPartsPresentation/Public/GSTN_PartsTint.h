// Destructible parts, presentation half. Placeholder damage tint (plan P10-min): no material asset, no event replay.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/GSTN_PartsLimits.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/GSTN_PartsState.h"
#include "GSTN_PartsTint.generated.h"

/**
 * Stateless math of the tint. The value of part i is a pure function of the part state: 0 = intact, 1 = broken, in between =
 * PartDamage / DamageAtCap. A function of the state and not of events, so a late join, a relevancy re-entry and a rollback show the
 * right tint with nothing to replay. It is a reversible cosmetic, so it may be fed predicted state (CP §6.5); only the irreversible
 * presentation (hidden bones, chunks) needs the confirmed state.
 */
UCLASS()
class GSTNPARTSPRESENTATION_API UGSTN_PartsTint : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Fills Out with one value per part (PartCount clamped to GSTN_PartsLimits::MaxParts) */
	static void ComputeTint(const FGSTN_PartsState& State, int32 PartCount, TArray<float>& Out);

	/** Tint of one part, 0 for an index outside the state */
	static float GetPartTint(const FGSTN_PartsState& State, int32 PartIndex);
};

/**
 * Writes the tint of a part state into the custom primitive data of every primitive component of the owner, float BaseIndex + part
 * index. A material reads it with a CustomPrimitiveData node, so no dynamic material instance and no new asset is needed.
 * Idempotent: the same state writes the same values, and a write that changes nothing is skipped. Owner-local and cosmetic: it
 * never reads or writes simulation state, and a dedicated server has nothing to show (the caller does not create it there).
 */
UCLASS(ClassGroup = "Combat", meta = (BlueprintSpawnableComponent))
class GSTNPARTSPRESENTATION_API UGSTN_PartsTintComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** First custom primitive data float used (part i goes to BaseIndex + i). Keep BaseIndex + 16 <= 36 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Parts|Presentation", meta = (ClampMin = "0", ClampMax = "20"))
	int32 BaseIndex = 0;

	/** Applies the state to the owner's meshes. Returns how many primitive floats were written */
	int32 ApplyState(const FGSTN_PartsState& State, int32 PartCount);

	/** Last tint computed, for tests and a debug read */
	const TArray<float>& GetLastTint() const { return LastTint; }

private:
	TArray<float> LastTint;
};

// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AttributeSet.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_MeterSetup.generated.h"

class UAbilitySystemComponent;

/**
 * A meter a kit shows and uses: which attributes hold it and how the UI divides it. The meter is identified by a tag under
 * the meter root, so a cost, a UI widget or a super looks it up by tag and no code needs the name of a meter.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATGAS_API FGSTN_MeterSetup
{
	GENERATED_BODY()

	/** Identity of the meter. A leaf under the meter root. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meter", meta = (Categories = "Combat.Meter"))
	FGameplayTag Meter;

	/** Attribute holding the current amount. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meter")
	FGameplayAttribute Value;

	/** Attribute holding the maximum. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meter")
	FGameplayAttribute Max;

	/** Meter points in one bar; bars are only a UI division. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meter", meta = (ClampMin = "1.0"))
	float PointsPerBar = 100.f;

	/** Bars the UI shows. The meter maximum is normally PointsPerBar times this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meter", meta = (ClampMin = "1", ClampMax = "16"))
	int32 Bars = 1;

	/** True when the tag and both attributes are set and the two attributes differ. */
	bool IsValid() const { return Meter.IsValid() && Value.IsValid() && Max.IsValid() && Value != Max && PointsPerBar > 0.f; }
};

/** Stateless readers over meters; they never write, so they are safe anywhere a UI or a rule needs a number. */
UCLASS()
class GSTNCOMBATGAS_API UGSTN_MeterUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The setup whose meter tag is exactly Meter, or an empty one. */
	UFUNCTION(BlueprintPure, Category = "Combat|Meter")
	static bool FindMeterSetup(const TArray<FGSTN_MeterSetup>& Setups, FGameplayTag Meter, FGSTN_MeterSetup& OutSetup);

	/** Full bars contained in Amount, rounded down. */
	UFUNCTION(BlueprintPure, Category = "Combat|Meter")
	static int32 GetFullBars(const FGSTN_MeterSetup& Setup, float Amount);

	/** Current amount of the meter on ASC, 0 when the ASC does not own the attribute. */
	UFUNCTION(BlueprintPure, Category = "Combat|Meter")
	static float GetMeterAmount(const UAbilitySystemComponent* ASC, const FGSTN_MeterSetup& Setup);

	/** Maximum of the meter on ASC, 0 when the ASC does not own the attribute. */
	UFUNCTION(BlueprintPure, Category = "Combat|Meter")
	static float GetMeterMax(const UAbilitySystemComponent* ASC, const FGSTN_MeterSetup& Setup);
};

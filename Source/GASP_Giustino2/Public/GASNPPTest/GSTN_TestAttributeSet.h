// GAS + NPP + Mover network test harness.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GSTN_TestAttributeSet.generated.h"

#define GSTN_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/** Placeholder attributes used by the GAS-NPP network tests. Replication is handled by Network Prediction. */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_TestAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UGSTN_TestAttributeSet();

	UPROPERTY(BlueprintReadOnly, Category = "Test")
	FGameplayAttributeData Health;
	GSTN_ATTRIBUTE_ACCESSORS(UGSTN_TestAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Test")
	FGameplayAttributeData MaxHealth;
	GSTN_ATTRIBUTE_ACCESSORS(UGSTN_TestAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "Test")
	FGameplayAttributeData Stamina;
	GSTN_ATTRIBUTE_ACCESSORS(UGSTN_TestAttributeSet, Stamina)

	UPROPERTY(BlueprintReadOnly, Category = "Test")
	FGameplayAttributeData MaxStamina;
	GSTN_ATTRIBUTE_ACCESSORS(UGSTN_TestAttributeSet, MaxStamina)

	// Probe attributes (M3 spikes). The two counters are bumped by instant effects from the triggered probe abilities, so a
	// double or phantom activation shows in the synced attribute state. The two scales are driven by infinite effects and
	// read every tick by the stats probe on UGSTN_GASNPPTestComponent.
	UPROPERTY(BlueprintReadOnly, Category = "Test|Probe")
	FGameplayAttributeData ProbeEventCount;
	GSTN_ATTRIBUTE_ACCESSORS(UGSTN_TestAttributeSet, ProbeEventCount)

	UPROPERTY(BlueprintReadOnly, Category = "Test|Probe")
	FGameplayAttributeData ProbeTagCount;
	GSTN_ATTRIBUTE_ACCESSORS(UGSTN_TestAttributeSet, ProbeTagCount)

	UPROPERTY(BlueprintReadOnly, Category = "Test|Probe")
	FGameplayAttributeData ProbeSpeedScale;
	GSTN_ATTRIBUTE_ACCESSORS(UGSTN_TestAttributeSet, ProbeSpeedScale)

	UPROPERTY(BlueprintReadOnly, Category = "Test|Probe")
	FGameplayAttributeData ProbeGravityScale;
	GSTN_ATTRIBUTE_ACCESSORS(UGSTN_TestAttributeSet, ProbeGravityScale)

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

private:
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
};

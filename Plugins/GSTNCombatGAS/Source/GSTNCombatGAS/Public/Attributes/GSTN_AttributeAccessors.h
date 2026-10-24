// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "AttributeSet.h"
#include "AbilitySystemComponent.h"

/** Getter, value getter, value setter and initter for one attribute of an attribute set of this plugin. */
#define GSTN_COMBAT_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

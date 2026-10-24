// Combat simulation (GAS-NPP fork).

#include "Movement/GSTN_StatsMovementModifier.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/GSTN_CombatAttributeSet.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "GSTN_CombatTags.h"
#include "Input/GSTN_CombatSettings.h"
#include "MoverComponent.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Stats/GSTN_StatsScaling.h"

namespace
{
	/** The value nothing in the simulation writes: the archetype (the Blueprint's template), else the class default */
	const UObject& GetBase(const UObject& Object)
	{
		const UObject* Archetype = Object.GetArchetype();
		return Archetype ? *Archetype : *Object.GetClass()->GetDefaultObject();
	}

	int32 ApplyToObject(UObject& Object, TConstArrayView<FName> Names, float Factor)
	{
		return FGSTN_StatsScaling::ApplyScaledFloats(Object, GetBase(Object), Names, Factor);
	}
}

FGSTN_StatsMovementModifier::FGSTN_StatsMovementModifier()
{
	// Never ends
	DurationMs = -1.f;
}

bool FGSTN_StatsMovementModifier::ResolveSpeedFactor(const UAbilitySystemComponent& ASC, float& OutFactor)
{
	bool bFound = false;
	const float WalkSpeed = ASC.GetGameplayAttributeValue(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute(), bFound);
	if (!bFound)
	{
		return false;
	}
	OutFactor = ASC.HasMatchingGameplayTag(GSTNCombatTags::Lock_Movement)
		? 0.f
		: FGSTN_StatsScaling::ComputeFactor(WalkSpeed, UGSTN_CombatSettings::Get().StatsReferenceWalkSpeed);
	return true;
}

int32 FGSTN_StatsMovementModifier::ApplySpeedFactor(UMoverComponent& MoverComp, float Factor)
{
	static const FName MaxSpeedName(TEXT("MaxSpeed"));
	static const FName MaxSpeedOverrideName(TEXT("MaxSpeedOverride"));

	const TArray<FName>& GaitNames = UGSTN_CombatSettings::Get().StatsSpeedProperties;
	int32 Written = 0;

	if (UObject* Shared = Cast<UObject>(MoverComp.FindSharedSettings_Mutable<UCommonLegacyMovementSettings>()))
	{
		Written += ApplyToObject(*Shared, MakeArrayView(&MaxSpeedName, 1), Factor);
	}

	for (const TPair<FName, TObjectPtr<UBaseMovementMode>>& Entry : MoverComp.MovementModes)
	{
		UObject* Mode = Entry.Value;
		if (!Mode)
		{
			continue;
		}

		// MaxSpeedOverride < 0 means "use the shared settings": scaling it would turn that into a speed
		double AuthoredOverride = -1.0;
		if (FGSTN_StatsScaling::ReadFloat(GetBase(*Mode), MaxSpeedOverrideName, AuthoredOverride) && AuthoredOverride >= 0.0)
		{
			Written += ApplyToObject(*Mode, MakeArrayView(&MaxSpeedOverrideName, 1), Factor);
		}
		Written += ApplyToObject(*Mode, GaitNames, Factor);
	}
	return Written;
}

void FGSTN_StatsMovementModifier::OnPreMovement(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep)
{
	if (!MoverComp)
	{
		return;
	}
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MoverComp->GetOwner());
	float Factor = 1.f;
	if (ASC && ResolveSpeedFactor(*ASC, Factor))
	{
		ApplySpeedFactor(*MoverComp, Factor);
	}

	// The gravity phase of the jump state of this frame (the combat model ticked before Mover): hold, cut, fall, fast fall
	if (UGSTN_CombatSimComponent* Combat = MoverComp->GetOwner() ? MoverComp->GetOwner()->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr)
	{
		Combat->ApplyJumpGravity(*MoverComp, TimeStep.StepMs * 0.001);
	}
}

FMovementModifierBase* FGSTN_StatsMovementModifier::Clone() const
{
	return new FGSTN_StatsMovementModifier(*this);
}

void FGSTN_StatsMovementModifier::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
}

UScriptStruct* FGSTN_StatsMovementModifier::GetScriptStruct() const
{
	return FGSTN_StatsMovementModifier::StaticStruct();
}

FString FGSTN_StatsMovementModifier::ToSimpleString() const
{
	return TEXT("Stats Movement Modifier");
}

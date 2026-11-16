// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#pragma once

#include "CoreMinimal.h"
#include "Debug/GSTN_InputHistorySource.h"

class APawn;
class UGSTN_CombatSimComponent;

/**
 * The combat component as a source of the input display (GSTNInputDebug): its input history through the const accessor
 * UGSTN_CombatSimComponent::GetInputHistoryView, the command it requested last through the request the sync state holds, the six standard
 * combat buttons and the plugin's standard command set. Read-only: it never writes or listens to the simulation.
 *
 * The command comes from the request and not from UGSTN_SimEventSubsystem because that event is published on the authority only, so a
 * predicting client would never see its own commands; the request (the frame it was made on and the binding it chose) is in the sync
 * state on every machine.
 *
 * Registered with FGSTN_InputSourceRegistry by the module as the finder of the combat component and as the default profile of the display.
 */
class FGSTN_CombatInputSource : public IGSTN_InputHistorySource
{
public:
	explicit FGSTN_CombatInputSource(UGSTN_CombatSimComponent& InComponent);

	/** The source of the pawn's combat component, null when it has none. The finder the module registers */
	static TSharedPtr<IGSTN_InputHistorySource> Find(APawn& Pawn);

	/** The six standard combat buttons in bit order (the letters A to F of the notation) */
	static TArray<FGameplayTag> GetCombatButtonTags();

	//~ IGSTN_InputHistorySource
	virtual bool IsSourceValid() const override;
	virtual const FGSTN_InputHistory* GetInputHistoryView() const override;
	virtual bool GetLatestCommandRequest(int32& OutFrame, FGameplayTag& OutCommand) const override;
	virtual void GetButtonTags(TArray<FGameplayTag>& OutTags) const override;
	virtual bool GetScriptSlots(TArray<FGSTN_InputSlotSpec>& OutSlots) const override;
	virtual FText GetSourceLabel() const override;
	virtual const UObject* GetSourceObject() const override;

private:
	TWeakObjectPtr<UGSTN_CombatSimComponent> Component;
};

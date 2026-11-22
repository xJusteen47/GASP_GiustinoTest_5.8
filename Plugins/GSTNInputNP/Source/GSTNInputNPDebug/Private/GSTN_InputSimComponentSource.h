// Development tooling of GSTN Input NP (PIE and development builds, never Shipping).

#pragma once

#include "CoreMinimal.h"
#include "Debug/GSTN_InputHistorySource.h"

class APawn;
class UGSTN_InputSimComponent;

/**
 * The standalone input component as a source of the input display (GSTNInputDebug): its input history and the command it requested last,
 * both read from the sync state through the component's public const accessors, and the buttons of its input config. It offers no command
 * set: which slots a project binds is data of the binding set, so the scripted box plays the notation and says no set is known.
 *
 * Read-only: it never writes or listens to the simulation. Registered with FGSTN_InputSourceRegistry by the module.
 */
class FGSTN_InputSimComponentSource : public IGSTN_InputHistorySource
{
public:
	explicit FGSTN_InputSimComponentSource(UGSTN_InputSimComponent& InComponent);

	/** The source of the pawn's input component, null when it has none. The finder the module registers */
	static TSharedPtr<IGSTN_InputHistorySource> Find(APawn& Pawn);

	//~ IGSTN_InputHistorySource
	virtual bool IsSourceValid() const override;
	virtual const FGSTN_InputHistory* GetInputHistoryView() const override;
	virtual bool GetLatestCommandRequest(int32& OutFrame, FGameplayTag& OutCommand) const override;
	virtual void GetButtonTags(TArray<FGameplayTag>& OutTags) const override;
	virtual FText GetSourceLabel() const override;
	virtual const UObject* GetSourceObject() const override;

private:
	TWeakObjectPtr<UGSTN_InputSimComponent> Component;
};

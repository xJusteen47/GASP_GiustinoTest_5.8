// Pure combat logic (no GAS, no Network Prediction).

#include "Hits/GSTN_HurtboxTypes.h"

#if WITH_EDITOR
#include "Input/GSTN_CombatSettings.h"
#include "Misc/DataValidation.h"
#include "Rules/GSTN_CombatRules.h"
#include "Rules/GSTN_CombatRulesValidation.h"

EDataValidationResult UGSTN_HurtboxSetDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FGSTN_ReactionMessage> Messages;
	FGSTN_CombatRulesValidation::ValidateHurtboxSet(*this, Messages);
	for (const FGSTN_ReactionMessage& Message : Messages)
	{
		if (Message.bError)
		{
			Context.AddError(FText::FromString(Message.Text));
			Result = EDataValidationResult::Invalid;
		}
		else
		{
			Context.AddWarning(FText::FromString(Message.Text));
		}
	}

	// The matrix is for the designer, who reads which hurtbox staggers and how far against the rules the match plays by default
	if (!Hurtboxes.IsEmpty())
	{
		if (const UGSTN_CombatRules* Rules = UGSTN_CombatSettings::Get().ResolveDefaultRules())
		{
			Context.AddMessage(EMessageSeverity::Info, FText::FromString(TEXT("Reaction matrix against the default rules:\n") + FGSTN_CombatRulesValidation::PrintHurtboxSetMatrix(*this, *Rules)));
		}
	}

	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

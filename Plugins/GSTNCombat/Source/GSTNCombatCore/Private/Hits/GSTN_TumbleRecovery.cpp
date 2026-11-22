// Pure combat logic (no GAS, no Network Prediction).

#include "Hits/GSTN_TumbleRecovery.h"

bool FGSTN_TumbleRecoveryResolved::IsAfterHitstun(const FGSTN_TumbleRecoveryRules* Rules, EGSTN_TumbleRecoveryOverride Override)
{
	switch (Override)
	{
	case EGSTN_TumbleRecoveryOverride::UntilGround:
		return false;
	case EGSTN_TumbleRecoveryOverride::AfterHitstun:
		return true;
	default:
		// No rules = the behaviour before the rule existed
		return Rules && Rules->Mode == EGSTN_TumbleRecovery::AfterHitstun;
	}
}

const FGameplayTagContainer& FGSTN_TumbleRecoveryResolved::GetAllowedCommands(const FGSTN_TumbleRecoveryRules* Rules, const FGSTN_TumbleRecoveryExtension* Kit)
{
	static const FGameplayTagContainer Empty;
	return Kit ? Kit->AllowedCommands : Rules ? Rules->AllowedCommands : Empty;
}

FGSTN_TumbleRecoveryResolved FGSTN_TumbleRecoveryResolved::Resolve(const FGSTN_TumbleRecoveryRules* Rules, const FGSTN_TumbleRecoveryExtension* Kit, EGSTN_TumbleRecoveryOverride Override)
{
	FGSTN_TumbleRecoveryResolved Out;
	Out.bAfterHitstun = IsAfterHitstun(Rules, Override);
	Out.AllowedCommands = GetAllowedCommands(Rules, Kit);
	return Out;
}

bool FGSTN_TumbleRecoveryResolved::IsCommandAllowed(const FGameplayTagContainer& Allowed, const FGameplayTag& Command)
{
	return Allowed.IsEmpty() || Command.MatchesAny(Allowed);
}

// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "Actions/GSTN_LinkActionProvider.h"

bool UGSTN_LinkActionTable::SetAction(int16 Id, const FGSTN_LinkActionInfo& Info)
{
	if (Id <= 0)
	{
		return false;
	}
	Actions.Add(Id, Info);
	return true;
}

bool UGSTN_LinkActionTable::ResolveLinkAction(int16 Id, FGSTN_LinkActionInfo& OutInfo) const
{
	if (const FGSTN_LinkActionInfo* Found = Actions.Find(Id))
	{
		OutInfo = *Found;
		return true;
	}
	return false;
}

// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_LinkKitExtension.h"

#include "GSTN_LinkTags.h"

const FSoftClassPath* FGSTN_LinkThrowSet::Find(const FGameplayTag& ThrowCommand) const
{
	using namespace GSTNLinkTags;
	return ThrowCommand == Command_Link_Throw_Forward ? &Forward
		: ThrowCommand == Command_Link_Throw_Back ? &Back
		: ThrowCommand == Command_Link_Throw_Up ? &Up
		: ThrowCommand == Command_Link_Throw_Down ? &Down : nullptr;
}

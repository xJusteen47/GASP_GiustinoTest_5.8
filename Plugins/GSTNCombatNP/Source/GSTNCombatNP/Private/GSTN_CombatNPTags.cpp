// Native tags of the combat Network Prediction adapter.

#include "GSTN_CombatNPTags.h"

namespace GSTNCombatNPTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mover_Frozen, "Combat.Mover.Frozen", "The pawn is held still by a hitstop freeze (FGSTN_LayeredMove_Freeze). A Mover tag, never granted to the ability system");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_FreezeSeconds, "Combat.SetByCaller.FreezeSeconds", "Freeze duration in seconds, read by UGSTN_FreezeMoveCreator");
}

// Native tags of the combat Network Prediction adapter.

#include "GSTN_CombatNPTags.h"

namespace GSTNCombatNPTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mover_Frozen, "Combat.Mover.Frozen", "The pawn is held still by a hitstop freeze (FGSTN_LayeredMove_Freeze). A Mover tag, never granted to the ability system");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_FreezeSeconds, "Combat.SetByCaller.FreezeSeconds", "Freeze duration in seconds, read by UGSTN_FreezeMoveCreator");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_MoveVelocityX, "Combat.SetByCaller.MoveVelocityX", "X of the world velocity of a knockback or push back move (cm/s)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_MoveVelocityY, "Combat.SetByCaller.MoveVelocityY", "Y of the world velocity of a knockback or push back move (cm/s)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_MoveVelocityZ, "Combat.SetByCaller.MoveVelocityZ", "Z of the world velocity of a knockback or push back move (cm/s)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_MoveDecay, "Combat.SetByCaller.MoveDecay", "Decay per frame of a knockback or push back move, as a fraction");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_MoveGravityScale, "Combat.SetByCaller.MoveGravityScale", "Gravity scale of a knockback move");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_MoveSeconds, "Combat.SetByCaller.MoveSeconds", "Lifetime in seconds of a knockback or push back move");
}

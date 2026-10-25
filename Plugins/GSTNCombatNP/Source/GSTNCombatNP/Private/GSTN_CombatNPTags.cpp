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
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_LaunchResetAirJumps, "Combat.SetByCaller.LaunchResetAirJumps", "1 when a surface launch gives the air jumps back (FGSTN_LayeredMove_Launch)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_LaunchResetCoyote, "Combat.SetByCaller.LaunchResetCoyote", "1 when a surface launch restarts the coyote window (FGSTN_LayeredMove_Launch)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventHit_Staggered, "Combat.Event.Hit.Staggered", "Tag of a HitLanded or HitReceived event: the hit staggered the victim");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventHit_Tumble, "Combat.Event.Hit.Tumble", "Tag of a HitLanded or HitReceived event: the hit made the victim tumble");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventHit_Reaction_StaggerInPlace, "Combat.Event.Hit.Reaction.StaggerInPlace", "Tag of a HitLanded or HitReceived event: the reaction tier was a stagger in place");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventHit_Reaction_PushBack, "Combat.Event.Hit.Reaction.PushBack", "Tag of a HitLanded or HitReceived event: the reaction tier was a push back");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(EventHit_Reaction_Launch, "Combat.Event.Hit.Reaction.Launch", "Tag of a HitLanded or HitReceived event: the reaction tier was a launch");
}

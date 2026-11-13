// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GSTN_AnimTags.h"

namespace GSTNAnimTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Anim, "Anim", "Root of the animation tags (GSTNAnimation)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Set, "Anim.Set", "Feature animation slots: one leaf per feature, declared by the feature's owner");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Trait, "Anim.Trait", "Cosmetic traits that change some animations; leaves are ini content");
}

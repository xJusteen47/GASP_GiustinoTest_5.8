// Modular cosmetics, pure layer.

#include "GSTN_CosmeticTags.h"

namespace GSTNCosmeticTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Slot, "Cosmetic.Slot", "Root of the classes of decoration points an item or the body offers");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Slot_Grip, "Cosmetic.Slot.Grip", "Anywhere on the grip; fallback of its leaves");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Slot_Grip_Pommel, "Cosmetic.Slot.Grip.Pommel", "End of the grip");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Slot_Guard, "Cosmetic.Slot.Guard", "Cross guard or the equivalent of a guard");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Slot_BladeBase, "Cosmetic.Slot.BladeBase", "Where the blade, head or barrel starts");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Slot_Tip, "Cosmetic.Slot.Tip", "Tip of the item, back from the reach line");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Slot_Charm, "Cosmetic.Slot.Charm", "Hanging charm point");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Slot_Body, "Cosmetic.Slot.Body", "Bone sockets of the game skeleton");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Slot_Body_Shoulder, "Cosmetic.Slot.Body.Shoulder", "Shoulder pads");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Slot_Body_Belt, "Cosmetic.Slot.Body.Belt", "Belt and hips");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Slot_Body_Wrist, "Cosmetic.Slot.Body.Wrist", "Bracers");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Slot_Body_Back, "Cosmetic.Slot.Body.Back", "Upper back");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Spline, "Cosmetic.Spline", "Root of the classes of decorative curves an item offers");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Spline_Spine, "Cosmetic.Spline.Spine", "Back of a blade or a shaft");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Spline_Edge, "Cosmetic.Spline.Edge", "Cutting edge, only if a theme wants it");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Spline_Band, "Cosmetic.Spline.Band", "Ring around a grip");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Spline_Rim, "Cosmetic.Spline.Rim", "Rim of a shield");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Theme, "Cosmetic.Theme", "Root of theme identities (designer content)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Style, "Cosmetic.Style", "Root of style channel identities");
}

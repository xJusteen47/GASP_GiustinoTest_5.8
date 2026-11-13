// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#include "GSTN_SurfaceTags.h"

namespace GSTNSurfaceTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Surface, "Surface", "Root of the surface tags owned by GSTNSurface. Static data on colliders, never in a sync state");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Type, "Surface.Type", "Root of the impact classes of surfaces. Leaves are ini content (Config/Tags/GSTNSurface.ini); the simulation stores only their table index");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Behaviour, "Surface.Behaviour", "Root of the traversal behaviours a surface offers. Static data, read through tag queries by consumers");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Behaviour_WallSlide, "Surface.Behaviour.WallSlide", "The surface can be wall slid on. Static data on the collider; the absence of this tag forbids wall slide");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Behaviour_WallClimb, "Surface.Behaviour.WallClimb", "The surface can be wall climbed. Static data on the collider");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Behaviour_Launch, "Surface.Behaviour.Launch", "Landing on the surface launches a non-tumbling character with the surface's launch parameters. Static data");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Behaviour_Stand, "Surface.Behaviour.Stand", "The surface is a walkable top a character can stand on, such as a platform top. Static data");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Behaviour_Slippery, "Surface.Behaviour.Slippery", "Reserved: a low-friction surface. Content may carry it; no reader yet");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Behaviour_OneWay, "Surface.Behaviour.OneWay", "Reserved: a surface that blocks from one side only. Content may carry it; no reader yet");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Launched, "Surface.Event.Launched", "Local presentation trigger raised when a surface launch is applied. Never read by the simulation, never replicated");

	void GetDeclaredTags(TArray<FGameplayTag>& OutTags)
	{
		OutTags.Append({
			Root_Surface, Root_Type, Root_Behaviour,
			Behaviour_WallSlide, Behaviour_WallClimb, Behaviour_Launch, Behaviour_Stand, Behaviour_Slippery, Behaviour_OneWay,
			Event_Launched });
	}
}

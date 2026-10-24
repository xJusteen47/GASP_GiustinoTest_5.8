#include "GSTN_CameraTags.h"

namespace GSTNCameraTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Camera, "Camera", "Root of the camera tags owned by GSTNCamera. Local presentation only: never granted by the simulation, never read by it");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mode_Follow, "Camera.Mode.Follow", "Identity of the far follow mode (Mario 3D World style). A mode definition names exactly one Camera.Mode leaf");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mode_Free, "Camera.Mode.Free", "Identity of the free camera mode (Astro Bot style, camera.md 2.4): calm vertical follow, look-ahead, recentre. Used where no section guides the camera");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mode_Shoulder, "Camera.Mode.Shoulder", "Identity of the over-the-shoulder aim mode");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mode_Section, "Camera.Mode.Section", "Identity of the mode driven by the active designer section");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mode_Duel, "Camera.Mode.Duel", "Identity of the Tekken-style PvP framing of two subjects");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mode_Arena, "Camera.Mode.Arena", "Identity of the shared arena framing for 3-4 players (Power Stone style): all fighters in one elevated 3/4 view with an arena-relative yaw");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Request_Aim, "Camera.Request.Aim", "Granted by an aiming ability or weapon kit (ActivationOwnedTags or an effect) while it aims; read locally to choose Shoulder. Predicted on the owner, never read by the simulation");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Request_Scope, "Camera.Request.Scope", "Parent of the per-scope requests granted by a scoped weapon's aim ability while aiming; local presentation only");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Section, "Camera.Section", "Root of the section identities; leaves are designer ini content (Camera.Section.<Name>) and are added to the local query container while that section is active");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_InSection, "Camera.State.InSection", "Computed locally by the camera component while the view target is inside a section. Never granted to an ability system");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_ShoulderBlocked, "Camera.State.ShoulderBlocked", "Computed locally: the active section forbids Shoulder. Never granted to an ability system");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_OneOpponent, "Camera.State.OneOpponent", "Computed locally: exactly one opponent is registered on the camera component (the default rules choose Duel). Never granted to an ability system");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_ManyOpponents, "Camera.State.ManyOpponents", "Computed locally: two or more opponents are registered (the default rules choose Arena). Never granted to an ability system");
}

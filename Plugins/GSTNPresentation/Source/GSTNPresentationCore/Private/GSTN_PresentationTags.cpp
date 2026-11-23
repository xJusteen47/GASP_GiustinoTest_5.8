// Presentation events and their effects (no GAS, no Network Prediction, no Mover).

#include "GSTN_PresentationTags.h"

namespace GSTNPresentationTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event, "Presentation.Event", "Root of the presentation events (GSTNPresentation): never in a sync state");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Jump, "Presentation.Event.Jump", "Leaving the ground or an air jump. Magnitude: upward speed (cm/s)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Jump_Ground, "Presentation.Event.Jump.Ground", "Jump from the ground. Magnitude: upward speed (cm/s)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Jump_Air, "Presentation.Event.Jump.Air", "Air jump. Magnitude: upward speed (cm/s)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Land, "Presentation.Event.Land", "Landing (not from a tumble). Magnitude: fall speed at contact (cm/s)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(RunStart, "Presentation.Event.RunStart", "The run starts from below stage 1. Magnitude: horizontal speed (cm/s)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StageUp, "Presentation.Event.StageUp", "The run goes up a stage. Magnitude: the new stage (2, 3...)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(StageDown, "Presentation.Event.StageDown", "The run goes down a stage, to 0 included (ends a sustained effect such as sprint lines). Magnitude: the new stage");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(RunStopPlant, "Presentation.Event.RunStopPlant", "The run stop clip begins: the front foot plants and slides. Magnitude: horizontal speed (cm/s)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(StopRelease, "Presentation.Event.StopRelease", "The run ends without a skid (stick released). Magnitude: horizontal speed before (cm/s)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skid, "Presentation.Event.Skid", "Run skid phases");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skid_Enter, "Presentation.Event.Skid.Enter", "Skid starts. Magnitude: sliding speed (cm/s)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skid_Loop, "Presentation.Event.Skid.Loop", "Skid reaches its sliding loop. Magnitude: sliding speed (cm/s)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skid_Exit, "Presentation.Event.Skid.Exit", "Skid comes to rest and turns. Magnitude: sliding speed (cm/s)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Turn180, "Presentation.Event.Turn180", "Running again the other way after a skid. Magnitude: horizontal speed (cm/s)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hit, "Presentation.Event.Hit", "Hit received (confirmed). Magnitude: hitstun frames");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Splat, "Presentation.Event.Splat", "Pinned to a surface by a splat. Magnitude: speed before (cm/s)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(WallBounce, "Presentation.Event.WallBounce", "A reacting victim bounces off a wall (a ceiling: the .Ceiling child). Magnitude: speed into the surface (cm/s)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(WallBounce_Ceiling, "Presentation.Event.WallBounce.Ceiling", "The bounce is off a ceiling (a cue table without a row of its own plays the WallBounce row). Direction: down");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GroundBounce, "Presentation.Event.GroundBounce", "A reacting victim bounces off the ground. Magnitude: fall speed (cm/s)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Bump, "Presentation.Event.Bump", "A slow reacting victim taps a wall or ceiling: the small default bump, not a bounce. Magnitude: 0");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Bump_Wall, "Presentation.Event.Bump.Wall", "The bump is off a wall. Direction: away from it");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Bump_Ceiling, "Presentation.Event.Bump.Ceiling", "The bump is off a ceiling");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tumble, "Presentation.Event.Tumble", "Tumble (launched hard enough to tumble)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tumble_Start, "Presentation.Event.Tumble.Start", "Tumble starts (Combat.State.Tumble added)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tumble_End, "Presentation.Event.Tumble.End", "Tumble ends (Combat.State.Tumble removed)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Knockdown, "Presentation.Event.Knockdown", "Downed on the ground");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GetUp, "Presentation.Event.GetUp", "Getting up from the ground");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Roll, "Presentation.Event.Roll", "Roll (downed roll or tech roll)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Dodge, "Presentation.Event.Dodge", "Dodge (ground long jump or air dodge)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shield, "Presentation.Event.Shield", "Shield events");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shield_On, "Presentation.Event.Shield.On", "Shield raised");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shield_Hit, "Presentation.Event.Shield.Hit", "A hit blocked by the shield");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shield_Break, "Presentation.Event.Shield.Break", "Shield broken");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Footwear, "Presentation.Footwear", "Footwear of a skin: the contact layer of footsteps and their rumble (surface audio). Never in a sync state");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Footwear_Clay, "Presentation.Footwear.Clay", "Soft clay-like contact: the stock GASP footstep (default of a skin without footwear)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Footwear_Metal, "Presentation.Footwear.Metal", "Metal boots (knight): clank, heavier rumble");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Footwear_Barefoot, "Presentation.Footwear.Barefoot", "Bare feet: soft slap, very light rumble");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Footwear_ShoeSole, "Presentation.Footwear.ShoeSole", "Rubber shoe sole: squeak and tap");
}

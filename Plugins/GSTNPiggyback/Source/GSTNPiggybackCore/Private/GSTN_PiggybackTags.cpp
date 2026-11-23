// Piggyback, pure part (no GAS, no Network Prediction, no Mover).

#include "GSTN_PiggybackTags.h"

namespace GSTNPiggybackTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Type_Mount, "Combat.Link.Type.Mount", "Link type identity of a mount (an ally riding a carrier), on its definition asset, on mount hits and on events. Declared by the piggyback plugin");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Role_Carrier, "Combat.Link.Role.Carrier", "The carrying role of a mount, the link's target, which keeps acting. Projected from the link state's Role by the link tick");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Role_Rider, "Combat.Link.Role.Rider", "The riding role of a mount, the link's initiator, parked on the carrier and acting through its own bindings. Projected from the link state's Role");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HitType_Interaction_Mount, "Combat.HitType.Interaction.Mount", "Interaction hit type of the mount ability's hitbox, which makes a mount request through the LinkRequest step. Set on the hit data");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Riding, "Combat.State.Riding", "The rider's mount is Active. Projected from the link state, removed by the same projection on any break, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Carrying, "Combat.State.Carrying", "The carrier carries a rider. An ordinary tag granted by the carry load effect and removed with it");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_MountPending, "Combat.State.MountPending", "The rider's pending phase before its mount is accepted, for cosmetics and locks. Projected from the link state's Phase, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(PairMove_ThrowRider, "Combat.PairMove.ThrowRider", "Identity of the reference pair move where the carrier throws its rider. On the pair-move definition, carried by its events");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(PairMove_BoostJump, "Combat.PairMove.BoostJump", "Identity of the reference pair move where the rider jumps off the carrier with a boost. On the pair-move definition, carried by its events");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Link_Mounted, "Combat.Event.Link.Mounted", "A mount formed. Authoritative event published once per key on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Link_MountRefused, "Combat.Event.Link.MountRefused", "A mount request was refused; the payload carries the verdict. Authoritative event published once per key on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_PairMove_Started, "Combat.Event.PairMove.Started", "A pair move started on both sides. Authoritative event published once per key on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_PairMove_Aborted, "Combat.Event.PairMove.Aborted", "A pair move was aborted before its outcome. Authoritative event published once per key on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BreakReason_Dismount, "Combat.Link.BreakReason.Dismount", "The rider left the carrier on purpose with the dismount command. Carried by the LinkBroken event; the wire form is the reason index");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BreakReason_Drop, "Combat.Link.BreakReason.Drop", "The carrier put its rider down with the drop command. Carried by the LinkBroken event; the wire form is the reason index");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BreakReason_Outcome, "Combat.Link.BreakReason.Outcome", "A pair move's outcome frame ended the mount. Carried by the LinkBroken event; the wire form is the reason index");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BreakReason_CarrierHit, "Combat.Link.BreakReason.CarrierHit", "A hit on the carrier above the mount rules' carrier threshold ended the mount. Carried by the LinkBroken event");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BreakReason_RiderHit, "Combat.Link.BreakReason.RiderHit", "A hit on the rider above the mount rules' rider threshold ended the mount. Carried by the LinkBroken event");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Link_Credit_Partner, "Combat.Link.Credit.Partner", "Event tag: the hit or KO event credits the partner of a pair move as well as the hitbox owner");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_CarryLoad_Ratio, "Combat.SetByCaller.CarryLoad.Ratio", "Set-by-caller key of the carry load effect: the rider's weight over the carrier's capacity, quantised, as 0..1");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interact_Target_Mount, "Interact.Target.Mount", "A mountable ally is the nearest interactable in reach. Projected from the interact probe's meaning; the mount binding requires it");

	void GetDeclaredTags(TArray<FGameplayTag>& OutTags)
	{
		OutTags.Append({
			Type_Mount, Role_Carrier, Role_Rider,
			HitType_Interaction_Mount,
			State_Riding, State_Carrying, State_MountPending,
			PairMove_ThrowRider, PairMove_BoostJump,
			Event_Link_Mounted, Event_Link_MountRefused, Event_PairMove_Started, Event_PairMove_Aborted,
			BreakReason_Dismount, BreakReason_Drop, BreakReason_Outcome, BreakReason_CarrierHit, BreakReason_RiderHit,
			Link_Credit_Partner, SetByCaller_CarryLoad_Ratio, Interact_Target_Mount });
	}

	void GetCommandSlotTags(TArray<FGameplayTag>& OutTags)
	{
		OutTags.Add(FGameplayTag::RequestGameplayTag(TEXT("Command.Link.Mount"), false));
		OutTags.Add(FGameplayTag::RequestGameplayTag(TEXT("Command.Link.Dismount"), false));
		OutTags.Add(FGameplayTag::RequestGameplayTag(TEXT("Command.Link.Drop"), false));
	}
}

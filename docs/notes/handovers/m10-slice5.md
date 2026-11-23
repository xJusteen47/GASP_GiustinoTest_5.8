# M10 slice 5: the hold as a GSTNLink Hand link (G1), pickup by focus, minion thrown credit (desktop, 2026-11-10)
Branch `claude/desktop-m10-5` from `claude/desktop-m10-4` (2a343957). Brief: `m10-slice4.md`, plan `m10-held.md` (PR #27). Doc: `Plugins/GSTNHeld/README.md` steps 6, 9, 10; `Plugins/GSTNLink/README.md` (M10 G1 paragraph).

**Done**
- G1 (GSTNLink, generic): `FGSTN_LinkCommandEntry::PartnerKind/PartnerSerial` (Form; default Fighter = unchanged); Form refuses a type that cannot coexist
  with the other channel's live link (`ForbiddenWithBody`); a Hand break without placement leaves the Body exit record alone. Watchdog: partner of
  another kind -> `IGSTN_LinkPartnerResolver` registered per kind (`UGSTN_LinkWatchdog::RegisterPartnerResolver`): gone = Forced orphan break after
  the grace, else own break rules with root distance; no resolver = left alone. Fixes a latent mix-up (object id = fighter id).
- G1 (GSTNHeld): the hold IS a Hand link of type `Combat.Link.Type.Held` (code-made, `FGSTN_HeldLinkType`; roles `Holder`/`HeldObject`). Host posts
  Form/Break (drop=Release, throw=Throw, holder state=Hit, KO=OwnDeath); `Combat.Held.Carrying` projected from the Hand link. Slice 2's
  `FGSTN_HeldFragment` and inbox kind `Held.Command` removed. Held subsystem = resolver for kind Object. New plugin dependency GSTNHeld -> GSTNLink.
- Pickup by focus: GSTNLink's probe notes its focus per entity/frame in `UGSTN_InteractSubsystem` (`NoteProbeFocus`/`FindProbeFocus`, host data, not
  sim state); the host gives a request the focused object (`FGSTN_PickupRequest::TargetObjectId`), else the nearest free one.
- Minion KO events carry the hit's `SourceTag` (Combat.Source.Thrown) and fall back to the detached instigator (GSTNMinion `FQueuedEvent`).
- Tests: `GSTNHeld.Runtime.{HandLink, WatchdogObjectPartner}` (HandFragment removed), `GSTNHeld.Core.PickupRace` +4 checks.

**Choices (owner can overturn)**: focused object already taken in the same frame -> nearest free object (plan said fumble; with r1's strict rule
two stacked objects made the second pawn fumble); GSTNLink's own break rules off for Held (the held rules decide; watchdog only on object destroyed);
minion `OnMinionHit` delegate unchanged (no source); re-pick delay still 0.64 s (owner question open).

**Gate**: builds b1 30 s, b2, b3 ok first try. Headless h1 1490/0/0 (`Saved/M10e/headless-h1.log`), h2 GSTNHeld 10/0/0. Harness r1: grab suite listen
2 % 5/5 PASS, held_pickup_throw_hit PASS, held_drop_and_break FAIL (strict focus, fixed); r2 held listen+2 2 % and dedicated+2 0 % 12/12 PASS,
0 ensures (`Saved/M10e/r2.log`). Not done: three-config build (Held off / Link off), PIE by eye, SUITE_MOUNT/latch reruns.

**M10 left (plan order)**: G6 carry load and size classes (hook: Held type role effect); G7/G8 rest (aim policy, Break/Bounce/Stick, object ghost
blend, friendly fire by mode); G9 thrown enemies; G12-G13 debug/editor/sweep.

Cost: ~200k context, Opus, estimated ~$11-13. Builds 3, headless 2, harness 2 (~6 + 3.5 min).

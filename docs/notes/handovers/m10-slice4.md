# M10 slice 4: pickup records in the interact probe (G2) and thrown-hit credit (G3) (desktop, 2026-11-10)
Branch `claude/desktop-m10-4` from `claude/desktop-m10-3` (82bf38b2). Brief: `m10-slice3.md`, plan `m10-held.md` (PR #27). Doc: `Plugins/GSTNHeld/README.md` steps 9-10.

**Done**
- G2 (GSTNLink, generic): `ResolveInteractTarget` also reads stamped records of the world's GSTNInteract registry (`FGSTN_LinkRecordSource`, a
  record provider that maps meanings by tag), for any kit; the link registry's meaning table merges the GSTNInteract settings' meanings. Still one
  `Interact.Target.*`. Tests `GSTNLink.NP.Interact.{RecordCandidate, StaleRecordIgnored, OfferAndRecordOneTag}` (plan's GrabStillWinsOnTie and
  OneTagProjected folded into the last one, with test offers at 200/40 around the record's 90).
- G2 (GSTNHeld): each rest of an object is a record (`FGSTN_HeldRecordStamp`, replicated; open on rest, close on pickup/throw, previous record's
  end carried); live after the interact rules' `MinAgeFrames` (40 = 0.64 s). The held tick no longer projects the tag; the module adds the
  `Interact.Target.Pickup` meaning (priority `PickupMeaningPriority` 90) to the interact settings. New plugin dependency GSTNHeld -> GSTNInteract.
- G3 (GSTNCombatCore/NP, neutral): tags `Combat.Source.Thrown/.Projectile`; `FGSTN_HitInboxEntry::SourceTag` + `DetachedInstigator` (events
  only); hit and KO events carry the source and name the detached instigator when the attacker id no longer resolves. GSTNProjectile fills both
  (shooter at release, else its player state; params `SourceTag`, default Projectile); the held throw sets Thrown. Tests
  `GSTNCombat.Hits.Credit.{DetachedInstigator, NoSourceUnchanged}`. Harness `held_pickup_throw_hit` +1 check (source and credit).
- Harness `held_drop_and_break` times moved +0.7/+0.6 s (records live 0.64 s after a rest).

**Choices (owner can overturn)**: record delay = interact `MinAgeFrames` (a dropped/landed object is pickable after 0.64 s); the host race still
picks the nearest object (can differ from the focus with two close objects); minion victims get no source tag (own event path).

**Gate**: builds b1 ok, b2 fail (C4456 shadowed local, fixed), b3 ok, b4 ok (log line); 10-20 s each. Headless h1 1487/0/0, h2 1489/0/0
(`Saved/M10d/headless-h2.log`). Harness r1: held listen+2 2 % PASS 7+5, dedicated+2 0 % PASS 7+5, grab suite listen 2 % 4/5 (grab_throw: second
pummel late at 2 %, final damage right); r2 grab_throw PASS at 2 % and 0 %. 0 ensures. SUITE_DEFAULT not rerun (G3 only adds tags when set).
Not done: three-config build (Held off / Link off), PIE by eye.

**M10 left (plan order)**: G1 Hand link with an object partner; pickup ability reading the focus id; G6 carry load and size classes (needs G1's
link effect); G7/G8 rest (aim policy, Break/Bounce/Stick, object ghost blend, friendly fire by mode); G9 thrown enemies; G12-G13 debug/editor/sweep.

Cost: ~130k context, Opus, estimated ~$9-11. Builds 4, headless 2, harness 2 (~7 + 2 min).

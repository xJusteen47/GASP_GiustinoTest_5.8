# DROPKICK-CAPSULE-1 handover: movement capsule on the torso, per-move capsule track (cloud, NOT BUILT, 2026-11-15)
Branch `claude/project-thread-0pka8k` from `claude/desktop-jab-dropkick-r1` 07b0ab3a. Cloud: nothing compiled or run; desktop builds and verifies.
- **Finding** (owner clip `reviews/jab-dropkick-r1/dropkick_r1_owner.mp4`, frames t=0.45 s and 1.0 s, side view): the depth marker line starts at
  the actor location (`GSTN_DepthSubject.cpp:135`, Actor adapter: Feet = actor location = capsule centre) and it starts under the HEAD. Estimated
  from pixels (not measured): capsule centre ~45 cm behind the torso, capsule bottom ~90 cm under the body. Same cause as the half-metre drop
  v102->v103: the standing 90 cm capsule lands while the horizontal body is still in the air, then the land clip drops it.
- **Body Capsule track** (`GSTNCombatCore/.../Timeline/GSTN_BodyCapsule.{h,cpp}`, data per move, any timeline, several per move): on its frames
  the capsule takes HalfHeight/Radius and sits at Offset (capsule centre from the mesh root, X fwd, Z up). Anchor Body = actor moves onto the body,
  mesh offset back (nothing moves on screen, MaxShiftPerFrame 20 cm); Anchor Capsule = capsule bottom stays, mesh moves (cancels a drop baked
  in the clip). Outside tracks: the pawn's own capsule (Body anchor, grows up from the ground). This is the general per-move capsule mechanism
  (ECB-like shortening) asked by the owner; only the dropkick gets tracks now.
- **Simulation** (`GSTNCombatNP/.../Movement/GSTN_BodyCapsuleModifier.{h,cpp}`): permanent Mover modifier queued with the stats modifier
  (`TryQueueStatsModifier`). Applied shape is a modifier field = Mover sync state (rollback, replication). Each substep: capsule size + base visual
  transform re-assigned from Applied; target from `UGSTN_CombatSimComponent::ReadEvaluatedTimeline` (new); a change queues
  `FGSTN_BodyCapsuleShiftEffect` on the same substep (applied before the mode moves; grow = move then grow, shrink = shrink then move; Body
  anchor moves are swept). `ApplyPresentation` from the combat component tick sets size/mesh on every machine (sim proxies run no modifier).
  Wire change: the modifier and effect serialize their shapes. Sim rate untouched, no project setting, no input.
- **Dropkick data**: `Tools/GAS/setup_dropkick_body_capsule.py` (after the kit and hold scripts): air [startup-6, land) hh 35 Body on the loop f0
  torso (mid pelvis/spine_03, measured from the clips), land [land, total) hh 35 Capsule, root on the ground. Log `Saved/AnimR2/setup_dropkick_body_capsule.txt`
  prints the measured torso and the expected drop at touchdown (estimate ~10-15 cm vs 53).
- **Forward push**: not used. The capsule moves to the body, the body does not move; the owner's offered push stays available as the existing
  per-move `MomentumSlide` data if the feel asks for it.
- **Other moves with the same issue (not changed, rule 12)**: any move or reaction whose body leaves the standing capsule: dair (horizontal
  phases), knockdown/lying reactions (victim lies inside a 180 cm capsule), crouch attacks. Each can get Body Capsule tracks by data later.
- **Tests added**: `GSTNCombat.Core.BodyCapsule.{Target,Step}`.
- **Desktop verify, in order**: 1) build (likely compile spots: `EGSTN_BodyCapsuleAnchor` default arg in the UFUNCTION, `Ar << bool`,
  `QueueInstantMovementEffect_Internal` visibility). 2) `Automation RunTests GSTNCombat.Core.BodyCapsule` + all `HoldUntilLanded`, `MontageSync`,
  `Timeline*` green. 3) editor: kit script, hold script, then this script; read its log (torso numbers sane: loop z ~70-100, land z ~15-30).
  4) PIE `show collision`: capsule on the torso in the air, touchdown with the back on the floor, no half-metre pop, standing capsule back at the end,
  no slide sideways. 5) capture per runbook + rule 10 review (side/back), harness `abilities`, `forced_rollback` `CMP OK`.
- **Known limits**: mesh axes assume the Mannequin (+Y forward, mesh -90 yaw); a wall blocking a Body-anchor shift leaves the mesh off by the
  blocked amount for that move; growth back to standing under a low ceiling is not checked (no encroach test).
- **Cost**: one Opus thread, no subagents, no builds: about $12-15.

# HIT-REACT-DIRECTIONAL handover: 4 directional additives on the torso, ground and air (desktop, worktree, 2026-11-16)
Branch `claude/hit-react-directional` (from c64d0471, pushed). Worktree `GASP_Giustino2_hitdir` (LFS not smudged: code only). Opus.
**NOT BUILT, NOT TESTED, ABP NOT CHANGED**: `_ue59/IN-USO.txt` held by stance-offset for the whole thread (rule 7). Stopped at a stable point.
## What was done (option B of handoffs/reazioni-ricerca.md)
- `UGSTN_ReactionAnimComponent` now implements `IGSTN_AdditiveReactionSource` (new, GSTNAnimationCore). On `Combat.Event.Hit.Received`:
  push = victim - attacker (horizontal); side = `FGSTN_DirectionalHitMath::SelectSide` (dot with actor forward/right, ties to front/back);
  weight = `IntensityWeight` (unstaggered 0.3, staggered 0.5..1 linear in hitstun up to 40 frames). Air set while `AirTags` (Tumble), fades
  out under `BlockedTags` (Knockdown, Splat). Newer hit replaces, old one fades 0.05 s. Holds at 0.07 s during hitstop, then plays out.
- Side is re-read for 0.05 s after the hit so it matches the body after the back-hit turn (turn untouched, still PASS).
- Sim change (1 line, presentation payload only): `GSTN_CombatSimVictim.cpp` puts `Hit.HitstunFrames` in the event's `EventMagnitude`.
- New anim node `FGSTN_AnimNode_DirectionalHitAdditive` + editor node: reads the request in PreUpdate (game thread), adds the side's
  additive clip masked to `spine_01` and children, or a **PLACEHOLDER** spine bend (25 deg at weight 1, peak 0.07 s, 0.4 s) when the side
  has no clip. No source / no layer = pure pass-through. `a.GSTN.HitAdditive 0` = off. `GSTN_CharacterAnimInstance` NOT touched
  (stance thread is editing it).
- Data: `UGSTN_ReactionAnimSet::DirectionalHits` (Ground/Air x Front/Back/Left/Right soft clips + tuning). `setup_reaction_anims.py`
  writes the tags and the clip table `DIRECTIONAL_CLIPS` (all None now = placeholders). Ground base = HitStand, air base = Tumble (existing).
- Tests: `GSTNCombat.GAS.ReactionAnim.Directional{Side,Weight,PlaceholderCurve}` (pure). GSTNCombatGAS now depends on GSTNAnimation.
## Next thread (in this order, after stance-offset frees _ue59 and commits)
1. Merge `claude/hit-react-directional` into the _ue59 branch; Run-Timed Build (45 min). Two fix tries max (rule 5).
2. Tests: `GSTNCombat.GAS.ReactionAnim` + the existing reaction/victim suites (no regression).
3. `setup_reaction_anims.py`, then `Tools/GASNPPTest/hitdir_abp_insert.py` (backs up the ABP, inserts after "GSTN Blend-Out Foot Lock",
   writes `Saved/HitDir/abp_before|after.txt`: check the diff is only the new node + 2 links, and that the montage slot is upstream).
4. Capture (rule 10) HitStand + jab from front, side, back with `a.GSTN.HitAdditive 1/0`; same frames. A cloud review looks before anything
   goes to the owner. Placeholders are NOT poses: real additive clips come from the anim pipeline under owner supervision.
## Da confermare (coordinator to owner)
- With the back-hit turn on, every STAGGERED hit is turned to read as Front, so Back/Left/Right only show on unstaggered hits (flinch).
  Option: keep the turn (recommended until real side clips pass review), or drop it for sets that have side clips.
- Intensity source = hitstun frames (not damage/knockback); 40 frames = full weight.
## Risks not checked
Untested C++ (written without a compiler); dedupe of a resimulated event (same push within 0.05 s) is a guess; mesh-space additive clips untested.
Cost: ~$5 estimated (no subagents).

# HIT-REACT-DIRECTIONAL handover: 4 directional additives on the torso, ground and air (desktop, worktree, 2026-11-16)
Branch `claude/hit-react-directional` (from c64d0471, pushed). Worktree `GASP_Giustino2_hitdir` (LFS not smudged: code only). Opus.
UPDATE 11:30: integrated in `_ue59` on `claude/hit-react-int` (= stance-offset d67011e4 + this branch). Build OK (124 s,
`Saved/Logs/ci/hitdir_build.log`). Tests `GSTNCombat`: 609 Success, 0 Fail (`grep -c "Result={Success}" Saved/Logs/ci/hitdir_test.log`),
incl. the 3 new ones. DA_ReactionAnims set (log `Saved/Reaction/setup_reaction_anims.log`: "0/8 clips, the rest PLACEHOLDER bend"; the
Capture preset run ended TIMEOUT only because the full editor does not quit, the script had finished and saved). ABP: node inserted by
`hitdir_abp_insert.py` (commandlet, 17 s, OK), chain is Slot ... -> Foot Lock -> **Directional Hit Additive** -> Stance Offset; diff
= only the new node + 2 links (`docs/notes/reviews/hit-react-directional/abp_diff.txt`). Backup: `Saved/Backups/GAS/hitdir/`.
**NOT CAPTURED / NOT LOOKED AT (rule 10)**: budget reached; nothing goes to the owner yet.
## What was done (option B of handoffs/archivio/2026-11/reazioni-ricerca.md)
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
## Next thread
1. (done: merge, build, tests, data, ABP.) Start from `claude/hit-react-int`.
2. Capture (rule 10) HitStand + jab from front, side, back with `a.GSTN.HitAdditive 1/0`; same frames. A cloud review looks before anything
   goes to the owner. Placeholders are NOT poses: real additive clips come from the anim pipeline under owner supervision.
## Da confermare (coordinator to owner)
- With the back-hit turn on, every STAGGERED hit is turned to read as Front, so Back/Left/Right only show on unstaggered hits (flinch).
  Option: keep the turn (recommended until real side clips pass review), or drop it for sets that have side clips.
- Intensity source = hitstun frames (not damage/knockback); 40 frames = full weight.
## Risks not checked
Never seen in game yet; dedupe of a resimulated event (same push within 0.05 s) is a guess; mesh-space additive clips untested.
Cost: ~$8 estimated (no subagents).

# Handover HIT-ARMS-2: HitStand pose per hit side, code (laptop, no editor, 2026-11-18)
Branch `claude/project-thread-9wkpe7` = `claude/hit-arms-1` (933b42ec) + merge of `claude/project-thread-8oy91z` (029fce74, hitstop hold at frame 4).
Worktree `GASP_Giustino2_hitside` on the laptop. Owner decision 2A. No asset touched (DA_ReactionAnims* untouched).
## Done (GSTNCombatGAS, presentation only)
- `FGSTN_ReactionSideAnimations` (GSTN_ReactionAnimSet.h): optional Front/Back/Left/Right clips + `SideSettleFrames` (3), as the
  `SideAnimations` field of `FGSTN_ReactionAnimRule`. An empty side plays the rule's `Animation`; ThenAnimation is shared.
- Mirror = a clip baked offline (`HITARMS_LEAD=r HITARMS_VERSION=V3R build_hit_arms_v2.py`), no mirror node in the ABP (choice applied:
  works with the montage slot as it is and lets a side be a pose of its own).
- Side = same math as the directional additives (`SelectSide` on the last Combat.Event.Hit.Received push vs. facing now). The hit event
  is now bound also when DirectionalHits is off but a rule has side clips; every hit records the push, weak ones too.
- Component: stage 0 picks `AnimationForSide`; during SideSettleFrames the side is re-read and a change swaps the clip at the same time,
  no blend, hitstop hold kept (back-hit turn, event a frame after the tag). After that the pose never flips.
- BP: `UGSTN_ReactionAnimSet::GetRuleAnimationForSide`, `SelectHitSide` (static); component `GetLastHitSide`, `GetReactionPoseSide`.
- Validation warning when only one of Left/Right is set.
## Prove
- Build: `Tools/CI/Run-Timed.ps1 -Preset Build -Log Saved/Logs/ci/hitside_build.log` → `.esito` "ESITO: OK", log "Result: Succeeded".
- Test: `-Preset Test -Tests GSTNCombat.GAS.ReactionAnim -Log Saved/Logs/ci/hitside_test.log` → 10/10 Success
  (`Select-String "Test Completed. Result={Success}"` on `hitside_test.log.ue.log`), 3 new: `ReactionAnim.Sides.{Fallback,SetQuery,Validation}`.
## Next (needs an editor, not done)
1. Bake V3R (lead right) with the script, then put HitStand back on V3 in the set (the script resets it).
2. In `DA_ReactionAnims_HitArms` HitStand rule: SideAnimations.Left = V3 or V3R, Right = the other: which clip has the LEFT lead arm
   decides (hit-arms-1 r3: on side_hitstand_l the left lead arm ended near the head, so check before assigning).
3. Capture front + left + right close-up (react_close), Fable review per side. 4. Not covered: the component swap path in PIE (no
   test world here); watch `LogGSTNReactionAnim Verbose` "pose side ->" lines in the capture.
## Cost estimate: ~4-6 $ (Opus, ~70k context, 1 build 25 min, 1 test run).

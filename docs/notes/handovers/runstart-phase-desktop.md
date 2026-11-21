# runstart-phase-desktop: run start entry/exit fix (systemic, switchable); hand-off PASS, entry FAIL (desktop, 2026-11-21)
Branch claude/runstart-phase-desktop (from claude/runstart-v8c-desktop 13824ee2). Lock _ue59 taken 00:30Z, freed at the end; owner's
editor was already closed at start (not touched). Re-check (rule 11): v8c ASSERT OK confirmed (Saved/RunStartV8c/v4r8a/assert.txt).
## Causes (measured, frames.jsonl combat.anim = clip/weight/time; curves runstart_phase_curves.py)
- Entry: a stock walk start plays first, then the run start is pose matched at ~0.13 s + 0.3 s QuadraticInOut -> rev-up invisible.
- Exit: loop schema PSS_Relaxed_Loops = Trajectory + Curve `Phase` + Heading (no feet); our `phase` 0..1 vs stock triangle ->
  loop entered on the other foot (a: R planted vs loop ~0.03 s before L plant) -> averaged feet = skipped step / hop (also v4r7).
## Done
- Unreal Assistant (Saved/Assistant/replies_runstart_phase.json): blend profile + Out curve, Block Transition/SamplingRange for entry,
  pose-matched loop entry. It denies a stock phase curve: wrong, our dumps show it (Saved/Routing/pss_channels.txt).
- Fix, divergence row 19: clip COPIES AS_GSTN_RunStart_{v4r7,v4r8a,v4r8b,v4r8c}h with stock-triangle phase from their contacts;
  sibling rows at variant+10 (BlendTime 0.05 CircularOut per owner "almost instant", SamplingRange 0-0.02); cvar
  gstn.RunStart.Handoff 1 (default 0: approved v4r7 rows untouched). Script runstart_handoff_ue.py via Save-Assets; real build OK.
- Captures runstart_handoff_captures.ps1: 4/4 ASSERT OK (Saved/RunStartPhase/<v>h/assert.txt). Both-feet-up frames in the loop
  hand-off 13-14 -> 3-4 (one-off script over frames.jsonl, Saved/Trash not kept; per-plant slides in the same order as before).
- Reviews (Fable, brief+grids+mp4): verdict.md in docs/notes/reviews/runstart-{v4r7h,v4r8ah,v4r8bh,v4r8ch}. ALL: hand-off FIXED
  (alternating planted steps, no hop/float). ALL FAIL on entry: [gioco] v82-v84 one-frame snap idle -> start pose (0.05 s blend
  reads as a cut, support foot slides 7-11 cm); [gioco] start weight V 0.82 -> 0.50 (v90) -> 1.0 (v99), likely a SECOND instance
  of the clip (walk->run gait Re-Enter re-picks the start at frame 0 with ForceBlend) = twitch v89 (a), upright torso v91-95 (b).
  Also c v93 1-frame twitch [dati montage, in the clip]; b R landing skim v113-118 [posa, old].
## Next (one code thread, Opus medium, ~6 $): RSH_BLEND 0.1 s (6 f) instead of 0.05; find/stop the gait Re-Enter re-pick of the
  start in the first ~0.3 s (Rewind Debugger or log both Blend Stack instances); then recapture 4 and review. Only then poses.
  Owner's new target (0.47 s to cross the first block) not applied here (variants' job).
COSTO: ~18 $ | ~25 min desktop | ~480k token review (4 x ~120k)

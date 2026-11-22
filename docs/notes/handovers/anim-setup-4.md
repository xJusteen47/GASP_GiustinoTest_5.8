# Anim setup 4 (desktop _ue59, Opus, 2026-11-19 19:55-21:00 UTC) - clip fixes saved, slide marker, slopes, review
Branch claude/anim-setup-3 (continued). 2 builds OK (Saved/Logs/ci/build_setup4_{1,2}.log "Result: Succeeded"). Lock released.
1. Saves: Tools/CI/Save-Assets.ps1 is the one agent save route (committed Tools/*.py only, no editor of the checkout open,
   Run-Timed 15 min). Owner rule: `PowerShell(& .\Tools\CI\Save-Assets.ps1 -Script Tools\*)` in user settings. Worked 6 times.
2. Hitstop (owner question): FGSTN_LayeredMove_Freeze (priority 250, OverrideAll, zero velocity, restores IncomingVelocity) freezes
   Mover in our line; timeline/montage/momentum slide pause too. Branch 8oy91z (029fce74, presentation only) NOT merged.
3. fix_clips_ue.py new steps: splice (first/last 0.2 s to the neighbour pose), floor_clamp (leg IK, vertical only), slide_marker.
   Saved on dash, run start, run stop L/R, skid. Clip check (Saved/Logs/ci/clipcheck4f.log, summary
   docs/notes/reviews/clip-check/summary.json): 13 GASP rows + these 5 PASS; Crouch_Exit FAIL (floor -1.5, both feet up f8-9).
   Check fix: hand-off feet measured from the pelvis turned by the hips (was armature object: skid read 85 cm).
4. Intended slides: float curve GSTN_FootSlide(_l/_r) on the clip. Our ABP pins feet in CR_Biped_FootPlacement from contact_l/r
   (asset scan), so curves_author.py zeroes contact under the marker; pop guard (SlideCurve) and check honour it. Skid r f1-20,
   run stops both f1-30. Notes: docs/notes/foot-lock-and-slides.md.
5. Both feet up back to idle (owner, jab): BlendOutFootLock SecondStepStart 0.6 -> 1.15 (ClampMin 1). Check rule added.
   Fable: jab1_long never both feet in the air.
6. Slopes: L_Test_Hub zone Z7 saved (hills 8/15/25/35 deg, PARK Z7 foot and Z7_top), captures PIE_CAPTURE_PARK + mv_slope_*.
7. Unreal Assistant (Tools/UE59/ask_assistant_boot.py, timed editor; conv id must be ''): it cannot open our assets and refused
   the ABP architecture question. Answers in docs/notes/assistant/. abp-architecture.md NOT written: build it from the assets.
8. Fable review docs/notes/reviews/anim-setup-4/review_fable.md: run stop (Mode 1, KIT_Test_Playtest) PASS, skid release/hold
   PASS, dash PASS (better), jab PASS, slopes up/down/idle PASS. Gioco queue: stop after a skid is GASP's stop even with
   RunStop.Mode 1; skid exit leg snap v144-150 (refactor item 3); capsule floats 12 cm until first input; dust lump after stops.
   Not yet seen: skid while running uphill, a longer downhill run (move the reverse earlier / start lower).
Next (coordinator order): air crouch landing, run start stomp, ABP architecture doc, refactor 3-7, camera P0, the gioco queue.
Cost: ~65 min desktop, 1 Fable subagent (~2 passes).

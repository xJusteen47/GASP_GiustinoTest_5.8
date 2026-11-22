# Skid root 180 exact (desktop, Opus medium, 2026-11-18) - clip and layer done, capture BLOCKED
Branch claude/project-thread-98dml5. Owner ask: first unmodified skid clip (SS_180_Turn, root never turns), root that turns 180 once, for GASP.
- Choice applied (rule 6/12): the owner's RTG_RootCorrection is untouched. An IK Retargeter copies the pelvis yaw as is, it cannot end on
  exactly 180 (SS_180_Turn2 re-retargeted = -188). The "copy of the script" is Tools/GAS/root_fix_skid_180.py: on a COPY of
  Ported57/Skid/SS_180_Turn it rebuilds root (xy = pelvis xy minus start offset, yaw = 180 x pelvis heading progress, monotonic, clamped)
  and re-expresses pelvis, ik_foot_root, ik_hand_root so every bone keeps its world pose.
- Result Ported57/SkidOwner/SS_180_Turn_Root180 (Saved/SkidOwner/root_fix_skid_180.log): pelvis heading 179.7, root yaw -180.0 (= 180),
  turn 10/50/90% at keys 2/7/24 of 26, root travel 45.0 cm, pelvis world drift 0.000 cm. EnableRootMotion off like the source.
- Skid layer pointed at it: Tools/GAS/run_skid_root180.py (fix + setup_skid_anim_layer.py with SKID_CLIP), ROOTFIX OK + ESITO OK in
  Saved/Logs/ci/py_skid_root180.log.ue.log (Select-String 'ROOTFIX|ESITO'). The run then hit TIMEOUT (900 s) because the editor did not quit:
  fixed now (save + quit_editor at the end of run_skid_root180.py).
- Capture mv_skid_base: TIMEOUT 900 s, 0 frames (docs/notes/reviews/skid-root-1/cap/run.log.esito, not committed). Cause: the editor killed
  at the first timeout left Saved/Autosaves/PackageRestoreData.json (ABP_GSTN_WallLayer, dirty and not ours), so the next start waits on the
  "restore packages" dialog at frame 0. Renaming that file was refused by Claude Code permissions. 2 timeouts = stop (rule 5).
- NEEDED: owner opens the 5.9 editor once and answers No to restore (or deletes that json), or allows the rename. Then:
  PIE_CAPTURE_VIEWS=az90,game; Tools/GASNPPTest/pie_capture_timed.ps1 -Kit KIT_Test_Overlord -Out docs/notes/reviews/skid-root-1/cap -Sequences mv_skid_base
  and check: single 180 turn (no clip+actor double turn), pelvis above floor, feet not sliding. Not looked at yet: no video.
- Fallback layer clip if it fails: SKID_CLIP=.../SkidOwner/SS_180_Turn2_Src (PASS in skid-ss180turn2-2).
- _ue59 lock released. Cost ~3 $.
- UPDATE 15:15: owner confirmed the cause (Mixamo import without root motion). The restore json was emptied by a later session; capture cap2 OK, REVIEW.md: facing and pelvis PASS. Lock released.
- Fable review (REVIEW-fable.md): FAIL. Facing and pelvis PASS; brake feet PASS. FAIL: pop v58->59 (head 31, feet 27-39 cm in one frame) and restart slide v76-88 (foot jump 29 cm, stride frozen while body glides). Labels gioco/dati montage (montage null in frames.jsonl). Next: Tools/GAS/diag_skid_clip_jumps.py (read only, quits itself) tells if the v59 jump is inside the clip; not run, _ue59 held by the MCP toolset thread.

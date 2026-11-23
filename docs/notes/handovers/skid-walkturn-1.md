# skid-walkturn-1: Mixamo walk turn 180 converted for the run pivot (desktop, 2026-11-20)
Branch claude/skid-walkturn-1 (from claude/loco-skid-runstart-1). Lock _ue59 taken and freed. ABP and choosers NOT touched.

## Results
- Source: Mixamo "sword and shield 180 turn.fbx" (Sword and Shield Pack; the Pro pack copy is byte-identical). "(2)" is the clip
  of the current skid (owner's SwordAndShield180Turn.UE.fbx).
- Conversion, all headless through Tools/CI/Save-Assets.ps1, assets in Content/GSTN/Animation/Mixamo:
  1. Tools/GAS/mixamo/convert_mixamo_clip.py: X Bot skin + clip imported (Source/), auto IK rig IK_Mixamo_XBot, auto retargeter
     RTG_Mixamo_to_UEFN -> Source/AS_GSTN_WalkTurn180_Mixamo_Mixamo_UEFN (0.8 s, 25 keys, root static). The owner's
     RTG_RootCorrection gives a wrong root on it (yaw -116 at frame 0: UEFN pelvis axes), that result was discarded.
  2. Tools/GAS/root_fix_skid_180.py (SRC/DST env) -> AS_GSTN_WalkTurn180: root follows pelvis xy, yaw exactly +180, turn
     10/50/90 % at keys 2/9/19 of 25 (Saved/SkidOwner/root_fix_skid_180.log).
  3. Tools/GAS/mixamo/prep_gasp_clip.py -> AS_GSTN_RunPivot_WalkTurn180 (the row clip): 0.8 s held tail (GASP exits at <= 0.75 s
     left), ik_foot_l/r baked (0.0 cm), root motion on, PoseSearchExcludeFromDatabase 0.2 s -> end; curves_author.py pivot
     (contact_l/r, phase, Foley); fix_clips_ue.py floor_clamp + ik_feet (Saved/ClipCheck/fix_report.json).
- Isolation gate (clip_check.py in Blender, docs/notes/reviews/clip-check/WalkTurn180_RunPivot/metrics.json): FAIL.
  a) planted foot drift 4.4 cm (l, f9-18) and 4.9 cm (r, f1-10), limit 3.5: retarget residue, small.
  b) handoff vs GASP run loop: calf_l 55 deg / feet 32 cm in, upperarm_l 45 deg / feet 41 cm out. Inherent: it is a WALK turn
     used for the RUN. The splice fix was refused by the conservation guard (arm speed -28/-29 %), correctly.
  Fix attempts: 1 (tail root held, floor clamp). The tail skate (79 cm) is fixed. No grid (no matplotlib in Blender 5.2).

## Owner decisions this session (scope: locomotion pivot/skid)
- B: stock GASP pivot through the state machine; our own entry conditions allowed if wired cleanly and the clip is set up.
- Pick the pivot clip by Gait like GASP (walk turn for Run, current skid for Sprint).
- Our clips go in OUR nested chooser(s) keyed by kit/weapon with a per-skin override, falling back to GASP stock clips.

## Next (new thread)
1. Decide gate b): accept the handoff FAIL for a walk-for-run trial (owner idea) or not; a) needs a speed-planting op or a
   foot-lock step (attempt 2).
2. Ask the Unreal Assistant: does a nested chooser row that returns nothing fall through to the next row inside a leaf with a
   PoseSearchColumn? Then: CHT_GSTN_PivotSet (column Gait + a kit/skin key from a data asset, not hard-coded per kit) as a row
   in Run F / Sprint Loco Transitions with IsPivoting, write the divergence in gasp-divergenze.md BEFORE the commit (rule 14),
   turn our Skid layered move off for that path (SKID_ROOT_TRACK=0 / no Skid state).
3. In-game capture vs stock base (mv_skid), then blind review (rule 10).

COSTO: ~12 $ | ~4 min desktop (headless only) | review 0 token

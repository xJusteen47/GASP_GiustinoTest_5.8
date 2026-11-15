# Handover: jab arc + dropkick r0 restore, round 1 (desktop, UE 5.9, 2026-11-15) — built, awaiting rule-10 review
Branch `claude/desktop-jab-dropkick-r1` (from hgy214 36dcdb44), folder `GASP_Giustino2_ue59`. Model Opus (poses) + Sonnet subagent (build/capture).
**Cards f8fb2092, a19e16a1; build fixed and variant captured in checkpoint 2 (4abaec30, +$10 extension). Next: cloud review.**

## Cards (the work to keep)
- Dropkick 1A: `cards/overlord/dropkick/restore_r0_dropkick.py` writes startup+loop = Antigravity r0 poses (legs together, arms crossed,
  fists `fingers 0`, r0 overlap delays chest1/head2/forearm2/hand3), loop key f12 moved to f11 (clip is 0-11); land f0 = r0 loop pose on the
  ground, f1-f2 blend into r1 f3 (arms open on impact), f3-31 = r1. r1 cards kept as `*.r1.backup.json`; land `_plan` renamed `_plan_r1`.
  Timing: Antigravity curves from `GASP_AnimPipeline_aaqr80/Tools/AnimPipeline/refs/ufd/` (owner tip 14:28Z); startup csv cut to 16 rows,
  active column kept from r1; r1 csvs as `*.r1.csv`.
- Jabs: owner's Cascadeur scene (autosave2-overlord jab, Step keys 0,5,7 | 17,19,21) dumped to `cards/overlord/jab/owner_r3/` and converted
  with `casc_to_author.py` (ratios 1.0). `jab_r3_plans.py` -> `owner_r3/*.plan.json` -> `pose_plan.py` -> cards. Jab 1: v3 startup f0-f4,
  owner O5 at f6 (contact), O7 at f8, follow-through f9-f11 (trunk +30/45/50% past O7, arm keeps its extension), hold f11, recoil f13,
  limb-led recovery, feet step with lifts (world targets). Jab 2: O17 f7, pre-snap f8, O19 f9 (contact), O21 f11, follow-through f12-14,
  recoil f16, recovery to idle f26. `anim_active`, `faf_game`, hitboxes, TL, AM untouched. After `pose_plan.py` run
  `jab_r3_plans.py --restore-csv` (pose_plan overwrites timing csvs). Previews `owner_r3/*.png` (not in git), looked at by me: arc OK.
  Not done: palm/wrist orientation (`hand_rot`) not converted from the owner's direction points.
  Copies of the owner's scene: `Saved/AnimPipeline/casc/overlord_jab_{owner_base_2026-11-15,r3}.casc`; r3 copy open in Cascadeur (unsaved, range set to 0-140, one key at f140): close without saving.
## Build: FIXED in checkpoint 2 (commit 4abaec30), ready for the rule-10 cloud review
- Cause of the frozen clips: `animpipe author <card>` was skipped (build re-baked the stale source take: edited=0, hand_r 97 cm off the
  poses). Fix: copy `sheets/raw/{idle_neutral_legbones,AS_Pilot_GanonFTilt_V2_frame0}.json` into ue59 `Saved/AnimPipeline/`, then
  `author` + `build` per card. After: at_poses hand_r 0.8 cm (jabs), dropkick <= 7.4 cm (startup), see `*_build.json`.
- Jab montages play `/Game/GASNPPTest/Combat/Overlord/AS_OverlordJab{N}V3`: refreshed without the kit script (duplicate `_Build` ->
  `consolidate_assets` -> rename); AM byte-identical to HEAD. Content of 3ab86e0c/dc1402f3 (frozen) is superseded by 4abaec30.
- Variant recaptured: `docs/notes/reviews/jab-dropkick-r1/{base,variant}/` mp4 normal + quarter, grids `*_base_vs_variant.png`.
- Checks all FAIL (`checks.txt`), for the reviewer: dropkick crossed arms penetrate (startup hand_r/spine_05 -11.8 cm f8, loop forearms
  -7.8 cm f5, land hand_l -9.3 cm f1) = likely `posa` on r0; jab hitstop_snap 31-48 cm; legibility tool KeyErrors on loop/land.
## Facts
Base captures: jab1, jab1_jab2_faf (v14-v90), dropkick_whiff (v36-v135), mp4 normal + quarter. Video: jab1 contact v27, jab2 contact v42,
dropkick release v55-58 (first active v58), land clip v102 (base) / v103 (variant). Tools: capture wrapper with 900 s limit (subagent
scratch, not kept), `rv_grid.py` Windows font fallback (committed). `AM_OverlordDAirV9` still dirty, not ours.
**Cost**: estimated $45-50 incl. checkpoint 2 (one Opus thread ~150k context + Sonnet subagent ~360k tokens): over the $35 budget, stopped at this point.

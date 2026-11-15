# Handover: jab arc + dropkick r0 restore, round 1 (desktop, UE 5.9, 2026-11-15) — STOPPED, nothing for the owner
Branch `claude/desktop-jab-dropkick-r1` (from hgy214 36dcdb44), folder `GASP_Giustino2_ue59`. Model Opus (poses) + Sonnet subagent (build/capture).
**Cards done (committed f8fb2092, a19e16a1); the build in Unreal is broken (2 attempts used, rule 5).**

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
## Build: BROKEN, do not merge Content from 3ab86e0c / dc1402f3
- Jab variant video `docs/notes/reviews/jab-dropkick-r1/jab1_base_vs_variant.png` (bottom row): the character stands still, v20-v50.
- `animpipe check`: all 5 cards `contact_faf: reach peak at frame 0`; the subagent read the same pose on every frame of the built clips
  (`checks.txt`, `loop_bones.txt`). Dropkick variant moves only between montage sections (suspected static clips too, not proven).
- Jab montages play a copy (`/Game/GASNPPTest/Combat/Overlord/AS_OverlordJab{N}V3`) made by the kit script, which must not run (owner's
  hitboxes). Used instead: duplicate `_Build` -> `consolidate_assets` -> rename; AM byte-identical to HEAD (`am_identity.txt`).
- Next thread (code/tooling, Sonnet medium, editor): open `/Game/AnimPipeline/Rig/AS_OverlordJab1V3_Build`, compare hand_r at f0/f6;
  if static, the bake (`animpipe/ue_rig.py` build, bake_to_control_rig) is the cause — v3 built fine on 5.9 on 11-14, so diff the card
  shape (dense pose_plan card, `planted: []`) against `overlord_jab1_v3.r2b.backup.json`. If `_Build` animates, the consolidate is the cause.
  Then rebuild, recapture variant (base already in `docs/notes/reviews/jab-dropkick-r1/base/`, press v20 jab / v42 dropkick), cloud review.
- Fists check still 0.95 openness on dropkick after `fingers 0`: check `checks.py` fists source (it may read the clip, which is static).
## Facts
Base captures: jab1, jab1_jab2_faf (v14-v90), dropkick_whiff (v36-v135), mp4 normal + quarter. Video: jab1 contact v27, jab2 contact v42,
dropkick release v55-58 (first active v58), land clip v102 (base) / v103 (variant). Tools: capture wrapper with 900 s limit (subagent
scratch, not kept), `rv_grid.py` Windows font fallback (committed). `AM_OverlordDAirV9` still dirty, not ours.
**Cost**: estimated $40-45 (one Opus thread ~150k context + Sonnet subagent ~360k tokens): over the $35 budget, stopped at this point.

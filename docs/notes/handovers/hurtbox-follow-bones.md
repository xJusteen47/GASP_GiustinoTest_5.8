# Hurtboxes follow the bones per move (cloud, 2026-11-16)

Branch `claude/project-thread-sj64s3` from `claude/project-thread-jytbuj` 50a91a0c. Cloud, Opus. **NOT COMPILED** (no UE in cloud). Owner chose A + "simple movement hurtbox by default, per kit" + "feet by hand per move".

## What changed
- **Pose** (`Hits/GSTN_HurtboxPose.h`, Core): per frame, from the running timeline + frame (sync state) and data only. `FGSTN_Hurtbox` gets `bFollowBone`, `BoneReference`, `DetailUse` (Both/SimpleOnly/DetailedOnly). Set gets `MovementDetail` (Simple, idle/locomotion) and `MoveDetail` (Detailed, during moves). Timeline gets `HurtboxDetail` override and `HurtboxBones`.
- **Hurtbox track** `FGSTN_TimelineTrack_Hurtbox` derives from the hitbox track (editor reuses lanes, keys, gizmo, Details, rename, parenting); `CountsAs` (set hurtbox name) + `bReplaceCountsAs` (hand-made feet). Runtime readers that took hitbox tracks via `GetPtr` now use `GSTNTimelineTracks::AsAttackHitbox` (phases, steer, active end, reach, kit mods, link, hit rules): a Hurtbox track never hits.
- **Kit**: `FGSTN_MovementHurtboxExtension` (kit Extensions) replaces the Simple body outside moves; copied into the table set per loadout revision (rollback-safe).
- **Consumers**: snapshot entry `HurtPose` (+ lag-comp record); attacker tick, minion attacker, projectiles, debug draw use posed shapes + extensions. Minion victims get the same pose.
- **Editor**: baker also bakes bones followed by any hurtbox set of the project that the mesh has (+ `HurtboxBones`); hurtbox set editor Generate menu: "Add simple body", "Follow bones in moves"; generation makes per-limb DetailedOnly following hurtboxes and adds the body; timeline viewport draws the posed hurtboxes of the set whose PreviewMesh is the preview mesh (green); Hurtbox track drawn green. Python: `unreal.GSTN_CombatEditorUtils.prepare_hurtbox_set(set, mesh, True)`.
- Old sets (Both, no follow) = default pose = identical behaviour. Existing bakes stay valid until a set follows bones (then timelines re-bake on save).

## Choices applied (rule 6)
1. Extension hits count as a set hurtbox (CountsAs, default first) so parts/reactions/states need no change. 2. Body = vertical capsule on root axis, radius 0.22 x height. 3. Hurtbox track spaces: Root/BakedSocket only. 4. Lag comp rewinds the pose (HurtStates still present-frame, as before). 5. Max 8 extensions per frame.

## Desktop build and test (Haiku low, ~3 $)
1. Close editor. Timed build `GASP_Giustino2Editor` Win64 Development (45 min). Errors: max 2 fixes, else write them here.
2. Headless: `GSTNCombat.Core.Hits.HurtboxPose` (5), `GSTNCombat.Editor.HurtboxFollow` (2), `GSTNCombat.Editor.Hurtbox`, `GSTNCombat.Editor.Bake`, `GSTNCombat.Core.Timeline`, `GSTNCombatNP` attacker + lag comp tests.
3. In editor Python: `prepare_hurtbox_set` on the playtest character's hurtbox set with its mesh, save the set, Bake all timelines. Screenshot TL_Overlord_Jab1 at an active frame (posed hurtboxes). Don't save maps; revert IC_/KIT_ dirtying.

## Open
- Hitbox Details on a Hurtbox track shape still lists hit data (ignored). Live preview before bake shows the reference pose.
- PIE check that a low poke between the legs hits in idle and that a kick's leg is hittable during the move.

# REACT-HOOK-1 handover: victim reaction hook, get-up roll travel, crouch mitigation (cloud, 2026-11-14)
Branch `claude/project-thread-2n1mrc` from ue59m1 `483c2494`. Cloud only: **NOT BUILT, NOT RUN** (no engine here). Read before: `anim-finish-3.md`, `reviews/react-v3/roll-travel.md`.
**Done (code + setup script):**
- **Reaction hook, data only**: `UGSTN_ReactionAnimSet` (data asset: rules tags -> clip, priority, optional chained 2nd clip, end Hold/Loop/BlendOut,
  restart event) + `UGSTN_ReactionAnimComponent` (GSTNCombatGAS `Cosmetics/`): reads the ASC tags each frame, plays AS_/AM_ on `DefaultSlot`,
  yields to ability timeline montages, never on a dedicated server, never writes sim state. New projected tag `Combat.State.Splat` (with Hitstun).
- **GroundHit -> LieUp**: one Knockdown rule chains them inside the component, next clip started 1.5 frames before the end with a 2-frame
  cross-fade: no game pose between. GA_GroundBounce_Test plays nothing (script clears it).
- **Roll travel** (roll-travel.md, 60 -> 62.5 Hz): `UGSTN_GA_GetupRoll` gets `TravelDistanceCm`, `TravelDirection`, `TravelKeys` (piecewise-linear
  distance; each segment = one constant push via `UGSTN_GE_PushBack`, rollback-safe sync frame). Roll 80 cm toward feet (0/6/10/16 = 0/0/.7/1),
  RollBack 65 cm toward head (0/5/8/10/12/13 = 0/0/.17/.68/.97/1). New optional kit field `FGSTN_GetupSet::RollBack`, started instead of Roll when
  stick . facing < 0 (facing from the Mover tracker). Distances are guesses inside the note's range: tune in GA_*_Test.
- **Crouch after GetUp/Roll (attempt 1, mitigation)**: get-up timelines HOLD the last pose and blend out over 20 sim frames (`HOLD_BLEND_FRAMES`
  in the script), so the game pose has time to settle under it. Root cause (motion matching pose history after a lying montage) NOT fixed: the
  ABP is not in git. HitStand/Tumble ends are now driven by the component (HitStand blends out over 8 frames holding its last pose).
- Tests (written, not run): `GSTNCombat.GAS.ReactionAnim.Select`, `GSTNDefence.NP.GetupRoll.TravelSegments`. READMEs updated.
**Not done:** ECB / shrinking capsule (sim + hurtbox change, needs its own small design); GroundHit from tumble height (the 90 cm drop is in the
clips: needs a GroundHit variant that starts at tumble height, anim job; the 4-frame blend in is data in DA_ReactionAnims).
**Desktop steps (one thread, editor CLOSED):**
1. In `GASP_Giustino2_ue59`: `git fetch origin claude/project-thread-2n1mrc` then `git merge --no-edit FETCH_HEAD`.
2. Build the editor target (max 2 fix attempts on compile errors; the unverified engine calls are in `GSTN_ReactionAnimComponent.cpp`:
   `CreateSlotAnimationAsDynamicMontage`, `Montage_PlayWithBlendIn`, `LeaderPoseComponent`).
3. Headless: `UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<proj>/Tools/GAS/setup_reaction_anims.py" -unattended -nullrhi`.
   It writes DA_ReactionAnims, the component on SandboxCharacter_Mover (shared BP: backed up to Saved/Backups/GAS), GA_GetupNeutral/Roll/
   RollBack_Test, the kits' roll back. Log: `Saved/Reaction/setup_reaction_anims.log`.
4. Tests `GSTNCombat.GAS.ReactionAnim`, `GSTNDefence.NP` then the full suite (was 1594 / 0 fail). Commit the content, push the branch.
5. Rule 10 before the owner: Play capture of hitstand, tumble, splat, knockdown->getup, roll, rollback IN GAME (not in place), cloud review.
**Cost:** one Opus thread, ~200k tokens context, no subagents; estimate ~$15.

# Air crouch, checkpoint 1 (cloud, 2026-11-16)

Owner ask (2026-11-16): an air crouched state with its own animation set, for clarity, since some aerials are done crouching.
Branch `claude/project-thread-ubv88e`, base `claude/integ-night-1116` 194ad7e3. Code NOT compiled (cloud): build steps below.

## What it does
- Airborne (Mover air modes: falling, flying; a wall mode is not air) + the kit's hold button down = `Combat.State.AirCrouch`.
  Projected every frame by the combat sim (`UGSTN_CombatSimComponent::ProjectAirCrouchTag`, from ProjectStateTags), never stored:
  rollback-safe with no new sync state.
- Per kit, data only: `FGSTN_AirCrouchKitExtension` (Kit > Extensions > Air Crouch). No extension = no air crouch. Fields:
  `bEnabled`, `HoldButton` (None = `Input.Button.Crouch`, LT in the example layout), `bKeepDuringMoves`, `BlockedTags`.
  Hit reactions and KO always block it. Copied into the table set like the movement hurtbox.
- Animation: an `AirCrouch` rule in `DA_ReactionAnims` (the existing state -> animation component): enter clip (optional) then loop,
  priority 5 (under every hit reaction), blend in 1 / out 3 frames. Clip `AS_AirCrouch_Loop_PLACEHOLDER` = copy of the sample crouch idle.
  PLACEHOLDER: the real pose goes through the animation pipeline (rules 10, 13); replace the asset, keep the name or rerun the script.
- `setup_reaction_anims.py` now keeps rules it does not own (so a rerun keeps AirCrouch).

## Defaults applied (rule 6), to confirm with the owner
1. Fall speed, gravity, air control: UNCHANGED while air-crouched (only tag + animation).
2. Hurtbox and pushbox: UNCHANGED. Next step if wanted: crouched shapes in `FGSTN_MovementHurtboxExtension` read while the tag holds.
3. During a move the state ends (`bKeepDuringMoves` off): the aerial's own clip owns the pose; it comes back after the move if LT is still held.
4. Ground crouch while held on landing is the existing Mover/GASP crouch (untouched).

## Build and content (desktop, one session, editor closed)
1. Build the editor target (45 min timeout script). New files: `GSTNCombatCore/.../Rules/GSTN_AirCrouch.{h,cpp}`,
   `GSTNCombatGAS/.../Kits/GSTN_AirCrouchContentLibrary.{h,cpp}`, test `GSTN_CombatTests_AirCrouch.cpp`.
2. Tests: `GSTNCombat.Core.AirCrouch` (2 tests) plus the usual `GSTNCombat` set.
3. `UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<project>/Tools/GAS/setup_air_crouch.py" -unattended -nullrhi`
   (log `Saved/AirCrouch/setup_air_crouch.log`). Revert the known dirtied assets (PM_BoostJump etc.), commit the new clip, DA and kits.
4. PIE check: jump, hold LT: crouch pose in the air; release: normal fall; aerial with LT held: aerial clip, then crouch again.
   Rule 10: capture before showing the owner.

## Open
- Per-kit animation set: today the clip is in the character's reaction set (one for all kits); per kit when the B3 kit hook lands.
- Rise / fall variants of the air crouch: need a rising/falling projected tag (`Combat.State.Falling` is declared but not projected).
- Mover crouch in the air: if the GASP pawn sends crouch intent in the air, the Mover capsule may already shrink there. Not checked.

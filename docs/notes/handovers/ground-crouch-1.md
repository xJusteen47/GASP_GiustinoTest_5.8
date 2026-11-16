# Ground crouch, checkpoint 1 (cloud, 2026-11-16)

Owner report (2026-11-16): holding crouch on the ground does nothing (light kit; in the air it works). Cause: ground crouch was GASP's IA_Crouch
(toggle, BP -> Mover stance modifier), out of the input mapping since 11-13; and Mover's stance modifier would be undone anyway by our body
capsule modifier, which assigns the capsule every substep. Branch `claude/project-thread-xe8mcz`, base `claude/integ-playtest-1116` b528f9a8
(`claude/kit-playtest-1116` was not on the remote yet: merge it in, no overlap expected). Code NOT compiled (cloud).

## What it does (all kits, data only, no BP wiring, no project settings)
- `FGSTN_GroundCrouchKitExtension` (Kit > Extensions > Ground Crouch). NO extension = defaults ON, so every kit crouches; add it only to tune/disable.
  Fields: bEnabled, HoldButton (None = Input.Button.Crouch, LT), bCrouchWalk, bKeepStanceDuringMoves, bKeepTagDuringMoves, capsule 55/0, CrouchHurtboxShapes, BlockedTags.
- Combat tick (`ProjectGroundCrouch`, inside ProjectStateTags): grounded (Mover tracker) + button held + not hit-locked -> stance; projects `Combat.State.Crouch`.
- `FGSTN_BodyCapsuleModifier` gets `bCrouched` (Mover sync state, NetSerialize): outside Body Capsule tracks the capsule goes to the crouch capsule
  (Body anchor: mesh still, bottom on feet), and the modifier reports `Mover.IsCrouching` -> GASP ABP / walking mode see a crouch as with IA_Crouch.
- Pushbox follows the capsule by itself. Hurtbox: CrouchHurtboxShapes (default feet..~1 m) while crouched outside moves (Simple level).
- Mover's own stance handling is switched off by the combat component (`bNativeGroundCrouch`, default on) so nothing fights the capsule.
- Air crouch unchanged; landing with LT held -> ground crouch next frame; takeoff -> capsule back to standing in the air.

## Defaults applied (rule 6), to confirm with the owner
1. Crouch walk ON (GASP behaviour). Smash-like "stick does not walk while crouched" = `bCrouchWalk` off (both shipped).
2. A move started crouched keeps the crouch capsule (no pop on a crouch attack); a move never starts the crouch. Tag drops during moves.
3. Hold, not toggle (GASP's IA_Crouch was a toggle).

## Build and check (desktop, editor closed)
1. Build editor target (45 min script). New: GSTNCombatCore Rules/GSTN_GroundCrouch.{h,cpp}, tests GSTN_CombatTests_GroundCrouch.cpp,
   GSTNCombatNP Tests/GSTN_CombatNPTests_GroundCrouch.cpp.
2. Tests: `GSTNCombat.Core.GroundCrouch` (3), `GSTNCombat.NP.GroundCrouch` (1), plus the usual GSTNCombat set (30 min script).
3. PIE: hold LT standing -> crouch pose, lower capsule; walk -> crouch walk; release -> stand; LT + attack -> crouch attack; jump with LT -> air crouch.
   Rule 10: capture before showing the owner. No content step (no asset changes).

## Open / risks
- GASP pawn BP reads `IsCrouching` from Mover (strings seen in SandboxCharacter_Mover); if its Stance comes only from WantsToCrouch the pose won't show: check in PIE.
- Uncrouch under a low ceiling is not checked (capsule grows anyway). Crouch speed: GASP walking mode's own crouch speed (stats modifier untouched).

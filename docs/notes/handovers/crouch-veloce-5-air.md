# Crouch veloce 5: air crouch (desktop _ue59, Opus, 2026-11-19) - pose PASS on all 3 traits, landing transition FAIL (stopped, 2 attempts)
Branch `claude/locomotion-finish-1` 4162c5ea. Brief: coordinator note "finisci tutto", item 2.

## Done
- Plan `Tools/GAS/crouch/crouch_plan_v5_air.json`: tuck by IK targets from the pelvis (`leg_ik` / `arm_ik` with `rel_pelvis`).
- Cause 1 (v7 FAIL, feet 40-60 cm off): the game's leg IK reads `ik_foot_*`; air clips left them on the fall pose. crouch_fk_build now
  always syncs ik_foot from the feet. Proof: Saved/Crouch/v8/cr_air/frames.jsonl v57 foot_l [12,-8,-30] rel pelvis (v7: [60,-12,-47]).
- Cause 2 (no enter): the character uses /Game/GSTN/Animation/Reaction/DA_ReactionAnims_HitAdd (SandboxCharacter_Mover), whose
  AirCrouch rule pointed at the loop only. Set enter + then loop there. DA_ReactionAnims (owner's) left untouched (edit reverted).
  Proof: Saved/Crouch/v10 frames.jsonl montage AnimMontage_0 v51-55 then AnimMontage_1.
- Review docs/notes/reviews/crouch-veloce-4/review_fable_air_v10.md: tuck, guard, enter/release PASS.

## Open (FAIL, not sent to the owner)
- Landing with crouch held (land_side v77-81): body stands up ~1-5 frames then ground crouch. Gioco/montage: the air crouch montage
  blends out (3 f) to the standing landing before the ground Crouch state is entered. Fix: keep the AirCrouch rule while landing with
  crouch held (or blend straight to Crouch Enter/Idle in 4 frames), check head top v76-86. Needs a step-1 capture of cr_land.
- Minor: v51-52 leg stretch at enter frame 0: start the enter from the actual fall legs or blend 2 frames.

# Crouch veloce, checkpoint 2 (desktop _ue59, Opus, 2026-11-19) - STOPPED: ground clips not visible, air pose FAIL (own check)
Branch `claude/crouch-veloce-2` = run stop 301ec0e0 + crouch-veloce-1 6bdca76d (merge 4eff0884) + this work. Builds OK (Saved/Crouch/build5.log, `grep -c "Result: Succeeded"` = 1).

## Done
- C++: `FGSTN_FastCrouchSettings` (DA_AnimSet_Locomotion_Default > Crouch: bFastCrouch, Enter/ExitClip, MoveSpeed 60) + pure `FGSTN_FastCrouchState::Step`
  (Off/Enter/Idle/Exit; landing crouched -> Idle directly; moving -> Off = GASP crouch walk) ticked in `UGSTN_RunPresentationComponent::TickFastCrouch`.
  ABP functions `IsAnimOwnerFastCrouch{Entering,Idle,Exiting}`, `IsAnimOwnerFastCrouching`, `GetAnimOwnerFastCrouchTime`. Cvars `gstn.FastCrouch.Mode`
  (-1 profile / 0 GASP / 1 fast), `gstn.FastCrouch.Log` (phase + current state of "State Controller" + ABP vars at each phase change).
- `GSTN_AnimBlueprintToolset::SetBlendProfileScales` (blend profile entries are not Python-exposed). `BLP_GSTN_FollowThrough` made (15 bones, handover-1 values).
- Clips by `Tools/GAS/crouch/crouch_fk_build.py` + `crouch_plan_v1.json` (GASP poses + offsets in character axes, per-bone lag, legs by IK, fists):
  AS_GSTN_Crouch_{Enter,Idle,Exit} (/Game/GSTN/Animation/Crouch), AS_AirCrouch_Enter + new loop content. Numbers: Saved/Crouch/crouch_fk_build.log
  (Enter pelvis -66.4 cm at f4, -63.4 settled = 3 cm overshoot; Exit +1.5 cm at f4).
- ABP_GSTN_Locomotion: alias "-> Fast Crouch" + states Crouch Enter/Idle/Exit (`Tools/GAS/crouch/crouch_states.py`, compiled + saved). Transition blend
  profile refused by Python (no `blend_profile` property): follow-through is baked in the clips instead.
- Captures (KIT_Test_Overlord, cr_ground/cr_air/cr_land added to pie_capture_move.py): base + v1 grids/mp4 in docs/notes/reviews/crouch-veloce-2/.

## Blockers (2 attempts used, rule 5)
1. GROUND: the state machine IS in our states (Saved/Crouch/diag2/run.log.ue.log, `grep "FastCrouch phase" | grep -c "state=Crouch Idle"` = 179) but
   the visible pose stays GASP's slow crouch, also with `DDCVar.ExperimentalStateMachine.Enable 1` (Saved/Crouch/v1sm). AnimGraph: BlendListByInt_0
   (0 = Motion Matching, 1 = TwoWayBlend(A = State Controller, B = BlendStack)); the alpha/index bindings could not be read from Python. Likely the
   live pose is MM or the BlendStack (chooser), so states that only hold their own player never show. SAME RISK for the Skid and Run Stop states.
   Needs a code thread (Opus high): read those bindings in the editor (or the Unreal Editor Assistant), then either (A) chooser raw rows + state entry
   `SetBlendStackAnimFromChooser` (GASP ANIM rule) or (B) ground crouch through the reaction montage path (proven visible; needs a "crouched and still"
   tag and an ExitAnim field).
2. AIR (visible, own look at v1/cr_air_in_az90.png): FAIL posa. One leg kicked forward straight, torso back. Cause: in crouch_fk_build a + pitch moves a
   DOWN-pointing bone's tip BACKWARD (axis up x fwd). Flip pitch signs for hanging bones (thighs, calves, upper/lower arms) in plan v2 before any review.
   v1 air assets reverted in the working tree (next commit) so the playtest air crouch is the old placeholder.

## State left
- Lock _ue59 released. _ue59 stays on claude/crouch-veloce-2 (contains run stop 301ec0e0). Fast crouch states are inert (pose never visible).
- No Fable review run (nothing passes the own check). Cost: ~150k context, Opus, no subagents.

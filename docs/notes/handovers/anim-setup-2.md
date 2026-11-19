# Anim setup 2 (desktop _ue59, Opus, 2026-11-19 18:40-~19:40 UTC) - skid foot pop: turn follows the clip + pop guard
Branch claude/anim-setup-2 (from claude/anim-setup-1 7f6c441c). Builds 3, ESITO OK (Saved/Logs/ci/build_setup2_{1,2,3}.log).
1. Cause (Saved/AnimSetup2/skid_probe.txt, skid_curves.txt, skid_root.txt from headless probes of SS_180_Turn_Root180): contact_r is a
   step 1 -> 0 at 0.317 -> 0.333; GASP's CR_Biped_FootPlacement pins the foot (Procedural_Feet, AllowFootPinning = on ground). The
   capsule turned 180 in the 8 exit frames (22.5 deg/frame) while the clip root turns 104 -> 112 deg in that window, so the pin held
   the foot through a spin and let go 13-15 cm off (cap1 frames.jsonl, world foot = actor + body.foot_r).
2. Fix A (sim, data): FGSTN_RunSkidTuning::TurnCurve/TurnCurveRate = share of the turn per skid time, baked from the clip's root yaw
   by Tools/GAS/skid_turn_curve_bake.py (also sets TurnSide from the clip). The capsule turns with it in the brake; the exit finishes
   what is left. Baked into DA_Locomotion_Default (26 samples, 30 Hz, Saved/AnimSetup2/skid_turn_curve.txt). Empty curve = old rule.
3. Fix B (anim, generic): pop guard in GSTN_AnimNode_BlendOutFootLock (PopDistance 8 cm, PopBlendTime 0.15 s, cvar
   a.GSTN.FootPopGuard): a foot moving in one frame > 8 cm more than its speed explains keeps its spot, the offset fades.
   Attempt 1 extrapolated the velocity and fired at every run foot plant (feet 20-25 cm under the floor, cap_guard): fixed in attempt 2
   (only a jump, never a stop).
4. Numbers (feet.txt in docs/notes/reviews/anim-setup-2, world move per frame cm; script scratch feet.py = frames.jsonl actor+body):
   right foot at the pin release f138: 15.0 (cap1) -> 10.3 (turn curve only, cap_turn) -> 0.4 then 6-7 cm/frame for 6 frames on the
   floor (cap_guard2). Exit left foot f147-149: 31.9/20.6/32.7 -> 2.7/4.2/4.0. Foot z stays -77..-79 (planted).
5. Review: docs/notes/reviews/anim-setup-2/review_fable.md: Fable PASS (right foot 11.1 -> 0.3 cm f137; left 31/21/34 -> 3/5/5
   f146-148; no floor issues; one turn). Follow-ups (game, guard attempts used: 2 -> next thread): a) clip entry f120-122 the guard
   freezes the left foot 3 frames then it travels late; b) exit f146-155 re-triggers swallow ~73 cm of a real swing, then a 28 cm/frame
   whip. Plan: no re-trigger while Smoothing, cap the offset (~20 cm, bigger = a real transition), fade with (1-t)^2 so the foot moves
   from the first frame. Exit turn kink 4.7 -> 10.3 deg/frame at f131 (rest frame) = the linear exit finish.
6. Tests: 1839 Success + 1 Fail (Saved/Logs/ci/test_setup2_1.log.ue.log, count "Result={Success}"); the Fail fixed and rerun
   Success (test_setup2_2.log.ue.log). KIT_Test_Light/Medium/Heavy "missing stat tags": ours, outdated by the owner's 2026-11-18 rule (all kits share movement,
   align_kits_movement_1118.py). Test now asks only WalkSpeed/JumpVelocity/Weight/MaxCarryWeight, same walk speed, weight order kept.
7. Owner question on low fps: the capture renders 7 views + PNG every frame (speed-up.md section 5). Captures now 3 views.
Open: the right foot still drags 6-7 cm/frame f139-145 on the floor = the clip's own foot slide after contact_r ends (clip space
   foot_r moves 6.6-13.5 cm per 30 fps key f10-f14 with ball on the floor) -> pose, for the per-clip isolation check thread.
   Exit turn: ~75 deg over 8 frames vs the clip's 17 deg; the clip's last 60 deg (0.6-0.833 s) are never shown (state blends out at
   ~0.5 s). Full sync needs the exit to last until the clip's turn ends (gameplay timing: owner).
Not started (owner gate 19:03: isolation check first): run start stomp, air crouch landing, dash leftovers; refactor items 3-7.
Unreal Editor AI assistant: not consulted (all checks headless; editor closed). Next thread: ask it about CR_Biped_FootPlacement unplant.

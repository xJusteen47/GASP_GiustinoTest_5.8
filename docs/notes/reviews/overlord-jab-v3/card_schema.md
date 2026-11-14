# animpipe card `author` reference (read-only research; sources: animpipe/{author,card,ue_rig,checks,verbs,__main__}.py)
Root: C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_feature\Tools\AnimPipeline (branch claude/project-thread-0pafeo)
Space everywhere: UE component space, X = character LEFT, Y = FORWARD, Z = UP, cm and degrees. Idle = sheets/raw/idle_neutral_legbones.json frame 0.
Rotation fields = rotation VECTOR in degrees (axis*angle), right-handed, WORLD(component)-axis, applied about the bone pivot (NOT euler order).
Source docstring: +rx tips up->back (forward lean = NEGATIVE rx; swings a hanging limb forward). Derived from rv() math (confirm on stick preview):
 +ry = roll about the forward axis (head top tips toward +X = left); +rz = yaw about up, +rz turns the front toward the character's RIGHT (-rz = yaw left; jab right-hand slap uses spine rz -34).

## 1. author.poses["<frame>"] fields (all optional; missing = idle, see GOTCHA)
author = {"take":"AS_Pilot_<Name>","frames":N,"thumb":"along"|"open","poses":{"0":{...},"4":{...}}}
- pelvis.loc [x,y,z]: ADDED to the idle pelvis position, component space (root-relative). pelvis.rot [rx,ry,rz]: R = rv(rot) @ R_idle (world axes at the pelvis).
- spine [rx,ry,rz]: TOTAL rotation of the trunk ABOVE the pelvis, spread /5 on spine_01..05 (world axes, on top of pelvis.rot). Chest total = rv(spine) @ rv(pelvis.rot).
- head [rx,ry,rz]: extra rotation spread /3 on neck_01, neck_02, head (world axes, additive to the trunk). rz = look left/right, rx = nod, ry = ear-to-shoulder.
- spine_chain / neck_chain {"<bone>":[rx,ry,rz]}: optional per-bone extra on top of the even spread (casc-import writes them; not needed by hand).
- clavicle_l / clavicle_r [rx,ry,rz]: world-axis rotation at the clavicle (shrug/protraction); the arm chain follows. Missing = idle.
- arm_l / arm_r {"hand":[x,y,z],"pole":[x,y,z],"hand_rot":[rx,ry,rz]}: CHEST frame = offset from the spine_05 position, axes turned with the chest. hand = wrist target, pole = elbow point (two-bone IK, elbow in the plane of the pole).
  Left hand +X, right hand -X. Examples (jab): arm_l hand [9,17.5,-20] pole [45,-5,-25]; arm_r strike hand [-57.7,11.9,6.2]. hand_rot = extra rotation of the hand (keeps idle angle to forearm otherwise);
  forearm roll = hand_rot = axis(forearm)*deg (stick_preview --roll writes it). Reach is clamped to 0.995*(l1+l2): an out-of-reach target silently shortens.
- leg_l / leg_r {"foot":[x,y,z],"pole":[x,y,z],"foot_rot":[rx,ry,rz]}: PELVIS frame = offset from the pelvis POSITION (incl. pelvis.loc), axes turned with pelvis.rot. foot = ankle target, pole = knee point.
  Idle in that frame: foot_l [17.9,15.0,-81.2], foot_r [-16.6,-17.9,-81.2] (left foot forward, right back). foot_rot = extra rotation on the solved foot. Ball/toes: no roll field (ball follows the foot).
- fingers [open,0,0]: 0 = template fists (default), 1 = open flat villain hand. BOTH hands together, no per-finger/per-hand control. Blended per frame.
- wrist_out [deg,0,0]: both hands bent at the wrist toward the body outside (same value both sides). thumb: author-level (not per pose) "along" (8 deg off index) | "open" (55 deg).
- bones {"<bone>":[qx,qy,qz,qw]}: owner pose taken whole (local to parent). Overrides the fields above for those bones; casc-import only, skip by hand.
- Not found: per-finger fields, separate neck yaw, toe roll, shoulder apart from clavicle, any root / whole-body translation field other than pelvis.loc.
- GOTCHA missing limb spec: a pose with no arm_x / leg_x solves that limb by plain FK from idle (a leg then follows the pelvis, NOT planted). Author in-betweens (frames()) lerp a missing hand/foot/pole as [0,0,0] (not idle).
  Harmless only because the build samples the author take AT CARD pose frames only: every card poses[].frame must be an author key, and give every key pose full arm_l, arm_r, leg_l, leg_r.
  Jab cards key their first/last pose with only leg_l/leg_r (idle feet) and let the arms default to idle.

## 2. Feet: pelvis-relative, NOT root-relative
World foot = pelvis_world + D @ foot, with D = rv(pelvis.rot), pelvis_world = idle_pelvis + pelvis.loc. A planted foot is therefore authored as foot = D^T (foot_world_idle - pelvis_world); that is why jab foot values change with pelvis.loc.
Helper: `python stick_preview.py <card> --plant` rewrites leg_l/leg_r (foot, pole = calf_idle + 45 cm toward the toe, foot_rot) of EVERY pose for the feet listed in card["planted"] (foot_l_ik_ctrl / foot_r_ik_ctrl) so ankle and orientation stay on idle. Edits the card in place.
 stick_preview.py exists ONLY on branch claude/desktop-anim-jab-1 (commit 74647bfb), not in this worktree: `git show claude/desktop-anim-jab-1:Tools/AnimPipeline/stick_preview.py`. Needs matplotlib+numpy (installed) and sheets/raw caches (present here).
Step 20-30 cm (no helper for it): author the stepping foot per key: planted value; lift key (foot y +10..15 and z +10..15 vs the planted value, in the pelvis frame, pole kept in front of the knee, foot_rot rx toe-down);
 land key (foot y = planted_y + 25 world, z back to planted_z, foot_rot idle). Remove that foot from card "planted" (planted_feet fails at >2 cm drift from frame 0; build pin only snaps drift <=1 cm / 2 deg to the first key).
 Leg length (~85 cm) clamps the reach, so a step needs pelvis.loc y forward too; then the other foot's pelvis-frame y shrinks by the same amount (--plant recomputes it for planted feet).

## 3. In-betweens
Build does NOT use the author in-betweens: ue_rig.build bakes source_anim to the rig, KEEPS only keys at card poses[].frame (every control is keyed at every pose frame), then sets cubic keys + tangents per segment.
 (author.frames() own interpolation = smoothstep t*t*(3-2t) over the whole pose dict, for the source take only; poses with `bones` blend quaternions.)
Spacing = weighted flat tangents, weights as a fraction of the segment (leave of its first key, arrive of its last key):
 charge_long (0.6,0.12): long slow ease-out of the key, fast arrival (windup); arrive_1_2 (0.12,0.6): fast departure, long soft settle into the next pose;
 hold_recoil (1/3,1/3): symmetric ease-in/out (flat 1/3 handles); limb_led_recovery = None = smart auto tangents, no stop at the key (flowing; default for recovery). Only these 4 are valid (card.load raises).
Per phase: card "spacing" {"startup","active","recovery"} picked by the phase of the segment START frame (from the CSV / anim_active). Per pose: poses[i]["spacing"] overrides the segment that STARTS at that pose.
 poses[i]["ease_out"] 0-1: a flat key followed by an unweighted segment leaves with this fraction as leave weight (opt-in).
part_delays {"chest","head","forearm","hand"} = frames; INTERIOR keys of the matching controls shift later (first/last key of each channel stay; a key landing >= the last pose frame is removed).
 Control substrings: chest = spine_05/chest/spine_04; head = head/neck; forearm = lowerarm; hand = "hand_". NOT lagged: pelvis, legs, upperarm, clavicle. Shifted keys take the profile where they land. 0 = off.
edits [{control,attr:"Location"|"Rotation",frames,delta:[x,y,z],why}]: add delta to the key value at those key frames on the exact rig control (hand_r_fk_ctrl, foot_l_ik_ctrl...); control-local cm/deg, NOT component space.
hitstop_blend [{frame,toward,amount,match:[control substrings]|[],why}]: key at `frame` moves `amount` (0-1) toward the key at `toward` on matched controls (after edits; Euler short way). Fixes hitstop_snap pops.
drop [{control,attr,frames,why}]: removes keys at those frames on ONE control = the only per-control sparse mechanism (it travels through). Otherwise a pose cannot key only some parts: all controls get a key at every pose frame.
 This rig: arms are FK (key *_fk_ctrl), legs IK (foot_*_ik_ctrl). Build order: drop -> edits -> hitstop_blend -> planted pin -> part_delays -> tangents.
origin (pose): "owner" (owner's Cascadeur pose: needs `source`, fidelity guard, locks) | "claude" (in-between added by `interp`, droppable). why: free text, required on exemptions/edits/drop/hitstop_blend/interp poses. source: {file,frame,tool} | null.
Card top level: move, class (jab|tilt|smash), anim, rig, fps (60), timing_csv, anim_active [a0,a1], faf_game, effector, source_anim (= /Game/AnimPipeline/Pilot/<author.take>), poses [{name,frame,source:null,spacing?,ease_out?}] (unique, ascending),
 spacing, part_delays, planted [], exemptions [], fists ["l","r"], co_motion_skip [], reach_axis ("y" | "-z"), sheet_extra {hold_window,strike_limb,strike_side | strike{limb,hiders,pose_checks}}, hitstop_max_cm, rules {}, approved [], manual {}.

## 4. Root motion: NOT supported
ue_write_take.to_tracks holds `root` constant on every frame; build keys only rig controls (root is never keyed; lock_lengths leaves root/pelvis free but unkeyed). The only whole-body displacement is pelvis.loc (body moves within component space; feet need explicit compensation, see 2). Root translation in the AnimSequence: not found.

## 5. `animpipe check` (block unless noted). Exemption = card exemptions [{"check":<name>,"frames":[..],"why":".."}] (read as exempt[check]).
legibility: legibility.py measure, striking part readable from the game cameras; thresholds legibility_thresholds.json; tune via sheet_extra strike{limb,hiders,pose_checks}; no frame exemption.
bone_lengths: every bone within 0.1 cm of reference. clearance: no interpenetration (gap<0); exempt {"check":"clearance","frames":[..],"pairs":["hand_l|lowerarm_r"]} (no pairs = all).
contact_faf: effector (card effector; jab = hand_r) reach vs pelvis along reach_axis (default y, "-z" stomp) must PEAK inside the anim_active window and clip frames-1 >= FAF.
planted_feet: for feet in card.planted (default BOTH if the key is absent) ankle+ball drift <= 2.0 cm from frame 0; exempt {"check":"planted",...}; planted [] = n/a.
fists: finger openness ratio <= 0.7; exempt {"check":"fists",...}; card "fists":["r"] tests only those sides ("fists":[] = none).
hitstop_snap: from each ACTIVE frame f to f+1 no bone (leaf turns by lever) travels > 10 cm (card hitstop_max_cm); exempt {"check":"hitstop_snap","frames":[active frames]}; or use hitstop_blend / move the travel before the hitbox.
co_motion: in the startup (0..first active frame) no arm/leg end stays <1 cm/frame in its parent's frame for >=3 frames while the rest travels >=4 cm; card "co_motion_skip":["arm_l"] or exempt {"check":"co_motion","frames":[..]}; planted legs auto-skipped.
source_fidelity (owner poses only). rules (opt-in, card "rules"): frame_0{charge,min_cm,max_frac}, knees_front{frames,tol}, feet_side_by_side{frames,min_cm}, villain_hand{frames,min_deg}.
measure only: motion_curves, twist_chain, motion_check (weight/overlap/balance; exempt {"check":"balance",...}), spacing, vs_source. Result: <data>/animpipe/<move>_check.json|txt.

## 6. Commands. cwd = Tools/AnimPipeline. Data dir = ../../../GASP_Giustino2/Saved/AnimPipeline (animpipe.local.toml). Under Git Bash set MSYS_NO_PATHCONV=1 (checks/review set it for their subprocesses).
author : python -m animpipe author cards/overlord/jab/<card>.json [--no-write]  (writes <move>_bones.json + runs ue_write_take.py -> /Game/AnimPipeline/Pilot/<take>; --no-write stops after the bone-length gate)
build  : python -m animpipe build <card.json>  (UE editor + bridge) -> /Game/AnimPipeline/Rig/AS_<Name>_Build + LS_<Name>_Build
check  : python -m animpipe check <card.json> [--src]
poses  : python -m animpipe poses <card.json> [--frames=0,6] [--take=] [--over=]  -> sheets/<move>_poses.png (stick grid from the BUILT take's bone cache)
render : python -m animpipe render <move> [--force] [--standalone --azs=90,0,45]  -> <data>/visual/<move>/r<N>/grid.png (public verbs also: status build check verify verdict approve; they take the move name; card paths go to the legacy commands)
review : python -m animpipe review <card.json> [--hitstop=8] [--at=N] [--force]  = build + check + ingame_judge.py (mp4 + strip in sheets/ingame; needs editor in L_PilotCapture)
diff   : python -m animpipe diff <card.json> [--vs=<older card.json>]  (max displacement per frame x part)   | validate: python -m animpipe card <card.json>
offline: python stick_preview.py <card.json> [--plant] [--roll=arm_r@6:-90] [--out=x.png]  (branch claude/desktop-anim-jab-1 only; no UE; writes sheets/<move>_stick.png + .txt; prints worst foot drift)
UE bridge: animpipe/ue.py ue.py PORT = 55976 (127.0.0.1, websocket, UE_MCP_Bridge) hard-coded; refused connection = editor/bridge not running on that port. Jab timing CSVs live in cards/overlord/jab/timing/ and must be copied to <data>/refs/ufd/ (handover).

## 7. DAir V7 card
Path: NOT in this worktree. Branch experiment/3d-silhouette-anim (commit 899d7524); worktree C:\Users\jacop\.gemini\antigravity\worktrees\GASP_Giustino2_feature\animate_3d_moves_unreal\Tools\AnimPipeline\cards\overlord\dair\overlord_dair_v7.json
- anim /Game/AnimPipeline/Rig/Overlord/DAir/AS_OverlordDAirV7_Build; source_anim ..Pilot/AS_Pilot_OverlordDAir_V6; author take AS_Pilot_OverlordDAir_V7, 43 frames, 10 poses (0,3,8,14,15,17,20,31,37,42) vs jab v2: 7-8.
- Every pose full: pelvis, spine, head, arm_l/r (hand,pole,hand_rot), leg_l/r, fingers (0.5 partial open), wrist_out, clavicle_l/r (~20 deg), spine_chain, neck_chain (casc-import numbers). Jab v2 has none of clavicle/chains.
- Spacing per pose: charge_long on 3,8,14; hold_recoil on 15,17,20; arrive_1_2 + ease_out 0.2 on 31; phase map startup charge_long / active hold_recoil / recovery limb_led_recovery. Poses carry source{casc file, tool casc-import}.
- part_delays {"chest":1,"head":2,"forearm":2,"hand":3} (jab v2: all 0).
- NOT used in V7: edits, drop, hitstop_blend, approved, rules, manual block (only a `_manual` text note about the additive "Manuale" layer; arms FK).
- Exemptions: balance + planted (aerial, all 43 frames), fists 0-14, 2 clearance exemptions with explicit "pairs" (crossed arms 3-42, head 3-14). planted [], reach_axis "-z", effector foot_l, anim_active [15,17], sheet_extra hold_window [15,31] strike_limb leg_l.
- Jab v2 (this worktree): cards/overlord/jab/overlord_jab1_v2.json, overlord_jab2_v2.json: planted both feet, hand_rot only via stick_preview --roll, jab2 exempts co_motion 2-7, spacing per pose arrive_1_2 / charge_long / hold_recoil / limb_led_recovery.

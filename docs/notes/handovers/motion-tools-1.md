# Motion Tools 1 (cloud, Opus high, 2026-11-17, branch claude/project-thread-t93zat from feature e79e401a)
Owner 22:09-22:14Z (ambito: authoring of move animations, all characters): a Cascadeur-like tool that works and is intuitive, to speed up
secondary motion and curves WHERE IT FITS (hand-keyed curves stay); plus optional solid parts (e.g. a stomp must not go under the floor).
Plan: project files handoffs/anim-physics-pass-ricerca.md, phase 1 = overlap + easing presets, pilot Ganondorf ftilt v31 VARIANT.
## Choices (rule 6, applied)
- Logic in **Python** (plugin `Content/Python/gstn_motion_tools`), panel in **C++ Slate** that calls it: one code path for owner, MCP and agy;
  a runtime bug is a hot reload, not a rebuild. Math with no `unreal` import, tested in the cloud (24 tests, command in the plugin README).
- Layers = **additive Control Rig sections** on the same track, one row each ("Easing" under "Overlap"). Not the 5.5+ Animation Layers
  panel object: its API is not confirmed exported; the section is the same Sequencer object, shown as its own row. Rows + settings in
  the sequence metadata tag `GSTN.MotionTools`.
- Key protection default ON (weight 0 on base keys + first/last frame, smoothstep over 2 frames). A baked clip (FK Control Rig, a key
  every frame) is detected (>=60% frames keyed): only ends protected + warning; pass the owner key frames (panel field / `key_frames=`).
- Solids: phase 1 = **floor guard** of the layer only (sphere radii = ground-contact fix, branch 27htqa). Fixing a BASE that goes through
  the floor is phase 2 (below).
## NOT done (cloud, no engine): nothing compiled, nothing run in the editor. Desktop steps (Haiku/Sonnet, after F4, rule 7):
1. Merge in _ue59 (IN-USO.txt), Run-Timed build (45 min), max 2 fix attempts. 2. Window > GSTN Animation > Motion Tools opens.
3. Variant (rule 13b, never the approved/liked one): duplicate `AS_GanonFTiltV31_Build` -> `AS_GanonFTiltV31_Overlap`, open it with
   "Edit with FK Control Rig" (bridge `begin` does the same), save the Level Sequence next to it as `LS_GanonFTiltV31_Overlap`.
4. Smoke test on a copy: Python `import gstn_motion_tools.selftest as st; st.run("<LS path>")` -> ESITO PASS/FAIL,
   `Saved/GSTNMotionTools/selftest.json`. If FAIL "neutral": the rig's scale channels move in an additive section (report, stop).
5. Pilot: chain Braccio sinistro (free arm) preset Pesante + Testa Leggero, Frame chiave = owner keys of v31 (from the move card),
   then Easing Scatto on the strike interval of the striking arm. Bake to `AS_GanonFTiltV31_Overlap` (Sequencer bake).
6. Capture base vs variant (same frames, docs/notes/reviews/ftilt-v31-overlap/), Fable review rule 10; only PASS to the owner.
## Phase 2 "Solidi" (design, ~10 $ with Pulisci/Baricentro): a third layer "Solidi" ABOVE Overlap, opt-in per move. Planes or boxes
(world, or "floor at the lowest foot of frame 0"); per frame, spheres of the ground-contact list: body spheres lift the pelvis control
along the normal (cap, eased), limb ends that already touched stay planted with the rig's IK controls (foot_ik), others are raised.
Same order as the runtime node, so authoring and game agree; review = the runtime node OFF should show no penetration.
Risks: 5.9 Python names unconfirmed (`ControlRigSequencerLibrary.get_local_control_rig_euler_transforms` / `get_control_rig_world_transform`,
section `set_blend_type`/`set_row_index`, channels `<ctrl>.Rotation.X`): failures show as a clear panel/log error. C++ panel not compiled.

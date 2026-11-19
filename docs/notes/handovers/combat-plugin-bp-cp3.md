# COMBAT-PLUGIN-BP CP3 handover (cloud, 2026-11-15)
Branch `claude/project-thread-p2xw3s` from 6bl38a `e2940d75` (CP2, compiles on desktop). **NOT BUILT, no assets made yet**: the desktop runs the steps below.
**Done (code + script, no .uasset)**
- Editor library `UGSTN_MoveGraphAuthoring` (GSTNCombatEditor, `Public/Graph/`): from Python adds variables (sync vars too), parent-class events, calls,
  get/set, make/break, branch; connects pins via the K2 schema; compiles; runs the CP2 sim-graph rules; dumps the graph. Base for the CP5 factory.
  Test `GSTNCombat.Editor.MoveGraph.BuildsAndCompiles`.
- `Tools/GAS/setup_double_shot_examples.py`: the double-press special twice, in `/Game/GASNPPTest/Examples/DoubleShot` + kits in `Examples/`.
  A data only: `GA_Example_DoubleShot_A1` (Event key f8 + Fire Projectile action, Tags `Example.State.DoubleShotChain` f8-20, cancel into special
  f9-20) and `_A2` (shot f6, speed x1.5), `KIT_Example_DoubleShotA` binds the special to A2 while the tag is up (jab chain pattern).
  B graph: one `GA_Example_DoubleShot_B` (On Move Key Fire -> shot + FollowUpState=1; On Move Frame: state 1 and `WasCommandEnteredWithin`
  (special neutral or directional, 8 f) -> state 2, Open Move Tag Window (timeline OnTagAdded buffer rule flushes the press), Wait Combat Frames 6
  -> On Combat Frames Elapsed -> second shot; key WindowEnd f20 closes). `KIT_Example_DoubleShotB`. Shot `DA_Example_Shot` (15 cm, 1800 cm/s).
- Tags in `Config/Tags/GSTNExamples.ini`; hub pads (1600,-1400) A and (1600,-1050) B in `build_test_hub_map.KIT_PADS` (spawned at Play, no sign).
- Docs: GSTNCombatEditor + GSTNCombatNP READMEs, architecture section 8 (Artifacts/ + artifact CLq2srEECQnd6bKDbBmm72 v3).
**Design choice (owner, game feel)**: A = second shot is its own move (own recovery); B = second shot inside one move (FAF 28 always). Both shipped.
**Desktop steps (Haiku low, owner editor closed, max 2 fix attempts each)**
1. `_ue59`: merge `origin/claude/project-thread-p2xw3s` (fast-forward from 6bl38a), build the editor target.
2. Tests: `GSTNCombat.Editor.MoveGraph`, `GSTNCombat.Editor.SimGraph`, `GSTNCombat.NP.MoveActions`, `GSTNProjectile.Core`.
3. Headless: `UnrealEditor-Cmd <uproject> -run=pythonscript -script="<repo>/Tools/GAS/setup_double_shot_examples.py" -unattended -nullrhi`
   (if KIT_Example is missing run `setup_designer_examples.py` first). Expect `saved 9` and no ERROR in `Saved/Examples/setup_double_shot.log`;
   graph dump in `Saved/Examples/double_shot_graph.txt`. Copy both to `docs/notes/reviews/cp3-double-shot/`. A failed graph: rerun with `s.run(force=True)`.
4. Commit the 9 new .uasset (Examples/DoubleShot/* + 2 kits) + logs; revert the known dirty PM_BoostJump/PM_ThrowRider/IC_GASNPP_Test/KIT_Test_Hits; push to this branch.
5. PIE on L_Test_Hub: each pad, Y once = 1 shot; Y twice within ~0.3 s = 2 shots, the second faster; Y twice slowly = two single shots. Video both kits.
**Risks**: struct text import of the Fire Projectile action (script checks and stops); B's Class Defaults (script re-sets Projectile, warns);
  likely compile spots: `AddDefaultEventNode` ghost state, `CanBeMade`/`bMadeAfterOverridePinRemoval` names on 5.9.
**Next**: CP4 cosmetic hooks (new thread); optional "hit-confirm follow-up" example (plan) folded into CP5.
**Cost**: cloud Opus, ~140k context, no subagents, ~$14.

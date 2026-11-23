# COMBAT-PLUGIN-BP CP4 handover (cloud, 2026-11-15)
Branch `claude/project-thread-ckzv21` from p2xw3s `1d1b0d5d` (CP3). **NOT BUILT, no .uasset made**: the desktop runs the steps below (after CP3's).
**Done (code + script + tests, presentation only, never read by the simulation)**
- Move cosmetic windows: `UGSTN_GA_BaseCombat::CosmeticWindows` (tag, timeline frames start/end(-1)/tail, Niagara on a socket, sound, freeze in
  hitstop, kill when interrupted) played by `UGSTN_MoveCosmeticsComponent` from the presented state through the pure `FGSTN_MoveCosmeticTracker`
  (no replay after a correction flicker, skipped frames still play, natural vs interrupted end). BP: On Move Cosmetic Begin/End + delegates.
- Hit cue `UGSTN_GCN_CombatHit` (GCN Burst child): placed at the hit context location, faced along the hit, deduped with the attacker's
  predicted cue (`ShouldPlayAuthoritativeCue`, which had no caller before). `UGSTN_CombatCueLibrary::GetCombatHitInfo`. Predicted cue context now carries the reaction.
- `UGSTN_HitPresentationComponent`: On Hit Predicted / On Hit Prediction Cancelled, `HitstopAnimRate` (default 0 = unchanged).
- `UGSTN_ReactionAnimComponent`: On Reaction Started/Ended; `DA_ReactionAnims.HitstopShake` = victim trembles 3 cm side to side relative to the
  screen during hitstop (attacker 0), fading over 0.2 s, scaled by camera distance; mesh offset only. **Visible change, default ON** (set 0 to undo).
- Editor `UGSTN_CosmeticAuthoring` (hitbox cue tag, active frames). Script `Tools/GAS/setup_cosmetic_hooks.py`. Tags in `Config/Tags/GSTNExamples.ini`.
- GSTNCombatNP now depends on Niagara (private module dep + .uplugin). Docs: 3 READMEs, architecture section 9 (Artifacts/ + artifact CLq2srEECQnd6bKDbBmm72 v4).
**Design choices (coordinator may relay)**: hit sparks stay on GAS cues (GCN Burst = data-only, works on every machine) instead of a new system;
cosmetic windows live on the move class (not the timeline track: no timeline editor work); cancel during active frames kills the trail, during the tail fades it.
**Desktop steps (Haiku low, owner editor closed, max 2 fix attempts each)**
1. `_ue59`: after CP3's steps, merge `origin/claude/project-thread-ckzv21` (fast-forward from p2xw3s), build the editor target.
2. Tests: `GSTNCombat.NP.MoveCosmetics` (5), `GSTNCombat.GAS.ReactionAnim` (incl. HitstopShake), plus CP3's list. Likely compile spots: Niagara
   spawn signatures in `GSTN_MoveCosmeticsComponent.cpp`, `TConstArrayView` from `{}` in the test, UBT warning about the Niagara plugin dependency.
3. Headless: `setup_designer_examples.py` (if GA_Example_* missing) then `UnrealEditor-Cmd <uproject> -run=pythonscript -script="<repo>/Tools/GAS/setup_cosmetic_hooks.py" -unattended -nullrhi`.
   Expect `done: 4 cues`; copy `Saved/Examples/setup_cosmetic_hooks.log` to `docs/notes/reviews/cp4-cosmetics/`.
4. Commit: SandboxCharacter_Mover (shared BP, script-edited, component GSTN_MoveCosmetics), 4 `Examples/Cosmetics/GCN_Example_Hit_*`, 3 `GA_Example_*`,
   the log; revert the known dirty PM_BoostJump/PM_ThrowRider/IC_GASNPP_Test/KIT_Test_Hits; push to this branch.
5. PIE L_Test_Hub, pad ESEMPIO, hit the dummy with X and RT: victim visibly trembles sideways in the hitstop (record 0.25x video, base vs
   `HitstopShake.VictimAmplitudeCm=0`); console/harness `GetCosmeticsInfo` on the attacker = "began,ended,interrupted,active" grows by 2 per move
   (trail + whoosh), active back to 0. Harness 2 clients with lag: no growth of "began" on corrections beyond one per move.
**Not done / next**: no hit VFX, trails or sounds exist in the project: Niagara/sound slots empty (log lists candidates). Next content step: a
minimal VFX kit (trail ribbon, spark in 3 sizes, 1 camera shake for Heavy+) then rerun with `force=True`, then a rule-10 review (VFX off/on).
Remote proxies: windows rely on the input history's latest frame in the interpolated state (unverified). CP5 = factory + guide, new thread.
**Cost**: cloud Opus, ~130k context, no subagents, ~$14.

# COMBAT-PLUGIN-BP CP2 handover (cloud, 2026-11-15)
Branch `claude/project-thread-6bl38a` from a5idak `0a546851` (contains CP1 + react-hook-6) + docs branch 0tmywc-docs merged (Artifacts/).
Code commit `774a8fc6`. **NOT BUILT here: the desktop thread "Build plugin and recapture moves on desktop" builds + tests this branch and pushes compile fixes (laptop RC request withdrawn).**
**Done**
- Event keys now RUN: on the crossing frame, inside the ability's sim step, once per crossing (CP1 cursor): CueTag (fork frame-stamped cue),
  GameplayEventTag (HandleGameplayEvent on own ASC: triggered abilities start, timelines adopted), then the key's new `Actions`, then On Move Key.
  If the event starts a replacing move the rest is skipped. Variant A of the double-press special (key -> event -> GA) now works in principle.
- Move actions `FGSTN_MoveAction` (GSTNCombatNP `Abilities/GSTN_MoveActions.h`), instanced structs on `FGSTN_TimelineTrack_Event::Actions`
  (Core names the base only by path): Send Event, Apply Effect, Open Tag Window (2 sync-var slots, loose tag, closes after N combat frames or
  at move end). BP nodes on the ability: Send Move Event, Apply Move Effect, Open/Close Move Tag Window, Run Move Action, Get Move Random
  Float/Integer (seed = sim frame + move start frame + salt). All refuse outside the sim step of an active move (`CanRunMoveActions`).
- Projectile: `UGSTN_ProjectileDefinition` data asset (`FGSTN_ProjectileShotSpec`), action Fire Projectile + node Fire Move Projectile with
  `FGSTN_ProjectileShotAdjust` (yaw/pitch/speed). Shared builder `FGSTN_ProjectileShot::Fire` now also used by `UGSTN_GA_FireProjectile`
  (behaviour unchanged: no adjust = old path bit for bit). `EGSTN_ProjectileAim` moved to GSTN_ProjectileDefinition.h (same module).
- Editor: `FGSTN_SimGraphRules` (GSTNCombatEditor, +BlueprintGraph dep) walks what sim events reach: Ability.SimLatent (Delay/timer error,
  async task warning), SimNonSyncVariable, SimWorldRead, SimRandom; Timeline.MoveAction for actions that cannot run. Track label shows actions.
- Tests: `GSTNCombat.NP.MoveActions.*` (4), `GSTNCombat.Editor.SimGraph.*` (2, real BP built in /Temp), `GSTNProjectile.Core.ShotAngleOffset`.
- Docs: GSTNCombatNP + GSTNProjectile READMEs; architecture doc section 7 (Artifacts/ + artifact CLq2srEECQnd6bKDbBmm72 v2).
**Limits / risks**: existing content whose Event keys already had a cue or gameplay event set will now fire them (by design; check the hub
  moves in the first playtest). A move has no event target (EventTarget shot = facing). Two shots of one owner in one frame = one ghost.
  Minion timelines do not run actions. Validator severity: only latent = error.
**Next**: build `_ue59` on desktop when free (owner rule) after the laptop build passes; CP3 (double-press 2-projectile special, data-only A
  and BP-graph B, Python/MCP content, video + cloud review) in a new thread.
**Cost**: cloud Opus, ~150k context, no subagents, ~$15 + laptop Haiku build.

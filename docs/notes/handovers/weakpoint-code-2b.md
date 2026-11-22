# Weak point precision, code slice 2b (laptop, 2026-11-09)

Branch `claude/laptop-weakpoint-2b` from merge tip 35f15178. Owner decisions of 19:24Z: strike kind and hard-surface behaviour per hitbox group,
recoil per attack or at least per strike kind, swing logic valid for punches/kicks. Doc: `Plugins/GSTNCombatNP/README.md`, bullet "Strike per hitbox group".

**Done**
- Hitbox group = `FGSTN_TimelineTrack_Hitbox`. New: `bOverrideStrike` + `StrikeKind`, `HardSurfaceBehaviour`, `SweepDeflection`, and its own
  `bHasRicochetResponse`/`RicochetResponse`. Without the override the move's values apply (kept as the default: existing content and slice 1 unchanged).
  `ResolveStrike(TrackIndex)`, `GetStrikeEndFrame`, `GetRicochetResponse(TrackIndex)` on `FGSTN_CombatTimelineData`; `FGSTN_ActiveHitbox::TrackIndex`.
- Sim: `FGSTN_CombatSyncCore::StrikeTrack` (int16; serialized only while a hard contact or a recoil is set, in reconcile). Only the contact group slides
  and skips the hard hurtbox; FinishThenRecoil recoil starts past THAT group's EndFrame (slash then thrust: the thrust never plays); ImmediateRecoil
  is decided per hit's group; stab-only hurtboxes test the group's kind.
- Recoil chain: group > move > weapon > kit > project default per kind. **Choice: the kind fallback lives in `UGSTN_CombatSettings`**
  (`SwingRicochetDefault`, `StabRicochetDefault`), copied into each table set and its checksum. Reason: one place for "the swing recoil" across all
  weapons, settable from ini without the editor. Unspecified groups have no kind default.
- Placeholders (owner asked): `Config/DefaultGSTNCombat.ini` = sample shove montages `M_relaxed_ragdoll_shove_stand_B_A__Montage` (Swing, 30 frames)
  and `_B_V__` (Stab, 24 frames). Sample content is not in git: on a machine without it the montage is null and only the recoil frames remain.
- Punch/kick check: nothing ties Swing to a weapon or blade (the "flat" is the hitbox shape's Z; a shape without a clear blade gives case A). Test.
- Validator: long-stab warning per group; warning for a group recoil switched on with 0 frames.
- Test `GSTNCombat.NP.Weakpoint.PerHitboxGroup`; `Ricochet.FallbackOrder` updated to the new `ResolveRicochet` signature.

**Not done**: editor UI for the per-group fields is the default details panel (no timeline-editor lane colour). Harness run not done (no
behaviour change for content without overrides; placeholders change PIE only for Swing/Stab moves with no recoil authored).

**Gate**: build `Saved/WP2b/build-b1.out` first try (246 s, reflection change). Headless `Saved/WP2b/headless-h1.log`: 1472 / 0 fail / 0 ensures (+1).

Cost: ~120k context, Opus, estimated ~$5-6. Builds 1, headless 1.

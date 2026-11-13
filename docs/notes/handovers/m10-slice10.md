# M10 slice 10: throw wind-up, G12-G13 (desktop, 2026-11-11)
Branch `claude/desktop-m10-10` from `claude/desktop-m10-9` (1efef002) + M11 plan docs (`claude/laptop-m11-plan` 4f192aa5, cherry-picked, docs only). Doc: `Plugins/GSTNHeld/README.md` step 15.

**Wind-up (owner's request)**: `FGSTN_HeldThrowWindup` (ReleaseFrames, Montage, PlayRate, RootMotionScale) per object (`bOverrideWindup`), project default
`DefaultThrowWindup`, thrown enemies `ThrownEnemies.Windup`. **Shipped at 0 = the slice 9 instant throw.** Release counted in combat frames from RT's
release (sync vars `WindupClock`/`WindupFrames` in `UGSTN_GA_HeldThrow`), montage via `PlaySyncedMontage` (placeholder `AM_M_relaxed_ragdoll_shove_stand_F_A`;
the `..._Montage` files in Shoves are redirectors, F_A__Montage points at B_A). Dev `gstn.Held.Windup [obj] [enemy]` (PIE only), `gstn.Held.Spawn .. Windup=N`.
**Choices**: aim is read at the release frame (steerable during the wind-up; camera aim keeps reticle/shoulder until then); a hit (any of the object's
`DropWhenHolderHasAny`) stops the wind-up, cancels, stops the montage, no throw, the host drops the object as today; enemies use the same setting and rule.
**Fork fix**: `UNpAbilitySystemComponent::TickAbilities` now ticks active ServerOnly instances (NonReplicatedInstances never ticked: the dev throw could
not wait; should also make `FireDelayFrames` work on `UGSTN_GA_FireProjectileServer`, untested). LocalOnly untouched (client resims).

**G12 (part)**: `gstn.Held.Debug 1` (state, holder line, flight ±10 frames, ghost error line); `UGSTN_HeldObjectDefinition::IsDataValid`. **Not done**:
hand-socket attach + release blend (spike S2), arc preview (needs a GSTNHeldEditor module), interact prompt, HoldFrames validator, the G12 video.
**G13**: SUITE_DEFAULT in one PIE (`run_suite`) **50/50 PASS**, 0 ensures. `held_bytes_perf` (`SUITE_HELD_BYTES`, recorded): 16 idle objects add 0 B/s
within the ±1 KB/s baseline noise (26 KB/s per client); 32-64 projectiles alive cost +0.75 ms on the whole editor frame (8.4 -> 9.15 ms, three worlds,
-nullrhi), listen and dedicated alike. Not measured: with rendering, per-update bytes of one flying object.

**Camera (item 3) not done, needs a choice**: GSTNCamera is on no pawn. Needed: `UGSTN_CameraComponent` on the player pawn BP (Blueprint) as view target,
the pawn implementing `IGSTN_CameraTagSource` that returns `UGSTN_PrecisionAimComponent::GetViewRequestTags()`, and spikes C1 (tick order vs NP
interpolation) and C4 (control rotation vs GASP look code). Making it the view target only while `Camera.Request.Aim` is set (so the default view stays
GASP's) is an architecture choice for the owner/coordinator.

**Gate**: builds b1-b3 ok (12-23 s). Headless GSTN 1498/0 (+1 `GSTNHeld.Core.ThrowWindup`), 0 ensures. Held harness (`Saved/Logs/m10j/`) listen 2 %
and dedicated 0 %: 7/7 old PASS, `held_throw_windup` PASS after 2 scenario fixes (ServerOnly tick, then reading the host after the release: each report
stalls the game thread ~0.25 s). **M10 is complete except** the G12/G13 items listed as not done above. Nobody has seen the wind-up in PIE.

Cost: ~140k context, Opus, estimated ~$9-12; a power cut (09:16-12:45) killed one harness run, nothing lost. Headless ~60 s, suite ~13 min, held harness ~9 min.

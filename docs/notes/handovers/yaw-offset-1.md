# Handover yaw-offset-1: sideways launch angle per hitbox (2026-11-16)

Branch `claude/project-thread-pkfl8v`, base `feature/gas-npp-5.8` d258c41c. Cloud thread, NOT compiled (no engine in cloud). Owner approved 2026-11-16 ("fallo").

## What
- `FGSTN_HitData::YawOffsetDeg` (Hit|Knockback, -180..180, default 0 = old behaviour, no asset changes). Turns the launch about Up after the
  Yaw Source picks the way: + = right, - = left (90 with AttackerFacing = straight to the attacker's right). Elevation stays AngleDeg.
  AutoLink with a moving hitbox ignores it (3D follow). Variants can override it (`EGSTN_HitDataField::YawOffsetDeg = 1 << 22`).
- `UGSTN_HitMathUtils::LaunchDirection(..., float YawOffsetDeg = 0.f)`: new trailing default param, so the other callers (Link throws test, tests) are untouched.
  Wired in `GSTN_HitSteps.cpp` (knockback step), `GSTN_CombatTick_Victim.cpp` (push promoted to launch), editor preview.
- Editor: Yaw Offset Deg row under Yaw Source + "Launch seen from above" compass (`SGSTN_YawCompass`, read only, dimmed with AutoLink)
  with a sentence; preview line now says "N deg sideways (+ right)".
- Tests: `GSTNCombat.Core.Hits.DI.LaunchDirection` (+7 checks), `GSTNCombat.Editor.HitData.KnockbackPreviewMatchesModel` (+7 checks).

## Desktop steps (one thread, _ue59 lock)
1. Close the editor. `git fetch origin claude/project-thread-pkfl8v` and merge it into the integration branch being built.
2. `Tools\CI\Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\build.log` (new .h/.cpp + UPROPERTY: full build, not Live Coding).
3. `Tools\CI\Run-Timed.ps1 -Preset Test -Log Saved\Logs\ci\test.log -Tests GSTNCombat` ; expected 0 failures, the two tests above green.
4. Open any timeline hitbox details: Knockback group shows Yaw Offset Deg and the compass.
Max 2 fix attempts (rule 5). Not checked: compile, the compass look, the DI interaction in play (DI turns ±15° on top, by design).

## Not done / to confirm
- No drag on the compass (number field only): cheap follow-up if the owner wants it.
- Sign convention + = right is UE yaw sense; da confermare con l'owner at playtest.
- Cost: ~3 $ (estimate, Opus, cloud).

# F4 crash fix (desktop, _ue59, 2026-11-18), branch claude/project-thread-98dml5

**Crash** (Saved/Crashes/UECC-...D756C860..._0000, 2026-11-17 23:55): `checkf` in Mover `FMoverDataStructBase::Interpolate` on
`[GSTN_AirTurnInputs]`, called by NP `TFixedSmoothingService::FinalizeSmoothingFrame` -> `FMoverDataCollection::Interpolate`.
The F4 air-turn input entry (GSTN_AirTurn.h) had no Interpolate/Merge/ShouldReconcile, which Mover asserts on for any input entry
that is smoothed. It fired as soon as a smoothed pawn was in the air with the air turn on.
The two older ensures (_0001/_0002, 23:35, "Gap longer than expected" in the interpolate service) are ensures, not crashes; not touched.

**Fix** (1 attempt): overrides on FGSTN_AirTurnInputs (nearest-frame rate, Merge keeps the larger rate, reconcile on rate change)
and the same on FGSTN_WallInputs (GSTN_WallMovementTypes.h), which had the same latent hole. Regression test
`GSTNCombat.NP.AirTurn.InputsSmoothing` (goes through FMoverDataCollection::Interpolate).

**Build** `Tools/CI/Run-Timed.ps1 -Preset Build -Log Saved/Logs/ci/build_crashfix.log`: ESITO OK (67 s).
**Tests** `Run-Timed.ps1 -Preset Test -Tests GSTN -Log Saved/Logs/ci/test_crashfix.log`: 1768 Success / 0 Fail
(count: Select-String "Result={Success}" Saved/Logs/ci/test_crashfix.log; was 1767 + the new test).
Not done: no PIE replay of the exact crash (no repro clip); the test hits the same call chain.
Cost estimate ~4 $. _ue59 built and free.

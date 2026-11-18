# Bug pass desktop 1: integration, build, tests, fixes (2026-11-18, desktop, own worktree GASP_Giustino2_bugpass)

Branch `claude/bugpass-integration` = origin/claude/project-thread-98dml5 (5b22e56e) + k9sj5a ab2746ac, 016vb8 305251a4, i8mmmj 2031201b, 3f1vdx 11695c8d (merged in that order).
- Merge: conflict in `GSTN_CameraAnchor.h` (98dml5 lacked the tilt-plane fields 3f1vdx added; took 3f1vdx's side, the .cpp uses them) and `camera-tiltshift-1.md` (both sides).
- Build: UBT refuses while ANY editor has Live Coding on (mutex is per engine exe): build own worktree with `-NoHotReloadFromIDE` (Run-Timed -Kind Build, see build3.log).
- Compile fix d73be79f: CP1/B7 left `GSTN_BodyCapsuleModifier.cpp:414` on the old `SweptHorizontalMove` signature (never compiled in the cloud).
- Tests (headless, own worktree, coordinator OK without the lock): full `GSTN` 1838/1841 (`Saved/Logs/ci/test_gstn.log.ue.log`, `Check-TestLog.ps1`); the 3 fails
  (KitArchetypes.Content, Toolsets SkidLayer copy, Preview.Spike.MontageSampling) are sample content missing from a fresh worktree (Skeleton == nullptr), as in earlier handovers.
  After the fixes: `GSTNCombat+GSTNDefence+GSTNLink+GSTNInput+GSTN.Camera` 1067/1068 (test_c1, only MontageSampling), `GSTNDefence+GSTNCombat.NP` 306/306 (test_d1).
- Worktree Content: untracked sample folders are junctions to `GASP_Giustino2_ue59/Content` (Audio, Characters, MetaHumans, ...). Never save them from here.

## Fixes (each attempt 1 of 2)
| # | Commit | What | Proof |
|---|---|---|---|
| C1 (+C4) | 292b947f | Bump knockback move carries `DragFrames` (= BumpFrames); its 0.25 drag acts only inside them, in the move and in the air momentum step (`ActiveDragPerFrame`). New SetByCaller `Combat.SetByCaller.MoveDragFrames`, net-serialized only when DecayPerFrame > 0. Owner 1A: a bump on a recovered character just bumps. | `GSTNCombat.NP.Hits.Impact.WallCeilingDefaultBounce` (asserts DragFrames 14, Frames > 14) |
| D1 | 71c8d742 | `FGSTN_TumbleLanding::AfterClocks`: last downed frame while not free (timeline / air, not KO) moves the end one frame on; the neutral get-up plays on the first free frame. | new `GSTNDefence.NP.Landing.GetupWhenEndCameBusy` |
| I1 | 2a22ff35 | IMC_GASNPP_Test RT/LT: Axial DeadZone 0.25 (owner "ok l1"). Script `Tools/GASNPPTest/i1_trigger_deadzone.py`. | fresh headless load (`C:\Users\jacop\gstn_bp\i1_dump.json`) |
| B1 aid | cecb996e | `UGSTN_RunPresentationComponent::GetScrambleLeanAndAlpha` (debug) + `pie_b1_lean_probe.py`. B1 math already covered: `RunStartSkid.ScrambleLowFps` Success. | |

## Still open
- PIE (needs the _ue59 lock; this thread was last in the queue): `pie_c1_bump_walk.py` (C1 walk speed after a bump, boot `C:\Users\jacop\gstn_bp\boot_c1.py`)
  and `pie_b1_lean_probe.py` (lean at t.MaxFPS 60/30/20). Launch: `Run-Timed.ps1 -Preset Capture -Script <boot>` from this worktree. See the PIE section below.
- K3 (camera after landing on slopes), I1 feel, B2/B6, B4, K4: feel checks for the owner's playtest; not scripted here.
- C1 with tumble air recovery + jump (air drift) is covered by the same drag window but not seen in Play.

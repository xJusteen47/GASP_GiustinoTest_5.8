# PUSHBOX-P4 handover: rigid core + stronger soft pushbox (desktop, 2026-11-16). DONE (code, tests, 2 captures)
Branch `claude/pushbox-p4` from `claude/pushbox-p3` bb582dcb, folder `GASP_Giustino2_ue59`. Opus, no subagents. Not pushed (owner approval in thread).
`AM_OverlordDAirV9` dirty and jab `director/` untracked: not ours, untouched. No asset, map or project setting saved. `_ue59/IN-USO.txt` set then freed.
## Code
- Core `GSTN_Pushbox.h`: `PushCapGrowth` 0.25 (cap = 3 cm + 0.25 x overlap, push grows with overlap); `ApproachKeptAtCore` 0.2 (own move AND
  velocity toward an overlapping body scaled 1 -> 0.2 from pushbox edge to core: you slow down and shove); core `bCoreEnabled`, `CoreRadiusScale` 0.5
  (15 cm, pair 30), `CoreRecoverPerFrame` 4. Pure `ConstrainMove`: horizontal only, along the centre line only, vs the others' START of frame;
  a move closes at most half the free gap (the other may close the rest); already inside: parts by half the recovery. Deterministic (snapshot only).
- Per character type: `FGSTN_BodyCollisionProfile` (Mode PushboxAndCore / PushboxOnly / None, CoreRadius) as `UGSTN_CombatSimComponent::BodyCollision`.
  Core from the STANDING capsule (a Body Capsule track grows the pushbox, never the core); disabled pushbox (track off, knockdown, tumble) = no core.
- NP: `ComputeFramePush` keeps the bodies; `FGSTN_BodyCapsuleModifier::OnPostMovement` applies `ConstrainFrameMove` as a swept move (world wins)
  into the frame's output sync state. Not Mover depenetration. Capture script: sequences `walk_into`, `head_landing` (PUSHBOX_SEQUENCES).
## Applied choices (rule 6)
- Constraint after the move (OnPostMovement, const_cast of the output state Mover passes) instead of a pre-move velocity hack: exact, no lag.
- Half-gap rule (try 1 let head-on cores reach 27.7 cm); approach 0.3 -> 0.2 (walk-in shove was ~90 cm/s, now ~56 cm/s). Both data.
## Proof
- Build `Saved/Logs/ci/pushbox_p4_build3.log.esito` OK. Tests `Run-Timed -Preset Test -Tests "GSTNCombat+GSTNLink+GSTNMinion+GSTNDefence"`:
  889 Success, 0 Fail (count `Result=\{Success\}` / `Result=\{Fail` in `pushbox_p4_tests3.log.ue.log`). New Core.Pushbox.CoreHeadOn (600 cm/s
  head-on: smallest gap 30.00, never cross, symmetric), CoreCorridor (4 bodies run at a wall in a 140 cm corridor: nobody through, smallest gap
  29.72, last 60 frames largest step 0.000 cm), CoreHeadLanding (vertical never touched, 2 cm/frame sideways). Infos in `pushbox_p4_tests2.log.ue.log`.
- Captures `pushbox_p4_cap1.log.esito`, `_cap2` OK (vd2, collision shown). `python Tools/GASNPPTest/pushbox_rows.py Saved/pushbox_p4/cap1 walk_into ...`:
  walk_into min centres 38.0 cm v56 (P3: 3.9), then ~40.8 cm, attacker shoving the dummy 0.9 cm/frame. Looked at game-view sheet v46-v115: stays behind.
  head_landing (cap2, dummy 365 cm): the faller meets the dummy's front at chest height (v92, 107 cm above), slides down its front, lands 43 cm away.
  Looked at az90 v84-v107: no pass-through, arms overlap the dummy's back (pushbox 24, P1 knob CapsuleRadiusScale). cap1 dummy 150 cm: jumped over.
- Files `docs/notes/reviews/pushbox-p4/`: walk_into(.mp4, quarter v40-v80), head_landing.mp4, head_overjump.mp4, sheets, rows.
## Open (for the owner via coordinator)
- Heads are not platforms: from directly above the core parts the two sideways (test). Platform option = data mode later (land on a top disc).
- Minions: lightweight movement sweeps WorldStatic/Dynamic only, steering separation between minions, no pushbox: fighters walk through them.
  Option: a minion snapshot pushbox/core from `Stats.CapsuleRadius` + a profile on the minion definition (one-way: fighters blocked). Not built.
- Corridor with 3+ REAL fighters not captured (harness has 2 bodies in play); covered by the pure test. Rule 10 cloud review of the videos.
**Cost**: estimated $8-10 (Opus ~110k context, 3 builds, 3 test runs, 2 captures).

# PUSHBOX-P1 handover: ECB ignores fighters + soft capped pushbox (desktop, 2026-11-15). DONE (P1), P2 not started
Branch `claude/pushbox-p1` from `claude/integ-dropkick-capsule` c2a3f3be (dair v9 capsule data), folder `GASP_Giustino2_ue59`. Opus, no subagents.
Not pushed (needs owner approval naming branch + commit). `AM_OverlordDAirV9` dirty and jab `director/` untracked: not ours, untouched. No asset, map or project setting saved.
## Code (commit 80dfa06d)
- Core `Timeline/GSTN_Pushbox.h`: `FGSTN_TimelineTrack_Pushbox` (bEnabled, Radius/HalfHeight 0 = from capsule, optional Offset), `FGSTN_PushboxSettings`
  on `UGSTN_CombatRules::Pushbox` (Gain 0.25, Cap 3 cm, CapsuleRadiusScale 0.8), pure `ResolveLocal`/`PairShare`/`ComputePush`; TimelineUtils Add/RemovePushboxTrack.
- Snapshot entry `Pushbox` (world cylinder, end of previous frame; off for KO, providers' entities). Combat tick `ComputeFramePush` after EnsureSnapshot;
  `FGSTN_BodyCapsuleModifier::OnPreMovement` queues `FGSTN_PushboxShiftEffect` (swept horizontal move, this substep). Parked link roles skipped.
- `UGSTN_CombatWorldSubsystem::Add/RemoveMoveIgnore` (ref-counted); Register/UnregisterEntity ignore fighters both ways; Link presentation uses it.
## Applied choices (rule 6)
- Owner said "procedi" without options: 1A push in the air too (same cap), 2A downed victim pushbox off (P2 data), 3A P1 then P2.
- Push applied as a per-frame swept displacement (instant effect, like the Body Capsule shift) instead of the plan's PushVelocity + layered move:
  no new sync state, cannot fight the freeze/knockback override moves, rollback-safe (recomputed from the snapshot every frame, incl. resim).
- Settings on the shared CombatRules, not per pawn: equal settings keep a pair's shares equal and opposite. Coincident tie: fixed world axis by id order.
## Proof
- Build `Saved/Logs/ci/pushbox_build1.log.esito` OK 290 s. Tests `Run-Timed -Preset Test -Tests "GSTNCombat+GSTNLink+GSTNMinion"`: 807 Success,
  0 Fail (count `Result=\{Success\}` / `Result=\{Fail` in `pushbox_tests1.log.ue.log`); new: Core.Pushbox.Resolve/Push, NP.Pushbox.CapsuleIgnore
  (converge: gap 56.00 cm after 120 frames, largest step 1.500 cm).
- Capture `pushbox_capture1.log.esito` OK (L_Test_Hub, KIT_Test_OverlordDAirV9, dair_hit, PIE_CAPTURE_COLLISION=1, vd2). `docs/notes/reviews/pushbox-p1/`:
  mp4 normal + quarter, az90 sheets v100-v129 (pushbox and old v9), `attacker_dx.txt` (scratch dx.py over both frames.jsonl, `loc[0]`):
  old v9 final dx -65.6 cm, pushbox -1.0 cm. Largest step 16.2 cm at v121 in BOTH = Body Capsule back to standing (mesh compensates), not the push.
- Looked at (az90 sheet once): no slide back. Torsos stay apart; attacker's forward arms reach the victim's chest v112-v120 (dair end pose) and the
  knees cross v124-v129 while the victim's reaction leans in. Pushbox radius 24+24: knob `CombatRules.Pushbox.CapsuleRadiusScale` (1.0 = 30 cm) if the review wants more room.
## Open
- Rule 10 cloud review of the mp4/sheets before anything reaches the owner. Dropkick capture with pushbox not done (P2 scope).
- P2 (new thread): downed/tumble have no timeline; knockdown is the Defence fragment (`KnockdownEndFrame`, tag Combat.State.Knockdown via the tick
  extension). Cleanest hook: a snapshot contributor `ModifyEntry` in GSTNDefence clearing `Entry.Pushbox` from data (per-state body shape set), or a
  tag->shape asset read in `ResolveSnapshotPushbox`. Tumble: core ActionState::Tumble available there directly.
- Grounded push can carry a fighter off a ledge (no ledge stop yet); check in playtest.
**Cost**: estimated $9-11 (one Opus thread ~120k context, 1 build, 1 test run, 1 capture).

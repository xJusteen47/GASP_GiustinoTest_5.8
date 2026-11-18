# Bug pass 2: GSTNDefence + GSTNLink C++ (cloud, read-only review), 2026-11-18

Scope (checkpoint 2 of "bug fix e test su tutto"): branch claude/project-thread-016vb8, based on claude/project-thread-k9sj5a (ab2746ac).
C++ in Plugins/GSTNDefence and Plugins/GSTNLink, no BP or asset edits. Nothing compiled here (cloud).
Order of review, riskiest first:
1. Everything changed since feature/gas-npp-5.8 (`git diff origin/feature/gas-npp-5.8..HEAD -- Plugins/GSTNDefence/Source Plugins/GSTNLink/Source`,
   23 source files, ~850 lines): tech bounce from impact speed, Self Landing Bounce track + own bounce ability, downed roll (dodge + stick, carried
   press), auto get-up scope, neutral get-up, hit while lying, downed fragment fields + net serialize, snapshot state tag, link ref-counted
   move ignores, throw knockback editor helpers. Read in full.
2. Older M6 code (closed at F14 with DEFENCE 13/13, LINK 19/19, headless 1178/0): pattern sweep (authority/net-mode branches, random, wall-clock
   time, static mutable state, divisions by data) over all non-test sources, plus full read of the defence hit steps (parry, shield), the
   outcome applier and punish handler, and the link tick extension's request/accept/partner-move/facing paths.
Tests in the two plugins: 208 IMPLEMENT_*_AUTOMATION_TEST (grep over the macros, this thread).

Result: no P1 found. No clear low-risk C++ fix was worth making in this pass, so no code changed (see "Why no fix").

## Findings (P1 visible/likely, P2 wrong but rare or data trap, P3 minor)

| # | Sev | Where | Issue | Evidence | State |
|---|---|---|---|---|---|
| D1 | P2 | GSTNDefenceNP/.../Sim/GSTN_TumbleLanding.cpp:572-581 (neutral get-up) | The neutral get-up only starts on the last downed frame if the body is then Actionable and grounded. If MaxDownedFrames (or the missed-tech knockdown) ends while the ground bounce / ground hit ability is still playing, the frame is missed, KnockdownEndFrame passes, and the body pops from lying to idle: the exact bug the get-up was added for. Data trap: MaxDownedFrames must be longer than bounce clip + ground hit. | code read | OPEN. Fix options for a code thread: start the get-up on the first free frame at/after the end, or hold the end until DownedFreeFrame. Changes sim timing, so it needs the Landing tests + a capture, not a blind cloud edit. |
| D2 | P3 | GSTN_TumbleLanding.cpp IsPlayerEntity (used by AutoGetupFor) | `Pawn->IsPlayerControlled()` needs the PlayerState: on a client it is false until the remote pawn's PlayerState replicates, and it changes if a controller unpossesses. With AutoGetupFor = DummiesOnly a player's downed end can differ server vs client for a few frames (reconcile). Default is Everyone, where it is not read. | code read | OPEN, only with DummiesOnly. Deterministic option: a flag in the defence attach data set at kit attach. |
| D3 | P3 | GSTNLinkNP/.../GSTN_LinkPresentationComponent.cpp:267 SetCollisionIgnoredPartner | IgnoredPartner is a weak pointer: if the partner is destroyed while linked, Previous is null and RemoveMoveIgnore is never called, so the (owner, dead partner) count stays in UGSTN_CombatWorldSubsystem::MoveIgnoreCounts. Harmless (TObjectKey of a dead actor is never reused), a small map leak per destroyed grabbed actor. | code read | OPEN, cosmetic. |
| D4 | P3 | GSTN_TumbleLanding.cpp:456-483 SelfKnockdown | Log-level UE_LOG on arm, disarm and land, inside the sim: repeats on every resim. Added for the dropkick tech-bounce diagnosis. | code read | KEEP while the dropkick bounce redo is open; then lower to Verbose. |
| D5 | P3 | GSTN_DefenceOutcomeApplier.cpp:72 (Broken) | Shield break stun counts from HitFrame, while block stun and parry recovery count from the hitstop end. A break with long hitstop eats part of its own stun. | code read | OPEN, design question (Smash counts break from the hit). No change. |
| D6 | P3 | GSTNLinkNP/.../GSTN_LinkTickExtension.cpp:263, 565 | SwapRoles warnings go to LogTemp, not the plugin's log category. | grep | OPEN, cosmetic. |

Checked and fine (no change): DownedRollCarry / WantsDownedRoll windows (buffer >= 1, future events skipped on resim); downed fields in the
fragment: ==, presence mask, both serialize paths; TechBounceHeight/Speed/BounceSpeedForHeight guards (NaN, equal min/max, zero gravity, flight
capped at 10 s); Self Landing Bounce ability read before the move is cancelled, kit bounce fallback; the hit-while-lying end; parry hurtbox mask
(bit 15) and shield block/break hitstop and quantize floor; punish handler (KO skip, hitstop extend only); link predicted request: alternatives
tried before the asked type on both the predicting and the authoritative path, same channel check; optimistic accept serials; partner move
start/abort/expiry; facing; mesh blend divisions guarded; DI/SDI stick divisions guarded; Dodge button read from the button layout (game
rules, same on every machine); the only net-mode reads (watchdog, presentation) are outside the simulation.

## Why no fix
Every finding is either a design or timing change in the simulation (D1, D2, D5: need the Landing tests and a capture on the desktop), a
diagnostic the dropkick thread still uses (D4), or cosmetic (D3, D6). Rule 12 + "only clear, low-risk": none qualifies for a blind cloud edit.

## Desktop / laptop checks (later thread with editor)
1. D1: KIT_Test_Playtest dummy, DA_DefenceRules with MaxDownedFrames shorter than the ground bounce clip: does the dummy pop up without a
   get-up? If yes, a code thread applies the "first free frame at/after the end" option + a Landing test.
2. D2: only if AutoGetupFor = DummiesOnly is ever used: PIE net harness, knock a client player down and count reconciles.
3. Suites still green on the k9sj5a build: `Run-Timed.ps1 -Preset Test -Tests GSTNDefence` and `-Tests GSTNLink` (no code changed here).

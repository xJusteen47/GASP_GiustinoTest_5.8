# Bug pass 3: core combat simulation C++ (cloud, review + 1 small fix), 2026-11-18

Scope (checkpoint 3 of "bug fix e test su tutto"): branch claude/project-thread-i8mmmj, based on claude/project-thread-016vb8 (305251a4).
Hit resolution, knockback, hitstop, state timers: GSTNCombatCore Hits/ (KnockbackModel, BalloonKnockback, ImpactMath, HitRunner, HitMathUtils DI,
TumbleRecovery), GSTNCombatNP Simulation/ (CombatTick_Victim in full for Resolve / StepImpact / StepDefaultSurfaceBounce / StepReaction /
EndReaction / ResolveHitFacing, CombatHitState canonical form + NetSerialize + ==, the diff since feature of CombatSimVictim, CombatTick_Attacker,
CombatTimelineLogic), Movement/GSTN_LayeredMove_Knockback, and Mover's MovementMixer::CheckPriority (EngineOverrides) for move mixing.
Not reviewed: snapshot / lag compensation, CombatTableSet, the rest of CombatSimComponent (next checkpoint). Nothing compiled here (cloud).

## Findings (P1 visible/likely, P2 wrong but rare or latent, P3 minor)

| # | Sev | Where | Issue | Evidence | State |
|---|---|---|---|---|---|
| C1 | P1? | GSTN_CombatTick_Victim.cpp StepDefaultSurfaceBounce (bump) + GSTN_LayeredMove_Knockback.cpp:62-120 | The wall/ceiling bump starts a knockback move with DecayPerFrame = BumpDragPerFrame (0.25) and Frames = max(.., TumbleProbeFrames 1800). After frame 1 the move adds -25% of the current horizontal velocity every frame, and it ends on a floor contact only after LandingEndsAfterFrames (240). So the drag keeps acting after the tumble recovers in the air (LeaveTumbleByAction leaves the move running: air drift of a jump / air dodge eaten) and on the ground after the landing and get-up, until move frame 240 (~3.8 s): the character should walk/run visibly slow after a wall bump. Launch and hard-bounce moves have decay 0, so the long life is harmless there. | code read (no PIE here) | OPEN, desktop check 1. Fix options for a code thread: the bump's move ends at its first floor contact (or at BumpFrames + landing), or the drag applies only while Combat is in Tumble. Changes sim timing: not a blind fix. |
| C2 | P2 latent (balloon OFF) | GSTN_LayeredMove_Knockback.cpp:30-55 + MovementMixer.cpp:234 | Mover gives a tie in priority to the OLDER move. During its warp frames the balloon move overrides velocity at priority 0, so a newer launch (a re-hit, or the hard bounce StepImpact starts off a wall/ceiling/floor) loses its launch frame to it; the old warp keeps flying the old path into the wall, and from frame 1 the new move is only additive (the frame-1 relaunch saves only launches with > 100 cm/s up). Strong launch next to a wall = the likely case. | code read of both files | OPEN, only if Balloon is turned on (integ-playtest-1116: CombatRules_Standard keeps it OFF). Fix option: a reaction move finishes the previous knockback moves, or the warp branch yields once a newer knockback move exists. |
| C3 | P3 | GSTN_CombatTick_Victim.cpp Splat case, health KO, ApplyZoneKO | bTumbleActionable (recovered tumble) was not cleared when a recovered tumble splats or is KO'd: the flag outlived the Tumble state (no gameplay read outside Tumble, but the action part never returns to default: extra bits on the wire until the next stagger). | grep of every write of the flag | FIXED: cleared on splat, health KO and zone KO. Test GSTNCombat.NP.Hits.Victim.TumbleAirRecovery extended (zone KO from a recovered tumble). |
| C4 | P3 design | StepImpact / StepDefaultSurfaceBounce on a recovered tumble | A bump (bBumpOnlyDuringHitstun is off by default) or a surface-row bounce on a recovered tumble extends HitstunEndFrame but does not lock again: the victim may act through the bump clip. | code read | OPEN, owner question with C1: should a bump re-lock a recovered tumble? |
| C5 | P3 latent | CombatTick_Victim.cpp Resolve, balloon block | The balloon shortens the hitstun after an applier's MinHitstunFrames floor was applied, so a requested floor can end shorter (balloon on + eligible launch only). | code read | OPEN. Option: apply the request floor after the balloon. |
| C6 | P3 design | Resolve, bAnyStaggered | Two staggering hits on the same frame: the LAST entry in inbox order (attacker, hitbox, hurtbox; stable) sets reaction, launch and hitstun, not the strongest. Deterministic, so no desync. | code read + GSTN_CombatInbox.cpp:104 | OPEN, design note (Smash: priority by hitbox id inside one attacker). |

Checked and fine (no change): knockback models (MinWeight / WeightReference / divisors ClampMin 0.001, push back on a vertical launch, hitstun floor,
hitstop clamps in both modes); balloon math loops bounded (GameFrames, WarpEndFrame, Evaluate), MaxFlightFrames 255 fits the uint8; ImpactProfile
pack/unpack; surface table lookup rebuild; hit sync state: the new fields (bounce chains, bump, last surface, balloon) are symmetric in the
canonical form, ==, the presence mask and both serialize paths, 4-bit counts match ClampMax 15; ground/wall chain reset on a new stagger; DI math
guards (zero stick, zero speed, elevation clamp ClampMax 1); AdvanceVictim MaxHealth guard; inbox sort stable; hitstop window / freeze end and
hitstun base; air timeout counting hitstop frames is documented on purpose (HoldUntilLanded.h:53).

## Desktop / laptop checks (later thread with editor)
1. C1 (first): KIT_Test_Playtest dummy in L_Test_Hub, launch it into a wall slowly enough for the bump; after it lands and gets up, walk it
   (or the player after being bumped): speed back to normal at once, or slow for ~4 s? Also with tumble air recovery on: bump, then jump: is the
   air drift dead? If yes, a code thread applies a C1 fix + a test.
2. C2: only before turning Balloon on: strong launch next to a wall, capture: does the victim bounce at the wall or slide along it?
3. Build + `Run-Timed.ps1 -Preset Test -Tests GSTNCombat.NP.Hits.Victim`, then `-Tests GSTNCombat` (C3 changed code; expect 0 Fail).

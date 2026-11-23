# Tumble air recovery (B) + balloon knockback — 2026-11-16

Thread "Let tumbled players recover in the air", cloud, Opus medium. Branch `claude/project-thread-4n8gx9` from ujco9r 9a2f4736. NOT COMPILED (no editor in cloud).

## B, tumble recovers in the air (owner approved)
- Rules `UGSTN_CombatRules::TumbleRecovery` (`FGSTN_TumbleRecoveryRules`, `GSTNCombatCore/Hits/GSTN_TumbleRecovery.h`): Mode AfterHitstun (DEFAULT, changes current behaviour) / UntilGround, `AllowedCommands` (empty = all, hierarchical: Command.Attack, Command.Movement.Jump, Command.Defence.Dodge, Command.Special).
- Kit: `FGSTN_TumbleRecoveryExtension` in kit Extensions replaces the command list. Actor: `UGSTN_CombatSimComponent::TumbleRecoveryOverride` (UseRules / UntilGround / AfterHitstun) for players vs AI and dummies.
- Sim: sync bit `FGSTN_CombatSyncCore::bTumbleActionable` (1 bit in the action part). `StepReaction` sets it when the hitstun ends in the air; `IsActionLocked` is false then, so Lock_Reaction drops (air drift) and the gate admits only the allowed commands. State stays Tumble (anim, wall impacts, tech/knockdown on landing). Any started command calls `LeaveTumbleByAction` (Actionable). New stagger, EndReaction, timeline start/clear clear the bit.
- NOT done (coordinator note): per-move "no recovery" hit flag; "dipende dalla gameplay ability" was about stances.

## Balloon knockback (owner asked study + setting)
- Study: docs/notes/balloon-knockback.md. Setting `FGSTN_ReactionTuning::Balloon` (in every KnockbackModel's Tuning), OFF by default; Ultimate constants 30/80 frames, x6, end 0.3, tumble only, set knockback exempt.
- Victim `Resolve`: on the final reaction, hitstun -> warped game frames (`FGSTN_BalloonMath::GameFrames`), the flight frames ride with the pending launch (`FGSTN_HitSyncState::PendingBalloonFrames`, 1+8 bits inside the launch block). `StepReaction` -> `FGSTN_ReactionMove::Balloon*` -> 3 new SetByCallers -> `FGSTN_LayeredMove_Knockback` overrides velocity with M(n) x classic velocity at warped time while M > 1, in Falling only. Same path, less time.
- Known edge: a burst (EndReaction) during the warp leaves the override running a few frames (warp ends before hitstun normally).

## Desktop build and checks
1. Build _ue59 (one thread at a time, IN-USO.txt). 2. Tests: `GSTNCombat.Core.Balloon`, `GSTNCombat.Core.TumbleRecovery`, `GSTNCombat.NP.Hits.Victim` (incl. new TumbleAirRecovery, BalloonKnockback), `GSTNCombat.NP.Ledge`.
3. PIE L_Test_Hub: launch a dummy upward (strong hit), after hitstun it may jump/attack in the air; landing without acting = knockdown as before. 4. Balloon: in CombatRules_Standard > KnockbackModel > Tuning > Balloon tick Enabled (do NOT save unless the owner wants it), compare flight time.
- Numbers in tests are hand-worked (Ultimate example 40 -> x2, x1.857), no log yet. Cost of this thread: estimate ~7 $.

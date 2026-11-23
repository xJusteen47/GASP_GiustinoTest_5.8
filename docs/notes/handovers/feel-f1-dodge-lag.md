# F1 dodge ending lag + re-dodge lock (cloud, Opus, 2026-11-17). Branch `claude/project-thread-kfq87p` from `claude/project-thread-v94blx` 721c9b9c
Owner 20:13Z (scope: ground long-jump dodge + air dodge reusing it): ending lag + 1A re-dodge lock. 20:19Z/20:21Z feel: at least a dive (Mario 64 dive/roll-out, Helldivers dive, M64 long jump), physical, may slide on landing with momentum, steerable in the air to pick the landing, not too strong.
## Done (all data, no project settings)
1. Kit extension `FGSTN_DodgeLandingKitExtension` (Core `Movement/GSTN_DodgeLanding.h`), copied to `UGSTN_CombatTableSet::KitDodgeLanding`; a kit without it gets the defaults:
   `EndLagFrames` -1 = the move's (movement track `LandingLagFrames`, now 8 on ground + air dodge, `FGSTN_MoveTimelines::DefaultDodgeEndLagFrames`), `RedodgeLockFrames` 10,
   `LockedCommands` (empty = Command.Defence.Dodge), roll-out `RecoveryCommands` (empty = off) + `RecoveryFromFrame`, belly slide `LandingSlide` (FGSTN_MomentumSlide).
2. Sync state: `FGSTN_CombatSyncCore::DodgeLanding` (landed frame, lag, lock; 1 bit unset, serialized, reconciled). Written on the landing frame by the dodge ability
   (`bDodgeLanding` on GA_GroundDodge/GA_AirDodge → `UGSTN_CombatSimComponent::OpenDodgeLanding`, WriteSyncState in the AS step like StartTimelineFromAbility).
   Cleared by a hit reaction/KO and when both windows end (Normalize after the victim step); a roll-out cuts the lag (CutEndLag after the input tick).
3. Gate (`IsCommandAllowed`, before the other rules): in the lag every command is refused except a roll-out; in the lock the locked commands are refused. Buffer unchanged.
4. Lag on the ground: ProduceInput zeroes the walk intent and holds the facing; vulnerable, no intangibility (the feet window ends at 40 % of the air).
5. Belly slide (default ON, applied choice): the momentum slide move with `bLandingSlide`, kit data via `ReadLandingSlide`, lives only during the lag. Fixed 4000 cm/s²,
   always "Along", no run carry, stops at ledges; then the walking mode keeps the speed left (1200 cm/s → ~690 after 8 f). Stick against it brakes (slide's StickBrake).
6. Presentation: `Combat.State.Dodge.EndLag` projected from the state for the lag frames (hook for a dive-landing clip; no clip yet).
## Frame counting (counted from 1)
Landing frame L = first frame the sim sees the ground (tracker, end of L-1). Lag 8 = frames L..L+7 (tag on, gate refuses); the ability ends on L+8 like the old air dodge lag.
Lock 10 = dodge refused on L..L+9, free from L+10. Timeline TotalFrames now has room for the lag (+8; air dodge 120+8).
## Applied choices (rule 6, to confirm at playtest)
8 f lag, 10 f lock (from the brief); belly slide on with 4000 cm/s²; roll-out OFF (owner never asked; coordinator idea, try with RecoveryCommands = Command.Movement.Jump, from frame 4).
## For the animation pass
Landing = belly/dive landing with a recovery (M64 dive), key it on `Combat.State.Dodge.EndLag`; a roll-out clip if the roll-out gets turned on. Identity (rule 13a): long jump, starts frame 1, vulnerable end.
## Tests (NOT run here; F4 builds and runs them)
New: `GSTNCombat.Core.DodgeLanding.*` (EndLagBlocksActions, RedodgeLock, ZeroIsBefore, RollOut, StateAndReplay, Defaults), `GSTNCombat.NP.Dodge.LandingInSyncState`.
Updated: `GSTNCombat.Core.Dodge.DefaultTimelines` (lag 8), `GSTNCombat.NP.State.*` and HitState idle-bit counts (+1 bit for DodgeLanding).
F4 checks in game: lag feel, slide distance, a dodge pressed at L+8 starts on L+10 only if the buffer holds it; a BP child of the dodges with a saved Timeline keeps its old LandingLagFrames.
Cost ~4 $ estimated (Opus, no build).

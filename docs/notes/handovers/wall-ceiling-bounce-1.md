# Wall + ceiling default bounce (cloud code, desktop build/test, Opus medium, 2026-11-17). Branch `claude/project-thread-h2hlp5` from `claude/playtest-fixes-1117c` 23fb94a
## Owner design (12:30-12:31Z) A launched victim hitting a wall/ceiling bounces BY DEFAULT, same rules as the ground (Smash); with little knockback a tiny
bounce with a clip, NOT the full tech bounce. Splat and hitbox-forced bounce are separate and unchanged.
## What changed (data, no Project Settings)
- `UGSTN_CombatRules::WallImpact` / `CeilingImpact` (`FGSTN_SurfaceBounceRules`, GSTN_ImpactTypes.h). bEnabled (on). Hard bounce: bSameAsGround (on) =
  GroundImpact threshold 1200 cm/s, only in hitstun, restitution 0.6, MaxBounces 2, window 90; off = own `Bounce` rules. Chain counted apart from the floor's,
  walls + ceilings together (sync `Core.Hit.WallBounceCount/FirstFrame`).
- Judge (setting, both behaviours shipped): Speed (default, like the ground) or RemainingHitstun (>= BounceMinHitstunFrames 20 = hard).
- Bump (under the hard bounce): push off the face BumpMinSpeed 150..BumpMaxSpeed 350 cm/s over impact 100..1200 cm/s (exponent 1), keeps 50% of the slide,
  no damage, nothing under 50 cm/s, also after hitstun (bBumpOnlyDuringHitstun off). BumpFrames 14: hitstun at least that long, one bump at a time.
- Presentation: `Combat.State.Bump` + `.Wall`/`.Ceiling` projected for BumpFrames (sync `BumpEndFrame/BumpKind`, rollback safe); event `Combat.Event.Impact.Bump`.
- Order: Defence wall tech, then surface row (splat/break/forced bounce) and hit Force, then this default. Hit Forbid Bounce = always bump.
- Impact step now runs for any entity with CombatRules (before: needed a surface table or a Defence extension) -> floors of such entities hard-bounce too.
## Choices applied (rule 6), da confermare (13d)
1. "Poco hitstun" judged by SPEED like the ground (owner's clarification); hitstun judge available as setting.
2. Bump size scales with impact; owner said "piccolo piccolo": 150-350 cm/s ≈ 2-8 cm off the wall. Tune in CombatRules_Standard > Wall Impact.
3. No clip exists: desktop must add reaction anim rules (RequiredTags Combat.State.Bump.Wall / .Ceiling, priority above Tumble, End BlendOut) with a placeholder.
## Desktop checks: see results below.

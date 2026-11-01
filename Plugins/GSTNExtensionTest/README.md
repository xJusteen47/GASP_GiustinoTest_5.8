# GSTNExtensionTest (test only)

A fake consumer plugin for M6 (plan F14, m6 spec 12 item 7 and section 14). It extends `GSTNLink` and `GSTNDefence` **from outside**, through
their public API only, the way M7 (mounts), M8 or M10 (held bodies) will. Nothing in either plugin, in `GSTNCombat` or in the combat
model's tick knows it. Off by default and never enabled in a shipped configuration; enable it next to both M6 plugins to run its tests.

| Module | Depends on | Adds |
|---|---|---|
| `GSTNExtTestLink` | `GSTNInputCore`, `GSTNCombatCore`, `GSTNCombatNP`, `GSTNLinkCore`, `GSTNLinkGAS`, `GSTNLinkNP` | a carry link type (`Combat.Link.Type.GSTNExtTestCarry`, two roles, the carried role parked) made by code and registered with `UGSTN_LinkRegistry::AddDefinition`, and `UGSTN_ExtTestCarryRule` (a `UGSTN_LinkRule` subclass: the shared checks, then a weight limit) |
| `GSTNExtTestDefence` | `GSTNCombatCore`, `GSTNCombatNP`, `GSTNDefenceCore`, `GSTNDefenceNP` | `UGSTN_ExtTestStep_Brace` (a `UGSTN_HitResolutionStep`: halves a braceable hit's knockback, no flinch, an event), inserted before the Parry step of a combat rules' `HitPipeline` |

The two modules never list each other (the same grep gate as the M6 plugins). Both are `Runtime` modules: native gameplay tags must be defined in
a module that client and server both load, the engine ensures on an `UncookedOnly` one.

To run the tests, list the plugin enabled in the local `.uproject` next to `GSTNLink` and `GSTNDefence` (UBT builds only enabled plugins; never
commit that line), build, then `Automation RunTests GSTNExtTest` (or `GSTN`). F14: `Saved/Logs/headless-f14-ext-probe.log` 7/0,
`headless-f14-both-ext-2.log` 1185/0 (1178 + 7).

## Tests (`GSTNExtTest.*`)

- `Link.NewTypeRegisters`: the registry gives the carry a wire index, its roles and its parked role, with no build error.
- `Link.FormsThroughTheLinkSteps`: a carry forms through GSTNLink's own LinkRequest step, outcome applier and LinkResponse inbox kind
  (target at F+1, carrier at F+2, one serial); the carried role is vetoed by the existing tick extension, the carrier acts; a live carry
  costs the same 1 + 22 bytes as any live link channel.
- `Link.OwnRuleRefuses`: the carry's rule refuses a heavy target inside GSTNLink's step (the request whiffs and posts nothing); the
  shared checks still run first.
- `Link.OwnBreakRules`: `UGSTN_LinkMath::EvaluateBreak`, which the watchdog runs for every type, applies the carry's own 50 cm leash and the
  shared death rule.
- `Defence.NewStepResolvesInPlace`: the resolved pipeline is HurtState < Brace < Parry < Shield < ComputeKnockback.
- `Defence.NewStepWritesTheOutcome`: the brace halves the knockback once, leaves the damage, and its event reaches the outcome.
- `Defence.ParryAndBlockUnchanged`: with the brace in front, the shield's perfect shield still parries and the bubble still blocks.

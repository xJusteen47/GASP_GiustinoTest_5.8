# GSTNPartsMinion

Bridge between [GSTNParts](../GSTNParts/README.md) and [GSTNMinion](../GSTNMinion/README.md): minions that lose parts (Helldivers style:
hundreds of small enemies lose a head and keep fighting). Design: [combat-parts.md](../../docs/architecture/combat-parts.md) §5.5 and
[combat-m8-minions.md](../../docs/architecture/combat-m8-minions.md) §4d.2.

Status: **P16 sub-gate A** (0.2, `EnabledByDefault: false`, not enabled in the project): part rows and the server hit flow. Sub-gate B
(the wire byte, promotion copy, `scenario_minion_parts`, the bench numbers) comes after a check-in. Neither parts nor minion lists this
plugin; enable it locally to run `GSTNParts.Minion.*`.

## What exists (pure, in GSTNPartsCore, no GSTNMinion include)

- `UGSTN_PartSetDefinition::bForMinions`: a set flagged for minions is validated with `FGSTN_PartsValidation::ValidateForMinions` (more than 8 parts is an error; a `Stats` break effect is an error).
- `FGSTN_PartsMinionExtension { PartSet, LethalParts }`. Default: no part loss is lethal, the head included.
- `FGSTN_PartsSnapshotExt::FromMinionStore(Set, BrokenMask, Damage[8])` and `ToMinionStore(State, Mask, Damage[8])`. Pure tests: `GSTNParts.Core.Minion.*`.

## What sub-gate A adds (this module)

- **Attaching the extension.** `UGSTN_PartsMinionSettings::TypeExtensions` (Project Settings > Plugins > GSTN Parts Minion, `Config/DefaultGSTNPartsMinion.ini`):
  minion type tag to extension. `UGSTN_PartsMinionSubsystem::RegisterDefinition` attaches one to an exact definition at runtime (wins over the tag;
  the tests use it). `UGSTN_MinionDefinition` is untouched: GSTNMinion never depends on GSTNParts.
- **Rows.** `UGSTN_PartsMinionSubsystem` (world subsystem) keeps `FGSTN_PartsMinionRow { BrokenMask, PartDamage[8] }` (9 B) per store slot, a
  type byte (0 = no parts), the slot generation and a view scratch. Sized once to the store capacity at the first part-bearing activation;
  a world with none allocates nothing. Reset on `OnMinionActivated`; a freed slot is never read for its next occupant (generation check).
- **Hit flow** (server, stage 2 of the batch, drain order): the victim view seam points the minion view at `FromMinionStore(row)`; the extra
  step slot runs `UGSTN_HitStep_PartResolveMinion` (the unchanged `UGSTN_HitStep_PartResolve` math; with standard rules it takes back the
  `ApplyDamage` damage and the `HealthWeight` scale the parts-aware pipeline would not have, and does nothing when the rules already list
  PartResolve or the type has no parts); the outcome seam writes `ToMinionStore(result)` into the row and returns "kill" when a part of
  `LethalParts` broke with this hit. The store's Health took `MainDelta` as the hit's damage. No gameplay effect, no fragment, no allocation of
  the bridge per hit. The `Kill` flag of `MainDamage` is not read for minions.
- **GSTNMinion seams used** (generic, inert when unbound, see [GSTNMinion README](../GSTNMinion/README.md) "Bridge seams"): `OnMinionActivated`,
  `SetExtraVictimStep`, and the two added for P16: `VictimViewExtension` and `ApplyOutcomeExtensions`.
- Tests (world, headless): `GSTNParts.Minion.{StoreOutcomeNoGE, HeadLossKeepsFighting, LethalPartKills, NoExtensionNoCost, ResetOnFree}`.

## What sub-gate B adds

`ExtByte = BrokenMask` through `bHasNetExt` and `SetExtByte` (only on change), promotion and demotion copy to and from `FGSTN_PartsFragment`, part
events through the outbox, the break effects Launch and Stagger as one-shots, `scenario_minion_parts` and the bench numbers.

## PERF-DEBT

- Per hit on a part-bearing minion (estimate, not measured): view build (a 32 B copy), a scan of the resolved pipeline (about 8 class tests),
  the part math, one `HealthWeight` re-run, one outcome lookup and a 9 B write: about 1 µs on the laptop, inside the 2 µs budget. Not counted:
  the pipeline's own allocations (PartResolve adds one `FInstancedStruct` to `OutcomeExtensions`, copied into the view of each later step);
  sub-gate B measures it in the bench. A type without parts pays two delegate calls and a byte test per hit, one pointer test in the step.
- Memory at 160 minions: rows 1440 B, type bytes 160 B, generations 320 B, view scratch 5120 B, about 7 KB, only once a part-bearing minion
  has activated. The view scratch could shrink to one entry per drain if the 5 KB ever matters.
- The part set loads synchronously the first time its type activates (once per type per world); sub-gate B should preload the sets with the wave.

## How to extend

Make a minion type lose parts: build a `UGSTN_PartSetDefinition` with `bForMinions` on, at most 8 parts, only MainDamage, Launch, Stagger and derived break effects; list the parts whose loss kills in `FGSTN_PartsMinionExtension::LethalParts` (leave it empty to keep fighting after any loss); map the minion's type tag to that extension in `UGSTN_PartsMinionSettings::TypeExtensions`. Add a new bridge behaviour in this module, never in `GSTNMinion` or `GSTNParts`.

# GSTNPartsMinion

Bridge between [GSTNParts](../GSTNParts/README.md) and [GSTNMinion](../GSTNMinion/README.md): minions that lose parts (Helldivers style:
hundreds of small enemies lose a head and keep fighting). Design: [combat-parts.md](../../docs/architecture/combat-parts.md) §5.5 and
[combat-m8-minions.md](../../docs/architecture/combat-m8-minions.md) §4d.2.

Status: **P15, interface only** (0.1, `EnabledByDefault: false`, not enabled in the project). The module is empty; it links against
`GSTNPartsCore` and `GSTNMinionCore` public headers to prove the edge. Neither parts nor minion lists this plugin. Run its checks through
the canary (the pure tests are `GSTNParts.Core.Minion.*`, in `GSTNPartsCore`).

## What exists now (all in GSTNPartsCore, pure, no GSTNMinion include)

- `UGSTN_PartSetDefinition::bForMinions`: a set flagged for minions is validated with `FGSTN_PartsValidation::ValidateForMinions` (more than 8 parts is an error; a `Stats` break effect is an error).
- `FGSTN_PartsMinionExtension { PartSet, LethalParts }`: the data a minion definition will carry. Default: no part loss is lethal, the head included.
- `FGSTN_PartsSnapshotExt::FromMinionStore(Set, BrokenMask, Damage[8])` and `ToMinionStore(State, Mask, Damage[8])`: the store (9 B per minion) to the snapshot extension the unchanged `UGSTN_HitStep_PartResolve` reads, and back.

## What P16 adds

Store columns (`uint8 BrokenMask`, `uint8 PartDamage[8]`) for part-bearing types; the extension on `UGSTN_MinionDefinition` (an M8 change, agreed with the owner first); the hit-flow glue (build the ext from the store, run the step, apply the outcome to the store with no GE and no fragment, die when `IsLethal(mask, lethalMask)`); the wire byte through the G10 seam (`bHasNetExt` and `SetExtByte` with the broken mask, only when it changes); promotion and demotion copy to and from the fragment; part events through the outbox. Tests `GSTNParts.Minion.*` and the M8 bench scenario.

## How to extend

Make a minion type lose parts: build a `UGSTN_PartSetDefinition` with `bForMinions` on, at most 8 parts, only MainDamage, Launch, Stagger and derived break effects; list the parts whose loss kills in `FGSTN_PartsMinionExtension::LethalParts` (leave it empty to keep fighting after any loss). Add a new bridge behaviour in this module, never in `GSTNMinion` or `GSTNParts`.

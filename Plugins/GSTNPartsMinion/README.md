# GSTNPartsMinion

Bridge between [GSTNParts](../GSTNParts/README.md) and [GSTNMinion](../GSTNMinion/README.md): minions that lose parts (Helldivers style:
hundreds of small enemies lose a head and keep fighting). Design: [combat-parts.md](../../docs/architecture/combat-parts.md) §5.5 and
[combat-m8-minions.md](../../docs/architecture/combat-m8-minions.md) §4d.2.

Status: **P16 sub-gates A and B** (0.3, `EnabledByDefault: false`, not enabled in the project): part rows and the server hit flow (A),
the broken mask on the wire, broken parts hidden on the puppets, the promotion copy and the bench (B). Neither parts nor minion lists this
plugin; enable it locally to run `GSTNParts.Minion.*` and `scenario_minion_parts`.

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

- **Wire.** A part-bearing type gets `UGSTN_MinionDefinition::bHasNetExt` from this bridge at the one moment its wire format is decided: the
  server's `OnDefinitionRegistering` (first spawn of the type in the world) and, on a client, the registry's `OnDefinitionResolving`, with the
  same rule (`ResolveUsableSet`) and the same config on both. The byte is the row's `BrokenMask`, written with `SetExtByte` only when a hit
  changed it; a dent writes nothing and the damage bytes never leave the server. A type without parts keeps 0 extension bits. A definition
  registered after its first spawn keeps its format (warning): flipping the flag then would desync the item codec. The flag is set on the
  definition object in memory (in the editor it stays set until the editor restarts; it is the same value the config gives every time).
- **Puppets.** `UGSTN_PartsMinionPuppetSubsystem` (not on a dedicated server) hides the bones (`FGSTN_PartDefinition::Bones`) of the broken
  parts on each presented puppet, snap: the listen host reads the server row (M8 §4d.1), a remote client its registry's ext byte. Nothing
  happens until a puppet's (minion, mask) changes; a pooled puppet reused by another minion is shown whole first.
- **Promotion.** On `OnMinionPromoted` the row goes to the fighter as a `FGSTN_PartsAttach::SetPendingSeed` (by part name, new small API in
  GSTNPartsNP); the parts tick extension merges it into the fighter's `FGSTN_PartsFragment` at its next AfterClocks (OR of the broken bits,
  max of the damage, no break serial, no event), and the fragment syncs it like any part state. The fighter needs its own part set (kit or
  `UGSTN_PartsAttachComponent`). Demotion (fighter back to minion) is not done.
- **Not done: the Launch and Stagger break effects** on minions. The outcome hook runs inside the drain before the drain writes its own
  reaction (it calls `LaunchMinion` for the knockback after the victim hitstop), so a launch from the hook would be overwritten or would
  overwrite the knockback, and there is no public stagger call. They need a seam that merges into the drain's reaction (a GSTNMinion change).
  Part events through the outbox are not done either (a minion has no outbox entry of its own yet).
- **Bench.** `gstn.PartsMinion.Bench.Parts [TypeTag] [HeadHurtboxes] [HeadCap]` (a transient part set on a type tag in every game world of the
  process, before the type's first spawn) and `gstn.PartsMinion.Bench.Report` (`PartsMinionBench server|client|puppets` lines: rows, masks by
  global id, ext bytes per client, per hit cost of view + step + outcome). Harness: `minion_parts_base` then `minion_parts` in
  `SUITE_MINION_BENCH` (Tools/GASNPPTest/minion_scenarios.py); `minion_parts` PASSes with "bridge not enabled" when this plugin is off.
- **GSTNMinion seams added for B** (inert unbound): `UGSTN_MinionCombatSubsystem::OnDefinitionRegistering`, `UGSTN_MinionClientRegistry::OnDefinitionResolving`.
- Tests: `GSTNParts.Minion.{ExtByteOnlyOnTypesWithParts, DirtyOnlyOnMaskChange, PromotionCopiesToFragment, CachedStepSameOutput}`
  (the last one: the cached step equals the first path on a sample of rows, boxes and damages; `Requests` is the one documented difference).
- **Bench puppets.** `missingBones 23` in P16-B was not a bone name problem: `gstn.Minion.Bench.PuppetMesh` was never set, so the bench puppets
  had the null mesh (no bones at all). The `minion_parts*` scenarios now set it to the GASP Manny (`SKM_Manny`, which has `head`, `foot_l`,
  `foot_r`, the bones of the bench part set); `GSTN_PARTS_PUPPET_MESH` overrides it (empty = old behaviour, the visual is then not judged) and the
  scenario checks `missingBones 0` with at least one hidden part.

## PERF-DEBT

At 160 minions, all of a part-bearing type (sub-gate B):
- Wire: 8 bits more in EVERY item update of a part-bearing type (the G10 codec writes the byte whenever the type's format has it, not only
  when it changed), i.e. +8 of the 119 fixed bits of an item = +6.7 % per update (the P16-B "+5 %" used the 20 B planning size and "only on
  change", both wrong). A mask change also makes the item dirty once (one update per break per item, never per dent). Types without parts:
  0 bits. The floor is the codec (GSTNMinion, not changed here). Cheaper alternative, for a later G-gate: one presence bit per item in the
  codec's flag field that says "ExtByte follows" (set only while the byte is non-zero or has not been acknowledged): an intact minion pays
  1 bit instead of 8 (-5.9 points), a broken one 9 bits. `minion_parts` reports the split: codec share and the update-count ratio against
  `minion_parts_base` (the same bench timeline in both runs, so the ratio is what the parts add: broken minions reacting and the mask sends).
- Client hide: two map lookups and a byte read per visible puppet per frame (at most `gstn.Minion.Present.MaxVisible`, 60), bone hide calls
  only on a change. The part set is resolved once per type per machine (a synchronous load the first time; preload with the wave).
- Server per part hit: measured by the bench (`hitUs` = view build + `UGSTN_HitStep_PartResolveMinion` + outcome write, `minion_parts`);
  budget 2 us. Before P16-C: view 0.19 + step 4.72 + outcome 0.15 = 5.06 us (laptop, 40 minions). What the step did per hit: ran the parent's
  step (soft pointer reads for the bands, the rules extension scan, a 56 B request with a soft class copy, the extra effects copied), scanned
  the resolved pipeline with three `IsChildOf` per entry, and re-ran HealthWeight into a second full `FGSTN_StepResult` (about a dozen
  members, 4 arrays, a tag) only to read one float. After: `ResolveCached` keeps per (part set, rules) the pipeline flags, the resolved
  bands and the minimum weight scale (`FStepCacheEntry`, filled at the first hit, dropped by every bridge registration and world start),
  evaluates HealthWeight's curve inline and writes no request. What stays per hit: the part math, one `FInstancedStruct` outcome (one
  heap allocation), `GetDamageProgress`. `gstn.PartsMinion.StepCache 0` runs the first path for an A/B on the bench. The after number is
  measured by the next `minion_parts` run (the writer could not run the editor): see the handover.
  P16-D (per-hurtbox table, measured `2.50 -> 2.49 us (no real gain, see perf-debt.md)` us per hit, was 2.75): the entry also holds, per hurtbox set, a row per hurtbox with the part
  index (no `FindPartByHurtbox` name search), the effective hardness (P18 override applied) and the penetration band for each tier 0..15
  (handed to `ApplyHit` through `FGSTN_PartHitInput::PreparedBandIndex`; the fighter path leaves it unset and looks the band up as before),
  plus the per-part lost-mass factors and the clamped minimum weight scale, so the weight is a product over the broken parts only. Still
  per hit: the `ExtensionData` scan for `FGSTN_PartHitData`, `ApplyHit` (screw/held-by/cap flags are read from the part there, one cache line),
  `GetDamageProgress`. A tier above 15 or an ignore-hardness hit on a row still resolves exactly (the latter takes band 0).
  P16-E (not yet measured, `1.88 us per part hit (from 2.50)` us per hit, was 2.49, target <= 1.8): the outcome goes through a scratch the bound bridge consumes (no
  `FInstancedStruct`, no copy in the runner's merge; `AddOutcomeScratchUser`, off when no bridge is bound), and the cache is keyed by raw
  pointers and dropped after every GC (module post-GC hook), so no `TObjectKey` per hit. Tests: `CachedStepTwoTypesBackToBack`, `ScratchOutcomeSameAsArray`.
- Memory: the A columns plus `LastBreakFrame` (4 B per slot, bench only, ~640 B) and per machine a set pointer per type; the puppet map holds
  one small entry per puppet ever used (pooled, so at most the pool size).

- Per hit on a part-bearing minion (estimate, not measured): view build (a 32 B copy), a scan of the resolved pipeline (about 8 class tests),
  the part math, one `HealthWeight` re-run, one outcome lookup and a 9 B write: about 1 µs on the laptop, inside the 2 µs budget. Not counted:
  the pipeline's own allocations (PartResolve adds one `FInstancedStruct` to `OutcomeExtensions`, copied into the view of each later step);
  sub-gate B measures it in the bench. A type without parts pays two delegate calls and a byte test per hit, one pointer test in the step.
- Memory at 160 minions: rows 1440 B, type bytes 160 B, generations 320 B, view scratch 5120 B, about 7 KB, only once a part-bearing minion
  has activated. The view scratch could shrink to one entry per drain if the 5 KB ever matters.
- The part set loads synchronously the first time its type activates (once per type per world); sub-gate B should preload the sets with the wave.

## Known limits

- **Net format (open risk for P16).** `FGSTN_MinionNetFormat::ExtTypes` is filled process-wide. A pack that is decoded before the registry is filled is read with the wrong table. Not yet guarded against.

## How to extend

Make a minion type lose parts: build a `UGSTN_PartSetDefinition` with `bForMinions` on, at most 8 parts, only MainDamage, Launch, Stagger and derived break effects; list the parts whose loss kills in `FGSTN_PartsMinionExtension::LethalParts` (leave it empty to keep fighting after any loss); map the minion's type tag to that extension in `UGSTN_PartsMinionSettings::TypeExtensions`. Add a new bridge behaviour in this module, never in `GSTNMinion` or `GSTNParts`.

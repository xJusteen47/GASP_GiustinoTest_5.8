# M4b doc deltas (for the final docs commit, P14)

Same use as `m4-doc-deltas.md`: what the code did differently from the M4b plan (`m4b-parts-plan.md`) and the spec (`combat-parts.md`, both on the
docs branch), one entry per checkpoint, folded into the docs in the closing commit. The plan says `Saved/Notes/`; `docs/notes/` is the current rule.

## P0 spikes (report: `m4b-spike-report.md`)

- P0a ran as a harness scenario kept in `Tools/GASNPPTest/parts_scenarios.py` (`spike_overlap_part`, not in `SUITE_DEFAULT`). Measured 1 index
  disagreement in 343 predicted hits (0.29 %, above the plan's 0.1 % line, statistically uncertain at this n); the inbox carried the authority's index
  in every case (238/238), so as built nothing but the attacker's own flash differs. It becomes damaging only if the parts plugin predicts the
  attacker's ricochet from the winner part alone (D5). Spec 3.4 should say so, and say that the host as attacker has no second computation.
- P0b could not be run as specified (no Niagara mesh emitter and no fractured Geometry Collection can be authored from Python; no deferred actor spawn
  in a PIE world). A proxy was measured: no mean cost, a hitch at the burst frame (max 157 ms in three worlds). Spec 6.4 should add "stagger the burst
  and pre-warm the pool"; the 16 ms line is to be re-measured in P12 on a standalone listen host.
- P0c: spec 5.1's table counts the payload only. The list framing adds the type id (1 B), the packed length (1 B) and the packed count (1 B for the
  first fragment): N = 16 is 19 B payload, 23 B in the list; zero fragments are exactly the flag bit. The wire is self-describing through the length, and
  N cannot be an instance field read in `NetSerialize` (the registry reads into a default instance): derive it from `FArchive::TotalSize()`. Use the
  byte encoding (bit packing saves 0 to 2 bytes).
- P0d: D1..D10 all present. The snapshot extension is `TArray<FInstancedStruct>` of a USTRUCT with a virtual destructor (not a fixed POD slot): the
  20 B/entity history budget of P7 has to be checked against that.
- Assumption ids used: A7 (highest Priority wins, ties lowest index; resolved attacker-side), A15 (no host mitigation), A16 (budgets 48/600/2).

- Lead decision on P0a (2026-10-25): keep the rule as is (highest Priority, ties lowest index, chosen attacker-side; the inbox carries the index and
  the authority wins); the 0.29 % is read as cosmetic for now. The 0.1 % line is not proven at n = 343: re-measure with >= 3000 hits in P8, when the
  attacker's ricochet depends on the part. A mispredicted ricochet must be corrected like any misprediction (the recoil rolled back by the authority's
  inbox entry), and the ricochet is never predicted from the winner part alone without that correction path.

## Import of GSTNParts P1-P3 from the desktop branch

The laptop skeleton (commit 0119e79, `GSTN_PartsTags` namespace, `FSoftObjectPath` settings) is superseded: `Plugins/GSTNParts` is now byte-identical to
`origin/claude/project-thread-v1plhd` at 98d0bc7 (its commits 1c8ed09 P1, 7029d34 P2, d5d3b6e P3, 98d0bc7 review fixes), so the later merge is clean.
Adaptations to our post-M4 GSTNCombatCore: see the list at the end of this file (empty if none). The entries below are the desktop file's, verbatim,
imported from commit 98d0bc7 (its title line dropped). Differences from the laptop plan text worth knowing: the tag namespace is `GSTNPartsTags`
(not `GSTN_PartsTags`), `FGSTN_PartsBakedCurve` stands in for the M4 `FGSTN_BakedCurve` (H2 now exists in GSTNCombatCore: the replacement the entry
mentions is a P4 task), `EGSTN_PartReactionTier` stands in for `EGSTN_HitReaction`, and `FGSTN_PartHitData` / `FGSTN_PartsRulesExtension` do not yet
derive from `FGSTN_HitExtension` / `FGSTN_RulesExtension` (both exist now: P4 task).


One line each: where, what differs, why. Collected per checkpoint; folded into `docs/` by plan P14 (the only docs commit).

## P1 (skeleton, tags, settings)

- m4b-parts-plan.md P1 (settings): the plan lists "default penetration table soft ref" in `UGSTN_PartsSettings`; the table type arrives
  in P2, so the field is added there, not in P1. Why: the type does not exist yet and a placeholder would change type later.
- m4b-parts-plan.md P1 (settings): "default weight curve" is `DefaultWeightScaleAtCap` (linear 1.0 down to that value at the cap)
  plus `MinWeightScale`, and a part set can carry its own curve. Why: a settings-level curve asset would need a soft load inside
  the bake at asset load; a number gives the spec's default (1.0 to 0.6) with nothing to load.
- m4b-parts-plan.md P1 (settings): `FGSTN_PartsPresentationBudget` (48 chunks, 4 s, 600 particles, 2 GC, 1.5 s sleep) is a struct in its
  own header so the rules extension (P2) and the presentation module (P10) share it. Why: CP §6.4 numbers as data.
- combat-parts.md §7 / plan P1: the ini tags (`Part.Type.*`, `GameplayCue.Combat.Part.*`) need the module to register the plugin's
  `Config/Tags` folder (the engine reads it only for the project and Game Feature plugins), as `GSTNSurface` does. Why: otherwise the
  leaves do not exist and the picker filters are empty.
- m4b-parts-plan.md P1 (`.uproject`): the plan says the `.uproject` stays untouched until the owner enables the plugin; the lead
  asked for the entry, so `GSTNParts` is appended after `GSTNCombatDebug`. Why: the tests only run with the plugin enabled.

## P2 (part data, penetration table, break effects, validators)

- m4b-parts-plan.md P2 / combat-parts.md §4.3: `FGSTN_BakedCurve` is named `FGSTN_PartsBakedCurve`. Why: the M4 plan (H2) declares a
  `FGSTN_BakedCurve` in `GSTNCombatCore` for the `HealthWeight` step; two types of one name cannot be included together (P4 needs
  both). When H2 lands, replace ours by the combat one (the table, the integer read and the tests move over unchanged).
- m4b-parts-plan.md P2 / combat-parts.md §3.1: `MaxReaction` is `EGSTN_PartReactionTier` {StaggerInPlace, PushBack, Launch}, not the M4
  `EGSTN_HitReaction`, which does not exist yet. Why: it keeps the part data complete and the same names and order, so assets load
  unchanged when P4 maps it onto (or swaps it for) the M4 enum.
- m4b-parts-plan.md P2: `FGSTN_PartHitData` and `FGSTN_PartsRulesExtension` do not derive from `FGSTN_HitExtension` / `FGSTN_RulesExtension`
  (neither exists; H1 and C3b/H4 add them). Why: no stub base with the same name may be created here (it would collide); adding a base to
  a struct without virtuals is a source-level change and assets keep loading. `FGSTN_PartsKitExtension` does derive from the existing
  `FGSTN_KitExtension`.
- combat-parts.md §3.1: `EGSTN_BrokenHurtboxPolicy::Replace` has no shape payload (`FGSTN_HitShape` is H1). The enum value exists, damage
  is computed as for `Keep`, validation warns. Why: the payload type does not exist yet.
- combat-parts.md §3.1: the set-level `Presentation` soft reference is not in `UGSTN_PartSetDefinition`. Why: `UGSTN_PartPresentationSet` is
  in the Presentation module (P10), which Core cannot name; P10 adds the reference (a soft path with an allowed class) to the set.
- combat-parts.md §3.1: the set carries its weight curve as an inline `FRuntimeFloatCurve` (`bOverrideWeightCurve`), baked in `PostLoad` and
  on edit; the default shape (1.0 to `DefaultWeightScaleAtCap`) needs no asset. Why: a soft curve asset cannot be loaded inside `PostLoad`.
- combat-parts.md §3.1: `BrokenMainTransfer` uses a negative value for "default = `MainTransfer` x 0.5" (`GetBrokenMainTransfer()`).
- combat-parts.md §3.3: `FGSTN_PartBreakEffect_Launch` fields (speed, up fraction) and `FGSTN_PartBreakEffect_Stats` entries (SetByCaller key and
  magnitude) are my choice; the spec names the effects without fields.
- combat-parts.md §3.4: the default band cue tags are empty in code (`GameplayCue.Combat.Part.Ricochet` is ini vocabulary and code never names a
  leaf, modularity §4.2b); the shipped table asset (P14) sets them. The lowest default band starts at `MinDelta` -15 (hardness 15 against
  penetration 0), the lowest possible delta; a table that stops higher falls back on its last band and validation warns.
- m4b-parts-plan.md P2 (tests): `Penetration.IgnoreHardness` needs the lookup with the flag, which is `UGSTN_PenetrationTable::LookupBand`
  (`UGSTN_PartMath::LookupBand` of P3 wraps it for Blueprint).
- m4b-parts-plan.md P2: added `Data.NumbersValidated`, `Data.PartSetAsset`, `Data.ExtensionsAndHitData`, `Data.StructsAreBlueprintTypes`,
  `Data.NoHardContentReferences` (the R-B and Blueprint gates as tests).

## P3 (pure part math)

- m4b-parts-plan.md P3: `FGSTN_PartsState` is a plain C++ POD, not a USTRUCT (fixed `uint8[16]` is not Blueprint-visible and the layout is the
  wire contract); `UGSTN_PartMath::ApplyHit` is C++ only. The BlueprintPure subset is `GetPenetrationBandIndex`, `QuantisePartDamage`,
  `DequantisePartDamage`, `IsPartBroken`, `CountBrokenParts`, `GetEffectiveWeight`. The names differ from the C++ statics because a UFUNCTION
  cannot share a name with an overload.
- combat-parts.md §4.2 / plan P3: "p breaks when it reaches `PartCap`" is implemented as "the stored byte reaches 255". With the ceil rule a
  part at more than 254/255 of its cap breaks, so the break is a pure function of the byte the fragment carries.
- combat-parts.md §4.2: a cascade breaks every descendant of a broken part, including unbreakable ones (an armour plate goes with the limb),
  and sets their byte to 255. `AllBroken` still counts breakable parts only. Spec silent; flagged as an open point.
- combat-parts.md §3.1: `Disable` on a broken part makes the hit the body's (full damage, part index `INDEX_NONE`, `bPassedToBody`); `Keep` and
  `Replace` damage the stump with `BrokenMainTransfer` and the band of the stump's hardness. A hit on a broken part never changes the byte.
- plan P3 `FGSTN_PartsProjection`: the "hurtbox mask" is a list of hurtbox **names** (`DisabledHurtboxes`), because hurtbox indices belong to the
  hurtbox set of M4; the P7 snapshot code maps names to indices. The hit-group mask is a `uint8` as in the spec (`GroupMask`).
- plan P3 `RestoreParts(State, Mask, Fraction)`: Fraction is the damage left as a fraction of the cap (0 = pristine), capped at 254/255 so a restored
  part is not broken; it never raises a part's damage; the serial advances once if any part unbroke. **Changed after review (B.1):** the
  signature is `RestoreParts(Parts, State, Mask, Fraction)`; a child cannot stay intact under a broken parent (combat-parts §3.1), so the
  broken ancestors of the masked parts are restored too (and returned in the result). `ApplyHit` also starts the cascade from the part that
  just broke instead of from the whole broken mask, so a state that does hold an intact child under a broken parent does not re-break the
  child (no second bonus, no serial event) when something else breaks. Mask 0 still means every part; a caller that computes a mask must
  test it for emptiness.
- plan P3 result type: `FGSTN_PartHitResult` also carries the band's `StaggerScale` and `HitstopScale`, `HitMainDelta` / `BonusMainDamage` / `MainDelta`,
  `bKill`, `bAllBrokenReached` so the P4 step needs nothing else from the math.
- plan P3 tests: the grab gate evaluation is in P3 (`GrabGate.Evaluation`, one test covering empty, min count, required, any-of); P9 adds the NP
  projection test. `Determinism.1000RandomSequencesTwoConfigs` runs 2 part sets x 1000 sequences x 40 hits twice each and compares the run
  fingerprint with a pinned value (the "second compile config" check is that value on another build configuration).
- plan P3 allocation test: a pass-through allocator installed with a compare and swap on the global allocator pointer counts the allocations of
  the test thread (with a control that must see a growing array allocate). No engine facility counts per thread.
- plan P3 acceptance: the golden hand values and the pinned fingerprint (0x827A1F0F6B36481A) pass in Development and DebugGame (`UnrealEditor-Win64-DebugGame-Cmd.exe`), 48 of 48 `GSTNParts.*`.

## Review fixes (2026-10-23)

- Validators (review B.3, B.4): the grab gate is checked against the parts that can ever break (breakable parts and the descendants of
  breakable parts, by the same cascade `ApplyHit` uses): a required part outside that set, an any-of gate with none inside it, or a
  `MinBrokenParts` above its size is an error. The `Launch` (finite non-negative speed, up fraction in 0..1), `Stagger` (frames not
  negative), `Stats` (a valid stat tag, finite magnitudes; a stat outside `Combat.SetByCaller.Stat` is a warning, compared as text because
  the root belongs to `GSTNCombatGAS`) and `DisableHurtbox` (no empty name) effects are checked, and a part type outside `Part.Type` warns.
  Band tables warn when the top band ricochets, when no band deals damage, and when a ricochet band keeps stagger or hitstop scales.
- Baked weight curve (review B.6): `UGSTN_PartSetDefinition` rebakes in `PostDuplicate` and `PostEditImport` as well as on load and edit. A set
  built with `NewObject` and filled by code still has to call `RebakeWeightCurve()` (documented on the function).
- `UGSTN_PenetrationTable::PostEditChangeProperty` no longer sorts on an interactive change (rows jumped while typing `MinDelta`).
- Tag namespace `GSTN_PartsTags` is now `GSTNPartsTags` (the files keep their names), to match `GSTNCombatTags` and `GSTNSurfaceTags`.
- Wording: the README and headers no longer say stagger is "the existing `Combat.State.Hitstun`" (no such tag is declared anywhere; the M4
  pipeline decides how stagger and hitstop reach the victim), no longer claim a set mismatch "is refused at connect" (nothing does that yet),
  and name the minion bridge as planned for M8, not built.
- Left as they are (design choices or theory, review B.11, B.12, minor list): the per-hit `ceil` rounding (documented in the README as a bias
  of up to half a byte per hit), `FRichCurve::Eval` in float at bake time (only linear or constant curves are bit-identical by construction; a
  fingerprint check at connect would cover it), `bWeakPointHit` on an already broken part, `Penetration` clamping in `Make`, FString vs FText
  messages in the validators, the `Mask == 0` meaning of "all" in `RestoreParts`, and the test hygiene items (`UCLASS(Hidden)` test types
  outside a test guard, the counting allocator, tests that read the project's ini defaults).

## P4 part resolve step (laptop, 2026-10-25; not in the desktop plugin)

- m4b-parts-plan.md P4 / combat-parts.md 4.2: the step **replaces `ApplyDamage`** in a pipeline that uses parts, besides `HealthWeight`. In M4 as built,
  `ApplyDamage` writes `DamageAdd = Hit.Damage` and a damage request with `Magnitude = Hit.Damage`, and the GAS layer applies the request's magnitude, not
  `Outcome.Damage` (which only advances the victim view). The plan's "ApplyDamage adds Outcome.Damage unchanged" is not how it is built, so a step that only
  wrote `Outcome.Damage` would leave the full hit on the victim. `PartResolve` therefore writes the damage and the request itself (and the hit's extra
  effects) and `FGSTN_PartsPipelineValidator` reports `ApplyDamage` next to it as an error. For an entity without a part set the step writes what `ApplyDamage`
  writes (test `Step.IdentityWithoutPartSet`), so the plan's "no part set = identity" still holds. No change in GSTNCombat.
- Plan P4 files: `Pipeline/GSTN_HitStep_PartResolve`, `GSTN_PartHitOutcome` (as `FGSTN_PartHitOutcome`), `GSTN_PartsPipelineValidator`, plus
  `GSTN_PartsSnapshotExt`. The snapshot extension holds a `const UGSTN_PartSetDefinition*` instead of the spec's `PartSetId` and starts with a type word,
  because `FGSTN_HitVictimView::SnapshotExtension` is a `const void*` and nothing else identifies it. A lag compensation history that outlives a frame must
  keep the id and rebuild the pointer (P7). `FGSTN_PartHitOutcome` carries the whole resulting state (mask, bytes, serial) and not only a delta, so the applier
  writes with one assignment; every member is a UPROPERTY because an `FInstancedStruct` copies property by property.
- Weight: read from the state **before** the hit (like `HealthWeight`; a break of this hit does not make this hit's launch stronger). Juggle: part times body.
  Knockback and hitstop scale: the hit part's (the body's for a body hit), not both. Threshold: part, else body, else unset. Cap: lower of part and body.
  `EGSTN_PartReactionTier` is mapped to `EGSTN_HitReaction` by value (static_assert); `FGSTN_PartsBakedCurve` was NOT replaced by `FGSTN_BakedCurve` (the
  part math keeps its own 16 bit curve, the combat one is Q2.14 and has no shared evaluation): left as is.
- A ricochet returns `Stop` with `bHit` false and an outcome entry flagged `Ricochet`; the attacker side decides the recoil through `IGSTN_AttackerOutcomeExtension` (P8).
- The tick-order note for P7: `FGSTN_HitPipelineRunner::AdvanceVictim` advances only the health or percent, not the parts extension (it is a `const void*`), so the
  second hit of one inbox drain would see the parts of the first hit's start. P7 has to rebuild the extension between the entries from the previous entry's
  `FGSTN_PartHitOutcome::GetResultState()` (the test `Step.OutcomeCarriesTheResultState` chains two hits that way).
- Base structs: `FGSTN_PartHitData` now derives from `FGSTN_HitExtension` and `FGSTN_PartsRulesExtension` from `FGSTN_RulesExtension` (source-level, assets keep loading).
- Tests: 12 `GSTNParts.Core.Step.*` (the plan's eleven plus `OutcomeCarriesTheResultState`); the stagger, tier and hitstop tests run the real `ComputeKnockback` and
  `Hitstop` steps with the weight driven model through `FGSTN_HitPipelineRunner`. The plan's "M4 fake-step pipeline test still green" is the combat suite (396 unchanged).
  Not done: a byte-for-byte state compare of the M4 scenarios with the plugin loaded and no part set (needs the PIE harness; the plugin adds nothing to the pipeline of the standard rules, whose pipeline does not list `PartResolve`).

## P5 GSTNPartsGAS (laptop, 2026-10-25)

- m4b-parts-plan.md P5 / combat-parts.md 5.4: the simulation event type (`FGSTN_SimEvent`, GSTNInputCore) has no payload slot, so `FGSTN_PartEventPayload { PartIndex, BreakSerial, Band }` travels packed in
  `Magnitude` (three bytes, below 2^24, exact in a float; 255 = none). The hit's damage is therefore not on a part event. Why: adding a payload field to the input
  core event is a change outside this plugin. A consumer reads it with `FGSTN_PartEvents::GetPayload`. Assumption: none (A-ids unchanged).
- Dedupe (spec 5.4 rule 4): the key is `(EntityRef, PartIndex, BreakSerial)` with the entity as an `FObjectKey` of the event's target (there is no entity id type in the
  combat plugin), and it applies to the break events only (Broken, AllBroken). Damaged, WeakPointHit and Ricochet legitimately repeat for one part and one serial (every hit),
  so keying them would drop real hits. The window (`FGSTN_PartEventDedupe`, 64 keys, oldest forgotten first) is bounded because the serial is 4 bits and wraps; it lives in the
  consumer (presentation, HUD), not in the sync state.
- Event order of one outcome entry: Ricochet, or Damaged then WeakPointHit; one Broken per newly broken part in ascending index; AllBroken last. The Broken payload carries the
  outcome's `ResultBreakSerial` (the serial advances once per hit that breaks, so a cascade shares it) and the band of the hit; AllBroken and the hit events carry the hit's part.
- Launch (spec 3.3, "through the knockback layered move like M4 7"): the layered move needs Mover, so this module stops at `FGSTN_PartLaunchRequest`, the shape of M4's
  `FGSTN_ReactionMove` (velocity, decay, gravity scale, frames), plus `MakeLaunchSpec`, which sets the six set-by-caller magnitudes the way `ExecuteReactionMove` does. The keys
  (`Combat.SetByCaller.MoveVelocityX...`) are declared in GSTNCombatNP, so the caller passes them in (`FGSTN_PartLaunchKeys`); P7 fills them. The launch fields were open in the
  spec: velocity = hit direction projected on the plane normal to Up times `Speed x (1 - UpFraction)` plus Up times `Speed x UpFraction` (component split, not an angle). Several
  launches in one hit: the highest speed wins, ties the lowest part index (one move, no sum).
- Stagger: `GetMinimumStaggerFrames` is the longest Stagger entry among the newly broken parts (negative counts as 0); `ApplyMinimumHitstun` is a max. How the floor reaches the victim's
  hitstun is the P7 applier's business (no hitstun tag exists, handover 37).
- Stats: `UGSTN_GE_PartStats` (infinite) is found by class AND part, the part index riding in the spec level (index + 1). Chosen over a per-part tag (no new tags) or a stored handle
  (state outside the sync state). Stats entries of one part are merged by key (equal keys add up). A Blueprint child must read only keys every Stats entry sets, because the engine logs an
  error for an unset set-by-caller key (found by the first test run); documented on the class and in the README.
- Which GE class applies is not data of the part set: the caller passes the class (P7 reads it from a setting or the kit). No new setting in P5.
- Projection: `FGSTN_PartTagProjection::Apply` SETS (0 or 1) every tag of the universe (native three, all Granted tags of the set's Tags effects, the base tags passed in) each frame,
  so a restore clears a granted tag with no memory of the previous frame. A base tag must not be granted by anyone else. `Removed` uses `MatchesAny`: a removed parent suppresses its
  children (the first test run showed `HasTag` has the opposite direction).
- Module dependencies: the uplugin now lists GSTNCombatGAS, GSTNInput and GameplayAbilities, so enabling GSTNParts needs those plugins; the Core module itself still needs none.
- Tests: 12 `GSTNParts.GAS.*` as the plan lists them, on a transient world with a stock ASC and the combat attribute set; the test effect and the test stat leaves live in the test files.

## P6 GSTNPartsNP: the parts sync fragment (laptop, 2026-10-25)

- Wire format = the P0c byte encoding, not the spec 5.1 bit table: payload `uint16 BrokenMask, N x uint8 PartDamage, uint8 BreakSerial` (3 + N bytes), and an EMPTY fragment (mask 0, serial 0, no
  damage) is a ZERO-byte payload, so the "1-bit empty flag" of the plan is not a bit inside the fragment: the list's length byte (0) is the flag. The payload is byte-aligned because the
  registry hands the fragment a `FMemoryWriter` (bits would round up anyway). N is derived from `Ar.TotalSize() - 3` on read; a size of 1, 2 or above 19 bytes, or a serial above 15, sets
  the archive error and the registry drops the fragment (warning) without moving the reader. Numbers (test `BytesMatchTable`): N = 6 payload 9 B (spec 8 B), N = 8: 11 B (10 B), N = 16: 19 B (19 B);
  in the list (flag bit + count + id + length + payload) 13 / 15 / 23 B; empty 25 bits (4 B); no fragment 1 bit. The test asserts payload = 3 + N and at most 2 B above the spec bit count.
- `PartCount` (N) is an instance field the owner sets (`SetState(State, N)`), used only to write the damage bytes; it is not state and not compared. `GetWireCount()` raises it to cover the
  highest damaged part or broken bit, so a fragment whose count was never set is never cut short. An empty fragment still travels (4 B incl. the list flag) and reads back as present and empty.
  P7 should therefore add the fragment at attach only if it wants the 4 B per part-bearing entity from frame 0; the alternative is to add it at the first damage (then "has a part set" must come from the kit, not from the fragment).
- Type id: `GSTNPartsFeatureIds::Parts = 16` in `Public/GSTN_PartsFragmentRegistration.h`, NOT in the central `GSTNFeatureIds` (GSTNCombatCore, off limits). STOP item: move the line into the central file when GSTNCombat is next
  touched (1..127 is the GSTN range; nothing else uses 16). The module registers it at startup and unregisters at shutdown (`FGSTN_PartsFragmentRegistration`).
- SEAM FINDING (STOP item for GSTNCombat): `FGSTN_HitSyncState` (GSTNCombatNP) has no export macro, so `FGSTN_CombatSyncState`/`FGSTN_CombatSyncCore` cannot be instantiated from another module (inline
  constructor and `ShouldReconcile` do not link: LNK2019 on `FGSTN_HitSyncState::NetSerialize/Canonical/IsDefault/operator==/Interpolated/ToString`). The P6 tests go through `FGSTN_SyncFragmentList` (what the state delegates to); the
  state-level wire cost stays pinned by `GSTNCombat.NP.Fragments.*`. P7 must not touch `FGSTN_CombatSyncState` members from GSTNPartsNP until the macro is added (needs `GSTNCOMBATNP_API` on `FGSTN_HitSyncState` and `GSTNINPUTCORE` linkage of `FGSTN_InputHistory`, which is already exported).
- GSTNPartsNP.Build.cs lists GSTNCombatNP and NetworkPrediction (P7 needs them); the uplugin lists GSTNCombatNP and NetworkPrediction, so enabling GSTNParts now pulls the NP fork in.
- Tests: 9 `GSTNParts.NP.Fragment.*` (RoundTrip0/1/2, N1_8_16EmptyAndFull, ShouldReconcileExact, BytesMatchTable, ZeroPartsZeroBytes, UnknownTypeIdIgnored, ToStringStable), with the list instead of the combat state (see above); the malformed-payload case is inside UnknownTypeIdIgnored.
- Per-client sync bytes report: no entity carries a part set before P7, so the measured bytes with the plugin enabled equal those without (the list is the same single bit). The harness cannot read `stat net`; the per-send cost of a part-bearing entity is the table above (+13 / 15 / 23 B for a damaged N = 6 / 8 / 16 fragment, +4 B empty). Host bytes/s = per-client x clients (listen).

## P7 part 1 (commits cdc740d, d83913c, db3343c, 3aca048 and the docs commit): the NP simulation half

- **Combat follow-ups (lead approved, no behaviour change)**: `GSTNCOMBATNP_API` on `FGSTN_HitSyncState`; fragment id 16 is `GSTNFeatureIds::Parts` (the plugin-local `GSTNPartsFeatureIds` is gone); and the D9 snapshot seam, see m4-doc-deltas.md "H6/H10 follow-up (from M4b P7)" (contributor registry, 32 B extension blob on the entry and the history record, `MakeVictimView` fills the view pointer, rebuild between inbox entries, `AppendEvents` on outcome appliers, `Owner`/`AbilitySystem` on the tick context). The P0 spike's "D9 yes" was wrong: the array existed, nothing filled or read it.
- **One snapshot type**: `FGSTN_PartsSnapshotExt` (Core, `Pipeline/`) is the extension; no `Sim/GSTN_PartsSnapshotExt.h` was added. It gained `Flags` (derived `FlagGrabLocked`, computed by the contributor from set and mask) and a static_assert of at most 32 bytes. The spec's 20 byte budget holds for the state (mask, 16 damage, serial, flags = 20 B); the type word and the set pointer (8, aligned) make it 32 B. The pointer replaces the spec's `PartSetId` byte: the set is kept alive by `FGSTN_PartsAttach` for as long as an entity uses it, so the history copy is safe; a byte would have needed a global id registry.
- **Names**: the tick extension is `FGSTN_PartsTickExtension` (not `UGSTN_`: it is not a UObject). Priority 100, type id `GSTNFeatureIds::Parts`.
- **GrabLocked semantics**: set while the gate is UNMET (an empty gate never locks, a whole entity with `MinBrokenParts = 1` is locked, one broken part opens it). The first drafts of the tests had it inverted.
- **Hurtbox overrides**: done in `FGSTN_PartsSnapshotContributor::ModifyEntry` on the entry's `HurtStates` (Intangible for the hurtboxes of broken `Disable` parts), not in the tick extension, because the snapshot is built before the hooks of any entity and the entry is what the attacker reads. A rewound attacker (lag compensation) gets the rewound extension but the CURRENT hurtbox states (the history keeps the extension, not the states): a hidden arm stays hidden for an attacker rewound to before the break. Acceptable for P7, listed for part 2.
- **Break effects**: derived ones (tags, hit groups, hurtbox hiding) and the Stats effects (only when `FGSTN_PartsTickExtension::SetStatsEffectClass` is given a class; applied for broken parts and removed for unbroken ones every frame, idempotent) are projections of the mask. The ONE-SHOT Launch and Stagger of a break are NOT applied: an outcome applier has neither the victim's reaction state nor an ability system, and `FGSTN_OutcomeApplyResult` carries only tags and effect classes. STOP item for part 2 or P8: a seam to raise the victim's hitstun floor / pending launch from an applier (or to read the outcome extensions in `RunVictimStep` after the drain). `FGSTN_PartBreakEffectApplier::Collect/MakeLaunchSpec` are ready for it.
- **Attach**: `UGSTN_PartsAttachComponent` binds the loadout-applied event of the owner's `UGSTN_LoadoutComponent` (and handles a loadout that was applied before it began play); the pawns of the test harness do NOT have it (it would be an edit of the shared `BP_GASNPP_TestCharacter`, owner's OK needed). The hand-run attached the set with `UGSTN_PartsKitLibrary.AttachPartSetToActor` on each world. The mismatch check at connect is `IsPartCountCompatible` (more bytes than the set has parts, or another recorded count); `ApplyPartSet` takes the received fragment as an optional argument, the component passes none (it cannot read the sync state at that point), so a real mismatch is caught only when the caller supplies the fragment.
- **Events**: `Combat.Event.Part.Damaged/WeakPointHit/Broken/AllBroken` are made by `AppendEvents` after HitReceived. A ricochet entry used to skip the pipeline in the victim step and so made no `Part.Ricochet` event; fixed in P9. Deduplication is the consumer's (`FGSTN_PartEventDedupe`); the unit test proves a redelivered hit reproduces the key.
- **Content**: `setup_gas_assets.py --parts-content` (idempotent, new assets only): `HBS_Test_PawnParts` (six hurtboxes: the three of `HBS_Test_Pawn` plus `arm_l`, `arm_r` and `core`; `HBS_Test_Pawn` itself has only three, so "six parts matching it" was not possible without editing it), `PS_Test_Pawn` (Torso, Head weak point, Legs, ArmL/ArmR with `Disable` policy and hit groups 1 and 2, Core weak point with `MainDamage` 25 % and `bKill` off), `GSTN_CombatRules_TestParts` (standard rules, `PartResolve` in place of `HealthWeight` and `ApplyDamage`), `KIT_Test_Parts` (copy of `KIT_Test_Hits` with the `FGSTN_PartsKitExtension`). The Python reflection of `FInstancedStruct` is avoided with two C++ helpers (`UGSTN_PartsContentLibrary`, `UGSTN_PartsKitLibrary`).
- **Tests**: headless `RunTests GSTN`, plugin enabled 506/506 (396 combat + 4 `GSTNCombat.NP.SnapshotExt` + 88 earlier parts (55 Core, 12 Step, 12 GAS, 9 Fragment) + 18 new `GSTNParts.NP.*`: Snapshot 3, Applier 5, TickExtension 4, Attach 3, Content 3); plugin disabled 400/400 (396 + the 4 combat seam tests; the 396 are unchanged). A unity-build name clash (`Flags`) only showed once the files were committed; renamed `TestFlags`.
- **PIE hand-run** (listen, players 3, one process, viewport, 60/15 ms 2 % loss, harness level, fresh PIE): `hit_basic` with the parts rules (in memory: `ConvertRulesToParts` on the shared rules object, pipeline TeamFilter, HurtState, HurtboxReaction, PartResolve, ComputeKnockback, Hitstop, MeterGain) and `PS_Test_Pawn` attached by hand to the victim on the three worlds. Result: three hits landed on the torso; the fragment of the victim reads `mask=0x0000 serial=0 dmg=[36,0,0,0,0,0]` on the host and both clients (equal on all worlds, only part 0 moved); main damage 15.6 on the authority and the owner (the standard run says 8 per hit, so the harness check "24 damage" differs by design). Victim's client rollbacks per hit: 1, 1, 1 (the check passes); unexplained reconciles 11 (combat 4, mover 4, asc 3), the same count (11, combat 4) as the same scenario run again in the same PIE with the standard rules and no set (baseline). The other `hit_basic` checks that failed (control hit, BakedSocket, WorldAtMoveStart, hitstop windows, cancel on hit, attacker rollback sampling) fail identically in the baseline run (a second scenario in one PIE: the pawns are not at the start pose): not caused by parts. No break was provoked (needs more damage than the torso cap 120); breaks are the scenarios of part 2.

## P7 part 2 (harness scenarios and network proof, laptop, 2026-10-25)

- **Scenarios** (`Tools/GASNPPTest/parts_scenarios.py`, `SUITE_PARTS`, not in `SUITE_DEFAULT`): `part_break`, `part_cap`, `part_hitgroups`, `part_rollback`, `part_host` (listen only, skipped by design on a dedicated server), `part_restore` (P7b). Each runs in a FRESH PIE; `parts_batch(names, lag, jitter, loss)` restarts PIE between them. Setup is IN MEMORY (never saved, put back by `parts_reset()`): the pawns' `HBS_Test_Pawn` gets the six hurtboxes of `HBS_Test_PawnParts` with every box but the target part's moved out of reach and the target's given the torso shape the Root track is known to reach (every hit lands on the target part, whatever the geometry); the target part gets hardness 0 (the "equal" band, 65 % of the hit's 8 = 5.2 per hit) and cap 12 (the third hit breaks it; cap 500 for the KO scenario); `ConvertRulesToParts` on the shared rules, `fill_standard` + `hit_rules_setup` afterwards; the set is attached by hand on every world (`AttachPartSetToActor`). No Blueprint, umap or uasset was touched or saved.
- **New harness hooks (C++, GSTNPartsNP `UGSTN_PartsKitLibrary`)**: `StartPartEventLog / GetPartEventLog / StopPartEventLog` record the `Combat.Event.Part.*` events a world's event subsystem publishes ("Type:Frame:Part:Serial:TargetName"); the project's test component only listens to the hit events. Damage override was not needed (cap and hardness are edited in memory).
- **Findings that changed the harness, not the plugin**: (1) the test move's tracks can also touch the third pawn (it has parts too), so a shot's hit rows are cut to the ones whose inbox row the victim received; (2) the sampler's tag lists carry `Test.*` tags only, so `Combat.Part.Broken` is read from the ability system directly (`has_tag`); (3) a hit on a broken Disable arm is still logged by the attacker side (the hurtbox is Intangible) but does no damage and makes no event, so the "hidden arm" check judges damage and events, not the hit row; (4) the forced reconcile of `np.ForceReconcile` collapses when several are requested close together, so `part_rollback` counts the rollbacks of all clients in the window of the break hit (9 at 60/15/2 %).
- **No plugin bug was found in P7 part 1** by the scenarios: state strings, events and the victim client's one rollback per hit were clean on the first run of `part_break`.
- **Bytes report** (the harness cannot read `stat net`): the fragment is added at the first damage (not at attach), so a pristine part-bearing entity costs 1 bit like any other. A damaged entity with N = 6 parts is 9 B payload / 13 B in the list on every send of its combat sync state; N = 16 is 23 B. Host (listen, 2 clients): 13 B x 2 clients per send of a damaged entity, at most 26 B per net update (about 1.6 KB/s at 60 updates/s, an upper bound: NP does not send an unchanged state every tick); without parts the added cost is 0 B. The plan's 24 B ceiling for 16 parts holds (23 B).
- **STOP item (Launch/Stagger one-shots of a break)**: kept data-only. An outcome applier has neither the victim's reaction state nor an ability system, and a tick extension that wrote `Core.Hit` pending launch / hitstun end would break the contract "an extension writes only its own fragment" and has no hit direction. The seam needed (P8 must add this M4 follow-up): a reaction request field (minimum hitstun frames + launch velocity) in `FGSTN_OutcomeApplyContext`, merged into `Core.Hit` in `RunVictimStep`. `FGSTN_PartBreakEffectApplier::Collect/MakeLaunchRequest` are ready for it.

## P7b part restore (laptop, 2026-10-25)

- **Why option (b) of the plan (the request as a field of the fragment) could not hold**: the combat model ticks BEFORE the ability system and nothing else writes its state during a frame (`UGSTN_CombatSimComponent` class doc), so a GE component, which runs in the ability system's tick, cannot write the fragment. Chosen instead (lead accepted): the request IS an infinite gameplay effect carrying `UGSTN_GEComponent_RestoreParts` (`GSTNPartsGAS`; fields `PartMask`, `DamageFraction`; a per-request mask rides in the spec level as mask + 1, the fraction as the set-by-caller `GSTNParts.RestoreFraction`). The ability system is rolled back with the rest, so an effect still on the target is a request not consumed yet, on every machine and after every rollback; the parts tick extension consumes it at `AfterClocks` of the victim's next combat tick (after the hits of that frame): `UGSTN_PartMath::RestoreParts` on the frame's output fragment, `Combat.Event.Part.Restored` into the frame's outbox, the effect removed. A re-simulated frame finds the effect again and does the same. Fragment layout and wire bytes are UNCHANGED (3 + N, 23 B at N = 16), the P6 bytes test is untouched.
- **Native default** `UGSTN_GE_PartRestore` (infinite, the component as a default subobject: `FindOrAddComponent` calls `NewObject`, which a constructor must not); any effect class with the component works (a healer enemy's ability applies it through the normal GAS path). `UGSTN_PartsLibrary::RequestRestore(Target, PartMask, DamageFraction, EffectClass, Instigator)` (`GSTNPartsGAS`, BlueprintCallable, authority only, outside the simulation) applies it. Mask 0 in code = the asset's mask (every part for the default class).
- **Combat follow-up (lead accepted, own commit "M4 follow-up: outbox on the combat tick context")**: `FGSTN_CombatTickContext::Outbox` (additive, set by `UGSTN_CombatSimComponent`, null in tests). Without it an extension has no way to publish an event that is not a hit event. Also listed under the H6/H10 follow-up in m4-doc-deltas.md.
- **Event**: `Combat.Event.Part.Restored` (native tag in `GSTNPartsCore`, 12 declared tags now); payload part index, the new break serial, no band; `FGSTN_PartEvents::AppendRestored`. The key (entity, part, serial) is distinct from the breaks' (restore advances the serial once).
- **State rules**: `RestoreParts` was already in the P3 math (clears bits, damage = fraction of the cap but never worse, broken ancestors restored with the part, serial +1 once). Derived effects (tags, hit groups, hurtbox hiding, the Stats effects when a class is set) reverse by themselves because they are projections of the mask. There is no `bReversible` flag: the one-shot effects (MainDamage, Launch, Stagger) have no trace to undo, so a flag would do nothing. A knocked out single-part set (a player) is NOT restored (KO wins); a validator warning on single-part sets was NOT added (it would fire on every player set and break the "no warnings" assertions of the data tests): the guard is in the tick extension.
- **Tests** (headless `RunTests GSTN` 511/511: 506 + 4 `GSTNParts.NP.Restore.*` + `GSTNParts.GAS.Restore.GEComponentRequests`; the tags test counts 12): `ClearsMaskAtFPlus1`, `RollbackRedeliveryRestoresOnce` (the frame simulated three times with the effect put back each time: same state, same one event), `RestoreThenBreakNewSerial`, `IrreversibleEffectsStay`. The forced reconcile across the restore frame is exercised in PIE by `part_restore` (two reconciles of the victim's client and one of the attacker's right after the request).

## P8 hit feel through parts (laptop, 2026-10-25)

- **Combat follow-up, own commit "M4 follow-up: reaction request on the outcome apply context"** (lead decision): `FGSTN_ReactionRequest` (minimum hitstun frames, launch velocity, the hit's frame stamped by the victim step) on `FGSTN_OutcomeApplyContext`. `RunVictimStep` (`FGSTN_CombatVictim::Resolve`) merges it as a hit of that strength would: the longer hitstun floor wins; a launch replaces the hit's when it is faster, or makes a Launch of a hit that only staggered or did not stagger, with the hitstun the knockback model gives a launch of that speed (found by the harness: without it the launch ended its hitstun on the frame it began and the owner never saw the state); a floor alone staggers in place. Neutral when unset. Test `GSTNCombat.NP.Hits.Victim.ReactionRequestMergesLikeAHit`.
- **Ricochet: `bHit` stays true (deviation from P4).** The step used to return `Stop` with `bHit` false. The attacker's loop (`FGSTN_CombatAttacker`) and the victim drain both drop a hit the pipeline refused (`!Outcome.bHit`) BEFORE the attacker outcome seam is asked, so a ricochet could never start a recoil. The step now stops the pipeline (no damage, no knockback, so no hitstop and no requests) and keeps `bHit`; the P4 test `RicochetBandNoStaggerNoHitstop` was updated. The seam (`FGSTN_PartsAttackerOutcome`) reads the Ricochet flag of the outcome entry the same run wrote, instead of deciding a second time.
- **Hitstop contributor = the step's existing modifiers.** `band.HitstopScale x part.Reaction.HitstopScale` was already written by P4 from static data; the combat knockback model applies `Modifiers.HitstopScale` to `Auto` only and an `Override` is final (the test move itself uses Override 8/6). P8 adds the tests, no code. Limits: the Auto formula's own knockback term (`HitstopPerKnockback`, 0 by default) depends on the weight, so with a non-zero value the attacker's frames can differ from the authority's by the broken-limb weight, an M4 property; the fixture sets it to 0 and says so.
- **`AirborneJuggleScale` is the combat rules' one** (`UGSTN_CombatRules::AirborneJuggleScale`, default 1, applied to the knockback of a tumbling victim by `ComputeKnockback`). I first added a second one to the parts settings and removed it again: A9's "global AirborneJuggleScale" already existed in M4. The per-part `JuggleFactor` and the body's multiply the weight (P4). Test `Reaction.AirborneJuggleScale` pins the composition.
- **A10 as built:** the threshold is on the stagger force (`Force x StaggerScale >= threshold`), the hitstun length comes from the knockback only (`K x HitstunFactor`, floor `MinHitstunFrames`), never from the force above the threshold; the break Stagger is a floor on top.
- **Break Launch/Stagger wired:** `FGSTN_PartsOutcomeApplier::Apply` calls `FGSTN_PartBreakEffectApplier::Collect` for `NewlyBrokenMask` with the entity's up and the hit's `HitboxToVictim` and merges the strongest launch and the longest stagger into the request. Pure function of the entry and the static set, so a re-simulated hit asks for the same.
- **`Combat.Event.Part.Ricochet` on the victim: resolved in P9** (m4-doc-deltas.md "P9 follow-up": the victim step keeps the outcome extensions of a flagged ricochet entry and `AppendHitEvents` asks the appliers for it).
- **Content (`setup_gas_assets.py --parts-content`, idempotent, new assets only; `UGSTN_PartsContentLibrary::SetBreakLaunch/SetBreakStagger` added for it):** `PS_Test_Pawn`: ArmR = the armoured plate (hardness 3 from P7, now stagger threshold 100 and MaxReaction PushBack: the light test move, penetration 0, bounces; a scenario lowers the hardness in memory to make it take the hit); Head = weak point, main transfer 2 (P7), stagger threshold 5 now; Legs = Launch 900 cm/s, 60 % up on break. KIT_Test_Hits, the standard rules and the Blueprints are untouched. The assets `PS_Test_Pawn`, `GSTN_CombatRules_TestParts`, `KIT_Test_Parts` are rewritten (the script rebuilds all properties from the specs).
- **Scenarios (`SUITE_PARTS` is now 10):** `part_ricochet` (hardness 3 plate, light blow: one flagged hit, identical row on the attacker's client, recoil start = hit frame + 1 on both copies and it outlives the 32 frame move, no hitstop window on either side, no damage or hitstun or part state, flagged inbox entry, no Part.Damaged/Broken, the attacker's client does not roll back during the shot), `part_stagger` (StaggerPerDamage 1 and 20 give the light and the heavy blow, thresholds pushed to push back 300 / launch 400 so the bracket 500 would launch: light on armour = damage and a flinch; heavy on armour = hitstun, `Reaction.PushBack` event, no rise; light on the weak point = hitstun; windows and action states equal on authority and owner), `part_weakpoint` (head damage = 2 x torso damage, WeakPointHit once), `part_break_launch` (hurtbox threshold 1000 so only the break staggers; blows 1 and 2 flinch, blow 3 breaks the legs and launches: staggered on both copies, one hitstun window, rise > 30 cm, peak vertical speed > 300, equal windows; blow 4 on the stump launches nothing; Broken once; every world ends within 15 cm).
- **Harness notes:** `parts_batch` needs a running PIE of the right mode when it is called (it counts the worlds to decide the pawn count); a failed first call leaves `status` at "n queued". `_PartsRig` shots now take `hold`/`release`. One transient failure seen: `part_weakpoint` counted 2 rollbacks per hit on the victim's client once (two reruns clean).
- **Results (laptop, 2026-10-25/26).** Headless `RunTests GSTN`: plugin enabled 524/524 (511 + 12 `GSTNParts.NP.*` HitFeel tests + `GSTNCombat.NP.Hits.Victim.ReactionRequestMergesLikeAHit`); plugin disabled 401/401 (400 + the new combat test). `SUITE_PARTS` (10 scenarios, fresh PIE each, 60/15 ms, 2 % loss): listen (3 players, one process) all PASS (break 15, cap 8, hitgroups 9, rollback 8, host 10, restore 17, ricochet 12, stagger 7, weakpoint 8, break_launch 10 checks); dedicated (2 clients) all PASS (host skipped by design). With the plugin disabled: `combat_spike` PASS 11, `hit_basic` PASS 15 once the host pawn and the victim were moved to their lanes first (a fresh listen PIE spawns the three pawns 60 cm apart, so the control attack of `hit_basic` hits the victim and the third pawn; 4 of its 15 checks fail for that reason alone, nothing else). P0a re-measure: see `m4b-spike-report.md` ("P0a re-measure with the ricochet prediction"): 3014 predicted hits, 1 disagreement (0.033 %), which flipped the predicted ricochet and cost 2 rollbacks of the attacker's client in that cycle.

## P9 grab gate, thickness, per-hitbox penetration (laptop, 2026-10-26)

- **Grab gate: mostly built before P9.** `FGSTN_GrabGate`, `UGSTN_PartMath::IsGrabLocked`/`ProjectDerived`, the validator rules and the `Combat.Part.GrabLocked` projection by the tick extension came with P2/P3/P7, with `GSTNParts.Core.GrabGate.Evaluation` and `GSTNParts.GAS.Projection.GrabLockedGate`. P9 adds `GSTNParts.NP.GrabGate.TagProjected` (a gate naming one required part: locked, locked after another part breaks, open after the required one breaks, locked again after a rollback) and the README section "Grab gate" with the rules-asset edit (designer adds the tag to the ignore list of `FGSTN_GrabRules::Requirements`; no M6 code, no foreign tag). Deviation: the plan's four `Core.GrabGate.{EmptyNeverLocked, MinBrokenParts, RequiredParts, AnyOf}` are the sections of the one existing `Core.GrabGate.Evaluation` test and were not split. Open: `scenario_part_grabbable` waits for the M6 grab ability.
- **Thickness / DeepDamage (owner-added feature, not in the plan).** `FGSTN_PartDefinition::Thickness` (0..1, `Parts|Damage`) and `FGSTN_PartHitData::DeepDamage` (negative = the hit's Damage, the sentinel is -1, the default). `UGSTN_PartMath::ApplyHit`: after the band decides the hit is not a ricochet, `D' = D x (1 - T) + DeepDamage x T` replaces D for both the main share and the part share (the band scales are linear, so applying it before or after them gives the same value; before keeps one place). T = 0 skips the blend, so existing parts are unchanged bit for bit (no test needed edits). The body (no part) and ricochet paths do not blend. `FGSTN_PartHitInput::DeepDamage` carries it. No `UGSTN_PartsContentLibrary` setters were added: the library has setters for break effects only, none for hit fields or part numbers. Tests `GSTNParts.Core.PartMath.Thickness.{ZeroIsUnchanged, OneUsesDeepDamageOnly, HalfBlends, DeepDamageDefaultsToDamage}`.
- **Per-hitbox penetration.** No plumbing needed: a hitbox variant's `FGSTN_HitDataOverride::ExtensionOverrides` replaces the `FGSTN_PartHitData` entry by struct type (`UGSTN_HitDataUtils::ApplyVariant`). `GSTNParts.Core.Step.PerHitboxPenetrationThroughVariantOverride`: penetration 0 ricochets on the arm (hardness 4), the override with penetration 6 lands on it. Per-hitbox (not per-variant) data is the hitbox's own `FGSTN_HitData::ExtensionData`, resolved by the same step.
- **Part.Ricochet on the victim:** see m4-doc-deltas.md "P9 follow-up". Open STOP items left: hurtbox states not in the lag history; connect-time set mismatch check (HANDOVER 40).

# M6 implementation plan: defence and links (plugins `GSTNDefence`, `GSTNLink`) (draft for owner review)

Plan only. Checkpoints are named **F0..F14**. Nothing here edits a running checkpoint of M4, M4b or M5: every dependency on
earlier work is a capability (**X1..X22**, section 2) with its spec section and, since M4 and M4b are complete, its **audited
state on `feature/gas-npp-5.8` at 0848345** (present, partial, missing). The two missing groups become the first two
checkpoints (F1, F2), as neutral `GSTNCombat` follow-ups in the style of the M4 follow-ups of M4b (`docs/notes/m4-doc-deltas.md`,
"H6/H10 follow-up").

Sources (short ids used below):

| Id | File | Used for |
|---|---|---|
| M6 | `docs/architecture/combat-m6-defence-links.md` (revision 2026-10-23) | The spec of this milestone; section numbers are M6's local ones |
| M4 | `docs/architecture/combat-m4-hits.md` | Pipeline, inbox, snapshot, lag compensation, reaction tiers, §7b surface impacts |
| CP | `docs/architecture/combat-parts.md`, `Plugins/GSTNParts/README.md` | `Combat.Part.GrabLocked`, `FGSTN_GrabGate`, `EffectiveWeight` (F12) |
| IN | `docs/architecture/interaction.md` | The interact button, `GSTNInteract`, `Interact.Target.*` (F9, F12) |
| M10 | `docs/architecture/combat-m10-held-objects.md` §6b | Thrown-body hand-off (reserved in F12) |
| M7 | `docs/architecture/combat-m7-piggyback.md` | What the link system must leave open (§7.0 of M6) |
| MT | `docs/architecture/modularity-and-tags.md` | Layering §3, Blueprint §3.8, replaceable content §3.9, tags §4, extension surface §5b |
| DD | `docs/architecture/game-design-direction.md` | Owner direction (wins over M6): §2e, §2f, §3, §4, §6, §6b |
| OQ | `docs/architecture/open-questions-m4-m10.md`, section M6 | Owner answers and open items |
| M4bP | `m4b-parts-plan.md` | Format model; the M4 follow-up procedure (deltas entry, lead OK, neutral without a contributor) |

Owner direction folded in (DD wins over M6 where they differ; M6 revisions of 2026-10-22/23 already apply it):

- **Shield only where the game mode enables it** (PvP yes, PvE no): `UGSTN_DefenceRules::bShieldEnabled` and the kit's shield entry
  must both exist; the shield slot is free for kits in PvE (M6 §1d).
- **Shield works like Rivals of Aether 2, in 3D** (owner direction 2026-10-26, after spike S5): spherical bubble of **fixed size**, no
  stick tilt, no shrink, no poke, no arc variant; break with an upward launch, shield stun and pushback by damage, out-of-shield options,
  grabs beat it, grounded only (M6 §5.3).
- **Parry = one resolution, two sources** (the shield's release-anchored perfect shield and the `ParryWindow` track of any move), in
  every mode (M6 §5.4).
- **No tech by default**: landing from tumble = stick-steered mini bounce, then the downed state (`Knockdown`), get-up roll by stick,
  get-up attacks 0..N per moveset, jump gets up. Tech stays a pluggable kit ability (M6 §6, owner 2026-10-23).
- **Grab is the interact button**: tap = grab, hold = pick up (M10); the context probe lives in `GSTNInteract` (IN, owner Mod.20).
- **Grabbability** = weight class (`MaxGrabWeight` vs the victim's effective weight) and broken parts (`Combat.Part.GrabLocked` by
  name), both data (M6 §8).
- **Listen server is the reference topology**; the host is a link partner in the spike and in every scenario (DD §6b).
- Armor and poise need nothing from M6 (DD §2f). No rolling movement mode.
- **Latch grabs on big enemies (owner, 2026-10-26 10:05Z):** grabs must also work on enemies too big to lift, where it is the
  **player who attaches to the enemy** (to impale it, for example) and the enemy keeps moving. In link terms: a link type whose
  **initiator's role is the parked one** (the player rides the enemy's socket) and whose target role is not parked and keeps acting.
  `spin-drill.md` §13b already reserves `Combat.Link.Type.Drill` as "a hit-grab on the Body channel": the drill is one latch.

**Owner approval (2026-10-26 10:05Z):** every item of section 6 is approved as written.

## 0. Ground rules for every checkpoint

- One checkpoint = one commit (short imperative subject, reason in the body). Gate before each commit: full build (Rider or UBT, not
  Live Coding, for header/reflection changes) in the **four plugin configurations** (neither plugin, `GSTNDefence` only, `GSTNLink`
  only, both); `Automation RunTests GSTN` green and the enabled/disabled counts explained against the previous HANDOVER; harness
  scenarios named in the checkpoint clean at 60 ms/15 ms jitter, with and without 2 % loss, once NP code is touched; M1..M4b
  scenarios still green with both plugins on and off. The disabled-plugin check is a hard gate (M6 §1b): with neither plugin the
  combat state shrinks to M4b's size and every M4b scenario passes unchanged.
- **Every test count in a handover is measured from a named log** (`-abslog`, `Select-String` counts); a count without its log is not a
  result (lesson of M4b P9, `docs/notes/takeover-state.md` HANDOVER 44).
- Network gate for every NP checkpoint: **listen server (host + 2 clients) primary**, dedicated + 2 clients secondary; the host has no
  prediction and no wire to itself: scenarios assert "zero unforced reconciles on the host" and "host-local events fire once". No
  `OnRep` glue: anything replicated runs through a function the server also calls. Simulation code has **no `IsServer()` branch** and
  **no topology branch** (M6 §2.1: the host gets no faster link formation, only no network delay).
- Epic coding standard (UE 5.8, `GSTN_` prefix, `TObjectPtr`, no `auto` outside lambdas/iterators); comments explain why and the
  simulation constraints; tuning values in `USTRUCT`s with `Category`/`meta`; stateless math in `*Math` libraries.
- Dependency order (M6 §1b): `GSTNDefenceCore` < `GSTNDefenceGAS` < `GSTNDefenceNP`; `GSTNLinkCore` < `GSTNLinkGAS` < `GSTNLinkNP`;
  **no module of one plugin lists a module of the other**; Core modules depend only on `Core`, `CoreUObject`, `GameplayTags`,
  `GSTNInputCore`, `GSTNCombatCore`; the project module gets only PRIVATE deps. A grep gate in every commit checks the `.Build.cs`
  files (Core/GAS never list NP; Defence never lists Link and vice versa).
- Native tags: `UE_DEFINE_GAMEPLAY_TAG_COMMENT`, one doc line each (who writes, projected or granted), declared in the owning
  module (`GSTN_DefenceTags.h`, `GSTN_LinkTags.h`); the other plugin's tags are read **by name** through
  `FGameplayTag::RequestGameplayTag(..., false)` (M6 §1c): `GSTNLink` never declares `Combat.State.Shield*`, `GSTNDefence` never
  declares `Combat.Link.*`, neither declares a `GSTNCombat` or `GSTNParts` tag. **No tag in sync state or snapshot POD** (indices
  only: rules index, profile index, link type index, reason index). Designer commands beyond the standard set go in
  `Config/Tags/GSTNCommands.ini`.
- README of each plugin is updated in the same checkpoint that changes a module, tag or setting (module table, tag table, "How to
  extend", settings). `docs/` is NOT edited in these commits except F14 (one docs commit). Doc deltas are collected per checkpoint in
  `docs/notes/m6-doc-deltas.md`; the handover is `docs/notes/takeover-state.md` (HANDOVER n), as in M4/M4b.
- **R-B Independence acceptance line (every checkpoint):** each plugin builds and its tests run **without the project module and
  without the other plugin**; no `/Game/` reference, no project include, no project ini key, no project tag; settings in the
  plugin's own `UDeveloperSettings` with defaults in `Plugins/<Plugin>/Config/Default<Plugin>.ini`; tests carry the `GSTNDefence.` /
  `GSTNLink.` prefix. Grep gate: no `ConstructorHelpers`, no `"/Game/`, no hard `TObjectPtr`/`TSubclassOf` to content where a soft
  reference works.
- **Blueprint extension (MT §3.8):** `UGSTN_LinkRule`, `UGSTN_LinkExitPlacer`, the pipeline steps and `UGSTN_LinkActionProvider` are
  `UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)` with ONE `BlueprintNativeEvent` entry point, const view in,
  result struct out, pure. Per-frame tick extensions stay C++. Rule structs `USTRUCT(BlueprintType)`, tag fields with
  `meta=(Categories=...)`; math in `BlueprintPure` libraries (`UGSTN_DefenceMath`, `UGSTN_SDIMath`, `UGSTN_LinkMath`).
- **Replaceable content (MT §3.9):** rules assets, link type definitions, parry profiles, tech sets, kit extensions are
  `TSoftObjectPtr`/`TSoftClassPtr`/Primary Asset IDs; link types are found by the asset manager (`GSTN_LinkType`), never listed in code.
  Simulation-read content (rules, link definitions, curves) resolves before the pawn's first NP tick and is identical on host and
  clients; indices (rules, profile, link type, reason) come from tables built in the same order everywhere.
- Test prefixes: `GSTNDefence.Core.*`, `.GAS.*`, `.NP.*`; `GSTNLink.Core.*`, `.GAS.*`, `.NP.*`; `GSTNCombat.Core.*`/`.NP.*` for the F1/F2
  follow-ups. Harness: `Tools/GASNPPTest/defence_scenarios.py` (SUITE_DEFENCE) and `link_scenarios.py` (SUITE_LINK), both importing
  `gasnpp_test`, scenario names as M6 §11.

## 1. Network and cost target: LISTEN server (host also plays)

| Statement | Consequence in this plan |
|---|---|
| Host is one of the link partners (DD §6b) | `scenario_link_spike` and every link scenario run **three pairings**: host grabber, host grabbed, client vs client; `place_pair` takes the host as either partner (F0, F10). The latch adds the pairing "parked initiator on a moving partner" (host as either) |
| Host has no prediction and no corrections | Link formation and break frames are the same on the host (F+1 / F+2, break at F+1 after the watchdog); the host's only difference is no network delay; no topology branch (M6 §2.1) |
| Watchdog is a server world subsystem at `FinalizeFrame` time | On the host it reads the host's own finalized frame F like any other instance; its cost is measured on the host (F0 S1) |
| Shield bubble predicted by the attacker from replicated state | On an interpolated victim a bubble-edge disagreement is a normal correction; the rate is measured (F0 S5) and the attacker's prediction never reads live components |
| Host upload is the cap | Idle defence/link fragments cost one bit each (absent); the byte table of M6 §10 is asserted in F14 and the spike reports host bytes/s with and without links |
| Reverse deliveries (parry punish, link response/command) exist on the server only | Owning clients never hold `Punish`/`LinkResponse`/`LinkCommand` entries; the attacker consumes the punish as **one** correction (F0 S3, F7) |

## 2. Dependencies, by capability, with their audited state

Capability ids X1..X22 are mine; "Needed by" lists the first F checkpoint that cannot be done without it. State audited on
`feature/gas-npp-5.8` at 0848345 (2026-10-26). **Missing or partial seams are built in F1/F2 as neutral `GSTNCombat` follow-ups**,
with the M4b procedure: documented in `docs/notes/m4-doc-deltas.md`, lead OK, inert without a contributor, M1..M4b tests and scenarios
unchanged. If a seam turns out to need a change that is not neutral, STOP and ask (section 4).

| Id | Capability | Spec | State at 0848345 | Needed by |
|---|---|---|---|---|
| X1 | **Sync fragment registry** (`FGSTN_SyncFragment`, instanced-struct fragments in the combat sync state, type ids in `GSTNFeatureIds`, absent = one bit) | M6 §1b, MT §5b | **Present** (`GSTNCombatCore/Public/Sync/GSTN_SyncFragment.h`; `Parts = 16`, game ids from 128) | F7, F10 |
| X2 | **Combat tick extension hooks** (`IGSTN_CombatTickExtension`, `EGSTN_CombatHookPoint`, registry by priority, `Owner`/`AbilitySystem`/`Outbox` on the context) | M6 §1b, §5.2 | **Present** (`GSTNCombatNP/Public/Simulation/GSTN_CombatTickExtension.h`); verify the four hook points M6 needs (after inbox drain, after clocks, before candidates, tag projection); **`BlocksActions` (can-act veto from a fragment) missing** | F2 (veto), F7, F10 |
| X3 | **Open inbox kinds** (`FGSTN_InboxEntry` with kind id + payload, handler registry, drain order (Frame, SenderId, kind, order), rollback trimming) | M6 §3 "Inbox types", §5.2 step 2 | **Missing**: the inbox is hit-only (`FGSTN_HitInboxEntry`, `FGSTN_HitInbox`, `PostHit`) | F1 |
| X4 | **`PostToInbox(TargetId, Entry)`** from any combat tick, no-op where the target does not drain | M6 §3 | **Missing** (only `UGSTN_CombatSimComponent::PostHit`) | F1 |
| X5 | Pipeline steps as data (`UGSTN_CombatRules::Pipeline`, `UGSTN_HitResolutionStep` const/pure, context with `OutcomeExtensions`, `bBlocked`/`bParried` placeholders) | M6 §9, M4 §3b | **Present** (`GSTN_HitResolution.h`, `GSTN_HitSteps.h`); `OutgoingEntries` on the context **missing** | F2 (outgoing), F7 |
| X6 | **Timeline track kinds registrable by an extension** (a `GSTNDefenceCore` track kind seen by the M3 timeline registry and the combat clock) | M6 §3 tracks, §1b | **Verify in F0 S0**: the M3 registry builds track kinds; the hold-stall hook on the clock step and the parry-window stamp hook are **missing** | F2 |
| X7 | **Snapshot extension contributors** (32 B blob per entry, lag-comp history record, `MakeVictimView` view pointer, rebuilt between inbox entries) | M6 §1b, M4 §5 | **Present** (M4 follow-up d83913c, D9) | F7, F10 |
| X7b | **Extra hurtbox seam** (`FGSTN_ExtraHurtbox { shape, priority above body, hit-type ignore tags }` contributed with the snapshot, reserved index range ≥ 0x80, picked by `PickHurtboxIndex`, carried in the lag history) | M6 §1b, §5.3 "Contact" | **Missing** | F2, F0 S5 |
| X8 | `FGSTN_HitData::ExtensionData` (instanced structs) and the per-hitbox override | M6 §3, M4 §3b item 5 | **Present** (`GSTN_HitData.h`); **`bParryable`, `bDisableDI`, `LinkFilter`, `LinkType` missing**; `bBlockable` is to be derived from `HitType` | F1 |
| X9 | **`FGSTN_EntityRef { Kind, Id, Serial }`** in `GSTNCombatCore`, kind ids in the central id file | M6 §7.0 item 1 | **Missing** | F1 |
| X10 | Kit extension list (`FGSTN_KitExtension`) and rules extension (`FGSTN_RulesExtension`) | M6 §1b, §1d | **Present** (used by `GSTNParts`) | F3, F5 |
| X11 | `FGSTN_TagRequirements`, Forced `ActivateEntry` requests (`ActiveSource`), standard slot set (`UGSTN_StandardSlots`), M3 cancel windows (`OnHit`/`OnBlock` conditions) | M6 §4, §8 | **Present**; `OnParry` cancel condition and the `LinkEstablished` buffer event **missing** (F7, F12 add them through the existing condition registry if it is open; else F2) | F7, F12 |
| X12 | Reaction request seam (`FGSTN_ReactionRequest`: min hitstun frames + launch velocity on `FGSTN_OutcomeApplyContext`) and the layered moves `Freeze`/`Knockback` | M6 §5.3 break launch, pushback; §5.5 SDI | **Present** (M4 follow-up cacce44; `FGSTN_LayeredMove_Freeze/Knockback`); the pushback needs a **"no tumble" knockback variant** and the SDI drift a **per-frame `DriftVelocity` on the freeze move**: F8 adds both as neutral data fields | F7, F8 |
| X13 | `UGSTN_HitMathUtils::ApplyDI` (M4 signature) | M6 §5.5 | **Present**; F4 adds the rules overload in `GSTNDefenceCore` (M4's stays a wrapper only if the owner wants the default rules moved; otherwise untouched) | F4, F8 |
| X14 | `SDICount` field in the hit state (reserved) | M6 §5.1 | **Present** (`GSTN_CombatHitState.cpp`); F8 keeps the wire slot and renames it `SDIDistanceUsed` in a neutral commit | F8 |
| X15 | Tumble contact record and probe of M4 §7b (surface impacts: `SurfaceTypeIndex`, impact outcome Splat/Bounce/BreakSurface) | M6 §6 | **Present** (M4 H11/H11b); verify the record name and that the combat tick reads it at end of F-1 (F0 S2) | F8 |
| X16 | Outcome applier registry with the `AppendEvents` seam; attacker outcome seam (ricochet) | M6 §8 throw hand-off, M4 §3b | **Present** | F7, F12 |
| X17 | `GSTNParts`: `Combat.Part.GrabLocked` projected from `FGSTN_GrabGate`, `UGSTN_PartMath::EffectiveWeight` | CP §7, M4b P9 | **Present** (P9 `GSTNParts.NP.GrabGate.TagProjected`) | F12 (`scenario_grab_gate`) |
| X18 | **`GSTNInteract`** (`IGSTN_InteractCandidateProvider`, `UGSTN_InteractMath::PickTarget`, `Interact.Target.*` root, `Input.Button.Interact`) | IN §3.3, M6 §8 | **Not on `feature`**: exists on the desktop branch `claude/project-thread-v1plhd` (`GSTNInteractCore`, `GSTNInteractRuntime`); lands on `feature` with the planned merge of that branch | F9, F12; fallback in section 4 |
| X19 | `GSTNCombatDebug` / Gameplay Debugger category (M5 E14/E15) | M6 §11 debug | **Not on `feature`** (desktop branch, M5); F13 adds console draws only; the Gameplay Debugger rows are an M5 E19-style hook done after the merge | F13 (optional) |
| X20 | M3 request array (`ActivateEntry` and friends, 4 slots) | M6 §7.0 item 5 | **Present**; F2 grows it to 6 slots with generic kinds (`EnterLinked`, `ExitLink`, `ApplyLinkEffect`, `RemoveLinkEffect`, `LaunchLinked`) | F2, F10 |
| X21 | Lag-compensation history (`RewindActors`, composed transforms) | M6 §7.3 (parked entity hurtboxes), §5.3 | **Present** (M4 H10) | F7, F10 |
| X22 | `UGSTN_WallQueryUtils` multi-probe, `UGSTN_MoverSimStateTracker` | M6 §6 contact probe | **Present** (project module / `GSTNInputNP`); the probe must live in a plugin module: F8 uses the M4 §7b probe already in `GSTNCombatNP` (X15), never the project's | F8 |

Dependency on **M7/M10**: none to build; §7.0 of M6 lists what must stay open (checked in F10 by the `FGSTN_FakeLinkHost` test and in
F14 by the "fake plugin" test: a test-only link type registered from a test module forms and breaks without touching `GSTNLink`).
Dependency on **M5**: none (the new tracks get editor rows later through M5's generic track widgets).

**What can start in parallel (section 7):** F3, F4, F5 are pure and need only X10/X11; F1/F2 need the combat core and the lead's
review; F6 needs F3; F7 needs F1, F2, F4, F6; F9 needs X18 or its fallback; F10 needs F1, F2, F5, F9.

## 3. Checkpoints (in order)

Legend: [module] - [files] - [tags] - [tests] - [acceptance] - [spec] - [size S/M/L]. Every checkpoint also carries the R-B
Independence line and the Blueprint/replaceable-content gates of section 0.

### F0 Spikes (throwaway code on a scratch branch or `Saved/` only; results to `docs/notes/m6-spike-report.md`)

Output: a report with numbers per sub-step, pass/fail against the thresholds, later folded into combat.md §17 by F14.

- **S0 Seam audit (reads only).** Confirm the table of section 2 against the code at the branch HEAD of the day (the desktop merge may
  move things): grep for `FGSTN_SyncFragment`, `EGSTN_CombatHookPoint`, `PickHurtboxIndex`, `FGSTN_HitInboxEntry`, `ExtensionData`,
  `FGSTN_ReactionRequest`, the M3 track registry, the M4 §7b contact record, `GSTNInteract`. Output: table "capability, present,
  location". **STOP and ask** for any X-row whose state is worse than the table says.
- **S1 Link spike (decides §7, M6 §2.1).** Listen host H + clients A, B at 60/15 ms and 2 % loss, then dedicated for comparison;
  pairings H grabber, H grabbed, A grabs B. B linked by a server-side test command (no pipeline). **Variant P** (parked
  `UGSTN_LinkedMovementMode`, zero velocity, presentation and hurtboxes from A's socket as each machine knows it) and **Variant F**
  (B's Mover driven to A's socket each tick). Measure, over 20 s of A walking, dashing and circling, plus forced reconciles on A and on
  B inside the link: `np.PrintReconciles` per client, B's rollbacks/s, presentation error of B's mesh against the server socket
  (p50/p95/max cm) on A's and on B's client **and on the host**, extra bytes/s on B's sync state, host frame time with the watchdog
  stub running. Also record: a queued modifier carrying a tag forces the Mover mode (reuse M4 spike 2.2), and the
  `FinalizeFrame`-time watchdog sees both entities' state of frame F on the server. **Latch pairing**: the parked side is the
  initiator (player) on the socket of a partner that walks, turns and jumps (a big test pawn driven by a server command); same
  measurements, same thresholds, on the player's own client (predicting its own parked state against an interpolated partner) and on
  the observer.
  **Pass for P**: zero unforced reconciles on A and B during the walk; exactly one correction at link start and one at link end per
  client; presentation error p95 ≤ 10 cm on A's client, ≤ 25 cm on B's, max ≤ 60 cm; forced reconciles inside the link converge in
  ≤ 2 frames. **STOP and ask** if P fails: F is the fallback and §7 is redesigned (forward prediction of the partner needs fork patches).
- **S2 Tumble contact probe.** The M4 §7b contact record (X15) read by the combat tick at end of F-1 during `Tumble`, with no
  reconciles, and the Tumble → Walking transition visible one frame later (CLAUDE.md rule 5). Decides the one-frame tech/bounce
  latency of §6. **STOP** if the record is not visible at end of F-1 without a Mover component query.
- **S3 Reverse delivery.** Victim resolution at F+1 posts a `Punish` entry to the attacker's inbox on the server (throwaway handler
  on the hit inbox); attacker client rollbacks per parry (expect 1) and the visible delay of the stun. Run for both parry sources
  once F7 exists; in F0 run it with a fake "always parry" step. **STOP** if the attacker needs more than one correction per parry.
- **S4 Mash through rollback.** Presses counted from the input history, forced reconcile mid-mash: the escape frame must be equal
  on server and client. **STOP** if the history window trimming loses presses across the reconcile.
- **S5 Shield bubble as an extra hurtbox (decides §5.3 contact rule).** A sphere contributed at the attacker's hit frame from the
  victim's replicated shield state (analytic value, tilt), on a victim (a) standing, (b) tilting, (c) draining while held, (d) at the
  minimum radius: rate of attacker predictions disagreeing with the server's block / poke / miss (expect 0 on the server and the owning
  client; on an interpolated victim report the rate and the radius/centre error that causes it), extra hit-detection cost per frame with
  8 shielding entities, and M4's hurtbox selection unchanged with no bubble. **Pass**: zero disagreements for (a), (c), (d) on the owner,
  ≤ 1 per 100 hits on an interpolated victim, cost < 0.1 ms. **STOP** if it fails: the fallback is the contact-point test, which cannot
  block hits that only touch the bubble (owner decision). **Result (2026-10-26, `docs/notes/m6-spike-report.md`)**: pass for (a), (c),
  (d) on every observer; (b) tilting failed on an interpolated victim (2 per 100). **Owner decision 2026-10-26**: the shield has no tilt
  and a fixed size (Rivals of Aether 2 style), so (b) and (d) no longer exist and the bubble stays an extra hurtbox.
- Acceptance: report with numbers; each STOP condition evaluated. Gates F2 (S5), F7 (S3, S5), F8 (S2), F10 (S1), F12 (S4). Size: M.

### F1 `GSTNCombat` follow-up 1: open inbox kinds, `PostToInbox`, entity ref, hit-data fields (neutral)

- Files: `GSTNCombatCore/Public/Sync/GSTN_EntityRef.h` (`FGSTN_EntityRef { uint8 Kind; uint16 Id; uint8 Serial }`, `operator==`,
  null value, `NetSerialize`; kind ids in the central id file next to `GSTNFeatureIds`: `Fighter = 0`, others reserved by name
  for `GSTNMinion`/`GSTNHeld`), `Hits/GSTN_HitData.h` (+ `bParryable = true`, `bDisableDI = false`, `EGSTN_LinkFilter LinkFilter =
  Any`, `FGameplayTag LinkType`, `IsBlockable()` derived from `HitType` through a rules-owned list of non-blockable hit types, default
  `Unblockable`), `GSTNCombatNP/Public/Simulation/GSTN_CombatInbox.h` (`FGSTN_InboxEntry { uint8 Kind; int32 Frame; int32 SenderId;
  FInstancedStruct Payload; }` with the hit entry as kind 0 wrapping today's `FGSTN_HitInboxEntry` unchanged; `FGSTN_InboxKindRegistry`
  (kind id → handler, registered by NP modules like the outcome appliers); `FGSTN_CombatInbox` replacing `FGSTN_HitInbox` with the same
  drain order extended to (Frame, SenderId, Kind, order) and the same rollback trimming), `UGSTN_CombatSimComponent::PostToInbox(TargetId,
  Entry)` (same refusal rule as `PostHit`, which becomes a wrapper).
- Tags: none. Ids: inbox kinds `Hit = 0`, reserved ranges for `GSTNDefence` (`Punish`) and `GSTNLink` (`LinkResponse`, `LinkCommand`) in
  the central id file; feature ids `Defence = 17`, `Link = 18` reserved there too.
- Tests: `GSTNCombat.NP.Inbox.{KindOrderStable, UnknownKindIgnoredWithWarning, TrimOnRollbackAllKinds, PostToInboxRefusedWhereNotTicking}`,
  `GSTNCombat.Core.EntityRef.{RoundTrip, NullAndEquality}`, `GSTNCombat.Core.HitData.{DefaultsKeepM4Content, BlockableDerivedFromHitType}`.
- Acceptance: all M1..M4b tests and `SUITE_DEFAULT` + `SUITE_PARTS` scenarios unchanged with the plugins of M4b on and off (counts and
  logs in the HANDOVER); the hit path's bytes and behaviour identical (the hit kind is the only one registered). Deltas entry in
  `docs/notes/m4-doc-deltas.md` ("M6 follow-up 1"). **Tell the user**: this touches the inbox container every hit goes through.
- Spec: M6 §3 "Inbox types", §7.0 item 1, §1b. Size: M.

### F2 `GSTNCombat` follow-up 2: extra hurtboxes, clock hooks, action veto, outgoing entries, request slots (neutral)

- Files: `GSTNCombatCore/Public/Hits/GSTN_HurtboxTypes.h` (+ `FGSTN_ExtraHurtbox { FGSTN_HurtboxShape Shape; uint8 Priority;
  FGameplayTagContainer IgnoreHitTypes; uint8 Source; }`), `GSTN_HitShapeUtils.h` (`PickHurtboxIndex` over body + extras: extras win
  by priority, an extra ignored for the hit's type is skipped, indices of extras are `0x80 + i`; absent extras = today's rule bit for
  bit), snapshot builder and lag-comp history record carry up to `MaxExtraHurtboxes = 2` per entity (contributed by the snapshot
  contributors of X7 through a new `AppendExtraHurtboxes` seam), the attacker's candidate loop and the victim view expose them;
  `GSTNCombatNP` clock step: a **track-kind hook** `IGSTN_TimelineTrackHook` (registered per track kind) called by the clock with
  the timeline frame and the input history view, returning `{ bStallThisFrame, bStampWindow(start, end, profile) }` so a
  `GSTNDefenceCore` track can stall the timeline (`TimelinePausedFrames++`, the combat clock does not pause) and stamp a window
  without the clock knowing parry; `IGSTN_CombatTickExtension::BlocksActions(Context) → bool` default false, OR-ed into the can-act rule
  before candidates; `FGSTN_HitResolutionContext::OutgoingEntries` (`TArray<FGSTN_InboxEntry>` applied by the victim step after the
  pipeline through `PostToInbox`); M3 request array grown from 4 to 6 slots with the generic kinds of X20 (data only, no handler yet).
- Tags: none.
- Tests: `GSTNCombat.Core.Hurtbox.{ExtraWinsByPriority, ExtraIgnoredByHitType, NoExtraIdenticalToM4, IndexRangeReserved}`,
  `GSTNCombat.NP.Snapshot.ExtraHurtboxInHistory`, `GSTNCombat.NP.Clock.{TrackHookStallsTimelineNotClock, HookStampsWindow}`,
  `GSTNCombat.NP.Tick.BlocksActionsVeto`, `GSTNCombat.NP.Victim.OutgoingEntriesPosted`, `GSTNCombat.NP.Requests.SixSlotsRoundTrip`.
- Acceptance: as F1 (everything of M1..M4b unchanged, both plugin states); the request array's wire size grows only when a new slot
  is used (absent slots one bit each). Deltas entry "M6 follow-up 2". **Tell the user**: this touches M4's hurtbox candidate selection
  and the combat sync state's request serialization (M6 §1b asks for it explicitly).
- Spec: M6 §1b (extension surface), §5.3 "Contact", §3 tracks, §7.0 item 5, §9 context. Size: M.

### F3 `GSTNDefence` plugin skeleton + `GSTNDefenceCore` data, tags, tracks, validators (pure)

- Files: `Plugins/GSTNDefence/GSTNDefence.uplugin` (`EnabledByDefault: false`, dependency `GSTNCombat`, `GSTNInput`), `README.md`,
  `Config/DefaultGSTNDefence.ini`, `Config/Tags/GSTNDefence.ini` (designer leaves only); `Source/GSTNDefenceCore/{Build.cs, Public/
  GSTNDefenceCoreModule.h, Public/GSTN_DefenceTags.h, Private/GSTN_DefenceTags.cpp, Public/GSTN_DefenceSettings.h, Public/Data/
  {GSTN_DefenceRules.h (UGSTN_DefenceRules: bShieldEnabled, FGSTN_ShieldRules, FGSTN_ParryRules, TArray<FGSTN_ParryProfile>,
  FGSTN_TechRules, FGSTN_DIRules, FGSTN_SDIRules, FGSTN_GroundBounceRules { GroundBounceSpeed, GroundBounceSteerDeg, MaxDownedFrames },
  IsDataValid), GSTN_Curve1D.h (FGSTN_Curve1D: quantized piecewise-linear points, integer-safe evaluation, no engine curve),
  GSTN_DefenceKitExtension.h (FGSTN_DefenceKitExtension : FGSTN_KitExtension { Rules, ShieldAbility, FGSTN_TechSet Tech, GetupSet }),
  GSTN_DefenceRulesExtension.h (FGSTN_DefenceRulesExtension : FGSTN_RulesExtension { default Rules }), GSTN_DefenceTracks.h
  (FGSTN_HoldWindowTrack, FGSTN_ParryWindowTrack with ProfileName baked to an index at load)}`.
- Tags (native, `GSTNDefenceCore` for commands/locks, the rest declared here but documented as GAS-owned per M6 §1c): `Combat.State.
  Shielding/ShieldStun/ShieldBroken/ParryWindow/ParryRecovery/Parried/Teching/Knockdown/Tech.Lockout`, `Combat.Mover.Tumble`,
  `Combat.Immunity.Parry/Shield`, `Command.Defence.Shield`, `Input.Lock.Command.Defence`, `Combat.Event.Blocked/Parried/
  ShieldBroken/Teched/KnockedDown`, `Combat.SetByCaller.ShieldDamage`. (Declaring every defence tag in Core keeps GAS and NP free of tag
  definitions; the M6 §1c "owner" column names the writer, not the declaring module. *Deviation to record if the lead prefers the
  split by module.*)
- Tests: `GSTNDefence.Core.Tags.{AllRegisteredAndDocumented, NoForeignRootDeclared}` (no `Combat.Link.*`, `Combat.Part.*`,
  `Interact.*`), `GSTNDefence.Core.Data.{RulesValid, EmptyParryProfilesRejected, ProfileNameBakes, Curve1DMonotonicAndQuantized,
  KitExtensionNoShieldMeansNone, TrackWindowsValidated}`.
- Acceptance: builds with the plugin disabled and enabled; editor loads; `GSTNCombat` tests unchanged; grep gate: Core includes no
  GAS/NP/Mover/Link header.
- Spec: M6 §1b, §1c, §1d, §3 (rules, tracks). Size: M.

### F4 `GSTNDefenceCore` math: shield value and bubble, parry, tech, DI/SDI (pure, goldens)

- Files: `Public/Math/{GSTN_DefenceMath.h (IsShieldAvailable, ShieldValueAt, ShieldCentreAt, MakeShieldBubble (fixed `RadiusCm`, centre
  on the entity's local up, no tilt), ShieldStunFrames, PushbackSpeed, EvaluateParryWindow, ResolveParry → FGSTN_ParryDecision, EvaluateTech,
  GroundBounceVelocity), GSTN_SDIMath.h (NextDisplacement, limiter), GSTN_DIRulesMath.h (ApplyDI rules overload, StrengthCurve,
  ElevationDeltaByElevation, ScaleByVictimTag)}`, `Public/Math/GSTN_DefenceViews.h` (`FGSTN_ParryContext`, `FGSTN_ShieldView`,
  `FGSTN_TechState` plain views used by the NP module and by tests; doubles, identical on every machine).
- Tags: none.
- Tests (goldens from hand values): `GSTNDefence.Core.Shield.{AvailableFlagTimesKit, ValueDrainRegenDelayClamp, RateChangeRestamp,
  RadiusFixedAtAnyValue, CentreArbitraryUp, StunAndPushbackScaleWithDamage}`,
  `GSTNDefence.Core.Parry.{ReleaseAnchorEdges, PressAnchorEdges, MinHeldFrames, ResolveBothSourcesEqualProfile, ShieldSourceFirst,
  MoveWindowEdges, CloseOnParry, HurtboxSelection, NotParryableFlag, ImmunityTagByName, ProjectileFlag}`, `GSTNDefence.Core.Tech.
  {PressInWindow, EarlyPressLockout, MashDoesNotExtend, LatePress, HitstopExcluded, ContactSameFrame, NoTechInHitstun}`,
  `GSTNDefence.Core.DI.{CurveAndVictimScale, MinKnockback, DisableFlag}`, `GSTNDefence.Core.SDI.{DriftUntilMax, MinZone,
  ArmoredBlocked}`, `GSTNDefence.Core.Bounce.SteerFromStick`.
- Acceptance: every function is deterministic for equal inputs (same results in two runs and on a second machine via the harness
  later); no world/actor/component reads.
- Spec: M6 §3 "Math", §5.3, §5.4, §5.5, §6. Size: M.

### F5 `GSTNLink` plugin skeleton + `GSTNLinkCore`: definitions, rules, state types, placers, mash/break evaluators, test double

- Files: `Plugins/GSTNLink/GSTNLink.uplugin` (`EnabledByDefault: false`, dependency `GSTNCombat`, `GSTNInput`), `README.md`,
  `Config/DefaultGSTNLink.ini`, `Config/Tags/GSTNLink.ini`; `Source/GSTNLinkCore/{Build.cs, module, Public/GSTN_LinkTags.h,
  Public/GSTN_LinkSettings.h, Public/Data/{GSTN_LinkTypeDefinition.h (UGSTN_LinkTypeDefinition PrimaryDataAsset `GSTN_LinkType`:
  LinkType tag, Channel (Body/Hand), two FGSTN_LinkRoleDef, FGSTN_LinkBreakRules, ExitPlacement class, RequestRequirements,
  ForbiddenWithBody, bInitiatorPredicts, MaxFrames, DebugColor, IsDataValid), GSTN_GrabRules.h (FGSTN_GrabRules, FGSTN_MashRule),
  GSTN_LinkKitExtension.h (FGSTN_LinkKitExtension : FGSTN_KitExtension { GrabRules, FGSTN_KitLinkAnims per role tag }),
  GSTN_LinkOutcomeTrack.h}, Public/Rules/{GSTN_LinkRule.h (abstract, EditInlineNew, Evaluate(FGSTN_LinkRequestContext) → Accept/Refuse),
  GSTN_LinkRule_Grab.h (requirements, weight, immunity frame, GrabLocked by name)}, Public/Placement/{GSTN_LinkExitPlacer.h,
  GSTN_ExitCollisionQuery.h (IGSTN_ExitCollisionQuery), GSTN_Placer_Ring.h, GSTN_Placer_AtSocket.h}, Public/State/{GSTN_LinkState.h
  (FGSTN_LinkState per channel, FGSTN_LinkActions, FGSTN_LinkRequestContext views), GSTN_LinkMath.h (MashInitialFrames, MashProgress,
  break evaluator, ParkedTransform(partner transform, role def))}, Public/Host/{GSTN_LinkHost.h (IGSTN_LinkHost), GSTN_FakeLinkHost.h
  (FGSTN_FakeLinkHost: holds link states, executes requests synchronously, no NP)}}`.
- **Latch (owner 2026-10-26)**: the role definition already says which role is parked (`FGSTN_LinkRoleDef::bParked`), so a latch is a
  definition whose **role 1 (initiator) is parked** on the partner's socket and whose role 2 is free: `bActsWhileLinked` on the role (no
  action veto, candidates allowed, movement unchanged), `bCollisionWithPartner` false, `SocketOnPartner` on the enemy (a part socket when
  `GSTNParts` is present, by name). `UGSTN_LinkRule_Latch`: accepted only on a victim whose effective weight is **≥ `MinLatchWeight`**
  (the mirror of the grab's `MaxGrabWeight`), tag requirements with their own immunity (`Combat.Immunity.Latch`, bosses that must not be
  ridden), `Combat.Part.GrabLocked` optional by data as for grabs. One latcher per partner in M6 (A19).
- Tags (native, declared in `GSTNLinkCore`): `Combat.Link.Linked/Pending`, `Combat.Link.Type.Grab`, `Combat.Link.Type.Latch`,
  `Combat.Link.Role.Grabber/Grabbed`, `Combat.Link.Role.Latcher/Latched`, `Combat.Immunity.Latch`, `Combat.Link.BreakReason.ShakeOff`,
  `Combat.Link.BreakReason.{Timeout, Distance, OwnDeath, PartnerDeath, Hit, PartnerState, Escape, Release, Throw, Forced}`,
  `Combat.Lock.Link`, `Combat.HitType.Grab/Throw/Pummel`, `Combat.Immunity.Grab`, `Combat.Mover.Linked`, `Interact.Target.Grab`
  (declared here per IN: `GSTNLink` keeps declaring its own target tag), `Command.Link.Grab/Pummel/Release/Throw.Forward/Back/Up/Down`,
  `Input.Lock.Command.Link`, `Combat.Event.LinkFormed/LinkBroken/Throw/GrabEscaped`. Reserved (declared by M7/M8/M10, not here):
  `Type.Mount/Ride/Held/Minion/Drill`, `Role.Carrier/Rider/Holder/HeldObject/Driller/Drilled`.
- Tests: `GSTNLink.Core.Tags.{AllRegisteredAndDocumented, NoForeignRootDeclared}`, `GSTNLink.Core.Data.{DefinitionValid,
  UnknownRoleTagRejected, ForbiddenWithBody}`, `GSTNLink.Core.State.{RoundTrip, SerialIdempotence, TwoChannels}`,
  `GSTNLink.Core.Mash.{InitialFramesByDamage, ProgressAndCap, FlicksCount}`, `GSTNLink.Core.Break.{Timeout, Distance, HitByRole,
  PartnerHitsNeverBreak, PartnerState, Forced}`, `GSTNLink.Core.Placer.{RingOrder, BlockedCandidatesFallback, AtSocketVelocity,
  DeterministicForEqualInputs}`, `GSTNLink.Core.Rule.Grab.{Requirements, Weight, ImmunityFrame, GrabLockedByName}`,
  `GSTNLink.Core.Rule.Latch.{MinWeight, ImmunityLatch, OnePerPartner}`, `GSTNLink.Core.State.ParkedInitiatorTransform` (parked transform
  derived for role 1 as for role 2),
  `GSTNLink.Core.FakeHost.{FormBreakSynchronous, OrphanDetected}`.
- Acceptance: builds with the plugin disabled and enabled; tests run with only `GSTNLinkCore` + `GSTNCombatCore`; a new link type
  definition works with no C++ (a test-only definition asset registered from the test).
- Spec: M6 §1b, §1c, §3 "Links", §7.0, §7.1, §7.5, §7.7, §8 (rules only); owner 2026-10-26 (latch). Size: L (split into F5a data/tags and F5b rules/placers/host if
  a session cannot finish it).

### F6 `GSTNDefenceGAS`: attribute mirror, shield / tech / bounce / get-up abilities, GEs, kit extension consumption

- Files: `Source/GSTNDefenceGAS/{Build.cs (GSTNDefenceCore, GSTNCombatGAS, GameplayAbilities), Public/Attributes/GSTN_DefenceAttributeSet.h
  (ShieldHealth mirror, MaxShieldHealth, ShieldRegenRate, ShieldDrainRate, IncomingShieldDamageMultiplier, GrabResistance; clamps),
  Public/Abilities/{GSTN_GA_Shield.h (child of UGSTN_GA_BaseCombat: startup, one HoldWindowTrack on Input.Button.Shield with
  bEndOnZeroShield, release; out-of-shield options = M3 cancel windows as data; CanExecute refuses where IsShieldAvailable is false or
  value ≤ 0 or Breaking), GSTN_GA_Tech.h (per tech type, Forced start), GSTN_GA_GroundBounce.h, GSTN_GA_GetupRoll.h, GSTN_GA_GetupNeutral.h
  (Press(Jump) requiring Combat.State.Knockdown)}, Public/Effects/{GSTN_GE_ShieldDamage.h (instant, SetByCaller, mirror only),
  GSTN_GE_ShieldRestore.h (request-with-attribute-delta seen by the model), GSTN_GE_ShieldBreak.h (cosmetic tags/cues)},
  Public/GSTN_DefenceLibrary.h (grant/revoke the attribute set and the shield binding from the kit extension + rules flag; the
  loadout builder hook that omits Command.Defence.Shield when the shield is unavailable)}`.
- Tags: none new (declared in F3). Cues (ini): `GameplayCue.Combat.Shield.{Raise, Hit, Break}`, `GameplayCue.Combat.Parry`,
  `GameplayCue.Combat.Tech`.
- Tests: `GSTNDefence.GAS.{AttributeClamp, ShieldBindingOmittedWhenUnavailable, ShieldAbilityRefusesWhenUnavailable,
  TechSetPerKit, GetupNeutralRequiresKnockdown, ShieldRestoreSeenAsRequest}` (ability tests with a fake ASC world, no NP).
- Acceptance: with `bShieldEnabled = false` the ASC gets no attribute set, no shield binding and `Input.Button.Shield` stays free for the
  kit's own binding; abilities never set character state directly (CLAUDE.md rule 2).
- Spec: M6 §4, §1d, §6 (owner defaults). Size: M.

### F7 `GSTNDefenceNP` part 1: fragment, snapshot + bubble hurtbox, clocks, `Parry` and `Shield` steps, punish, events

- Files: `Source/GSTNDefenceNP/{Build.cs (GSTNDefenceGAS, GSTNCombatNP), Public/Sim/{GSTN_DefenceFragment.h (feature id 17: the M6 §5.1
  fields of the shield, move parry window, stuns, tech, knockdown, punish; NetSerialize/ShouldReconcile/Interpolate/ToString; idle =
  defaults = one bit), GSTN_DefenceSnapshotContributor.h (RulesIndex with bShieldEnabled and bKitHasShield, shield stamp/press/release,
  move parry stamps, immunity tags as bits; AppendExtraHurtboxes contributes the bubble with IgnoreHitTypes from the rules'
  non-blockable list), GSTN_DefenceTickExtension.h (prio 110: after clocks: defence clocks, hold stall and parry
  window stamp through the F2 track hook; before candidates: BlocksActions for ShieldStun/ShieldBroken/Punished/Knockdown; tag
  projection), GSTN_DefenceInboxKinds.h (Punish kind handler: ActionState Punished until PunishEndFrame, cancel timeline, hitstop)},
  Public/Pipeline/{GSTN_HitStep_Parry.h (ResolveParry on the snapshot: nullify, hitstop both, meter, event Parried with source,
  LastParryFrame, OutgoingEntries += Punish; defender side by source), GSTN_HitStep_Shield.h (bubble hit: block, shield damage,
  re-stamp lower, shieldstun, pushback through the reaction request seam as a no-tumble knockback, hitstop × multiplier, meter; body
  hit while Raised (only a kit radius that leaves a hurtbox uncovered): Continue as an ordinary hit, no event; break: ShieldBroken state, BreakLaunch up via the reaction request, event)},
  Public/GSTN_DefenceOutcomeApplier.h (state writes at F+1: stamps, mirror requests every 16 frames while changing; AppendEvents)}`.
  `OnParry` cancel condition registered from `LastParryFrame` (X11).
- Tags: none new.
- Tests: `GSTNDefence.NP.Fragment.{RoundTripIdleOneBit, ActiveBytesWithinTable, Interpolate}`, `GSTNDefence.NP.Snapshot.{BubbleContributed,
  NoBubbleWhenUnavailableOrHitstunOrBroken, RulesIndexPerEntity}`, `GSTNDefence.NP.Steps.{ParryBeforeShieldOrder, ShieldBlockNoDamage,
  BodyHitWhileRaisedContinues, BreakLaunchUp, GrabPassesThroughBubble, ModeOffSkipsShieldNotParry, OutgoingPunishPosted}`, `GSTNDefence.NP.
  Tick.{HoldStallsTimelineNotClock, VetoWhileStunned, TagsProjected}`, plus a forced-rollback test on the fragment.
- Harness (`defence_scenarios.py`, listen + dedicated, loss 0 and 2 %): `scenario_shield_block`, `scenario_shield_bubble`,
  `scenario_shield_oos`, `scenario_shield_break`, `scenario_shield_mode_off`, `scenario_grab_shield` (needs F12:
  stubbed with a `Grab` hit type until then), `scenario_parry`, `scenario_parry_move`.
- Acceptance: blocked hit identical on server and both clients (value, radius, stun); parry costs the attacker one rollback and both
  sources give the same outcome and rollback count; with the PvE rules asset nothing of the shield exists and the counter move still
  parries; M4b `SUITE_PARTS` still green with `GSTNDefence` on.
- Spec: M6 §5.1, §5.2, §5.3, §5.4, §9 (steps 5, 6), §11 tests. Size: L (split F7a shield/bubble, F7b parry/punish if needed).

### F8 `GSTNDefenceNP` part 2: landing (bounce, downed, get-up), optional tech, DI rules, SDI drift

- Files: `Public/Sim/GSTN_TumbleLanding.h` (combat-tick rules on the M4 §7b contact record at end of F-1: owner default = Forced
  `UGSTN_GA_GroundBounce` with stick steer, then `Knockdown` state and `KnockdownEndFrame` (0 = never, `MaxDownedFrames`); get-up roll /
  attack / neutral through the existing bindings requiring `Combat.State.Knockdown`; tech only when the kit's `FGSTN_TechSet` is non-empty:
  the §6 state machine (presses from history, lockout, window, type from the stick and the contact normal), success cancels the §7b
  impact outcome, failure resolves the impact first), `Public/Sim/GSTN_DIWiring.h` (the DI rules overload at `HitstopEndFrame` through the
  existing hit-state path; `bDisableDI`), `Public/Sim/GSTN_SDIDrift.h` (per-frame drift velocity on the freeze layered move through the
  existing GE + move-creator path; `SDIDistanceUsed` = the renamed `SDICount` wire slot, X14). Neutral `GSTNCombat` data fields added by
  this checkpoint (deltas entry): `FGSTN_LayeredMove_Freeze::DriftVelocity`, `FGSTN_ReactionRequest::bNoTumble` (used by F7's pushback),
  `SDICount → SDIDistanceUsed`.
- Tags: none new.
- Tests: `GSTNDefence.NP.Landing.{BounceSteered, DownedThenGetupRoll, GetupAttackFromSlot, JumpGetsUp, MaxDownedFrames, TechSuccessCancelsImpact,
  TechFailureResolvesImpactFirst, NoTechSetNoLockout}`, `GSTNDefence.NP.DI.RulesOverloadApplied`, `GSTNDefence.NP.SDI.{DriftSweptByMover,
  MaxDistancePerHit, ThrowBlocksSDI}`.
- Harness: `scenario_landing_bounce` (new: bounce direction follows the stick, downed, roll, jump get-up), `scenario_tech_ground`,
  `scenario_tech_wall` (kit with a tech set), `scenario_sdi` (rules asset values), re-run `scenario_hit_*`/`impact_*` of M4.
- Acceptance: identical decisions after forced reconciles; a wall tech works on a wall-mode wall; with no tech set the shield button
  during tumble does nothing and starts no lockout; M4 impact scenarios unchanged.
- Spec: M6 §6 (owner revision), §5.5, §9 step 9. Size: M.

### F9 `GSTNLinkGAS`: registry, requirement checks, interact candidate provider, anims by role

- Files: `Source/GSTNLinkGAS/{Build.cs (GSTNLinkCore, GSTNCombatGAS; GSTNInteractCore when X18 is on the branch), Public/
  GSTN_LinkRegistry.h (UGSTN_LinkRegistry: every UGSTN_LinkTypeDefinition from the asset manager, type tag ↔ uint8 index from a stable
  table (sorted by tag name), role tags, channel, reason table with plugin-appended reasons; definitions whose tags are unregistered are
  reported by IsDataValid and skipped), Public/GSTN_LinkRequirementLibrary.h (tag requirement checks, Combat.Lock.Link), Public/
  GSTN_LinkInteractProvider.h (IGSTN_InteractCandidateProvider: grabbable entities inside InteractReachCm and the facing arc, with
  GrabLocked/immunity/weight pre-filter so the probe never projects Interact.Target.Grab on an ungrabbable target), Public/
  GSTN_LinkAnimSelection.h (FGSTN_KitLinkAnims per role tag → the anim layer of the role, consumed by the M3 anim pattern)}`.
- Tags: none new (F5 declared them).
- Tests: `GSTNLink.GAS.Registry.{IndicesStableAcrossLoadOrder, UnknownTagSkipped, ReasonTableAppend}`, `GSTNLink.GAS.Interact.
  {ProviderFiltersUngrabbable, OneTargetTagPerFrame}` (the second with `GSTNInteract`; without it, the fallback projection of section 4).
- Acceptance: a test-only link type registered from a test module gets an index and roles without touching `GSTNLink` (M6 §7.0 item 3).
- Spec: M6 §7.0 items 3, 4, §8 (context), IN §3.3. Size: S.

### F10 `GSTNLinkNP` part 1: fragment, parked mode, formation through the inbox, presentation transform, debug draw

- Files: `Source/GSTNLinkNP/{Build.cs (GSTNLinkGAS, GSTNCombatNP, Mover), Public/Sim/{GSTN_LinkFragment.h (feature id 18: FGSTN_LinkState
  × 2 channels + FGSTN_LinkActions + GrabImmunityEndFrame + MashProcessedFrame; idle one bit), GSTN_LinkSnapshotContributor.h (Link.Role,
  GrabImmunityEnd, immunity bits; anchor substitution of a parked role's hurtbox transform from the partner's snapshot transform × socket ×
  offset, deterministic, baked socket only), GSTN_LinkTickExtension.h (prio 120: after inbox drain: nothing; after clocks: link tick
  (request timeouts, role input flags, PendingBreak on Release); before candidates: BlocksActions for Role.Grabbed and no candidates;
  tag projection: Combat.Link.* from Role, Interact.Target.Grab through the provider; a role with bActsWhileLinked gets no veto and
  keeps its candidates: the latched enemy fights on), GSTN_LinkInboxKinds.h (LinkResponse and
  LinkCommand handlers: formation at F+2 on the initiator, break/place at F+1 on both sides, Serial idempotence)}, Public/Pipeline/
  {GSTN_HitStep_GrabImmunity.h, GSTN_HitStep_LinkRequest.h (Grab: rule chain of the definition → accept: target Link written, ActionState
  Linked, EnterLinked request, cancel timeline, OutgoingEntries += LinkResponse{accepted}; refuse: LinkResponse{refused}; Throw/Pummel:
  validate Serial/partner else Stop)}, Public/Movement/{GSTN_LinkedMovementMode.h (zero velocity, no gravity, no collision response, no
  input; parked transform = last real position), GSTN_LinkedModifier.h (carries Combat.Mover.Linked, queued once), GSTN_LinkedModeTransition.h},
  Public/GSTN_LinkPresentationComponent.h (GetLinkedTransform() on this machine: mesh attachment to the partner's socket, capsule not
  attached; camera and aim consumers are the owner-OK item of section 6), Public/GSTN_LinkDebugDraw.h (gstn.Combat.Debug.Links 1)}`.
- Tags: none new.
- Tests: `GSTNLink.NP.Fragment.{RoundTripIdleOneBit, TwoChannelsBytes, Interpolate}`, `GSTNLink.NP.Formation.{AcceptedAtF1ResponseAtF2,
  RefusedWhiffsNoHitstop, SimultaneousFirstWins, ChainRefused, PredictedClearedOnTimeout, SerialIdempotentAfterRollback,
  LatchParksInitiator}`, `GSTNLink.NP.Parked.{ModeEnteredByTag, HurtboxFromPartnerSocket, NoInputNoGravity, InitiatorFollowsMovingPartner}`,
  `GSTNLink.NP.Tick.{GrabbedHasNoCandidates, LatchedKeepsCandidates, TagsFromRole}`.
- Harness (`link_scenarios.py`, three pairings, listen + dedicated, loss 0/2 %): `scenario_link_spike` (the S1 measurement as a
  permanent scenario with its thresholds), `scenario_link_rollback` (forced reconciles on each client over formation frames).
- Acceptance: S1 thresholds hold with the real mode; formation at F+1/F+2 on host and clients alike; a client with `GSTNLink` disabled
  joining a session with it enabled is refused at connect by the fragment registry (existing rule).
- Spec: M6 §7.1, §7.2, §7.3, §7.4, §9 (steps 3, 4). Size: L.

### F11 `GSTNLinkNP` part 2: watchdog, break rules, exit placement, `LinkBreakOnHit`

- Files: `Public/GSTN_LinkWatchdog.h` (server world subsystem at the `FinalizeFrame`-time publication point: per Serial-matched pair
  evaluate break rules, honour PendingBreak, detect orphans after a 3-frame grace, ask the placer for each side through the real
  `IGSTN_ExitCollisionQuery` (capsule sweep, server only), post `LinkCommand{Break, Serial, Reason, Exit, ExitVelocity}` stamped F to both
  inboxes; events LinkBroken and GrabEscaped), `Public/Pipeline/GSTN_HitStep_LinkBreakOnHit.h` (PendingBreak per role rules),
  `Public/Movement/GSTN_LinkExitRequest.h` (teleport as a Mover instant move through the existing GE path, modifier removal).
- Tags: none new.
- Tests: `GSTNLink.NP.Watchdog.{BreakAtF1BothSides, OrphanAfterGrace, StaleSerialIgnored, OnePathForPipelineBreaks}`, `GSTNLink.NP.
  Break.{Distance, Timeout, Death, PartnerState, HitByRole}`, `GSTNLink.NP.Exit.{NeverInsideGeometry, DeterministicForEqualInputs}`.
- Harness: `scenario_link_break_distance` (server moves the grabber away; kill of either side), `scenario_grab_immunity` (regrab refused
  for N frames after a break), `scenario_link_rollback` extended over break frames.
- Acceptance: breaks only through the watchdog (two frames after the cause, one correction on clients); both sides clear `Link` the same
  frame; the host as grabber or grabbed breaks with the same frame rules.
- Spec: M6 §7.5, §7.6, §7.7, §9 step 8. Size: M.

### F12 Grabs: commands, pummel, throws, mash, immunity, grab gate (closes M4b's deferred grab scenario)

- Files: `GSTNLinkCore` slot set extension (`UGSTN_StandardSlots` + the grabber slots of M6 §8 as data: Pummel, Throw.Forward/Back/Up/Down,
  Release = Press(Interact) requiring Role.Grabber; Grab = Press(Interact) requiring Interact.Target.Grab; **latcher slots**:
  `Command.Link.Latch.Attack` = Press(Attack) requiring Role.Latcher (a timeline whose hitbox has `LinkFilter LinkedPartnerOnly`, e.g. an
  impale: heavy penetration on the part under the socket, data of the kit), Release = Press(Interact) requiring Role.Latcher; the latch
  itself is the same tap on a target that projects `Interact.Target.Grab`: the provider picks grab or latch by the victim's weight, so
  one button serves both), `GSTN_LinkShakeOff.h` (the latched enemy breaks the link by its own ability: a Forced request that sets
  `PendingBreak` with reason `ShakeOff`; the watchdog does the rest; mash on the enemy side is not built, AI decides), `GSTNLinkNP/Public/Sim/
  GSTN_LinkThrow.h` (FGSTN_LinkOutcomeTrack key at the release frame: direct `Throw` hit entry to the partner's inbox, own Link cleared with
  outcome Throw, Placer_AtSocket, GrabImmunityEndFrame on the partner through the break command, grabber cooldown), `GSTN_LinkMash.h` (link
  tick: raw press events and flicks newer than MashProcessedFrame reduce EndFrame; initial EndFrame from FGSTN_MashRule × GrabResistance),
  pummel = `LinkFilter LinkedPartnerOnly` + `bNoReaction` hit data (no hitstun/knockback, meter and damage through the pipeline,
  PummelFreeFrames), `LinkEstablished` buffer event + the grabber's cancel window into `Command.Link.*`; `ThrowHit.ExtensionData` carries
  an opaque reserved slot for M10's `FGSTN_ThrownBodyExtension` (ignored with a warning when `GSTNHeld` is off).
- Tags: none new.
- Tests: `GSTNLink.NP.Grab.{PummelKeepsLink, ThrowDirectHitNoOverlap, ThrowSkipsParryShield, MashEscapeFrameDeterministic,
  ImmunityAfterReleaseAndThrow, GrabberCooldown, WeightGate, GrabLockedByNameWithParts, GrabLockedAbsentWithoutParts}`,
  `GSTNLink.NP.Latch.{WeightPicksLatchOverGrab, AttackHitsPartnerOnly, ShakeOffBreaksThroughWatchdog, LatchedEnemyKeepsActing}`.
- Harness: `scenario_grab_throw`, `scenario_grab_mash` (forced reconcile mid-mash), `scenario_grab_shield` (real grab through the bubble),
  `scenario_grab_interact` (tap = grab, tap as grabber = release; the hold/pickup rows only with `GSTNHeld`, else asserted absent),
  `scenario_grab_gate` (heavy enemy refused while `GrabLocked` or above `MaxGrabWeight`, accepted after the parts break / weight falls; with
  and without `GSTNParts`: this is M4b's deferred `scenario_part_grabbable`), `scenario_throw_at_enemy` only as "plain M6 launch with
  `GSTNHeld` off" (the hand-off itself is M10), `scenario_latch_big_enemy` (a heavy test pawn above `MinLatchWeight` walks and turns
  under a server command: the player's tap latches, the player rides the socket on host and clients within the S1 thresholds, the
  enemy keeps moving and attacking, the latch attack hits only the partner (and, with `GSTNParts`, the part under the socket), release
  and shake-off break through the watchdog, no unforced reconciles; three pairings).
- Acceptance: all link states equal on host and clients after pummels and a throw; escape on the predicted frame; grab passes through
  a shield; M4b's P9 grab gate closes.
- Spec: M6 §8, §7.0 item 5 (actions block), §9 steps 3, 4, M10 §6b (reserved). Size: L.

### F13 Content, tools, debug

- Content (plugin-owned under `Plugins/GSTNDefence/Content` and `Plugins/GSTNLink/Content`, soft refs, LFS): `DA_DefenceRules_Test`
  (PvP, `bShieldEnabled = true`, short numbers: shield 100, drain 0.4/frame, perfect-shield window 5, tech window 20, lockout 40) and
  `DA_DefenceRules_TestPvE` (`bShieldEnabled = false`), `GA_Shield_Test`, `GA_CounterTest`
  (ParryWindow + OnParry cancel), tech set (InPlace, Roll, WallTech), bounce/get-up set, `DA_LinkType_Grab`, grab move, pummel, four
  throws, `FGSTN_MashRule` test values (grab base 90 frames). Built by `Tools/GAS/setup_defence_link_content.py` (idempotent, backs up,
  same style as `setup_gas_assets.py --parts-content`). **Shared test kit/character edits need the owner's OK (section 6)**: until
  then the harness attaches the kit extensions to the test character at runtime like `attach_part_set_to_actor` does for parts.
- Debug: `gstn.Combat.Debug.Shield 1` (bubble sphere, value bar, parry window colour by source), `...Links 1`
  (line between partners, socket gizmo, serial, remaining frames), `...Tech 1` (window/lockout bar), console-only; the Gameplay Debugger
  rows follow M5's category (X19) after the desktop merge.
- Validators: `IsDataValid` on rules and link definitions already exist (F3, F5); a `validate_defence_content.py` pass like M4b's.
- Tests: `GSTNDefence.Content.SampleAssetsValid`, `GSTNLink.Content.SampleAssetsValid` (editor-world load + validate).
- Acceptance: every scenario of F7–F12 runs from this content; the content script reruns to identical hashes.
- Spec: M6 §11 (content, debug). Size: M.

### F14 Full sweep, byte table, docs commit

- Sweep: `SUITE_DEFENCE` + `SUITE_LINK` in listen (host + 2 clients, three pairings for links) and dedicated + 2 clients, 60/15 ms, with and
  without 2 % loss; `SUITE_DEFAULT` + `SUITE_PARTS` in the four plugin configurations (neither, Defence, Link, both); the "fake plugin"
  test (a test-only link type and a test-only defensive step from a test module); byte table of M6 §10 asserted from the fragment tests
  (idle ≈ 1–2 B, active ≤ 27 B, link channel 21 B raw); host bytes/s with and without links from the spike scenario; the grep gate on
  `.Build.cs` files.
- Docs commit (the only `docs/` edit): combat.md §12b (links, as built), §17 (spike results pointer to `docs/notes/m6-spike-report.md`),
  §18 milestone row M6 with status; testing.md (the two suites, the content script); CLAUDE.md one line per plugin in "Where things
  are"; both READMEs "How to extend" (M6 §14) and "Extending in Blueprint"; `docs/notes/m6-doc-deltas.md` closed with the plan corrections.
- Acceptance: M6 §12 items 1–8 verified except the M10 hand-off (7b second half) and M7/M8 users; HANDOVER with every count from a named log.
- Size: M.

## 4. Spikes first and stop-and-ask conditions

| Stop condition | Where | Why |
|---|---|---|
| Any X-row of section 2 worse than audited (seam missing or reshaped) | F0 S0, F1, F2 | M6 must not edit the combat core beyond the two neutral follow-ups; ask for a lead-approved follow-up or a plan change |
| Variant P fails the S1 thresholds | F0 S1 | §7 is redesigned around forward prediction of the partner (fork patches, combat.md §12.7): owner decision |
| Contact record not readable at end of F-1 without a component query | F0 S2 | Would need a Mover query from the combat tick (CLAUDE.md rule 1) |
| Parry needs more than one correction on the attacker | F0 S3 | Reverse delivery design (§5.4) changes |
| Presses lost across a reconcile during a mash | F0 S4 | History window trimming change in `GSTNInput`, another plugin's owner |
| Bubble disagreement above thresholds or cost ≥ 0.1 ms | F0 S5 | Fallback contact-point test cannot block bubble-only touches: owner decision |
| A follow-up (F1/F2) cannot be made neutral (changes an M4 count, byte or scenario) | F1, F2 | Lead OK as a non-neutral change, or a different seam |
| `GSTNInteract` not on `feature` when F9 starts | F9 | **Fallback**: `GSTNLinkGAS` projects `Interact.Target.Grab` itself from its own provider (nearest grabbable in reach, facing arc), behind a `bUseInteractPlugin` setting; swapped to the plugin's probe when it lands, scenario `grab_interact` keeps its asserts |
| A scenario needs `IsServer()` or a topology branch to pass | F7–F12 | Design smell (CLAUDE.md rule 1, M6 §2.1) |
| Camera follow of the linked transform needs project camera code | F10 | Owner OK (section 6); until then the camera keeps its target and the scenario notes it |

## 5. Assumptions for unanswered owner questions

OQ ids refer to section M6 of `open-questions-m4-m10.md`; where the spec marks *Proposal* without an OQ id the assumption is listed by
name. A = assumption id used in the commit bodies.

| OQ / item | Assumption planned on | Affects | Cost to change |
|---|---|---|---|
| M6.1–M6.3, M6.6, M6.7 (shield) | **Answered/narrowed by the owner**: PvP-only Smash-style bubble, data per kit (M6 revisions) | F3, F6, F7 | done |
| M6.8 (tech) | **Answered 2026-10-23**: no tech by default; bounce + downed + get-ups; tech pluggable | F6, F8 | done |
| A1 Parry anchor and window | `Anchor = Release`, `WindowFrames = 5`, `MinHeldFramesBeforeRelease = 0` (recalled Ultimate values, unverified) | F4, F13 | Low: data |
| A2 Shield numbers | Max 100 in test content (spec recalls 50), drain so a full hold ≈ 5 s, regen about half as fast, `RecoverFractionAfterBreak = 0.6`, break launch up | F13 | Low: data |
| A3 Attacker not pushed back on block | Only the defender gets pushback in this version | F7 | Low: one more reaction request |
| A4 Grab prediction | Initiator predicts hitstop + `Pending` flag only; no predicted role change for grabs (M7 mount may) | F10 | Medium: predicted role needs a rollback-safe presentation path |
| A5 Link variant | Parked (P) unless S1 fails | F0, F10 | High if F: fork patches |
| A6 Interact tie rule (M6.22, M7 §7) | Nearest to facing, ties by `InteractPriority` (Grab, Pickup, Mount); grab fires on the press | F9, F12 | Low: data of the probe |
| A7 SDI as held drift | Per-frame drift velocity on the freeze layered move, swept by Mover, `MaxDistanceCmPerHit`, not allowed while armored or on throws | F8 | Low: data; Medium if the owner wants direction events (M1 `Mash` condition) |
| A8 Move parry window in hitstop | Window length in combat frames (hitstop consumes it); `bWindowPausesInHitstop` not built | F7 | Low: one flag |
| A9 Hold window stall | Stalls the timeline frame only; the combat clock, hitstop and meters run | F2, F7 | Low |
| A10 Supers vs shield | `Unblockable` per hit; finishers unblockable, setup hits blockable with large `ShieldDamage`; parryable unless `bParryable = false` | F13 content | Low |
| A11 Knockdown bonus and auto get-up | `MaxDownedFrames = 0` (never) by default; `NoTechKnockdownBonusFrames` only with a tech set | F8 | Low |
| A12 Co-op rescue (teammate hit frees a grabbed player) | Not built; `bPartnerHitsBreak = false`, other attackers' hits break by `MinKnockbackToBreak` | F11 | Low: a break rule |
| A13 Thrown-enemy hand-off | Reserved extension slot only; plain launch without `GSTNHeld` | F12 | Medium in M10, none here |
| A14 Tag declaration module | All defence tags declared in `GSTNDefenceCore`, all link tags in `GSTNLinkCore` (M6 §1c names the writer as owner) | F3, F5 | Low |
| A15 Request slots | Grown 4 → 6 in `GSTNCombat` with generic kinds (M6 §7.0 item 5) | F2 | Low |
| A16 Gameplay Debugger rows | After the desktop merge (M5 category); console draws in F13 | F13 | Low |
| A17 Bubble ignore list | Non-blockable hit types = `Unblockable`, `Grab`, `Throw`, from a rules-owned list (not a flag on each hit) | F1, F7 | Low |
| A18 Shield mirror cadence | Mirror written on events and every 16 frames while changing | F7 | Low |
| A19 Latch (owner 2026-10-26) | One latcher per partner; the latch is the same interact tap, chosen by the victim's weight (`≥ MinLatchWeight` latch, `≤ MaxGrabWeight` grab, neither = refused); the latched enemy keeps acting, breaks by `ShakeOff` (its own ability), `Release`, `Timeout`, death; the latcher's attacks hit only the partner; what an impale does to a part is `GSTNParts` data (penetration), not link code | F5, F10, F12 | Low: data on the definition and the rules; several latchers on one boss is an M8 item (`MaxPartnersPerChannel`) |
| A20 Latch and minions | Big enemies that are M8 minions latch through M8's hit flow once `FGSTN_EntityRef::Kind = Minion` exists; in M6 the test partner is a heavy fighter pawn | F12, M8 | Medium: M8 wire |

## 6. Blueprint and shared-asset edits that need the owner's OK before they happen

**Status: all items below approved by the owner on 2026-10-26 10:05Z** ("per ora tutto ciò che hai messo nella sezione 6 è a posto").

- Enabling `GSTNDefence` and `GSTNLink` in the `.uproject` (both `EnabledByDefault: false`); until then enabled locally for tests, never
  committed (as with `GSTNParts`).
- The two `GSTNCombat` follow-ups (F1, F2): the inbox container and the hurtbox candidate selection every hit goes through, and the request
  slot serialization. Neutral by construction, but M6 §1b asks to tell the user.
- Adding the defence/link steps to the test `UGSTN_CombatRules` asset used by the M4/M4b scenarios (order of M6 §9), and the defence/link
  kit extensions + grabber slots to the shared test kit/character (`BP_GASNPP_TestCharacter`, `KIT_Test_*`, the hurtbox set). Until then
  the harness attaches them at runtime (F13).
- `Input.Button.Interact` in `Config/Tags/GSTNCommands.ini` (designer tag shared with M7/M10) and the IMC entry; listing in
  `Config/DefaultGame.ini` `AbilitySystemMappingContexts` if a new mapping context is needed.
- Camera follow of `GetLinkedTransform()` for a parked player (project camera code / `GSTNCamera` on the desktop branch) and the
  hitstop wall-lock interplay (M6 §7.3, §15).
- Which rules asset (PvP/PvE) the harness and the sandbox use by default; the per-game-mode selection is M12's.
- Changes to project Blueprints, maps, `BS_GASNPP_Test`, root `Config/*.ini`: none planned (tags in plugin ini).
- Docs: M6/combat.md edits only in F14.

## 7. Risks and parallelisation hints

| Risk | Mitigation |
|---|---|
| F1/F2 are not neutral in practice (bytes, counts, scenarios) | Hard gate: M1..M4b counts and scenarios identical, both M4b plugin states; the M4b follow-up procedure (deltas entry, lead OK) |
| Parked approach quality on interpolated observers | S1 thresholds; `scenario_link_spike` stays in the suite |
| Watchdog frame visibility / NP id order | S1 records it; if an order effect appears, breaks are stamped F+1 (M6 §15) |
| Bubble edge mispredicted on interpolated victims | S5 rate; attacker reads the snapshot only; presentation predicted, state corrected once |
| Two optional plugins coupling through code | Build.cs grep gate; four-configuration build; tags read by name |
| Desktop branch merge moves seams (M5, interact, animation) | S0 re-audit on the day's HEAD; X18/X19 fallbacks |
| Balance (perfect-shield window, mash lengths, break stun) | Everything is data in two rules assets; F13 content keeps short test numbers; M5 validators later |
| Fabricated or unverified test results in handovers | Section 0 rule: counts only from named logs; the lead verifies mtimes and logs after each session |
| Camera / project code touched by links | Owner-OK item; scenario notes the missing follow until then |

**Parallel threads (separate sessions or machines can take at once):**

- Thread A (pure, starts now, no machine conflicts beyond a build): F3 → F4; F5 (F5a, F5b) beside them.
- Thread B (combat core, lead review): F0 S0 → F1 → F2 → F0 S5 (needs F2) → S1/S2/S3/S4 as throwaways.
- Thread C (defence NP): F6 → F7 → F8, after A and B.
- Thread D (link NP): F9 → F10 → F11 → F12, after A and B; F10 waits for S1.
- Late: F13 beside C/D once content types exist; F14 last.
- Serial constraints: F1 before F2 (inbox before outgoing entries); F2 before F7 (bubble seam, clock hook); F4 before F7 (math before
  steps); F5 before F9 before F10; F10 before F11 before F12; F7's `scenario_grab_shield` completes only after F12.
- Desktop sharing (process, not plan): one editor and one build at a time with the desktop thread; the holder says when the machine is
  free; the merge of the desktop branch into `feature/gas-npp-5.8` comes first, so that F0 S0 audits one HEAD.

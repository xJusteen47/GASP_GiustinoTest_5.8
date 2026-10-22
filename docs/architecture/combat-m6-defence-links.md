# Combat M6 — Defence and links (implementation specification)

Part of the combat design ([combat.md](combat.md)): read §5.2–5.6, §9, §12, §12b, §17 there, the outline in
[combat-m5-m9.md](combat-m5-m9.md), and the M1–M4 specifications; M6 extends the M4 pipeline, inbox, hitstun and DI
([combat-m4-hits.md](combat-m4-hits.md)). Section numbers below are local to this file. Items marked *Proposal*
resolve an *Open* question of the outline and are for the user to confirm; the matching questions are in
[open-questions-m4-m10.md](open-questions-m4-m10.md) (section M6).

> **Conventions:** "move" means a **timeline ability** (`UGSTN_GA_BaseCombat`, combat.md §11). Frames are NPP fixed-tick
> frames (62.5 Hz). "End of F-1" is the state the combat tick reads (combat.md §5.2). All new timers are **frame stamps**
> in the combat sync state (combat.md §5.5), never GE durations.

> **Revision 2026-10-22 (owner direction, [game-design-direction.md](game-design-direction.md) §3, §4, §6, §6b; it wins over this
> file):** (1) **shield and dodge are kit-dependent**: the shield below is the **default kit's reference implementation**, data per kit
> (§1d); dodge is M4 §1b. (2) The **grab button is the general interact button**, tap/hold variants through `GSTNInput` conditions
> (§8). (3) **Grabbability** depends on weight/size class and on broken parts (`Combat.Part.GrabLocked` from `GSTNParts`) (§8).
> (4) A grabbed enemy can be **thrown at other enemies**: the thrown body is a hitting projectile owned by M10 §6b (§8). (5) Tech
> failure uses the **surface-impact rules of M4 §7b** (§6). (6) Reference topology is a **listen server**: the host is one of the
> link partners in the spike and the harness (§2, §11). All new choices are *Proposal*.

## 1. Scope

**Goal:** the defensive half of the platform-fighter ruleset in 3D, plus the generic link system that grabs use now and
piggyback (M7) and held objects (later) reuse. In: shield (directional, `ShieldHealth`, shieldstun, break), parry, tech
(ground, wall, roll: a tech ability, never a movement mode) with anti-mash lockout, knockdown, SDI/DI rules asset, the link system (spike, state, request/
accept, parked movement mode, watchdog, exit placement, break rules, debug draw), grabs (grab, pummel, throws,
mash break, immunity), and the harness/tests for all of it.

Shield and the defensive options are the **default kit's reference implementation** (§1d); other kits swap them by data.

Out: mount/dismount, carry load, pair moves, riding animation (M7); minions as defenders or grabbers (M8); editor UI
for the new tracks (M5 gets them later through the generic track widgets); get-up attacks beyond one neutral get-up
(*Proposal* in §13); reflecting projectiles (field reserved, §3).

## 1b. Plugin and module boundaries

**Rule (project owner):** each plugin must be usable on its own. M6 therefore adds **two optional plugins** beside
`GSTNCombat`, both depending on it and never the reverse, and gives `GSTNCombat` a small generic extension surface
instead of M6-specific fields. (Section 5.1's fields live in these plugins as extension fragments; the table there says
*what*, this section says *where*.)

| Plugin / module | Contents | May depend on | Must NOT depend on it |
|---|---|---|---|
| `GSTNCombat` / `GSTNCombatCore`, `GSTNCombatGAS`, `GSTNCombatNP` (existing) | **Extension surface added by M6** (generic, no defence or link knowledge): `FGSTN_SyncFragment` (instanced-struct fragments in the combat sync state, each with `NetSerialize`/`ShouldReconcile`/`Interpolate`/`ToString`, registered by type id, absent = zero bytes); `IGSTN_CombatTickExtension` (hooks at fixed points of the §5.2 order: after inbox drain, after clocks, before candidates, tag projection); `FGSTN_InboxEntry` as an open `FInstancedStruct` payload with a handler registry (the `Hit` kind stays here); `PostToInbox`; pipeline steps already data (`UGSTN_CombatRules::Pipeline`); `UGSTN_TimelineTrack` kinds registered by extensions; snapshot extension data (`FGSTN_SnapshotExtension`); `UGSTN_HitData::ExtensionData` (`FInstancedStruct`, carries `LinkType`, `LinkFilter`, `bParryable`...); `FGSTN_EntityRef` (neutral entity reference, `GSTNCombatCore`, §7.0) | as today | `GSTNDefence`, `GSTNLink` |
| `GSTNDefence` / `GSTNDefenceCore` (pure) | `FGSTN_ShieldRules/ParryRules/TechRules/DIRules/SDIRules`, `UGSTN_DefenceRules`, `UGSTN_DefenceMath`, `UGSTN_SDIMath`, `FGSTN_HoldWindowTrack` data, `FGSTN_Curve1D` | Core, GameplayTags, **GSTNInputCore**, **GSTNCombatCore** | GAS, NP, `GSTNLink` |
| `GSTNDefence` / `GSTNDefenceGAS` | `UGSTN_DefenceAttributeSet`, `UGSTN_GA_Shield`, `UGSTN_GA_Tech`, GEs, defence native tags | GSTNDefenceCore, GSTNCombatGAS, GameplayAbilities | NP, `GSTNLink` |
| `GSTNDefence` / `GSTNDefenceNP` | defence sync fragment, pipeline steps `Parry`/`Shield`, tech state machine, `FGSTN_TumbleContactModifier`, SDI/DI wiring, `Punish` inbox kind, snapshot extension | GSTNDefenceGAS, GSTNCombatNP | `GSTNLink` (it only *reacts* to `Combat.State.*` and the generic `Linked` tag, §1c) |
| `GSTNLink` / `GSTNLinkCore` (pure) | `UGSTN_LinkTypeDefinition`, `FGSTN_LinkBreakRules`, `FGSTN_MashRule`, `FGSTN_GrabRules`, `UGSTN_LinkExitPlacer` + `IGSTN_ExitCollisionQuery`, `FGSTN_LinkOutcomeTrack` data, mash/break evaluators, `FGSTN_LinkState`/`FGSTN_LinkActions` as value types, `UGSTN_LinkRule` (per-link-type request rule, §7.0), `IGSTN_LinkHost` (what a link needs from its host; implemented by the NP module and by the `FGSTN_FakeLinkHost` test double) | Core, GameplayTags, GSTNInputCore, GSTNCombatCore | GAS, NP, `GSTNDefence` |
| `GSTNLink` / `GSTNLinkGAS` | link native tags (§1c), `UGSTN_LinkRegistry` (open registry, §7.0), link requirement checks on tags | GSTNLinkCore, GSTNCombatGAS | NP, `GSTNDefence` |
| `GSTNLink` / `GSTNLinkNP` | link sync fragment, `LinkRequest`/`GrabImmunity`/`LinkBreakOnHit` steps, `UGSTN_LinkedMovementMode` + modifier + transition, `UGSTN_LinkWatchdog`, `UGSTN_LinkPresentationComponent`, link inbox kinds, debug draw | GSTNLinkGAS, GSTNCombatNP, Mover | `GSTNDefence` |
| (M7) `GSTNPiggyback`; (M10) `GSTNHeld` | mount link type, pair moves; held objects. They register link types, rules and actions into `GSTNLink` (§7.0) and declare their own link/role tags (§1c) | `GSTNLink` (+ `GSTNCombat`) | each other |

- **Cross-plugin coupling goes through tags and data, not types.** Grab hits ignore shields because the `Shield` step
  (in `GSTNDefence`) skips hit types whose asset says `bBlockable = false`; `GSTNLink` sets that on its hit types.
  Defence reads "can this victim act" from the generic `Combat.State.*` tags and from `GSTNCombat`'s can-act rule (which
  the link fragment feeds through `IGSTN_CombatTickExtension::BlocksActions`). Neither knows the other's classes.
  A grab-while-shielding rule is data in `UGSTN_CombatRules` (step order), where both plugins' steps meet.
- **Enable/disable independently**: each plugin is a separate `.uplugin` with `"EnabledByDefault": false`, listed as a
  dependency of the *game's* plugin list only when wanted; `GSTNCombat` loads and runs with neither (the hit pipeline of
  M4 is unchanged). With only `GSTNDefence`, grabs do not exist; with only `GSTNLink`, no shield or tech exists and
  `Tumble` simply ends in M4's `Actionable`. `UGSTN_CombatRules::Pipeline` lists steps by class: a step class from a
  missing plugin is reported by the rules asset's `IsDataValid` (and skipped with a log) rather than crashing.
  Sync fragments of a disabled plugin are not registered, so the combat state shrinks to M4's size.
- **Kit/content independence**: defence and link assets are standalone data assets referenced from kits through the
  generic `FGSTN_KitExtension` list (`FInstancedStruct`; M3 kit gains it as part of the extension surface), so a kit
  without a shield simply has no entry.
- **Isolated tests**: `GSTNDefence.Core.*` and `GSTNLink.Core.*` Automation tests run with only that plugin plus
  `GSTNCombatCore` loaded (no GAS, no NP, no world), exactly the Core test convention of testing.md; fake contexts and a
  fake `IGSTN_ExitCollisionQuery`. The NP modules are exercised by the harness scenarios of §11 grouped per plugin
  (`scenario_defence_*`, `scenario_link_*`): a run with `GSTNLink` disabled must keep every defence scenario clean and
  vice versa. A CI check greps module `.Build.cs` files: Core/GAS modules must not list NP modules, and no module of one
  optional plugin may list a module of the other.
- **Prerequisite work in `GSTNCombat` (first task of the milestone, before any M6 feature):** the extension surface
  above, with its own Automation tests (fragment serialization round trip with zero, one and two fragments; handler
  registry; absent-fragment bytes = 0) and `scenario_combat_spike` + M4 scenarios re-run clean. Tell the user: it
  touches the combat sync state's serialization.

## 1d. Kit-dependent defence (owner direction §6)

There is no standard shield or dodge. This file specifies the **default kit's reference implementation**; everything is selected by the
kit, never assumed by `GSTNCombat` or by the pipeline:

- `FGSTN_DefenceKitExtension : FGSTN_KitExtension { TSoftObjectPtr<UGSTN_DefenceRules> Rules; TSubclassOf<UGSTN_GA_Shield> ShieldAbility;
  FGSTN_TechSet Tech; }` on the kit (modularity-and-tags.md §5b). `UGSTN_CombatRules::Defence` is only the **default** for kits
  without the extension; a kit with an empty extension has **no shield** (the `Shield` step sees no `Raised` flag), a kit with another
  `ShieldAbility`/rules asset has its own arc, drain, parry window or no parry at all (`WindowFrames = 0`).
- The `Shield` and `Parry` steps read the **victim's rules index from the snapshot extension** (`FGSTN_DefenceSnapshotExt::RulesIndex`,
  baked at load), so two fighters with different shields resolve differently in the same pipeline, deterministically.
- A kit whose defence is not a shield at all (counter stance, energy barrier, armour stance) adds its own `UGSTN_HitResolutionStep` and
  ability following §14, with the same rules: a pure decision on the snapshot, state in a fragment, no change to the model.
- **Dodge is not in this file**: base PvE dodges (ground long-jump dive, air dive roll, feet-only intangibility) are timeline
  abilities of the default kit, M4 §1b. Tech rolls (§6) are tech abilities, not a movement mode; **no rolling movement mode is built**.

## 1c. Gameplay tags

Policy (CLAUDE.md, combat.md §9): tags are **native** (`UE_DECLARE_GAMEPLAY_TAG_EXTERN` in the owning plugin's
`*Tags.h`, defined in its `.cpp`, one header per module family); **ini** (`Config/Tags/GSTNCommands.ini`) only for
designer-made input tags (buttons, commands, their lock tags) — the plugin's *standard* command and button tags are
native. "Adds/removes" names who changes the tag at runtime; projected state tags are **absolute counts set by the
combat tick** under `Combat.State.*` (combat.md §5.4) and nothing else grants them.

| Tag | Source | Owner (declares) | Meaning | Added / removed by |
|---|---|---|---|---|
| `Input.Button.Shield` | native (exists, M1) | GSTNCombatCore | Shield/tech button (**no longer part of the grab chord**) | input config |
| `Input.Button.Interact` | ini (designer tag, `GSTNCommands.ini`, shared with M7/M10) | `GSTNInput` config | The grab / interact button; tap and hold variants (§8) | input config |
| `Combat.Interact.Target.Grab` | native | GSTNLinkGAS | Projected: a grabbable entity is the nearest interactable in reach (§8); `.Mount` is declared by `GSTNPiggyback`, `.Pickup` by `GSTNHeld` | projected by the link tick extension |
| `Combat.State.Shielding` | native | GSTNDefenceGAS | Shield raised (after startup) | projected by defence tick, removed on release/stun/break |
| `Combat.State.ShieldStun` | native | GSTNDefenceGAS | Stalled by a blocked hit | projected until `ShieldStunEndFrame` |
| `Combat.State.ShieldBroken` | native | GSTNDefenceGAS | Long stun after shield break | projected until `BreakEndFrame` |
| `Combat.State.ParryWindow` | native | GSTNDefenceGAS | Parry window open | projected for the window |
| `Combat.State.ParryRecovery` | native | GSTNDefenceGAS | Defender's short post-parry recovery | projected |
| `Combat.State.Parried` | native | GSTNDefenceGAS | Attacker punished by a parry | projected until `PunishEndFrame` |
| `Combat.State.Teching` / `.Knockdown` | native | GSTNDefenceGAS | Tech animation / knocked down | projected |
| `Combat.State.Tech.Lockout` | native | GSTNDefenceGAS | Tech press ignored (anti-mash); UI/AI | projected while `F < TechLockoutEndFrame` |
| `Combat.Mover.Tumble` | native | GSTNDefenceNP | Mover tag carried by the tumble/contact modifier | modifier add/remove |
| `Combat.Mover.Linked` | native | GSTNLinkNP | Mover tag carried by the linked modifier; triggers `UGSTN_LinkedModeTransition` | modifier add/remove on link start/end |
| `Combat.Link.Linked` | native | GSTNLinkGAS | Entity is in any `Body`-channel link | projected from the body `Link.Role != 0` |
| `Combat.Link.Pending` | native | GSTNLinkGAS | Predicted link awaiting response | projected |
| `Combat.Link.Type.Grab` | native | GSTNLinkGAS | Link type identity (also on events) | on definition assets, event tags |
| `Combat.Link.Type.Mount`, `Combat.Link.Type.Ride` (reserved) | native | `GSTNPiggyback` (M7) | Link type of piggyback | same |
| `Combat.Link.Type.Held`, `Combat.Link.Type.Minion` (reserved) | native | `GSTNHeld` (M10) / `GSTNMinion` (M8, later) | Link type holder-object / grabbed minion | same |
| `Combat.Link.Role.Grabber` / `.Grabbed` | native | GSTNLinkGAS | Role for binding requirements and `ActivationBlockedTags` | projected from `Link.Role` |
| `Combat.Link.Role.Carrier` / `.Rider` | native | `GSTNPiggyback` (M7) | Roles of the mount link | projected from `Link.Role` |
| `Combat.Link.Role.Holder` / `.HeldObject` | native | `GSTNHeld` (M10) | Roles of the held link (objects have no ASC: `HeldObject` only in events and rules) | `Holder` projected from `Link.Role`; `HeldObject` never a loose tag |
| `Combat.Link.BreakReason.*` | native | GSTNLinkGAS (generic: `Timeout Distance OwnDeath PartnerDeath Hit PartnerState Escape Release Throw Forced`); each plugin its own (M7: `Dismount Drop Outcome CarrierHit RiderHit KO Removed Teleport`) | Reason carried by `LinkBroken`; wire form is the registry's reason index (§7.1) | emitter of the break |
| `Combat.Lock.Link` | native | GSTNLinkGAS | Prevents an entity from accepting a link request (read, never projected) | abilities/GEs (ordinary counts) |
| `Combat.HitType.Grab` / `.Throw` / `.Pummel` | native | GSTNLinkGAS | Hit types (skip shield/parry; throw validates link) | on `FGSTN_HitData` |
| `Combat.HitType.Normal/Unblockable/Projectile` | native (exist, M4) | GSTNCombatGAS | unchanged | — |
| `Combat.Immunity.Grab` | native | GSTNLinkGAS | Cannot be grabbed (bosses, mid-link) | kits, GEs (ordinary counts); checked by `LinkRequest`; granting it mid-link breaks the link |
| `Combat.Immunity.Parry` / `.Shield` | native | GSTNDefenceGAS | Attack cannot be parried / blocked (victim- or ability-side) | kits, abilities, GEs |
| `Command.Defence.Shield` | native (standard set) | GSTNDefenceCore | Shield command (`Hold(Shield)`) | slot set |
| `Command.Link.Grab` / `.GrabHeavy` / `.Pummel` / `.Release` / `.Throw.Forward` `.Back` `.Up` `.Down` | native (standard set) | GSTNLinkCore | Grab (tap), heavy grab (hold) and grabber-role commands (`Command.Link.Mount/Dismount/Drop` are declared, as ini, by `GSTNPiggyback`; `Pickup/Drop` of held objects by `GSTNHeld`) | slot set |
| `Input.Lock.Command.Defence`, `Input.Lock.Command.Link` | native | GSTNDefenceCore / GSTNLinkCore | Lock a command family | abilities/GEs (ordinary counts) |
| `Combat.Event.Blocked/Parried/ShieldBroken/Teched/KnockedDown` | native | GSTNDefenceGAS | Authoritative events (§5.6 combat.md, server outbox) | published by the combat model |
| `Combat.Event.LinkFormed/LinkBroken/Throw/GrabEscaped` | native | GSTNLinkGAS | Same | published by the watchdog/pipeline |
| `Combat.SetByCaller.ShieldDamage` | native | GSTNDefenceGAS | GE magnitude key | set by the damage code |
| `Combat.Relationship.*`, `Combat.Hurtbox.*` | native (exist) | GSTNCombatGAS | unchanged | — |

Designer-made commands (new throws, a second shield) go in the ini; a game that wants a different shield *button* adds
`Input.Button.*` there. Tag hierarchy rule: a plugin declares only tags under its own roots (`Combat.Link.*`,
`Combat.Immunity.Grab`, `Combat.Interact.Target.Grab` for Link; `Combat.State.Shield*/Parry*/Tech*`, `Combat.Immunity.Parry/Shield` for Defence), and
reads the other plugin's tags by *name through `FGameplayTag::RequestGameplayTag(…, false)`*, never by an extern
declaration, so each plugin builds and runs alone.

## 2. Spikes first (record results in combat.md §17)

1. **Link spike (decides §7).** Reference topology: **listen server** (owner direction §6b): host H plus clients A and B, at
   60/15 ms and 2 % loss; then the same with a dedicated server for comparison. Run every pairing: **H is the grabber** (A or B is
   linked), **H is the grabbed** (A grabs H), and client against client (A grabs B). Client A (grabber) walks,
   dashes and circles; the partner is linked through a server-side test command (no hit pipeline yet).
   - **Variant P (parked)**: B enters `UGSTN_LinkedMovementMode` (zero velocity, no collision with A); B's presentation
     and hurtbox transform derive from A's socket as known on each machine (server: authoritative; A's client:
     predicted; B's client: A interpolated).
   - **Variant F (follow)**: B's Mover is driven each tick to A's socket transform as B's machine sees it (A
     interpolated on B's client).
   - **Measure** (harness `scenario_link_spike`, 20 s of A moving, plus forced reconciles on B and on A inside the
     link): `np.PrintReconciles` count per client; B's rollbacks per second; presentation error of B's mesh against the
     server's socket transform (p50/p95/max, in cm) sampled on B's client and on A's client; extra bytes per second on
     B's sync state.
   - **Host specifics**: the host is authoritative and has no wire to itself, so its own states never correct; record the
     presentation error **on the host** too (a client partner is simulated, not interpolated, on the host, so the expected error is
     the smallest) and the host frame time with the watchdog running (§7.6). The frame rules (formation F+1/F+2, break at F+1) are
     the same for the host as for a client: **no topology branch**, so the host gets no faster link formation, only no network delay.
   - **Pass for P**: zero unforced reconciles on B and on A during the walk; exactly one correction at link start and
     one at link end per client; presentation error p95 ≤ 10 cm on A's client and ≤ 25 cm on B's (interpolation lag of
     A's transform, not of B's), max ≤ 60 cm; forced reconciles inside the link converge in ≤ 2 frames to equal state.
   - **Expected for F**: ≥ 1 unforced reconcile per second on B (constant mispredicts against an interpolated target).
     Record the number even if P passes; F is the fallback only if P fails the thresholds, in which case §7 is
     redesigned (stop and tell the user).
   - Also record: whether Mover can be forced into a mode by a queued modifier carrying a tag (reuse spike 2.2 of M4),
     and whether the `FinalizeFrame`-time watchdog (§7.6) sees both entities' state of frame F on the server.
2. **Tumble contact probe.** A Mover modifier probes ground/wall during Tumble (`UGSTN_WallQueryUtils`) and writes a
   contact record in Mover's sync state; check that the combat tick reads it through `UGSTN_MoverSimStateTracker` at end
   of F-1 with no reconciles, and that the Tumble → Walking transition on landing is visible one frame later (rule 5 of
   CLAUDE.md). Decides the one-frame tech latency of §6.
3. **Reverse delivery.** Parry: victim's resolution at F+1 posts a `Punish` entry to the attacker's inbox on the
   server; measure the attacker client's rollbacks per parry (expect 1) and the visible delay of the attacker's stun.
4. **Mash through rollback.** `Mash` presses of the grabbed counted from history, forced reconcile in the middle of the
   mash: escape frame must equal on server and client.

## 3. Core types (`GSTNDefenceCore` / `GSTNLinkCore`, pure, unit tested)

**Defence rules** — `UGSTN_DefenceRules` (DataAsset, referenced by `UGSTN_CombatRules::Defence`, selected per game
mode like the rest of the rules):

| Struct | Fields |
|---|---|
| `FGSTN_ShieldRules` | `ArcDegrees` (default 140; 360 = bubble), `ElevationLimitDeg` (90 = any), `TurnRateDegPerFrame` (0 = facing locked at press), `StartupFrames`, `ReleaseFrames`, `DrainPerFrame`, `RegenPerFrame`, `RegenDelayFrames`, `ShieldStunBase`, `ShieldStunPerDamage`, `MaxShieldStun`, `ShieldHitstopMultiplier`, `PushbackSpeed` (cm/s) and `PushbackFrames`, `BreakStunFrames`, `BreakLaunch` (cm/s up), `RecoverFractionAfterBreak`, `IncomingShieldDamageScaleByRelationship` (map, lets PvE enemies chip less/more) |
| `FGSTN_ParryRules` | `WindowFrames` (default 5), `ReopenLockoutFrames` (shield re-press within this of the last release gets no window), `DefenderRecoveryFrames`, `HitstopFrames`, `AttackerStunFrames` (by hit class: `TMap<tag, int32>`, default entry), `bParryableProjectiles`, `bReflectProjectiles` (reserved, not implemented), `MeterGain` |
| `FGSTN_TechRules` | `WindowFrames` (default 20 before contact), `LockoutFrames` (default 40 after a press), `LateFrames` (0), `bTechInHitstunNotTumble` (false), `InPlaceFrames`, `RollFrames`, `RollDistance`, `WallTechFrames`, `bAllowWallTech`, `KnockdownFrames`, `NoTechKnockdownBonusFrames`, `IntangibleFrames` (the moves' own `HurtboxModifier` windows win; this is only the fallback) |
| `FGSTN_DIRules` | `MaxDIDegrees`, `StrengthCurve` (`FGSTN_Curve1D`: stick magnitude → factor, piecewise linear, quantized points, no engine curve in Core), `MinKnockbackForDI`, `ScaleByVictimTag` (`TMap<FGameplayTag, float>`: bosses 0), `bDIOnPlaneOnly` (true, as decided) |
| `FGSTN_SDIRules` | `DistanceCm`, `MaxPerHit`, `MinZone` (Tilt), `RequireNewDirectionEvent` (true: a held direction does not repeat), `MinFramesBetween`, `bScaleByHitstopFrames`, `ScaleByVictimTag` |
| `FGSTN_GrabRules` | `Requirements` (`FGSTN_TagRequirements` on the victim, default ignore `Combat.State.Hitstun/.Tumble/.Knockdown/.ShieldBroken`, `Combat.Link.Linked`, `Combat.Immunity.Grab`, `Combat.State.Intangible`; **`Combat.Part.GrabLocked`** is added by the designer to the ignore list when `GSTNParts` is used, by data and by name, §8), `MaxGrabWeight`, `HeavyMaxGrabWeight` (§8), `TapMaxFrames`, `HoldFrames`, `InteractReachCm`, `InteractPriority` (ordered meanings, §8), `ImmunityFramesAfterRelease`, `ImmunityFramesAfterThrow`, `Mash` (`FGSTN_MashRule`), `PummelFreeFrames`, `bGrabBeatsShield` (true) |
| `FGSTN_MashRule` | `ButtonMask` + `bCountFlicks` (Tilt direction events count), `BaseFrames`, `FramesPerDamagePercent`, `MinFrames`, `MaxFrames`, `ReducePerPress` (frames), `ResistanceAttribute` scaling |

**Math** — `UGSTN_DefenceMath` (stateless, doubles, identical on every machine):
`IsInShieldArc(ShieldYawDeg, Rules, HitSourceDirOnPlane, HitSourceElevationDeg)`;
`ShieldValueAt(const FGSTN_ShieldValue&, int32 Frame, const FGSTN_ShieldRates&)` (analytic, §5);
`EvaluateTech(const FGSTN_TechState&, history view, contact frame, Rules) → {bSuccess, PressFrame}`;
`EvaluateParryWindow(PressFrame, HitFrame, ReleaseFrame, Rules)`; `MashProgress(...)`.
`UGSTN_HitMathUtils::ApplyDI` gains the rules overload (`ApplyDI(Launch, StickOnPlane, KnockbackMagnitude, VictimTags,
const FGSTN_DIRules&)`): M4's signature stays as a wrapper. `UGSTN_SDIMath::NextDisplacement(...)`.

**Hit data additions** (`FGSTN_HitData`, M4 type extended; defaults keep M4 content valid): `bParryable` (true),
`bBlockable` (derived: false for `Unblockable`/`Grab`/`Throw`), `bDisableDI`, `EGSTN_LinkFilter LinkFilter (Any /
LinkedPartnerOnly / ExcludeLinkedPartner)` (pummel hits only the partner; sweeps by a grabber ignore the partner),
`FGameplayTag LinkType` (for `Grab` hits: which link to create), `ShieldDamage` keeps its meaning (0 = derive from
`Damage × ShieldDamagePerDamage`, a `FGSTN_ShieldRules` field).
New hit types (native tags): `Combat.HitType.Grab`, `.Throw`, `.Pummel`.

**Timeline tracks** (Core, `FGSTN_TimelineTrack` children, M3 mechanisms): `FGSTN_HoldWindowTrack { int32 StartFrame,
EndFrame; FGameplayTag HoldButton; bool bEndOnZeroShield; }` — while inside the window and the button is held (history
`Hold`), the timeline frame stalls (`TimelinePausedFrames++`, the combat clock does **not** pause, unlike hitstop);
used by shield and reusable for any held move. `FGSTN_LinkOutcomeTrack { TArray<FGSTN_LinkOutcomeKey { int32 Frame;
EGSTN_LinkOutcome Outcome (Release/Throw/KeepLink/SwapRoles); FGSTN_HitData ThrowHit; FName ExitPlacement; }> }` — the
grabber's throw/release moment (§8).

**Links** — `UGSTN_LinkTypeDefinition` (PrimaryDataAsset, type `GSTN_LinkType`): `FGameplayTag LinkType`, roles
(`FGSTN_LinkRoleDef { FGameplayTag RoleTag; bool bParked; FName SocketOnPartner; FTransform SocketOffset; bool
bCollisionWithPartner; }` × 2), `FGSTN_LinkBreakRules Break`, `ExitPlacement` (class), `RequestRequirements`,
`bInitiatorPredicts`, `MaxFrames`, `DebugColor`. `UGSTN_LinkRegistry` (subsystem, built from the definitions on
loadout/rules load, like the timeline registry of M3 §9): `LinkType tag ↔ uint8 index` (the wire form).
`FGSTN_LinkBreakRules { float MaxDistanceCm; int32 MaxFrames; TArray<FGSTN_RoleHitBreak { role tag; float
MinKnockbackToBreak; bool bPartnerHitsBreak; }> HitBreaks; bool bBreakOnPartnerDeath/OwnDeath; FGSTN_TagRequirements
PartnerMustNot (e.g. Hitstun of the carrier); }`.
`UGSTN_LinkExitPlacer` (abstract, EditInlineNew, const, deterministic): `Place(const FGSTN_ExitQuery&) →
FGSTN_ExitResult { FTransform Transform; FVector Velocity; bool bValid }`; queries the world through a
`IGSTN_ExitCollisionQuery` interface (capsule sweep), so tests use a fake. Default `Placer_Ring`: candidates in a
fixed order (type-specific preferred direction, then 8 directions at `Radius`, then above), first collision-free
capsule wins, fallback = partner's position + up.

**Inbox types** — M4's hit entry becomes one alternative of `FGSTN_InboxEntry { EGSTN_InboxType Type (Hit, Punish,
LinkResponse, LinkCommand); int32 Frame; int32 SenderId; payload }`; drained in (Frame, SenderId, type, order) order;
rollback trimming unchanged (§6.3). Payloads: `Punish { Kind (Parried, ShieldBroke…); int32 Frames; }`, `LinkResponse {
bAccepted; uint8 LinkSerial; uint8 LinkTypeIndex; }`, `LinkCommand { Kind (Break, Place); uint8 LinkSerial; Reason;
FTransform Exit; FVector ExitVelocity; }`. `PostToInbox(TargetId, Entry)` is called from any combat tick and does
nothing where the target's instance does not drain inboxes (same rule as hit delivery, combat.md §12.4).

## 4. GAS side (`GSTNDefenceGAS` / `GSTNLinkGAS`)

- **`UGSTN_DefenceAttributeSet`**: `ShieldHealth`, `MaxShieldHealth`, `ShieldRegenRate`, `ShieldDrainRate`,
  `IncomingShieldDamageMultiplier`, `GrabResistance` (scales mash length), `TechSkill` is *not* an attribute (rules
  only). Clamped in `PreAttributeChange` (`ShieldHealth` in [0, Max]). **Authority split (*Proposal*)**: the
  per-frame value lives in the combat sync state (analytic, §5) so an idle or shielding player costs no per-frame GE
  and no attribute churn; the `ShieldHealth` attribute is a **mirror** written by a sparse request (shield raised,
  released, blocked hit, break, every 16 frames while changing) for UI, AI and effects, and `Max*`/rates are read from
  the attributes by the model. UI that needs smoothness calls `UGSTN_CombatSimComponent::GetShieldValue(Frame)`.
  Effects that must change the shield value (pickups) use `UGSTN_GE_ShieldRestore`, which the combat model sees as a
  request-with-attribute-delta on its next tick (reads the mirror's base-value change against the last mirrored value).
- **`UGSTN_GA_Shield`** (child of `UGSTN_GA_BaseCombat`): timeline = startup frames, one `HoldWindowTrack` (button
  `Input.Button.Shield`, `bEndOnZeroShield`), release frames; cancel windows inside the hold window into jump, dodge,
  `Command.Link.Grab` and attacks (shield-drop and jump-out come from standard M3 cancel windows, no new code).
  Tags granted on activation: none (state tags are projected, combat.md §5.4).
- **`UGSTN_GA_Tech`** (timeline ability, kit-assigned per tech type through `FGSTN_TechSet` on the kit: InPlace, RollLeft,
  RollRight, WallTech, WallJumpTech): started by a **Forced** `ActivateEntry` request from the model (ActiveSource
  `Forced`, M3). Intangibility = its `HurtboxModifier` window. Roll/wall movement = its movement track.
- **`UGSTN_GA_Knockdown`/get-up** (*Proposal*: one neutral get-up ability with intangible frames, started the same way;
  further get-ups later).
- **`UGSTN_GA_LinkedHold`** (grabbed side presentation and tags): not an ability; the grabbed's reaction is state +
  Mover mode + the role's animation layer chosen by kit (`FGSTN_KitLinkAnims`, per role tag). Abilities of the
  grabbed are blocked by `ActivationBlockedTags` `Combat.Link.Role.Grabbed` (set in the base ability defaults).
- **GEs**: `UGSTN_GE_ShieldDamage` (instant, SetByCaller `Combat.SetByCaller.ShieldDamage`, only the mirror, §3 above),
  `UGSTN_GE_GrabImmunity` is **not** a GE (frame stamp, §5); `UGSTN_GE_ShieldBreak` (cosmetic tags/cues only).
- Native tags added (`GSTN_CombatTags.h`; one-line comment each on who grants/reads it):
  - States (projected): `Combat.State.Shielding`, `.ShieldStun`, `.ShieldBroken`, `.ParryWindow`, `.ParryRecovery`,
    `.Parried` (attacker punished), `.Teching`, `.Knockdown`, `.Tech.Lockout`.
  - Links: `Combat.Link.Linked`, `Combat.Link.Type.Grab`, `Combat.Link.Role.Grabber/Grabbed`
    (`Rider/Carrier`: `GSTNPiggyback`; `Holder/HeldObject`: `GSTNHeld`), `Combat.Link.Pending`.
  - Mover: `Combat.Mover.Linked`, `Combat.Mover.Tumble`.
  - Hit types: `Combat.HitType.Grab/Throw/Pummel`.
  - Immunity (granted by kits/bosses/GEs, read by rules): `Combat.Immunity.Grab`, `.Parry` (cannot be parried), `.Shield`.
  - Commands (`Command.` root, standard slot set; patterns in §8): `Command.Defence.Shield`, `Command.Link.Grab`, `.GrabHeavy`,
    `.Pummel`, `.Throw.Forward/.Back/.Up/.Down`, `.Release` (`Link.Mount/.Dismount` are `GSTNPiggyback`'s).
  - Events: `Combat.Event.Blocked`, `.Parried`, `.ShieldBroken`, `.Teched`, `.KnockedDown`, `.LinkFormed`, `.LinkBroken`,
    `.Throw`, `.GrabEscaped` (outbox, tagged with hit type, relationship, link type, roles; §5.6 combat.md).
  - SetByCaller: `Combat.SetByCaller.ShieldDamage`.

## 5. Combat model: sync state and per-frame rules (`GSTNDefenceNP` / `GSTNLinkNP` over `GSTNCombatNP`)

### 5.1 Sync state additions (table; idle values cost ~0 bits)

| Field | Purpose |
|---|---|
| `int16 ShieldValue`, `int32 ShieldStampFrame`, `uint8 ShieldFlags` | Analytic shield: value at `ShieldStampFrame` in 1/4 points; flags = `Raised`, `Breaking`. Re-stamped at every change event. |
| `int32 ShieldPressFrame`, `int32 ShieldReleaseFrame`, `uint16 ShieldYawQ` | Press frame defines the parry window; release frame regen delay and `ReopenLockout`; yaw quantized 360/65536. |
| `int32 ShieldStunEndFrame`, `int32 ParryRecoveryEndFrame`, `int32 BreakEndFrame` | Stuns by end-frame stamps. |
| `int32 TechLockoutEndFrame`, `int32 LastTechPressFrame`, `int32 TechEndFrame`, `uint8 TechType` | Tech state machine (§6). |
| `int32 KnockdownEndFrame` | Knockdown after a missed tech. |
| `int32 GrabImmunityEndFrame` | Grab immunity stamp. |
| `FGSTN_LinkState Link` | §7.1. |
| `int32 MashProcessedFrame` | Last history frame counted by the mash rule (grabbed). |
| `int32 PunishEndFrame`, `uint8 PunishKind` | Attacker parry-stun etc. |
| `uint8 SDIUsed`, `int32 LastSDIFrame` | M4's `SDICount` extended. |

`EGSTN_ActionState` gains `ShieldStun`, `ShieldBroken`, `Knockdown`, `Teching`, `Punished`, `Linked` (M4 had
`Actionable`, `InTimeline`, `Hitstun`, `Tumble`). All fields in `NetSerialize`/`ShouldReconcile`/`ToString`/
`Interpolate` (combat.md §5.5 rule 4). Sizes in §10.

### 5.2 Per-frame order (extends M3 §7.2; new steps in bold)

1. M2/M3 steps 1–2 (sample, history, watched tags, reset rules, timeline clock; hold stall of `HoldWindowTrack` is part
   of the clock step: `TimelinePausedFrames++` while inside the window and held and not `ShieldStun`).
2. **Inbox drain** (M4 §7, now all types): `Hit` → pipeline (§6.3 below); `Punish` → `ActionState = Punished` until
   `PunishEndFrame`, cancel timeline, hitstop; `LinkResponse`, `LinkCommand` → §7.
3. **Defence clocks**: `ShieldStunEndFrame` reached → clear; break end → shield value = `RecoverFractionAfterBreak`,
   state `Actionable`; `KnockdownEndFrame`/`TechEndFrame` reached → `Actionable` (a get-up ability may be requested);
   `PunishEndFrame` reached → `Actionable`; shield drain/regen is analytic (no per-frame work).
4. **Tech** (§6) when `Tumble`: process new tech presses, evaluate contact.
5. **Link tick** (§7.4): mash, role-input flags, request timeouts.
6. M3 steps 3–10 (adoption … tag projection) unchanged, but candidates are filtered by `Combat.Link.Role.Grabbed`
   (no candidates) and by `ShieldStun`/`ShieldBroken`/`Punished`/`Knockdown` (cannot act).
7. Tag projection adds the §4 state tags; role tags from `Link.Role`.

### 5.3 Shield: algorithms

- **Raise**: command `Command.Defence.Shield` = `Hold(Shield, 0)` anchored by the button press; binding requires
  `Input.Context.Grounded` (*Proposal*: no air shield, no wall shield; air defence is dodge). Execute as any timeline:
  `ShieldPressFrame = F`, `ShieldYawQ` from the aim policy (`SnapToStick` if tilted, else facing), `ShieldFlags.Raised`
  from the end of startup (`StartupFrames`) — hits during startup are normal hits. If `ShieldValue ≤ 0`,
  `Breaking`, or `F < ReopenLockout` → the command cannot execute (binding availability: a lock tag
  `Combat.State.Tech.Lockout` is *not* reused; the model refuses through `CanExecute`).
- **Value**: `ShieldValueAt(F)` = while `Raised`: `max(0, V0 − Drain × (F − Stamp))`; after release: `V0` until
  `ShieldReleaseFrame + RegenDelayFrames`, then `min(Max, V0 + Regen × (F − that))`. `Drain`/`Regen`/`Max` read from
  `UGSTN_DefenceAttributeSet` at end of F-1; a change of rates re-stamps on the next event (documented approximation).
  Zero value while held → `bEndOnZeroShield` ends the hold window (timeline proceeds to release; no break, only a
  forced drop with `ReleaseFrames`).
- **Turn**: while held and `TurnRateDegPerFrame > 0`, `ShieldYawQ` moves toward the stick yaw by at most that rate.
- **Directional arc** (`IsInShieldArc`): `Dir` = victim position (end of F-1) → hit source, projected on the movement
  plane (world up; shield is grounded-only, so the plane is horizontal). Source = `FGSTN_InboxEntry::HitLocation`; for
  `Projectile` hits with `|HitboxVelocity| > 50` the source direction is `−HitboxVelocity`. If `|Dir2D| < 1 cm` (hit
  from straight above/below) it counts as inside when the elevation limit allows. Angle = `acos(dot(Facing2D, Dir2D))`;
  inside iff ≤ `ArcDegrees / 2` and elevation (`atan2(|dz|, |Dir2D|)`) ≤ `ElevationLimitDeg`. Boundary inclusive.
- **Pipeline step `Shield`** (§6.3): conditions: victim `Raised` at the hit's `Frame` (not at resolution frame, so the
  F+1 delay does not change the outcome), `bBlockable`, in arc, relationship `Enemy` or `Neutral` (ally hits resolve by
  their variant; a variant may set blockable). Outcome: `bBlocked`; shield damage = `HitData.ShieldDamage` (or derived)
  × victim `IncomingShieldDamageMultiplier` × `IncomingShieldDamageScaleByRelationship`; value re-stamped lower;
  shieldstun frames = `min(Max, Base + PerDamage × Damage)`, `ShieldStunEndFrame = HitstopEnd + that`; no damage, no
  knockback, no hitstun; optional `PushbackSpeed` as `FGSTN_LayeredMove_Knockback` over `PushbackFrames` **on the victim
  along the hit direction, no tumble**; hitstop both sides = knockback model hitstop × `ShieldHitstopMultiplier`;
  meter gain per rules (defender small). If the new value ≤ 0 → **break**: `ActionState = ShieldBroken`,
  `BreakEndFrame = F + BreakStunFrames`, `ShieldFlags.Breaking`, `BreakLaunch` up, `Punish` entry not sent, event
  `ShieldBroken`. The attacker predicts `OnBlock` from the snapshot with the same pure function (§6.2); a prediction the
  server disagrees with is a normal correction.
- **Shield and grabs/supers**: `Grab`/`Throw` ignore shield (`bGrabBeatsShield`). Supers: hit type `Unblockable` only on
  hits the designer marks (*Proposal*: super finishers unblockable, setup hits blockable and parry-able, shield damage ×
  tuned so a full bar-shield breaks in 2 hits).
- **Prediction**: the shielding player predicts raise, hold, release. The block outcome arrives as a victim correction
  (one rollback, as any hit, M4 §12.7); presentation (spark, shield flash) is predicted on the attacker's machine from
  the predicted `OnBlock`.

### 5.4 Parry

- **Window**: `[ShieldPressFrame, ShieldPressFrame + WindowFrames)` (startup frames count as part of it) and only if
  `ShieldPressFrame − ShieldReleaseFrame(previous) ≥ ReopenLockoutFrames`. Evaluated at the hit's stamped `Frame`.
  `Combat.State.ParryWindow` projected for the window (UI, AI reads).
- **Pipeline step `Parry`** (before `Shield`): window open, `bParryable`, not `Combat.Immunity.Parry`, in arc **or**
  within a (wider, `ParryArcDegrees` = shield arc by default) arc → hit nullified: no damage, shield value untouched,
  both hitstop `ParryHitstopFrames`, victim `ParryRecoveryEndFrame = HitstopEnd + DefenderRecoveryFrames`
  (actionable early; cancel into any command — the reward), meter gain, event `Parried`, and an outgoing entry
  `Punish{Parried, AttackerStunFrames[HitClass]}` to the **attacker's inbox** (reverse delivery, §2.3 spike).
- The attacker consumes it at F+2 (victim resolved at F+1): `ActionState = Punished`, current timeline cancelled,
  stun through `PunishEndFrame`. The attacker predicted hitstop only (snapshot window test, same pure function), so
  the stun arrives as one correction. Projectiles: parried hit is simply nullified (reflect later).

### 5.5 SDI/DI (rules asset)

- **DI**: at `HitstopEndFrame`, `ApplyDI(Launch, Stick, KnockbackMagnitude, VictimTags, DIRules)`: stick magnitude →
  `StrengthCurve`, × `ScaleByVictimTag`, rotation = `MaxDIDegrees × Strength × perpendicular component`; skipped when
  knockback < `MinKnockbackForDI` or `HitData.bDisableDI`. Vertical component unchanged (decided).
- **SDI** (during hitstop): each **new direction event** (history) with zone ≥ `MinZone`, at least `MinFramesBetween`
  after `LastSDIFrame`, `SDIUsed < MaxPerHit` → instant movement effect of `DistanceCm` (× hitstop scale) along the
  stick on the plane; `SDIUsed++`. The move is requested through a GE (instant, swept by Mover collision), so a wall
  or ledge stops it. Not allowed while armored (`bSDIAllowedWhenArmored`, default false) or on throws.
- Both rule sets are per game mode (rules asset), so PvE can scale DI/SDI by victim tag (bosses ignore them).

## 6. Tech and knockdown

- **Contact record**: the record type `FGSTN_TumbleContactSyncState` and the probe are shared with **M4 §7b** (surface impacts: the
  knockback move already needs the contact, with `SurfaceTypeIndex`); `FGSTN_TumbleContactModifier` is M6's tech extension of that
  probe, not a second one. `FGSTN_TumbleContactModifier` (`FMovementModifierBase`, queued with the hitstun knockback while
  `Tumble`) probes in `OnPostMovement`: ground contact (grounded flag, impact normal and speed) and wall contact (the
  project's `UGSTN_WallQueryUtils` multi-probe, no wall mode entry) and writes `FGSTN_TumbleContactSyncState {
  EContactKind; FVector NormalQ; int16 ImpactSpeedQ; int32 ContactFrame; }` into Mover's sync state. The combat tick
  reads it from the tracker at end of F-1, so the earliest reaction is **one frame after the contact** (accepted; the
  window counts presses up to that frame, §6 algorithm). Wall modes cannot capture a tumbling entity: transitions are
  blocked by `Combat.State.Tumble` (`UGSTN_AbilityWallEntryTransition` already requires the wall ability).
- **Algorithm** (combat tick, `ActionState == Tumble`, frames after `HitstopEndFrame`):
  1. For each tech-button (`Input.Button.Shield`) press event in the history newer than `LastTechPressFrame` and not
     before `HitstopEndFrame`: if `press < TechLockoutEndFrame` → ignored (mash penalty: it does **not** extend the
     lockout); else `LastTechPressFrame = press; TechLockoutEndFrame = press + LockoutFrames`.
  2. If the contact record is new (`ContactFrame > LastHandledContact`, state) at frame `c`: success iff
     `LastTechPressFrame` valid and `0 ≤ c − LastTechPressFrame ≤ WindowFrames` (`+ LateFrames` for a press after).
     Success → `ActionState = Teching`, `TechType` from the stick at `c` (neutral = InPlace, left/right relative to the
     contact normal's tangent = roll, wall contact = WallTech, stick away from wall = WallJumpTech), `TechEndFrame`, request
     `ActivateEntry(UGSTN_GA_Tech[type])` (Forced), `TechLockoutEndFrame = 0`, event `Teched`, buffer event.
     Failure → **first the surface impact of M4 §7b** is resolved at the same contact (hit data, victim response, surface table):
     a non-`None` outcome (`Splat`, `Bounce`, `BreakSurface`) replaces the defaults below; with no impact data the defaults apply:
     ground: `ActionState = Knockdown`, `KnockdownEndFrame = c + KnockdownFrames (+ NoTechBonus)` and bounce
     velocity applied by request; wall: bounce off (reflect velocity, remain `Tumble`). **A successful tech cancels the impact
     outcome entirely** (no splat or bounce damage; the wall tech is the reward). Hit data `bNoTech` (M4 §7b) skips the tech
     evaluation and goes straight to the impact.
  3. Hitstun without tumble (`bTechInHitstunNotTumble` false): presses are ignored and do **not** start a lockout.
- **Lockout display**: `Combat.State.Tech.Lockout` is projected while `F < TechLockoutEndFrame` (UI/AI).
- Because the press is read from the history, a forced reconcile or re-simulation reproduces the same decision; no timer
  other than frame stamps exists.

## 7. Link system (`GSTNLink`: `GSTNLinkCore` + `GSTNLinkNP`)

### 7.0 Requirements from the plugins built on links (M7, M10, M8)

M6 owns the link system and its tag subtree `Combat.Link.*` (modularity-and-tags.md §4.2). `GSTNPiggyback`, `GSTNHeld` and
later `GSTNMinion` add link behaviour only through the generic points below; none of them edits `GSTNLink` or `GSTNCombat`, and
`GSTNLink` names none of them. Each point has a do-nothing default.

1. **Entity reference.** `FGSTN_EntityRef { uint8 Kind; uint16 Id; uint8 Serial }` (4 bytes, `GSTNCombatCore`, with
   `operator==` and a null value) replaces the NP proxy id as `PartnerId`, because a link partner may be a fighter, a minion or an
   object. `Kind` 0 is `Fighter` (`Id` = NP proxy id); other kind ids are small integers allocated in the central id registry
   file (the same one as the sync fragment type ids, modularity-and-tags.md §5b) and declared by the plugin that owns them
   (`Minion` by `GSTNMinion`, `Object` by `GSTNHeld`). `Serial` guards against slot reuse (minions: low bits of the handle
   generation, M8). Pipeline internals keep their `int32` entity id; the ref is the sync-state and link form.
2. **Link channels.** Each entity has two channels, `Body` (grab, mount: parks the entity, `Combat.Link.Linked`, the only channel
   that enters `UGSTN_LinkedMovementMode`) and `Hand` (held objects: never parks, never blocks the holder's actions by itself).
   `FGSTN_LinkState` exists once per channel; `UGSTN_LinkTypeDefinition::Channel` selects it. A type's role tags project from its
   channel's state. Which type may coexist with which is data on the definition (`TArray<FGameplayTag> ForbiddenWithBody`): a
   mount allows a `Hand` link (a rider can hold and throw), a grab does not.
3. **Open link-type registry.** `UGSTN_LinkRegistry` (`GSTNLinkGAS`) is built from every `UGSTN_LinkTypeDefinition` found by the
   asset manager, whichever plugin ships it (type tag, role tags, channel, break rules, parked flag, socket, exit placer); the wire
   index is assigned from a stable table. A plugin adds a type by shipping a definition asset and declaring its tags; a definition
   whose tags are not registered (plugin missing) is reported by `IsDataValid` and skipped. Generic break reasons are listed in §1c;
   a plugin appends its own to the registry's reason table.
4. **Link rules.** `UGSTN_LinkRule` (abstract, `EditInlineNew`, const, deterministic; `GSTNLinkCore`): `Evaluate(FGSTN_LinkRequestContext&)`
   returns Accept or Refuse(reason). Rules are referenced by the link type definition and run by the `LinkRequest` step on the
   target side (grab rules are `FGSTN_GrabRules` behind the same interface). A plugin's rule is its own subclass (M7:
   `UGSTN_LinkRule_Mount`).
5. **Link actions and partner moves.** A generic block `FGSTN_LinkActions { uint16 LastRefusedSerial; int32 RefusedFrame; int32
   CooldownEndFrame; Active { int16 Id; int32 StartFrame }; PendingStart { int32 Frame; int16 Id }; Exit { int32 Frame; position, yaw,
   velocity, flags } }` in the link fragment, `LinkCommand`/link inbox payloads gain a generic `Id`/`TargetFrame` (`PartnerMove`), and
   `UGSTN_LinkActionProvider` (registered per link type) resolves `Id` to what plays on each side. Generic request types
   `EnterLinked`, `ExitLink`, `ApplyLinkEffect`, `RemoveLinkEffect`, `LaunchLinked` extend M3's request array (4 to 6 slots).
   Hitbox flags `bInteractionPickNearest` and `bAffectsLinkPartner` are part of `FGSTN_HitData` (M4). Piggyback's meanings of
   `Payload` (carry load), `Id` (pair move index) and `CooldownEndFrame` (remount cooldown) are M7's.
6. **Exit placement.** `UGSTN_LinkExitPlacer` + `IGSTN_ExitCollisionQuery` (§7.7) is the only exit helper; M7's mount placer is
   `Placer_Mount`, a subclass registered by the plugin (M7 does not add `UGSTN_LinkUtils`).
7. **Snapshot.** The anchor substitution for parked roles is done by `GSTNLinkNP`'s snapshot extension (`FGSTN_SnapshotExtension`,
   modularity-and-tags.md §5b); a plugin supplies socket and offset through the link definition, and its own per-entity data
   (M7: weight, carry limit) as a further `FGSTN_SnapshotExtension`.
8. **Test double.** `IGSTN_LinkHost` and `FGSTN_FakeLinkHost` (holds link states, executes requests synchronously, no NP) are
   in `GSTNLinkCore`, so `GSTNHeld.GAS.*` and `GSTNPiggyback.Core.*` tests run without NP.

Tag ownership summary (the full table is §1c): `GSTNLink` declares `Combat.Link.Linked/Pending`, `Type.Grab`, `Role.Grabber/Grabbed`,
the generic `BreakReason`s, `Combat.Lock.Link`, `Combat.HitType.Grab/Throw/Pummel`, `Combat.Immunity.Grab`; `GSTNPiggyback` declares
`Type.Mount`, `Role.Carrier/Rider`; `GSTNHeld` declares `Type.Held`, `Role.Holder/HeldObject`. `GSTNCombat` declares no link tag.

### 7.1 State (`FGSTN_LinkState`, a `GSTNLink` sync fragment, one per channel)

`uint8 LinkTypeIndex` (0 = none), `uint8 Role` (index into the definition's two roles), `uint8 Phase` (None/Pending/Active/Exiting),
`FGSTN_EntityRef PartnerId` (§7.0), `uint8 Serial` (incremented per link formation, distinguishes old and new links), `int32 LinkFrame`,
`int32 EndFrame` (`MaxFrames` timeout and mash-adjusted; 0 = none), `uint8 Flags { Predicted, PendingBreak }`, `uint8 BreakReason`
(registry reason index), `uint8 Payload` (type-specific, e.g. carry load), `int16 LocalYawQ` (anchor-relative facing). One such
state per link **channel** (§7.0). **Rollback behaviour:** it is ordinary sync state, restored with the model. The *inbox* entries that form
or break a link are not (they re-arrive from re-simulated senders), and a restored older state with `Serial` lower than
a newer inbox entry's simply processes the entry again (idempotent: an entry whose `Serial` is already applied or
older is ignored). A predicted link whose `LinkResponse` never comes (refused) is cleared after `RequestTimeoutFrames`
(default 12) by the link tick.

### 7.2 Formation (request/accept through the inbox, combat.md §12b)

1. Initiator's hitbox (HitType `Grab`, `LinkType` in `HitData`) overlaps the target: a normal hit delivery (M4 §6),
   with `bInitiatorPredicts` → initiator's own side: **hitstop + `Link.Flags.Predicted` only** for grabs (*Proposal*: no
   predicted role change for grabs, only for mount in M7; the grab confirm feel comes from hitstop and presentation).
2. Victim (server) resolution at F+1: `GrabImmunity` step → `LinkRequest` step: `FGSTN_GrabRules::Requirements`, target
   not `Linked`, `F ≥ GrabImmunityEndFrame`, link type requirements. Accepted → target `Link = {type, role 2, partner =
   attacker, Serial = max(own, attacker's snapshot Serial) + 1, LinkFrame = F+1 }`, `ActionState = Linked`, request
   the mode switch (§7.3), cancel timeline, entry `LinkResponse{accepted}` to the initiator. Refused → `LinkResponse
   {refused}` (nothing else happens: the grab whiffs without hitstop on the victim).
3. Initiator at F+2 consumes the response: role 1, `Link` written (`Serial` from the response), cancels its predicted
   state, opens the grabber's cancel window into `Command.Link.*` slots (a `LinkEstablished` buffer event). Both
   entities' `Combat.Link.*` tags project from `Role`. During F+1 the grabbed is parked while the grabber has no role
   yet: parked placement derives from the partner's transform and the definition, **not from the partner's role**.
4. Simultaneous grabs on one victim: the first entry in drain order wins; the others get refused (target `Linked`).
   Grabber grabbing a grabber, chains: refused by the requirements.
5. Events `LinkFormed` (server outbox, M2 §7b) when the response is sent.

### 7.3 Parked movement mode and presentation

- **`UGSTN_LinkedMovementMode`** (`UBaseMovementMode`, module `GSTNLinkNP`, shared by grabbed, rider and any other parked role):
  zero velocity, no ground snap, no gravity, no collision response, does not read input; keeps the capsule at its
  last real position (this is the **parked transform**: stable on every machine). Entered by
  `FGSTN_LinkedModifier` (queued once, carries the Mover tag `Combat.Mover.Linked`) through
  `UGSTN_LinkedModeTransition` (a `UBaseMovementModeTransition` on that tag); left by the same transition when the
  modifier ends (`LinkCommand{Break/Place}` requests its removal and a teleport, §7.7). Spike 2.1 confirms the mechanism.
- **Derived transforms**: `UGSTN_LinkPresentationComponent` (on pawns) exposes `GetLinkedTransform()` = partner's
  transform on **this machine** × partner socket (`FGSTN_LinkRoleDef::SocketOnPartner`, root-relative baked socket if
  `BakedSocket`, else a live socket from the anim instance for presentation only) × offset. Used by: mesh attachment
  (the mesh component is attached to the partner's mesh socket; the capsule is not), the per-step snapshot (hurtbox
  transforms of a parked entity), camera target (the player's camera follows `GetLinkedTransform`, **a project camera
  change: tell the user**), aim origin. Hit queries of a linked entity use the baked socket (deterministic), never the
  live mesh. On a client that does not simulate the partner the partner is interpolated, and so is the parked entity's
  derived transform, which is the intended trade (spike 2.1).
- Collision between the two capsules is ignored while linked (`bCollisionWithPartner` false), restored at exit.

### 7.4 Link tick (per entity, step 5 of §5.2)

- Grabbed: `Mash` (§8) updates `EndFrame`; reaching `EndFrame` sets `Flags.PendingBreak`, reason `Escape`.
- Role inputs: grabber commands come from normal bindings with requirement `Combat.Link.Role.Grabber`; the **release**
  command (`Command.Link.Release`) sets `PendingBreak`, reason `Release`.
- Predicted link without response past the timeout → cleared.
- Nothing in the tick moves another entity or writes its state.

### 7.5 Break rules (evaluated by the watchdog and the pipeline, §7.6)

Reasons: `Timeout` (`F − LinkFrame ≥ MaxFrames`), `Distance` (partner distance from the expected socket >
`MaxDistanceCm`), `OwnDeath`, `PartnerDeath`, `Hit` (`HitBreaks` per role, `MinKnockbackToBreak`; **hits from the
partner never break unless `bPartnerHitsBreak`**, so pummel keeps the link), `PartnerState` (e.g. grabber enters
hitstun/tumble/knockdown/`ShieldBroken`/`Punished`: grabbed released, grabber loses the role), `Escape`, `Release`,
`Throw`, `Forced` (game code, cutscene, `Combat.Immunity.Grab` granted mid-link).

### 7.6 `UGSTN_LinkWatchdog` (server world subsystem; the only break authority)

Runs on the server after all NP instances of frame F have ticked (from the `FinalizeFrame`-time publication point,
combat.md §5.6; spike 2.1 verifies both states are visible). It reads each combat component's **finalized state of
F** and, for every link (`Serial`-matched pair): evaluates the break rules, honours `PendingBreak` flags, and detects
**orphans** (one side has a link whose partner's state lacks it, after a 3-frame formation grace). On a break it asks the
`UGSTN_LinkExitPlacer` for each side and posts `LinkCommand{Break, Serial, Reason, Exit, ExitVelocity}` stamped F to
**both** inboxes; they are consumed at F+1 (both sides clear `Link` the same frame, `ActionState = Actionable` or the
reason's state, mode transition out, teleport request with the exit transform). Entries whose `Serial` does not match the
current link are ignored. Pipeline-caused breaks (hit, death) set `PendingBreak` and are handled the same way, so there
is **one** break path. Events `LinkBroken` (reason) and, for escapes, `GrabEscaped`, come from here. Break latency:
2 frames after the cause; clients see it as one correction.

### 7.7 Exit placement

`ExitPlacement` of the link type (and, for throws, the outcome key's `FName`) selects a placer: `Placer_Ring` (release,
escape: ahead of the former partner), `Placer_AtSocket` (throws: launch from the grabber's socket, velocity from the
knockback of the throw hit instead), `Placer_Mount` (M7). The placer sweeps the victim capsule against the world (server
only, through `IGSTN_ExitCollisionQuery`), never ends inside geometry, and is deterministic for equal inputs. The result
travels in `LinkCommand`; the teleport is a Mover instant move through a GE (the existing rule: no direct state writes).

## 8. Grabs, pummel, throws, mash, immunity

- **Grab = the interact button (owner direction §3).** The old `Chord(Attack, Shield)` is replaced by the single button
  `Input.Button.Interact`, shared with mount (M7 §7) and pickup (M10 §8). Variants come from `GSTNInput` conditions, so no new
  input code (input.md §6): *Proposal*

  | Input | Command | Meaning |
  |---|---|---|
  | **Tap** (fires on the press, not on the release, to keep the grab responsive) | `Command.Link.Grab` = `Press(Interact)`, requirement `Combat.Interact.Target.Grab` | Standing grab; with the other targets the same tap means Mount / Pickup (their plugins' commands, requirement their own `Combat.Interact.Target.*`) |
  | **Hold** (`HoldFrames`, default 15, while the tap's startup is still cancellable) | `Command.Link.GrabHeavy` = `Hold(Interact, MinFrames = HoldFrames)`, same requirement, one tier above the tap | **Heavy grab**: slower startup and recovery, accepts targets up to `HeavyMaxGrabWeight` instead of `MaxGrabWeight`; for M10 the hold on an object is its heavy pickup |
  | Tap as grabber | `Command.Link.Release` = `Press(Interact)`, requires `Combat.Link.Role.Grabber` | Release (was `Press(Shield)`) |
  | Hold as grabber | unbound in the first version (reserved, e.g. a charged pummel) | none |

  **Which meaning a tap has** is resolved by *context*, not by an extra command: the link tick extension (tag projection hook)
  runs `UGSTN_InteractMath::PickTarget` on the end-of-F-1 snapshot: candidates from registered `IGSTN_InteractCandidateProvider`s
  (`GSTNLink`: grabbable entities; `GSTNPiggyback`: mountable allies; `GSTNHeld`: objects) inside `InteractReachCm` and the facing
  arc, the **nearest to the facing direction wins, ties by `InteractPriority`** (data, default Grab, Pickup, Mount; *Proposal*, the
  owner has not decided the tie of an ally and an enemy, M7 §7 question). It projects **exactly one** `Combat.Interact.Target.*`
  tag; each meaning's binding requires its tag, so the bindings never compete. Cost: one broadphase query per entity with an
  Interact binding, pure function, no component access. Airborne binding absent by default.
  The grab move's timeline has a hitbox of HitType `Grab`
  and `LinkType = Combat.Link.Type.Grab`; it also opens a cancel window `OnHit` (= link established) into the grabber
  slots. The probe only chooses the meaning; whether the grab connects is still the hitbox and `LinkRequest`.
- **Grabbability: weight/size class and broken parts (owner §1, §3).** `LinkRequest` accepts a `Grab` only if the victim passes
  (a) the tag requirements, (b) **weight**: the victim's `EffectiveWeight` from its `FGSTN_HitVictimView` (the M4 `HealthWeight`
  curve of remaining health, or, with `GSTNParts`, `UGSTN_PartMath::EffectiveWeight` with lost limbs) is `≤ MaxGrabWeight` of the
  grabber's rules (tap) or `≤ HeavyMaxGrabWeight` (hold), so the size/weight class is data and a battered enemy gets lighter, and
  (c) **broken parts**: `Combat.Part.GrabLocked` (projected by `GSTNParts` while its `FGSTN_GrabGate` is unmet) is in the ignore
  list of `Requirements`, so a too-big enemy is **ungrabbable until the required parts break**, then the tag disappears. M6 names
  no part concept: `GrabLocked` is looked up by name (`RequestGameplayTag(…, false)`), `GSTNParts` needs no M6 change
  (combat-parts.md §7). Bosses keep `Combat.Immunity.Grab` and are never grabbable. Designers may use weight, the part gate or both
  (both must pass); without `GSTNParts` the weight rule alone works.
- **Grabber slots** (bindings with requirement `Combat.Link.Role.Grabber`; one slot set, `UGSTN_StandardSlots` extended):
  `Command.Link.Pummel` = `Press(Attack)` with a neutral stick; `Command.Link.Throw.Forward/.Back` = `Press(Attack)`
  or `Press(Strong)` + `Direction(Facing, forward/back, MinZone = Tilt)`; `.Up` = `Press(Strong)` + neutral stick,
  `.Down` = `Press(Attack)` + `Hold(Crouch)` (*Proposal*, all rebindable data). Release: `Command.Link.Release` =
  `Press(Interact)` (the grab button again, see the table above; was `Press(Shield)`).
- **Pummel**: a short timeline whose hitbox has `LinkFilter = LinkedPartnerOnly`, HitType `Pummel`; it resolves through the
  pipeline (damage, meter) but `bPartnerHitsBreak` is false and no hitstun/knockback is applied (`Pummel` skips the
  knockback step: HitData with `FixedKnockback = 0` and a flag `bNoReaction`); `PummelFreeFrames` after formation block it
  (no instant mash). Grabbed's mash `EndFrame` is **not** reduced by pummels.
- **Throw**: the throw timeline has an `FGSTN_LinkOutcomeTrack` key at the release frame: the grabber's model posts a
  `Hit` entry (HitType `Throw`, `ThrowHit` data, attacker yaw) to the partner's inbox (**a direct hit, no hitbox
  overlap**), clears its own `Link` at that frame (outcome `Throw`), sets `GrabImmunityEndFrame`-for-partner through the
  `LinkCommand{Break}` (§7.6, watchdog sees the grabber side gone → orphan). Victim pipeline for `Throw`: `LinkRequest`
  step validates `Serial`/partner, skips `Parry` and `Shield`, runs `ComputeKnockback` (DI disabled unless
  `!bDisableDI`), `ApplyDamage`, `Hitstop`, and the victim leaves `Linked` for `Hitstun`/`Tumble` through the same break
  command. Throw damage, knockback and meter credit use the same events (`HitLanded`/`HitReceived`, `Throw`).
- **Mash**: grabbed side, link tick: unconsumed presses of `ButtonMask` (and Tilt flicks) in the history newer than
  `MashProcessedFrame` each reduce `EndFrame` by `ReducePerPress`; initial `EndFrame = LinkFrame +
  clamp(BaseFrames + FramesPerDamagePercent × DamagePercent, Min, Max) × GrabResistance`-scaling. The M1 `Mash`
  condition is **not** used for this (it is a command matcher); the model reads raw press events so no slot fires while
  `Linked` (the grabbed has no candidates). Presses never reduce below the escape frame being already past.
- **Immunity**: `GrabImmunityEndFrame = Frame + ImmunityFramesAfterRelease/AfterThrow` set on the victim when its link
  ends; pipeline step `GrabImmunity` (before `LinkRequest`) ignores `Grab` hits then (hit does not register, no
  hitstop). Also set on the *grabber* briefly after a throw (`GrabberCooldown`, a rules field), to stop instant
  regrabs on bosses.
- **Throwing a grabbed enemy at other enemies (owner §3; hand-off to M10).** The M6 throw ends the link as above (a `Throw` hit
  to the partner, no hitbox overlap). When the partner is an **enemy** (a fighter, or a minion once M8 lands), the throw hit carries a
  `FGSTN_ThrownBodyExtension` in `ThrowHit.ExtensionData` (declared by `GSTNHeld`, ignored with a warning if the plugin is off:
  then the throw is the plain M6 launch). `GSTNHeld`'s outcome applier (M4 §3b seam) consumes the `FGSTN_ThrownOutcome { ThrowerRef,
  Serial }` that the `Throw` resolution writes into `Ctx.OutcomeExtensions` and turns the **thrown victim into a hitting
  projectile** following [M10 §6b](combat-m10-held-objects.md): a fighter stays an NP entity launched through the knockback layered
  move, with `Combat.State.Thrown` projected and a thrown-body hitbox group (`ThrownBody`) credited to the thrower; a minion
  becomes the projectile itself. M6 owns only the grab, the gate above, the throw hit and the link break; damage to the targets,
  credit, `Penetration`, wall impact (M4 §7b) and landing are M10's. `GrabImmunityEndFrame` still applies to the thrown enemy
  after the throw (no instant regrab mid-flight).
- **PvE**: bosses/minions use `Combat.Immunity.Grab` (refused at `LinkRequest`); enemies that grab players use the same
  hitbox and pipeline (player is `Linked`, mash applies, throws are their timeline's outcome keys); AI input source
  sends `Command.Link.*` through `RequestCommand`.

## 9. Resolution pipeline (M4 §3 extended; order is normative)

1. `TeamFilter` (M4)
2. `HurtState` (M4): intangible → `Stop`; invincible/armor set flags on the context.
3. **`GrabImmunity`** — `Grab` hits against an immune victim → `Stop` (no registration).
4. **`LinkRequest`** — `Grab`: gate and form (§7.2) → `Stop` (a grab does no damage); `Throw`/`Pummel`: validate the
   partner link and `Serial`, else `Stop`; other hit types → `Continue`.
5. **`Parry`** (§5.4) → on success `Stop`.
6. **`Shield`** (§5.3) → on block `Stop` after its own hitstop/meter work (it does not call `ComputeKnockback`).
7. `ComputeKnockback`, `ApplyDamage`, `Hitstop`, `MeterGain` (M4, unchanged)
8. **`LinkBreakOnHit`** — if the victim is `Linked` and the hit qualifies (§7.5) → `PendingBreak`.
9. **`Tumble/Tech`** bookkeeping: a launch with tumble sets `Combat.Mover.Tumble` and queues the contact modifier.

The context gains: `bGrab`, `bThrow`, `Link` info, `OutgoingEntries` (reverse deliveries applied after the pipeline),
`bBlocked`, `bParried` (M4 had them as placeholders, now written), `ShieldDamageApplied`. Steps stay `const`, pure over
their inputs, and **never read live components** (combat.md §5.3). The `Shield`/`Parry` steps and the attacker's
prediction call the same `UGSTN_DefenceMath` functions on the snapshot (which now carries `bShielding`, `ShieldYaw`,
`ParryWindowEndFrame`, `Link.Role`, `GrabImmunityEnd`, `Immunity tags` per entity), so a disagreement means only that the
victim's state really differs.

## 10. Sync state additions: byte estimates

Raw widths (quantization as above); serialization writes defaults as one bit and frame stamps as variable-length deltas
from the state's frame, so an idle fighter costs ≈ 0 in delta mode (combat.md §15).

| Group | Raw (worst case) | Typical idle | Typical active |
|---|---|---|---|
| Shield (`ShieldValue` 2, stamp 4, flags 1, press 4, release 4, yaw 2) | 17 B | ~1 B | ~8 B |
| Stuns (shieldstun, parry recovery, break, punish) | 4 × 4 + 1 = 17 B | 0 | ~4 B |
| Tech + knockdown (lockout, last press, end, type, knockdown end) | 17 B | 0 | ~6 B |
| Grab immunity | 4 B | 0 | ~2 B |
| `FGSTN_LinkState` per channel (type 1, role 1, phase 1, partner `FGSTN_EntityRef` 4, serial 1, linkFrame 4, endFrame 4, flags 1, reason 1, payload 1, localYawQ 2; body + hand = 2 channels) | 21 B per channel | 1 bit | ~9 B |
| `MashProcessedFrame`, SDI extras | 9 B | 0 | ~3 B |
| **Total** | **~78 B** | **~1–2 B** | **~20–25 B** |

Inbox entries are **not** sync state (component memory, trimmed on rollback, M4 §7). Network bytes while linked: the
link spike measures the real figure; budget ≤ 2 B/s/entity extra at idle, ≤ 40 B/s/entity while a link is held.

## 11. Content, tools, harness

- Test content: `GSTN_Kit_TestDefence` additions on the test character: shield move (`GA_Shield` with a hold window),
  grab move, pummel, four throws (forward and down enough for tests), tech set (InPlace, Roll, WallTech), knockdown
  get-up; defence rules asset `DA_DefenceRules_Test` with short numbers (shield 100, drain 0.4/frame, parry window 5,
  tech window 20, lockout 40, grab base 90 frames). A Python helper builds them like M3/M4 content.
- Debug: `gstn.Combat.Debug.Shield 1` (arc wedge, value bar, parry window colour), `...Links 1` (line between partners,
  socket gizmo, serial, remaining frames), `...Tech 1` (window/lockout bar), Gameplay Debugger additions.
- **Harness scenarios** (`gasnpp_test.py`; 60/15 ms with and without 2 % loss; clean = snapshots equal, no unexplained
  reconciles, plus the expected one per hit/grab on the victim's client):
  `scenario_link_spike` (§2.1, **every pairing with the host as grabber, as grabbed and as neither**), `scenario_shield_block` (c1 jabs c2 shielding: blocked, shield value down, shieldstun
  frames, no knockback, c1 `OnBlock` cancel), `scenario_shield_arc` (hit from behind passes through; edge angles),
  `scenario_shield_break` (repeated hits: break, stun, recover), `scenario_parry` (shield pressed 3 frames before the hit:
  nullified, c1 `Punished`, c1 rollbacks = 1; press 6 frames before: plain block), `scenario_tech_ground` (press at
  c−3 teches in place; c−25 no tech; two presses: lockout), `scenario_tech_wall`, `scenario_sdi` (rules asset values),
  `scenario_grab_throw` (c1 grabs, three pummels, throws forward; all states equal),
  `scenario_grab_interact` (tap = grab, hold = heavy grab, tap as grabber = release; context picks the nearest of grab/pickup/mount;
  one `Combat.Interact.Target.*` tag at any frame), `scenario_grab_gate` (heavy enemy refused while `GrabLocked` or above
  `MaxGrabWeight`; accepted after the parts break / weight falls; with and without `GSTNParts`), `scenario_throw_at_enemy` (grabbed
  enemy thrown into two others: hand-off to the M10 thrown body, credit, link breaks once), `scenario_grab_mash` (c2 scripted
  presses: escapes on the predicted frame, forced reconcile mid-mash), `scenario_link_break_distance` (server moves the
  grabber away; and kill of either side), `scenario_grab_immunity` (regrab refused for N frames), `scenario_link_rollback`
  (forced reconciles on each client inside a link, over formation and over break frames). New harness helpers:
  `place_pair(distance, facing)`, `link_snapshot()`, `server_post_link_command(...)`. **Every scenario runs in the listen-server
  configuration (host + 2 clients) and in dedicated + 2 clients** (owner direction §6b); `place_pair` takes the host as either
  partner. "Server" in these scenarios means the authority, which in the reference configuration is the host machine.
- **Automation tests** (`GSTNCombat.Core.Defence.*`, `.Link.*`): shield arc at boundary/behind/above, projectile source
  direction, analytic shield value (drain, regen delay, clamp, rate change re-stamp), parry window edges and reopen
  lockout, tech state machine on fake histories (press in window, early press with lockout, mash during lockout does not
  extend, late press, hitstop-excluded presses, contact same frame as press), DI curve and `ScaleByVictimTag`, SDI limiter
  (new event only, min frames, max per hit), mash progress and cap, pipeline order with fake contexts (every hit type ×
  every state), inbox ordering of all four types and rollback trimming, `FGSTN_LinkState` round trip of
  `NetSerialize`/`ShouldReconcile`/`Interpolate`, serial idempotence (stale `LinkCommand` ignored), link registry
  indices stable, break rules evaluator, exit placer with a fake collision world (blocked candidates, fallback), hold
  window stall (clock vs combat clock).

## 12. Acceptance criteria

1. Spikes recorded in combat.md §17 with the §2.1 pass/fail result; if P failed, work stops and the user decides.
2. Scenarios of §11 clean; M1–M4 tests and scenarios (`scenario_hit_*`, `scenario_combat_spike`) still clean.
3. Blocked hit: no damage or knockback, shield value and stun equal on server and both clients; a hit from outside the
   arc deals normal damage.
4. A parry costs the attacker's client one rollback; a parry vs block is decided by the hit's frame, not by resolution
   latency (press at hit−4 vs hit−6 verified).
5. Tech: press inside the window techs, outside misses, lockout verified; wall tech works on a wall mode wall;
   identical decisions after forced reconciles.
6. Link: formation, parked follow, throw and escape; link adds no unforced reconciles while the grabber moves; breaks only
   through the watchdog; grabbed cannot act; immunity enforced.
7. A new link type or defensive step is addable following §14 without touching the model's tick.
7b. Grab is the interact button: tap, hold and release behave as §8 and exactly one interaction target tag exists per frame;
    grabbability follows weight and `GrabLocked` by data only; a grabbed enemy thrown at enemies reaches the M10 thrown body
    (or is a plain M6 throw with `GSTNHeld` off); tech failure resolves through the M4 §7b impact rules and a successful tech
    cancels them.
7c. Shield and the other defensive abilities are chosen per kit; a kit with no defence extension has none; no rolling movement
    mode exists.
8. Core and GAS modules acquire no NP dependency (checked as in M4); combat.md §12, §12b, §17 and testing.md updated.

## 13. Resolution of the outline's open questions (*Proposal*, user decides)

- **Kit-dependent defence (owner §6)** — *Proposal*: the shield below is the **default kit's reference implementation**; the
  numbers, the arc, the parry window and even the existence of a shield are per kit data (§1d). Another kit's defence is another
  rules asset, ability and, if needed, pipeline step.
- **Shield/parry inputs in 3D PvE** — *Proposal*: one **limited** shield (a `ShieldHealth` pool that drains while held,
  regenerates after a delay, breaks into a long stun), **directional** (140° default arc on the movement plane), on the
  Shield button, grounded only; parry = the first 5 frames of the press (no separate button), with a re-press lockout so
  holding-and-releasing is not a free parry. An always-available block is rejected (removes risk in co-op fights). PvE
  rule via `IncomingShieldDamageScaleByRelationship` and `Combat.Immunity.Parry` on bosses' signature attacks.
- **Supers vs shields** — *Proposal*: `Unblockable` is a per-hit flag (`HitType`), not per super; super finishers are
  unblockable, setup hits blockable with a large `ShieldDamage`; supers stay parryable unless the hit also carries
  `bParryable = false`.
- **Added by this spec** (smaller decisions, same status): no air or wall shield; shield facing locked at press (turn rate 0)
  with a data hook; grabs are not predicted beyond hitstop; knockdown with one neutral get-up; tech uses the Shield button
  and needs `Tumble`, not plain hitstun; a grab beats shield and cannot be parried.

## 14. How to extend

- **A new link type** (e.g. held object, carry): create a `UGSTN_LinkTypeDefinition` (tag, two roles, parked flag, socket,
  break rules, placer), add its role tags and, if it needs commands, slots with `Combat.Link.Role.*` requirements;
  `UGSTN_LinkRegistry` assigns the wire index automatically. Formation is a hit type + `LinkRequest` requirements; extra
  semantics (carry load, pair moves) are a `UGSTN_LinkRule` subclass and link actions registered by the plugin (§7.0; M7). No change to the
  watchdog or the mode.
- **A new defensive pipeline step**: subclass `UGSTN_HitResolutionStep`, write only to the context, add it to
  `UGSTN_CombatRules::Pipeline` at the right position (§9), add its pure decision to `UGSTN_DefenceMath` if the attacker
  must predict it, a state stamp in the sync state (all four serialization places) and a snapshot field, and tests with fake
  contexts. Reverse effects use `OutgoingEntries`.
- **A new tech type or get-up**: a timeline ability in the kit's `FGSTN_TechSet` + a `TechType` selector rule; no model change.
- **A new break reason**: an enum value and a rule in `FGSTN_LinkBreakRules`, evaluated in the watchdog.
- **A new interact meaning** (open a chest, talk): an `IGSTN_InteractCandidateProvider`, a `Combat.Interact.Target.<Name>` tag in
  the owning plugin and a binding requiring it; add it to `InteractPriority`. No change to the grab or the input system.
- **New inbox entry kinds**: payload struct + handler in step 2 of §5.2; remember rollback trimming and `Serial` idempotence.

## 15. Risks

- **Parked approach quality** (§2.1): if presentation error on the observer exceeds the thresholds, the fallback is
  forward-predicting the partner of a link only, which needs fork patches (combat.md §12.7); decide with numbers.
- **Watchdog frame visibility**: it assumes the server finalizes all instances of frame F before it runs and that
  re-simulation never happens on the server. If an NP id order effect appears, post breaks one frame later (stamp F+1).
- **Break latency of two frames** and the one-frame tech latency are visible in fast play; measure in the harness and
  tune windows (`WindowFrames`) rather than the model.
- **Camera follows the derived transform** (touches project camera code and the hitstop wall lock interplay): ask before
  editing; Blueprints changed only with notice (CLAUDE.md).
- **Shield mirror drift**: attribute and sync value can diverge if a GE edits `ShieldHealth` directly; use
  `UGSTN_GE_ShieldRestore` and add a development-build assert comparing both at event frames.
- **Server-only inbox types on the client**: `Punish`, `LinkResponse`, `LinkCommand` never exist on the owning client; any
  logic assuming them locally is a bug. Local split-screen drains them (M4 note) and is tested by one scenario variant.
- **Host as link partner** (listen server): the host machine runs the watchdog, all links and its own frame; a host that grabs or is
  grabbed has the same frame rules but no network delay, so clients feel the asymmetry through presentation (interpolated host);
  measured in the spike (§2.1) and mitigated, if needed, only in presentation. If the host leaves, links end with the session.
- **Interact context ambiguity**: nearest-to-facing with a priority list can pick a meaning the player did not intend (an ally
  and an enemy in reach); the tie rule is an owner question, the probe is data-driven so it can change without code.
- **Thrown-enemy hand-off** depends on `GSTNHeld` (M10 §6b) and on the M4 outcome-applier seam; without them a throw is the
  plain M6 launch.
- **Mash fairness and PvE bosses grabbing players**: `MaxFrames` caps; co-op rescue (teammate hit frees the player) is a rule
  to confirm (question list).

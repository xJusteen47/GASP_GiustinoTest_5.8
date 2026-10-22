# Combat M10 — Held and thrown objects (implementation specification)

Part of the combat design ([combat.md](combat.md)): read §5, §12, §12b, §13, §13b there, M4 ([combat-m4-hits.md](combat-m4-hits.md))
and the M6/M7/M8 outlines in [combat-m5-m9.md](combat-m5-m9.md). Section numbers below are local to this file. Promotes the
"Later — held and thrown objects" item (combat.md §13b) to **milestone M10**; combat.md §13b and §18 are updated by the
acceptance criteria. Written 2026-10-21; M6–M8 are not implemented, so section 2 lists what is assumed from them.

> **Revision 2026-10-22 (owner direction, [game-design-direction.md](game-design-direction.md) §1, §3, §4, §6b; it wins over
> this file):** (1) **grabbed enemies (fighters or minions) thrown at other enemies are a first-class case**, no longer
> "phase 2": §6b. (2) The pickup/grab button is the general **interact** button, with hold/tap variants: §8. (3) Reference
> topology is a **listen server**: host-thrown objects have zero latency and the host pays the subsystem cost: §13b. All new
> choices are *Proposal*.

## 1. Scope

**Goal:** a character picks up an object, carries it, drops it or throws it; a thrown object flies, hits (or is caught,
or breaks) with the ownership and credit of its thrower. Objects are items (consumables), picked-up **weapons**,
**throwable props** of several size classes, and **enemies** (grab a minion or a broken-down enemy fighter, throw it at others:
first-class by owner direction, §6b). Everything that is not
a fighter is server-simulated and light (the minion philosophy, combat.md §13), with the thrower predicting its own side.

In: object definitions and kinds, the `Held` link (holder / object), pickup, drop and throw abilities, the projectile entity
and its motion, `Projectile`-space hitboxes (promised by M4 §6 step 1), ownership/credit, catching and deflecting
(phase 2), thrown enemies (§6b), carry load and role-only bindings, replication, harness. Out: thrown **allies as a pair move**
(M7), the grab itself and its gate (M6; grab gating by broken parts is `Combat.Part.GrabLocked`, [combat-parts.md](combat-parts.md) §7), the surface-impact
rules themselves (M4, referenced in §6b), grabs (M6: a
grab links two entities; here the partner of a `Held` link is an object), destructible level geometry, physics-driven
(Chaos) objects, inventory/crafting, persistence of objects across sessions.

Phasing (*Proposal*): **M10a** props and items (pickup, carry, drop, throw, hit, break), **M10b** held weapons and
durability, **M10c** catching and deflecting, **M10d** thrown enemies (§6b; owner direction makes it a headline feature, so the owner may pull
it ahead of M10b; it needs M6 grabs and, for minions, M8). Each phase ends with its acceptance subset.

## 2. Assumptions about M6, M7, M8 (revise when those specs land)

1. **Link model of M6 (§7.0, M6 owns it, M10's requests are folded in there).** `FGSTN_LinkState` (per channel: `LinkTypeIndex`, `Role`, `Phase`,
   `PartnerId`, `Serial`, `LinkFrame`, `EndFrame`, `Flags`, `BreakReason`, `Payload`) is a sync fragment of `GSTNLink`; link creation by the
   server through the inbox and a `LinkRequest` step that runs the type's `UGSTN_LinkRule`; role tags projected (combat.md §12b).
   `PartnerId` is an **`FGSTN_EntityRef { uint8 Kind; uint16 Id; uint8 Serial }`** (4 bytes, `GSTNCombatCore`); `GSTNHeld` declares the
   `Object` kind id (and `GSTNMinion` the `Minion` kind) in the central id registry.
2. **Link channels** (M6 §7.0 item 2): an entity has a *body* channel (grab, mount) and a *hand* channel (`Held`), so a rider can hold
   and throw an object. Coexistence is data on the link type definition (`ForbiddenWithBody`); the matrix of section 12 is its
   content for the types known today.
3. Links can be broken by rules (`bHitBreaksLink` per role, distance, death) and a link-owned action blocks conflicting ones.
4. M6's **open link-type registry** (`UGSTN_LinkRegistry`, M6 §7.0 item 3): `Held` is a `UGSTN_LinkTypeDefinition` asset shipped by
   `GSTNHeldGAS` (tags, role tags, channel `Hand`, break rules, no parked role on the holder), registered by tag with no edit to
   `GSTNLink` or `GSTNCombat`. `FGSTN_LinkTypeDefinition` of the earlier draft is that asset.
5. Role-only bindings are generic (requirement tags on kit bindings, M3). Load scaled by partner weight follows the pattern of M7's `UGSTN_GE_CarryLoad`, but `GSTNHeld` ships its own `GE_HoldLoad` and never depends on `GSTNPiggyback`.
6. M8 provides `UGSTN_MinionCombatSubsystem` and `IGSTN_HittableProvider` (the snapshot seam) usable as the model for the object subsystem;
   minions have no NP id (`FGSTN_EntityRef` kind `Minion`).
7. M4's `EGSTN_HitboxSpace::Projectile` exists and is declared "owned by the projectile system (later)": M10 supplies it.

## 3. Spikes first (record results in combat.md §17)

1. **Throw feel:** thrower predicts a cosmetic projectile at the release frame; the server spawns the authoritative one;
   measure position error at handover at 60/15 ms and 2 % loss, for a lob and a flat throw. Target: error ≤ 25 cm, hidden
   by a 6-frame blend (section 9).
2. **Hold attach on proxies:** an object attached to a hand socket of an **interpolated** holder (no jitter, no scale
   artefacts, no extra reconcile on the holder); compare attach to the mesh socket with an offset from the root.
3. **Catch with analytic positions:** the server evaluates `PositionAt(frame)` of a flying object at the catcher's
   rewound view time; hit/miss disagreement against what the catcher saw.
4. **Entity refs:** `FGSTN_EntityRef` for objects through the link spike of M6 (extra bytes in the sync state, `ShouldReconcile`
   noise when the object is created or destroyed).
5. **Pickup race:** two clients pick up the same object on the same frame; deterministic winner, loser feedback.

## 4. Plugin and module boundaries

Held objects are optional and self-contained: a **new plugin `GSTNHeld`** on top of `GSTNCombat` and `GSTNLink` (M6 owns the link system and `Combat.Link.*`; M6 §7.0 holds the requirements of this milestone).

| Module | Contents | Allowed dependencies | Forbidden |
|---|---|---|---|
| `GSTNHeldCore` (Runtime) | Pure, deterministic: `FGSTN_BallisticSegment`, `UGSTN_ProjectileMath`, `FGSTN_ProjectileStepper`, object/size/kind enums, `FGSTN_ThrownDamageProfile`, throw-direction math, `IGSTN_WorldQuery` (sweeps) | Core, CoreUObject, GameplayTags, `GSTNCombatCore`, `GSTNLinkCore` | GAS, NP, Mover, Engine physics, anything `*NP`/`*Stock*` |
| `GSTNHeldGAS` (Runtime) | `UGSTN_ObjectDefinition`, `UGSTN_ObjectComponent`, `UGSTN_ObjectSubsystem`, projectile pool and actors, resolution steps (`HeldLinkRequest`, `Catch`, `ProjectileImpact`), damage/impact GEs, cue tags, `FGSTN_ProjectileTrack`, link type definition for `Held` | HeldCore, `GSTNCombatGAS`, `GSTNLinkGAS`, GameplayAbilities (same name in both worlds), GameplayTasks | NP, Mover, adapters |
| `GSTNHeldNP` (Runtime) | Executes the `Projectile` track and `ReleaseLink` events from the NP combat model, writes `FGSTN_LinkState` for `Held`, ghost projectile and handover, `GE_HoldLoad` application, NP frame source for the object subsystem | HeldGAS, `GSTNCombatNP`, `GSTNLinkNP` | `*StockGAS` |
| `GSTNHeldStockGAS` (Runtime, with M9) | Same for the stock adapter | HeldGAS, `GSTNCombatStockGAS` | `*NP` |
| `GSTNHeldEditor` (Editor, with M5) | Object definition preview, projectile arc preview in the timeline editor | HeldGAS, `GSTNCombatEditor` | runtime modules importing it |

- **What must not depend on `GSTNHeld`:** every `GSTNCombat*`, `GSTNLink*`, `GSTNDefence*`, `GSTNPiggyback*`, `GSTNMinion*` and `GSTNInput*` module, the neutral M9 check
  (`module_rules.json` of combat-m9 §4 gets the `GSTNHeld*` rows and a rule "no neutral module includes `GSTNHeld`").
- **Neutral hooks that `GSTNLink` and `GSTNCombat` must provide**: the open link-type registry, `FGSTN_EntityRef`, link channels and `IGSTN_LinkHost` (all M6 §7.0, not M10 work); and, as M10.0 additions to M4, each small:
  `IGSTN_ProjectileHitboxProvider` in `GSTNCombatCore` (positions/velocities of projectiles owned by a thrower, queried by the
  M4 attacker step for `Space = Projectile`); `IGSTN_FrameSource` (NP frame or stock step; M8 may reuse it);
  `IGSTN_HitInboxTarget` (victim inbox, adapter-neutral). Without `GSTNHeld` these have no implementers and cost nothing.
- **Enable/disable:** removing `GSTNHeld` from the `.uproject` leaves Core/GAS/adapters compiling and running; kits may keep
  held-role bindings, which are inert (no link of that type is ever created) and flagged by `IsDataValid` as "Held plugin
  absent". Enabling needs one adapter module matching the project's adapter (NP or stock), the plugin lists both and the
  inactive one is skipped by a `PlatformAllowList`-style module condition (`ShouldLoad` check at startup).
- **Isolation tests:** `GSTNHeld.Core.*` Automation without any adapter or GAS; `GSTNHeld.GAS.*` with a **fake link host**
  (`FGSTN_FakeLinkHost`: holds `FGSTN_LinkState` entries and executes link requests synchronously, no NP) driving
  pickup → hold → throw → impact → landed; harness scenarios (section 14) per adapter; the canary project of M9 builds
  `GSTNHeld` + `GSTNHeldStockGAS` with no GASNPP.

## 5. Gameplay tags

Per CLAUDE.md: native tags (`UE_DECLARE_GAMEPLAY_TAG_EXTERN`) except designer-extensible families, which live in ini. One
CLAUDE.md exception is added for **object type tags** (designer-made, like commands): `Plugins/GSTNHeld/Config/Tags/GSTNHeldTypes.ini`.
Ownership rule: `Combat.Link.*` belongs to `GSTNLink` (M6 §1c); the tags of the `Held` link type and its roles are declared by the plugin that
owns the type, `GSTNHeld`, like `GSTNPiggyback` does for `Mount`. Data referencing them is validated by `IsDataValid` ("Held plugin absent").

| Tag | Declared by | Meaning | Added / removed by |
|---|---|---|---|
| `Combat.Link.Type.Held` | `GSTNHeldCore` (link types are declared by their plugin, M6 §1c) | Link type holder ↔ object | Link request step (server) |
| `Combat.Link.Role.Holder` | `GSTNHeldCore` | Role tag of the carrier; requirement of throw/drop bindings | Projected (combat.md §5.4) from `FGSTN_LinkState` |
| `Combat.Link.Role.HeldObject` | `GSTNHeldCore` | Role of the object side (objects have no ASC; used in events and rules) | Event/rule data only, never a loose tag |
| `Combat.Interact.Target.Pickup` | `GSTNHeldCore` (native, projected) | The nearest interactable is an object in reach (M6 §8 probe) | Requirement of `Command.Link.Pickup`; projected by the M6 link tick extension from this plugin's candidate provider |
| `Command.Link.Pickup`, `Command.Link.Drop` | `GSTNHeld` ini `GSTNHeldCommands.ini` (designer family, CLAUDE.md input-tag exception) | Commands (slots with no default pattern, a kit sets them) | Kit bindings; the lock tags `Input.Lock.Command.Link[.Pickup]` come with the family |
| `Combat.Held.Size.Light` / `.Medium` / `.Heavy` / `.Huge` | `GSTNHeldCore` (native) | Size class: selects bindings, carry load, allowed actions | Object definition (static); projected on the holder as `Combat.Held.Carrying.<size>` |
| `Combat.Held.Carrying` (root) and `.Light/.Medium/.Heavy/.Huge` | `GSTNHeldCore` | Holder state: carries an object of that size | Projected from the link (adapter), absolute counts; removed on link end |
| `Combat.Held.Kind.Item` / `.Weapon` / `.Prop` / `.Minion` | `GSTNHeldCore` | Object kind, filters kit permissions (`AllowedKinds`) | Object definition |
| `Held.Type.*` (e.g. `Held.Type.Crate`, `.Barrel`, `.Bat`) | `GSTNHeld` ini | Designer object types; identity in events, quests, cues, kit allow lists | Designers |
| `Combat.Weapon.Role.Held` | `GSTNHeldCore` | Third weapon role (beside Primary/Support, M3): a weapon granted by holding | Object definition (`HeldWeapon`) |
| `Combat.HitType.Interaction.Pickup`, `.Catch` | `GSTNHeldCore` | Interaction hitbox types (like `.Mount`, combat.md §12b) | Timeline hitboxes of pickup/catch abilities |
| `Combat.HitType.Projectile` | `GSTNCombatGAS` (M4 already names it; combat tags live in `GSTN_CombatTags.h`, M3 §3) | Hit type of projectile hitboxes | Projectile hitbox spec |
| `Combat.Source.Thrown`, `Combat.Source.Projectile` | `GSTNHeldCore` | Credit tags in hit/KO events (thrown object vs launched projectile) | Added by the impact step to the event tags |
| `Combat.State.CatchWindow` | `GSTNHeldCore` | Victim may catch a matching object now | Granted by the catch ability's timeline `Tags` track (window), never by code |
| `Combat.State.Throwing` | `GSTNHeldCore` | Throw ability between start and release (blocks drop/pickup, fumble rule) | `ActivationOwnedTags` of the throw ability |
| `Combat.Event.Held.Picked` / `.Dropped` / `.Thrown` / `.Caught` / `.Deflected` / `.Broken` / `.ProjectileHit` | `GSTNHeldCore` | Authoritative events (combat.md §5.6), server only | Subsystem/step, exactly once |
| `Input.BufferEvent.Picked` | `GSTNHeldCore` | Buffer reset event (a pickup flushes pending attack presses) | Adapter at link creation |
| `GameplayCue.Held.Pickup`, `.Throw`, `.Impact`, `.Break`, `.Catch` | `GSTNHeldGAS` | Cues for presentation | Abilities / impact step |
| `Combat.SetByCaller.ImpactScale` | `GSTNHeldGAS` | SetByCaller on the damage GE: thrown-damage scaling | Impact step |

Reused unchanged: `Combat.Relationship.*`, `Combat.Event.Hit.*`, `Combat.State.Hitstun/Hitstop`, `Combat.SetByCaller.Damage`.

## 6. Object kinds and definitions

- **`UGSTN_ObjectDefinition`** (DataAsset, `GSTNHeldGAS`): `FGameplayTag Type` (`Held.Type.*`), `Kind`, `Size` (tag), `Weight`, actor
  class (mesh, collision profile; a hold actor and a world actor may differ), `FGSTN_PickupVolume` (shape in object space),
  `HoldSocket` + per-kit offset table (`TMap<kit tag, FTransform>`), `Durability` (uses/hits before it breaks, 0 = infinite),
  `ThrowProfile` (`FGSTN_ThrownDamageProfile`), default `FGSTN_ProjectileMotion`, `ImpactBehaviour`
  (`Break / Bounce / Stick / Explode via GE`), `bCatchable`, `bDeflectable`, `bHittableWhileHeld`, `HeldWeapon`
  (`UGSTN_WeaponDefinition`, optional), `RespawnPolicy` (none / after N s at origin / pooled).
- **Items:** consumables with a `UseAbility` bound to the usual attack command while carried (eat, drink) and throwable by
  default as `Light`.
- **Weapons:** carrying grants a weapon-bindings **overlay layer** with priority over the kit's primary weapon; the layer is
  **precompiled** into the loadout table for every weapon in the kit's `AllowedHeldWeapons`, selected by a sync variable
  from the link's object type, so it never recompiles in the simulation (M3 compiled table, `LoadoutRevision` unaffected).
  `HeldCharges` is spent by uses; at 0 the object breaks or turns into a `Light` prop.
- **Props:** size classes decide carry load (`GE_HoldLoad`: speed/jump scaled by `Weight`, same pattern as M7's carry-load GE, own class),
  allowed commands while carrying (`FGSTN_HoldRules` per size: `Light` allows everything, `Medium` blocks shield and
  air-attacks, `Heavy` allows walk, drop, throw only, `Huge` needs `MaxCarryWeight` and slow walk).
- **Minions** (M10d): `Kind.Minion` is not a definition but a *state* of an M8 minion: see §6b.

## 6b. Thrown enemies (owner direction 2026-10-22; *Proposal*)

Flow wanted by the owner: a large enemy cannot be grabbed; the player breaks parts off it; now it is grabbable and can be
thrown, for example **at other enemies**. M6 owns the grab and its gate, M8 and `GSTNParts` own the victim data, this section owns
the **throw and the thrown body as a projectile**.

**Contract with M6 and M4 (consumed, not redefined).** [M6](combat-m6-defence-links.md) §8 ("Throwing a grabbed enemy at other
enemies") ends the grab link with a `Throw` hit whose `ThrowHit.ExtensionData` carries an **`FGSTN_ThrownBodyExtension`**
(declared by this plugin; ignored with a warning when `GSTNHeld` is off, so the throw is then the plain M6 launch). The `Throw`
resolution writes an **`FGSTN_ThrownOutcome { ThrowerRef, Serial }`** into `Ctx.OutcomeExtensions`, and this plugin's
`IGSTN_OutcomeExtensionApplier` (the M4 §3b seam, [combat-m4-hits.md](combat-m4-hits.md) §3b) consumes it and turns the victim
into a hitting projectile as described below. This file defines **no** second throw payload, outcome struct or "launch with
credit" seam: the seam is M4 §3b item 3 plus these two structs. Surface impacts are M4 §7b.

| Case | Representation while thrown | Why |
|---|---|---|
| **Minion** (M8) | The store slot is **held by the link and then flies as a `FGSTN_ProjectileState`** (analytic segment, `FGSTN_EntityRef{Minion}` as the object's identity); no NP, no component. Out of the grid and attack queries while held or flying (M8 §4d.3); on landing it returns to the store (stun, or `Dying`) | Same cheap server path as props; keeps the 40+ budget |
| **Enemy fighter** (NP entity, hand-made or promoted) | It **stays an NP entity**: the M6 grab parks it, the release is a launch through the **same knockback layered move** as a hit (M4), with `Combat.State.Thrown` projected for the flight. The `FGSTN_ThrownOutcome` applier (M4 §3b) starts it; the projectile hitbox is **generated by the thrown entity's own tick extension** (`Space = Root`, group `ThrownBody`) and attributed to `ThrownOutcome.ThrowerRef` | An NP entity cannot be removed from prediction; its transform already replicates. Uses the M4 §3b outcome-applier seam (no extra seam) |

Common rules:
- **Hitboxes and damage.** The thrown body is a projectile with hitboxes (`Combat.HitType.Projectile`, `Combat.Source.Thrown`,
  plus `Combat.Source.ThrownEnemy`). `FGSTN_ThrownDamageProfile` takes size and **effective weight** from
  `UGSTN_PartMath::EffectiveWeight` when the thrown entity has parts (broken limbs lose mass, combat-parts §4.3), and `Penetration`
  from the object definition or the entity's kit (a boulder-like heavy enemy breaks parts of the target, combat-parts §3.2).
  `MaxHits` allows a bowling-ball pierce; hit groups per thrown body, as for props (§10).
- **Credit.** `Owner`/`TeamAtThrow` = the thrower (`FGSTN_ThrownOutcome::ThrowerRef`, §10): KO of a target hit by the thrown enemy credits the thrower;
  the thrown enemy's own team is irrelevant while `ThrownBody` is active (friendly fire among enemies is the point). A hit
  on the thrown entity while held or flying is ignored by default (`bHittableWhileHeld = false`, per definition).
- **Impact with surfaces.** Hitting a wall or floor causes **wall splat, bounce or destruction by designer data**. These are
  **the surface-impact rules of M4 §7b** (`EGSTN_ImpactOutcome`: hit data, then victim, then surface, first match wins, *Proposal*;
  the owner has not yet fixed which owns the choice): M10 **references, does not redefine** them. A thrown prop keeps `ImpactBehaviour` (§6); a thrown *enemy* never uses
  it and takes M4's rules, so a thrown enemy and a launched enemy behave the same at a wall. Destruction of a thrown enemy follows
  the entity's own death path (KO event, crystal shatter, combat-parts §6.3).
- **Throw feel.** Same ghost and `ThrowSerial` handover (§9) for the thrower's presentation; the thrown fighter needs none (it is
  already an NP proxy), only a short blend of the released pose.
- **Ungrabbable entities.** Nothing here overrides the grab gate: a heavy enemy is thrown only when M6 let the grab happen
  (`Combat.Part.GrabLocked` absent, `Combat.Immunity.Grab` respected).
- **Carry load.** `GE_HoldLoad` uses the effective weight, so carrying a half-broken boss is slower than a minion; heavy enemies
  may be unthrowable (`MaxCarryWeight`), by data.

## 7. The `Held` link

- `FGSTN_LinkState` of the holder: `LinkType = Held`, `Role = Holder`, `PartnerId = object ref`. The object's state (in the object
  subsystem, section 13) mirrors `Holder` and `LinkSerial`. Created by the **server** from a pickup (section 8), predicted on the
  picker's side (allowed by combat.md §12b for mount-like links; here for **static objects** only).
- **Parked object:** while held, the object's simulation is suspended; presentation attaches to the holder's hand socket
  locally on every machine (from the replicated link), the held weapon's hitboxes are `Root`-space keys with the kit
  offset (deterministic; no animation evaluated). A held object may be hittable (`bHittableWhileHeld`): hits go to its
  durability and may break the link (`Combat.Event.Held.Broken`).
- **Break rules** (`FGSTN_LinkBreakRules` of the `Held` type): holder hit with hitstun above a threshold or knocked down
  (default: any hitstun → drop), holder death/KO, holder grabbed or mounted by a hostile link (matrix, section 12), distance
  safety to the object (teleport/respawn), timeout for timed objects, object destroyed. A broken link places the object at a
  safe spot beside the holder (server) with a small toss (`DropVelocity`), state `Landed`.
- Carry load, bindings and locks come from tags: `Combat.Held.Carrying.<size>` in binding requirements, `GE_HoldLoad` via the
  adapter at link creation/removal (an absolute effect, not a countdown).

## 8. Pickup, drop, throw (abilities)

- **Interact button** (owner direction §3): pickup, grab and mount share **one button**, `Input.Button.Interact` (declared by M6,
  [M6](combat-m6-defence-links.md) §8, not redefined here). M6 §8 owns the arbitration: tap = `Command.Link.Grab` (and, by context,
  this plugin's Pickup or M7's Mount), hold = `Command.Link.GrabHeavy`, tap as grabber = `Command.Link.Release`; a context probe
  (`UGSTN_InteractMath::PickTarget`: nearest to the facing direction, ties by `InteractPriority`, default Grab, Pickup, Mount)
  projects **exactly one** `Combat.Interact.Target.*` tag. This plugin **declares `Combat.Interact.Target.Pickup`** (native,
  projected, modularity-and-tags.md §4.2), registers an `IGSTN_InteractCandidateProvider` for objects in reach (`PickupPriority`
  orders objects among themselves only), and `Command.Link.Pickup` **requires** that tag, so it never competes with the grab or the
  mount. The hold on an object is its **heavy pickup** (M6 §8: `GrabHeavy`'s object meaning; `HeavyMaxGrabWeight`-style limit by
  `MaxCarryWeight` and size class). Interact is blocked while carrying, as `Pickup` is.
- **Pickup** (`Command.Link.Pickup`, bound with requirements `Combat.Interact.Target.Pickup` and *not carrying*): a timeline ability with an `Interaction.Pickup`
  hitbox that **hits objects**: objects register a pickup volume with the world subsystem (hittable class `Object`, relationship
  `Neutral`). The resolution step `HeldLinkRequest` (object side, server) accepts if: state `Idle/Landed`, not held, kit
  `AllowedKinds` and `MaxCarryWeight` (attribute), the entity has a free hand channel, object not on the thrower's
  "no re-pickup for `RepickupLockFrames`" list for enemies' throws. Several candidates in reach: nearest to the facing
  direction (`PickupPriority`). The picker predicts attach + animation immediately; refusal restores (object pops back, a
  `Combat.Event.Held.Picked` is only sent on confirmation). **Simultaneous pickups:** earliest `RequestFrame`, then lower entity
  id wins (deterministic, same rule on every machine that resolves); loser gets a fumble cue.
- **Drop** (`Command.Link.Drop` or crouch+Pickup in the kit): short timeline; at its release key the link ends, the object
  lands at the placement socket; no hitbox, `Combat.Event.Held.Dropped`.
- **Throw** (any attack command with requirement `Combat.Link.Role.Holder` and a size tag; one binding per size and per direction,
  as for any role-only action): timeline ability with `Combat.State.Throwing`, a `Projectile` track key (section 9) and a
  `ReleaseLink` event at the same frame. At the release frame the **combat tick** requests the release (frame-stamped, executed
  once, combat.md §5.2) and the throw parameters are computed from the holder's end-of-F-1 state: origin = baked hand socket
  of the throw montage (`BakedSocket`, deterministic), direction from the ability's `AimPolicy` (stick world direction by
  default, `Camera` for aimed throws, pitch from `AngleDeg` or the camera), speed from the size class and a hold-charge curve for
  chargeable throws, `InheritVelocity` (default 0.5) of the holder's velocity.
- A throw interrupted before the release frame by hitstun: the link breaks, the object drops (fumble). After the release frame
  it flies regardless.
- Held weapons: the weapon's own timeline abilities replace the kit's attack bindings by the overlay layer (section 6).

## 9. The projectile entity

**Options considered**

| Option | For | Against |
|---|---|---|
| A. NP model per projectile, predicted on the thrower | Exact, rollback-consistent | Spawn/destroy of NP instances in prediction, id agreement between machines, rollback of spawned entities and their hits, cost per entity (~0.04 ms per re-simulated frame); breaks the 40+ budget |
| B. Server-spawned replicated actor only | Simple, authoritative | Thrower sees the throw RTT/2 late: bad feel |
| C. **Deterministic analytic segments + server authority + predicted ghost (recommended)** | Cheap, no per-frame sync state, `PositionAt(t)` for any t (lag compensation, catch, interpolation), same code on both adapters | Needs the world-contact events to be authoritative; bounces/rolls are server events |

**Recommendation (*Proposal*): C, no NP model.** A projectile is `FGSTN_ProjectileState { Ref; FGSTN_EntityRef Owner; uint8
OwnerSerial; FGenericTeamId TeamAtThrow; uint32 ThrowSerial; FGSTN_BallisticSegment Segment; uint8 BounceCount; uint8 HitCount;
uint8 Charges; }` with `FGSTN_BallisticSegment { double StartFrame; FVector Start; FVector Velocity; float Gravity; float Drag;
FVector AngularSpin; }`. `PositionAt(frame)` is **closed form** (gravity + linear drag), double math. The server's
`UGSTN_ObjectSubsystem` steps projectiles at the fixed tick (after fighters' ticks, like minions): sweep the segment's
step against the world (`IGSTN_WorldQuery`: capsule/sphere sweep, filter by profile) and against hurtboxes through the M4
query code; on world contact apply `ImpactBehaviour`: a new segment (bounce with restitution), `Landed` (becomes a normal
object: resting, pickup again), break, stick or explode. Segments, impacts and hits are **events** (`FGSTN_ProjectileEvent`),
which is all that replicates.

- **Ghost projectile** (thrower's machine): at the predicted release frame the adapter spawns a cosmetic actor driven by the same
  `PositionAt` and the thrower's predicted launch parameters. When the replicated server projectile arrives it is matched by
  `ThrowSerial = hash(thrower ref, timeline start frame, track key index)` (deterministic, **no prediction keys**); the ghost
  adopts the server's segment and **blends its error over `ProjectileSmoothFrames` (6)**; if the server never spawns it (throw
  refused or interrupted), the ghost fades after `GhostTimeoutMs` (RTT + 100). Ghosts never hit anything (no gameplay).
- **Other machines:** replicated state; display time = interpolated server frame; position = `PositionAt(displayFrame)`;
  segment changes at events.
- **Dormancy:** resting objects are `DORM_DormantAll`, woken by pickup or a hit. Idle objects cost no bandwidth and no tick.
- **Budget:** `MaxActiveProjectiles` (default 32) per world, pooled actors; the oldest `Landed` object beyond the cap is recycled by `RespawnPolicy`.
- **Lag compensation:** none for projectiles in flight (*Proposal*: objects are slow, the victim state is server truth;
  `bRewindForFirstFrames` per object definition for fast ones, using `PositionAt` of the rewound time); catching uses it (section 11).

## 10. Projectile hitboxes, ownership, credit

- **Track:** `FGSTN_ProjectileTrack : FGSTN_TimelineTrack { TArray<FGSTN_ProjectileSpec> Keys; }`, `FGSTN_ProjectileSpec { int32 SpawnFrame;
  bool bUseHeldObject (else `ProjectileClass`); FGSTN_ProjectileMotion Motion; EGSTN_ThrowAim AimMode; FGSTN_HitboxSpec Hitbox (Space =
  Projectile, shape in object space, default from the object definition); uint8 MaxHits; int32 MaxLifetimeFrames; bool
  bBreaksOnHit; }`. `FGSTN_ProjectileMotion { float Speed; float Gravity; float Drag; float Restitution; uint8 MaxBounces; float
  InheritVelocity; float SpinRate; }`. Registered as a track type by `GSTNHeldGAS`; `GSTNCombatEditor` draws it generically.
- **Space = Projectile:** the M4 attacker step queries `IGSTN_ProjectileHitboxProvider` for the **thrower's** live projectiles
  (`Owner == this` and `Hitbox` active); projectiles of a player are therefore evaluated in the **server's step of the
  subsystem**, not in the thrower's combat tick (they outlive the throw ability). A projectile hit creates the same inbox
  entry as a melee hit (`AttackerId = owner`, `HitType = Projectile`, `SourceKind = Thrown/Projectile`), so the victim pipeline,
  hitstop, DI and events are shared.
- **Per-object hit groups:** `HitTargets` ring lives in the projectile state (one hit per target per throw); `MaxHits` allows pierce.
- **Damage profile** (`FGSTN_ThrownDamageProfile`): `BaseDamage × Curve(ImpactSpeed/RefSpeed) × SizeMultiplier`, knockback by weight;
  fed to `FGSTN_HitData` at hit time (SetByCaller `Combat.SetByCaller.ImpactScale`). Relationship variants as hitboxes
  (combat.md §12.3): allies ignored by default, a friendly-fire mode applies the variant.
- **Attacker side:** thrown hits are **not** predicted on the thrower beyond the ghost: no attacker hitstop by default
  (`AttackerHitstopMultiplier = 0`), meter gain and `HitLanded` event arrive from the server; the thrower's HUD uses the
  authoritative event.
- **Ownership and credit:** `Owner` and `TeamAtThrow` are snapshots at launch: credit survives the thrower's death, team swap
  or disconnect. A hit event carries `Instigator = owner`, tags `Combat.Source.Thrown`, `Held.Type.*`, the owner's context.
  **Re-ownership:** a catch sets `Owner = catcher` (new `ThrowSerial`, `ChainDepth + 1`); a deflect sets `Owner` to the deflector and
  keeps `ChainDepth`. KO credit goes to `Owner`; the previous owner is an assist when `ChainDepth > 0` and within `AssistFrames`.
  Environmental kills by objects (a barrel exploding after a hit) credit the last owner.

## 11. Catching and deflecting (M10c)

- **Catch:** the defender's catch ability grants `Combat.State.CatchWindow` for N frames (timeline `Tags` track) and has an
  `Interaction.Catch` hitbox. Pipeline step `Catch` (before damage, like `Parry` in M6): if the projectile is `bCatchable`, the
  victim has the window, the relationship allows it (allies always; enemies per `UGSTN_CombatRules`), the object's size is allowed,
  and the catcher has a free hand channel → no hit; the link `Held` is created with the catcher as holder, `Owner = catcher`,
  `Combat.Event.Held.Caught`. Judged on the **server** with the projectile's `PositionAt` at the catcher's rewound view time
  (`ViewAge` of its window), so a catch the catcher saw is a catch. Predicted on the catcher's side only for the animation;
  the link arrives as a correction. Failed catch = normal hit if the projectile overlaps the hurtbox.
- **Deflect** (a melee hitbox of any hit type hits a `bDeflectable` object): resolution step `ProjectileInteraction`: reverse the
  velocity along the hit direction scaled by the hit's knockback, new segment, owner change, optional damage to the object.
- Catching a thrown **ally** is not an object catch (M7 pair moves).

## 12. Interaction with grabs and piggyback

| Situation | Rule (*Proposal*) |
|---|---|
| Holder is grabbed (M6) | `Held` breaks, object dropped at the holder's feet (`DropOnGrab`); grab succeeds. |
| Holder tries to grab | Blocked while carrying `Medium+`; `Light` objects are dropped by the grab startup. |
| Grabbed entity tries to pick up | Refused (parked movement mode). |
| Rider (M7) picks up / throws | Allowed with the hand channel; rider throws from the carrier's mount socket (aim from the rider's stick); carrier is unaffected. |
| Carrier picks up while carrying | Refused unless the kit sets `bCanHoldWhileCarrying` (then `Light` only, extra `GE_CarryLoad`). |
| Mount attempt on a holder (carrier) | Refused unless `Light`; otherwise the carrier drops first via a prompt ability. |
| Object thrown onto a mounted pair | Hits whichever hurtbox it touches first; the carrier/rider `bHitBreaksLink` rules apply normally. |
| Thrown ally as a pair move (M7) | Not an object: owned by M7; shares only `FGSTN_ProjectileMotion` math if wanted. |
| Thrown **enemy** after a grab (owner direction 2026-10-22) | Owned by this file, §6b: the M6 grab link ends at release; the thrown body is the projectile, credit to the thrower. |
| Enemy grabbed while it carries an object (a `Held` link on its hand channel) | The object is dropped (`DropOnGrab`), same row as the holder grabbed. |
| Hold + hitstun on the holder | Drop (section 7). Held weapon with armor frames: keeps the link. |
| Two holders, one object | Impossible: the object has one holder (`LinkSerial`); pickup race rule (section 8). |
| Link spike variants (M6: parked transform) | Held objects are parked **visually only**; the holder's transform is untouched. No new correction source. |

## 13. Sync state and replication

- **Combat sync state (additions):** `FGSTN_LinkState` (assumption 1, hand channel), `HeldChargesMirror` (uint8, from the object, for HUD),
  `ThrowSerial` of the current throw (uint16), `CatchWindowEndFrame` (end-frame stamp, section combat.md §5.5). Idle cost stays ~0.
  Every field in `NetSerialize`, `ShouldReconcile`, `ToString`, `Interpolate` (rule 4 of combat.md §5.5).
- **Object state** (not NP): `UGSTN_ObjectComponent` replicated properties on the object actor: `State (3 bits: Idle, Held, Flying, Landed,
  Broken)`, `Holder ref`, `LinkSerial`, `FGSTN_ProjectileState` (quantized: positions at 1 cm, velocity 10 cm/s, ~28 bytes when
  flying), `Charges`. Sent on events (pickup, throw, impact, break); resting objects dormant; no per-frame updates while flying.
  Net priority raised for objects flying near a local player.
- **Frame domain:** launch frame is in the **frame source of the active adapter** (NP fixed frame, or stock local step), converted to
  a server time stamp (ms) in the replicated state so clients with different frame bases agree.
- **Ordering inside the frame:** fighters (combat 47 → ASC 48 → Mover 50), then the object subsystem for the same frame: projectile
  hits reach victims' inboxes stamped F and are drained at F+1 like melee hits (combat.md §12.4).
- **Bandwidth (budget):** throw ≈ 36 bytes once, impact ≈ 16 bytes per event, pickup ≈ 8 bytes; at 3 throws/s/player ≈ 0.15 kB/s.
- **Rollback:** the holder's `Held` link and throw request are in the rolled-back state; the object subsystem is server-only and
  never re-simulates, so a re-simulated release frame only affects the ghost (re-spawned deterministically by `ThrowSerial`).

## 13b. Listen server (owner direction §6b)

- **Host-thrown objects have zero latency.** The host is the authority: its throw spawns the authoritative projectile on the
  release frame, with no ghost and no handover; thrown-enemy hits it causes resolve the same frame. Remote clients see the same
  throw after replication, and clients' throws pay the ghost-and-handover of §9. Host advantage is accepted by default; no
  mitigation is designed here.
- **Catch judged on the host** uses `PositionAt` at the catcher's rewound view time for remote catchers and the current time for the
  host's own catches, with no rewind (the same rule as lag compensation for the host's own hits).
- **Cost.** `UGSTN_ObjectSubsystem`, up to `MaxActiveProjectiles`, thrown-enemy hitboxes and presentation run in the host's frame
  together with its own game: the budget of §16.4 is measured on the host configuration. Replication to clients is event-only
  (§13), so host upload stays small (about 0.15 kB/s per client at 3 throws/s, plus about 28 B per flying object update).
- **Harness:** every scenario of §14 also runs in the listen-server configuration (host + 2 clients); the host as thrower and as
  target are both covered.

## 14. Content, tools, harness

- Test content: `Held.Type.Crate` (`Medium`, prop), `Held.Type.Rock` (`Light`), `Held.Type.Bat` (weapon, 20 charges), a three-bounce
  barrel; kit bindings by size; throw montage with a baked hand socket; dummy target.
- Debug: `gstn.Held.Debug 1` draws pickup volumes, held state, projectile segments (colour by owner), `PositionAt` markers at ±10 frames,
  ghost vs server positions, link state text.
- Harness scenarios (`gasnpp_test.py`, 60/15 ms, 0 and 2 % loss, both adapters where applicable): `scenario_pickup_throw_hit` (c1 picks up,
  throws at c2: object state, damage, knockback, credit equal; one rollback on c2), `scenario_throw_ghost` (handover error ≤ 25 cm, no
  duplicate actor), `scenario_pickup_race`, `scenario_drop_on_hit` (holder hit before release), `scenario_catch` (ally catches, ownership
  moves, link on both machines), `scenario_deflect`, `scenario_held_grabbed` (matrix rows), `scenario_object_dormancy` (no traffic when
  resting), `scenario_throw_forced_reconcile` (forced rollback over the release frame: exactly one projectile, one event),
  `scenario_throw_enemy` (c1 grabs a minion, throws it at three others: credit to c1, hits and wall impact per designer data,
  store slot returned or destroyed; the same with a part-broken enemy fighter), `scenario_throw_host` (host as thrower, zero-latency
  spawn, clients see one projectile; host as victim).

## 15. Tests

- **Automation (`GSTNHeld.Core.*`):** `PositionAt` equals stepping in doubles (1e5 random segments, ≤ 0.001 cm), identical on repeated runs,
  bounce segments conserve speed × restitution, throw aim math (stick, camera, pitch clamp), damage profile curve, owner/credit
  chain arithmetic, `ThrowSerial` stability, size/kind enum rules.
- **`GSTNHeld.GAS.*` with `FGSTN_FakeLinkHost`:** lifecycle pickup → hold → throw → fly → impact → landed → pickup again; break rules table; pickup race
  determinism; catch step ownership; pool recycling at the cap; events published exactly once.
- Boundary tests (combat-m9 §4) include the `GSTNHeld*` rows.

## 16. Acceptance criteria

1. Spikes recorded; scenarios of section 14 clean as defined in M4 §10 (snapshots equal; no reconciles besides forced ones and the
   expected one per hit on the victim's client).
2. Pickup, carry, drop and throw of a prop work for two clients at 60/15 ms and 2 % loss; the thrower sees the throw leave on the release
   frame (ghost), handover error ≤ 25 cm.
3. Credit: KO and assist events name the thrower after the thrower leaves; a catch moves the credit.
4. Idle objects: zero bandwidth and zero tick (dormancy test). 32 active projectiles stay within the performance budget set by spike
   (record ms/frame on the **listen-server host**, which also renders, and on a dedicated server for comparison).
5. `GSTNHeld` removed from the project: everything else builds and passes; boundary check green with the `GSTNHeld*` rows.
6. Thrown enemies (§6b): a minion and a part-broken enemy fighter are grabbed with the interact button, thrown at other enemies,
   hit them with credit to the thrower, and hit a wall with the M4 surface-impact outcome chosen by data; ungrabbable enemies stay
   ungrabbable. The interact button's tap/hold variants work with the input system's conditions, no new input code.
7. combat.md §13b replaced by a pointer to this file, §18 gets the M10 row, tags documented in input.md/combat.md as needed.

## 17. Risks

- **Link model not ready** (`FGSTN_EntityRef`, hand channel, registry are M6 §7.0 requirements): M10 depends on M6 being built with them; if M6 ships without them, M10.0 adds them to `GSTNLink` as a small M6 follow-up, never to `GSTNHeld`.
- **Hand attach jitter on interpolated holders:** spike 2; fallback is attaching at the root with a baked per-animation offset.
- **Throw feel when the server disagrees** (interrupted, refused): ghost fades; acceptable if rare, measured in spike 1.
- **Analytic motion vs complex geometry:** bounces on slopes and stairs produce odd rolls; mitigation: simple impact behaviours first, `Stick`
  or `Break` for awkward props; no Chaos physics.
- **Credit abuse** (throw/catch loops for kills): `ChainDepth` cap and `AssistFrames` tuning.
- **Thrown enemy fighter is still an NP entity** (§6b): its self-generated hitbox and the `FGSTN_ThrownOutcome` applier (M4 §3b seam, M6 §8) touch M4/M6; if
  the seam is not there, M10d ships minions first. Host advantage on throws (§13b) is accepted unless the owner asks for parity.
- **Content explosion** (sizes × kits × directions of bindings): bindings keyed by size and generic overlays; validator reports missing
  size/direction bindings per kit.
- **NP cost** of `Combat.Held.Carrying` tag projection and link fields on every combat tick: end-frame stamps and absolute counts keep it flat;
  measure with the M2 size test.

## 18. How to extend

- *New object:* a `UGSTN_ObjectDefinition` asset, a `Held.Type.*` ini tag, bindings only if it is a weapon or a new size class. No code.
- *New size class:* a native tag in `GSTNHeldCore`, a `FGSTN_HoldRules` entry, bindings with the new requirement tag.
- *New impact behaviour:* subclass `UGSTN_ProjectileImpactBehaviour` (EditInlineNew, const) in `GSTNHeldGAS`.
- *New interaction with an object* (open a chest, light a torch): a resolution step on the object side, an interaction hit type tag,
  an event; no change to the link types.
- *Another link-like object relation* (rope, carried by two): register a link type in the M6 registry from your own plugin.
- *Another adapter:* implement `IGSTN_FrameSource` and the release/ghost executor in a new `GSTNHeld<Adapter>` module; the scenarios of
  section 14 are the checklist.

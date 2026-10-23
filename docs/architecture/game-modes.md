# Game modes — Levels, Expedition and how PvP fits (specification)

Status: **Proposal, draft for owner review, 2026-10-23.** Scope: what a "mode" is in this project, the two PvE modes the owner asked for (a designer-authored
**Levels** mode and a Helldivers-style **Expedition** mode with a randomly generated large map and missions), how PvP fits as another experience, the
frameworks they share (missions, respawn and checkpoints, spawn director, procedural generation, mode verbs), and the budgets on a listen host. No code.

Owner request (2026-10-23, verbatim): "Ci dovrà essere una modalità a livelli scelti dal designer (livelli in 3d, stile Mario 3d world), e una modalità alla
helldivers dove si genera casualmente un grande livello e bisogna terminare delle missioni. Nella modalità PVE non c'è lo scudo. [...] Si può cambiare kit a
metà partita raccogliendone uno. [...] è prevista la presenza di Zaino come in helldivers 2." Same day: "in generale praticamente ogni cosa dovrebbe essere
un'abilità pluggabile" (as in Fortnite, because it makes design easy), and backpacks attach on top of the kit implant (owned by the loadout spec).
Earlier direction that applies ([game-design-direction.md](game-design-direction.md), it wins over this file): §2d trampolines and arbitrary gravity, §2f
shield only in PvP, death = health at the cap or a KO zone (ravines), many enemies, enemies as platforms, §3 interact button, §6b listen server.

Sources: [modularity-and-tags.md](modularity-and-tags.md) §1.2–1.3, §3.4, §3.6–3.9, §4.1–4.2b, §5, §5b, §6; [many-enemies-replication.md](many-enemies-replication.md);
[combat-m8-minions.md](combat-m8-minions.md) §4d.1, §8.3, §10, §11; [combat-m4-hits.md](combat-m4-hits.md) §3b, §7b; [combat-m6-defence-links.md](combat-m6-defence-links.md) §1d, §8;
[combat-m10-held-objects.md](combat-m10-held-objects.md); [traversal-surfaces.md](traversal-surfaces.md); [plans/README.md](plans/README.md); combat.md §5.6. Companion specs written in
parallel, **referenced, not duplicated**: [combat-loadout-mods.md](combat-loadout-mods.md) (kits, kit pickups, support weapons, mods, backpacks) and [depth-marker.md](depth-marker.md).

Notation as in modularity-and-tags.md: *Decision* = decided elsewhere or by the owner; *Proposal* = this file's suggestion; *Unsure* = not verified against engine source;
**[E]** = my estimate to be measured. **Everything beyond the owner's sentences above is *Proposal***; open points are questions GM.1–GM.15 (section 11).

> **Revision 2026-10-23 (owner rule, applied throughout):** player-facing mode actions (activate a terminal, call a reinforcement or a drop, call extraction,
> revive) are **gameplay abilities** granted by the mode's Game Feature / ability sets, never hard-coded mode logic. The mode frameworks own **state and rules**
> (server-authoritative); abilities are the **verbs** (§2.4).

## 0. Summary

1. A **mode is an Experience** (modularity §3.4): a `UGSTN_ExperienceDefinition` plus Game Feature plugins. `GSTNMode_Levels` and `GSTNMode_Expedition` (PvP later: `GSTNMode_Versus`)
   are thin: content, level sets, and the data that picks each framework's rules.
2. Shared logic lives in small frameworks that know no mode and no sibling: `GSTNMission`, `GSTNRespawn` (checkpoints, reinforcements, stocks), `GSTNSpawnDirector` (+ bridge to
   `GSTNMinion`), `GSTNProcGen`, `GSTNModeVerbs` (the one ability base for mode actions). They talk through tags and Gameplay Messages (modularity §5).
3. **Procedural map on a listen host (recommendation, §3): the host generates, replicates a compact integer layout (a few KB), every machine builds the same
   authored modules from it.** Seeded generation on every machine is rejected for anything with collision; it stays acceptable for cosmetic scatter only.
4. **Mission state is server-authoritative gameplay state, outside the rollback simulation** (it must count once, it is global, a one-RTT delay is invisible).
   Only the player's verb is predicted; its confirmation is one authoritative event (§4.3).
5. Levels = designer maps + checkpoints + a `Reach` goal (a mission) + camera sections + score. Expedition = generated map + mission chain + extraction + reinforcements + spawn
   director. Both reuse M8 packs (**150 is a population with about 60 awake**, M8 §4d.1). Anything that changes the rollback simulation is fixed by the Experience before pawns
   simulate and never toggled mid-match (§2.5).

## 1. Scope and the mode list

| Mode | Source | What it is (Proposal unless noted) | Experience tag |
|---|---|---|---|
| **A. Levels** | owner (two sentences) | Designer-authored 3D stages, Mario 3D World style, co-op PvE: mostly linear stages, shared checkpoints, a goal, designer-chosen camera sections, KO zones (ravines) and trampolines, enemies placed by the designer. | `Experience.Mode.Levels` |
| **B. Expedition** | owner (two sentences) | Helldivers style: a large map generated at random from authored modules, a set of missions, optional ones, an extraction, reinforcement of fallen teammates, an enemy director with patrols and breaches, drops from the sky. | `Experience.Mode.Expedition` |
| **C. Versus (PvP)** | owner: "PvP exists", shield only here | Another experience on the same frameworks: authored arenas, teams, KO zones as the main kill, stocks or timed respawn, shield on, no missions, no minions. **Not designed here**; section 2.3 only shows its toggles so nothing blocks it. | `Experience.Mode.Versus` |

Out of scope: kits, pickups, support weapons, mods and backpacks (**[combat-loadout-mods.md](combat-loadout-mods.md)**: this file only *spawns* their pickups and drops and decides whether
mid-match changes are allowed); the depth marker (`depth-marker.md`); lobby, matchmaking, save games and progression (GM.3); AI behaviour content. Reference topology:
**listen server**, so every generation, spawn and mission rule runs on the host and is measured together with the host's own frame (dedicated stays supported, same code).

## 2. Composition with the modularity model

### 2.1 Plugin map (*Proposal*; layers and rules of modularity §3.1–3.2, §3.7)

| Plugin (modules) | Layer | Job | May depend on | Owns tag roots / leaves |
|---|---|---|---|---|
| `GSTNMission` (`Core`, `Runtime`, `Editor`) | L3 optional | Objective rules, mission graph and chains, replicated mission state, markers. **No combat, GAS, NP or Mover.** | GameplayTags, NetCore, message router (modularity §5 item 4, *Unsure* in 5.8) | `Mission.*`, `Message.Mission.*` |
| `GSTNModeVerbs` (`Core`, `NP`) | L3b | `UGSTN_GA_ModeVerb`, the requestable-prop candidate provider for the interact probe (M6 §8), `Command.Mode.*`, republisher of verb events as messages. | `GSTNCombat` (any module), `GSTNCore` (payload structs, see below), the interact probe's `Core` | `Ability.Mode.*`, `Command.Mode.*`, `Combat.Interact.Target.Objective`, `Message.Mode.*` |
| `GSTNRespawn` (`Core`, `Runtime`) | L3 optional | Checkpoints, respawn policies, reinforcement pool, stocks, player-spawn service interface. No combat dependency: it reads KO through messages. | GameplayTags, NetCore, message router | `Message.Respawn.*` |
| `GSTNSpawnDirector` (`Core`, `Runtime`) | L3 optional | Pressure model, spawn patterns (wave, patrol, breach), zones, budgets. Spawns through `IGSTN_SpawnSink`; knows no minion. | GameplayTags, message router | `Spawn.*`, `Message.Spawn.*` |
| `GSTNSpawnDirectorMinion` (one module) | bridge | `FGSTN_SpawnSinkMinion`: turns director requests into `UGSTN_MinionSpawner::Spawn` (M8 §10) and `UGSTN_MinionWaveDefinition`s. No tags, no content. | exactly `GSTNSpawnDirector` + `GSTNMinion` | none |
| `GSTNProcGen` (`Core`, `Runtime`, `Editor`) | L3 optional | Pure layout solver (integer grid, own RNG), module definitions, layout description, level builder, loading flow. | GameplayTags, NetCore, Engine | `Generation.*`, `Message.Generation.*` |
| `GSTNProcGenPCG` (one module) | bridge to an engine plugin | Optional cosmetic scatter per module through the engine PCG framework. Never collision, nav or gameplay. | `GSTNProcGen`, PCG | none |
| `GSTNMode_Levels`, `GSTNMode_Expedition` | L4 Game Feature | Experience asset, level set / expedition definitions, mission definitions, module pool, verb ability sets, tag ini, test levels. | L3/L3b frameworks above, `GSTNLevelObjects`, `GSTNCombat`. **Never another L4**: the Experience lists `GSTNMinion`, `GSTNHeld`, `GSTNSpawnDirectorMinion` as features to activate. | `Experience.Mode.<Name>` (ini) |
| `GSTNCore` (L2, already a *Proposal*, modularity §3.6) | L2 | Second customer here: the payload structs of `Message.Mode.Verb` and `Message.Respawn.*`, which the emitter and several frameworks must share without depending on each other. | L1 only | shared helpers |

Edges added: none between frameworks (siblings; modes compose them, bridges join pairs). Only `GSTNModeVerbs` touches combat (the ability base is in `GSTNCombatNP`, as `GSTNDefenceNP`
does for the shield). `GSTNExperience`, `GSTNLevelObjects`, `GSTNMinion`, `GSTNHeld`, `GSTNLink`, `GSTNPlatform` are used as specified here and not changed. Separate plugins from the start: Versus needs `GSTNRespawn` without missions.

### 2.2 Experience, activation order, what a mode is made of

A mode's Experience (`UGSTN_ExperienceDefinition`, modularity §3.4) contains: default `UGSTN_PawnData` (ability sets of the verbs, input config, default kit
asset of the loadout spec), the Game Feature names to activate, and **data entries that choose each framework's rules**: `UGSTN_CombatRules` (M4), the defence
rules (M6 §1d), the loadout rules (loadout spec), `UGSTN_RespawnRules`, `UGSTN_SpawnDirectorRules`, `UGSTN_MissionSet`, `UGSTN_GenerationRules` (Expedition only),
`UGSTN_ScoreRules`. Frameworks register their asset on activation through a Game Feature action (`UGameFeatureAction_AddComponents` adds the replicated state
components to the GameState; *Unsure*: that this action accepts the GameState class in 5.8 without a modular GameState base, check in spike G4).

Order, host and clients: (1) map and Experience resolved (map settings; the host's URL option, never a client's); (2) Game Features Registered, Loaded, Active,
actions run: **simulation-relevant data is final here**; (3) Expedition only: generation, layout replication, build (§3.6); (4) pawn data applied, ability sets
granted; (5) pawns spawn through `IGSTN_PawnSpawnService` and start simulating; (6) server-only state starts (missions, director). Steps 1–2 reuse Lyra's rule
"the game mode does not spawn pawns until the experience is ready" (modularity §1.2). The **content signature** of modularity §3.9 includes the Experience, the module pool
and `GenerationVersion`; a mismatch refuses the connection.

### 2.3 What each mode toggles (data, never code)

"Class" tells where the toggle sits relative to the rollback simulation (§2.5): **Sim** (identical on all machines before pawns exist, never changes), **Server**
(host state, replicated, may change any time), **Pres** (presentation, per machine).

| Toggle | Class | Where it lives | Levels | Expedition | Versus |
|---|---|---|---|---|---|
| Shield | Sim | defence rules `bShieldEnabled` (M6 §1d); the shield binding is then not built and the slot is free for kits | **off** (owner) | **off** (owner) | on (owner) |
| Friendly fire | Sim | `UGSTN_CombatRules` relationships (M4; **undecided**, existing question M4.12) | open | open | team rule |
| KO zones | Sim | level actors `AGSTN_KOZone` (M4 §3b); present where the designer or a module puts them | ravines | ravine modules | arena edges |
| What a KO means | Server | `UGSTN_RespawnRules` by `KOReason` (Health or Zone) | checkpoint | reinforcement | stock |
| Mid-match kit change by pickup | Sim | a flag in the loadout rules (loadout spec); this file only decides the value and places pickups | designer-placed | drops and found kits | question GM.13 |
| Backpack and support-weapon drops | Server | `UGSTN_DropTable` entries naming the loadout spec's pickups (§6.3) | optional, placed | yes | no |
| Minion budgets | Server | `UGSTN_SpawnDirectorRules` + M8 settings, overridden per Experience (modularity §3.9 override order) | §7.3 | §7.3 | none |

### 2.4 Abilities are the verbs (owner rule, 2026-10-23)

Division of labour: **the framework owns state and rules** (what is active, progress, pool counts, whether a request is valid) and is server-authoritative;
**an ability owns the player's action** (input binding, animation and timeline, predicted feedback such as a hold bar, cost and cooldown through the usual GAS
settings, and one authoritative request at its confirmation frame). A mode adds, removes or changes a verb by changing an **ability set** in its Game Feature
(modularity §3.4); no mode-specific `if` exists in a framework. Test: disable the Game Feature and the verb vanishes while the framework state is untouched.

| Verb | Ability identity tag | Target source | Server-side handler (state owner) |
|---|---|---|---|
| Activate a terminal, arm a charge, open a gate | `Ability.Mode.Interact` | the interact probe's target (`Combat.Interact.Target.Objective`) | `GSTNMission`: objective rule `Activate` |
| Call a reinforcement (bring back a fallen teammate) | `Ability.Mode.CallReinforcement` | aim point | `GSTNRespawn`: pool, placement, pod |
| Call a drop (supply pod: kits, support weapons, backpacks) | `Ability.Mode.CallDrop` | aim point | drop service of the mode (§6.3) over `UGSTN_DropTable` |
| Call extraction | `Ability.Mode.CallExtraction` | the extraction console (interact target) | `GSTNMission`: objective `Extract` |
| Revive in place | `Ability.Mode.Revive` | a downed teammate | **not specified**: needs a "downed" combat state that does not exist (GM.9) |
| Carry and deliver an item | M10's pickup, carry, drop and throw abilities (**no new verb**) | held object | `GSTNMission` reads the confirmed M10 events |

Mechanics (*Proposal*): `UGSTN_GA_ModeVerb` derives from `UGSTN_GA_Base` (Blueprintable; designers author the timeline and presentation as in modularity §3.8). Data per
ability: `FGSTN_VerbSpec { VerbTag, HoldFrames, TargetSource, MaxRange }`. At the confirmation key it posts an authoritative event through the combat outbox
(`UGSTN_SimEventSubsystem`, combat.md §5.6: server, from finalized-frame state, once, never from a re-simulation). `GSTNModeVerbs` republishes it as the message
`Message.Mode.Verb` (payload in `GSTNCore`: verb tag, instigator, target, quantised aim point, magnitude); frameworks **subscribe by verb tag, validate against
their own state, then mutate it**. A refusal changes nothing; the client sees the ability end and its hold bar vanish (one cancel, no state rollback needed).
**Dependency to confirm (D-1):** today the timeline `Event` track is cue-only (M8 §1); an "authoritative event key" that posts to the outbox is a small generic
seam in `GSTNCombatNP` that this spec needs. Without it the fallback is a server request through the combat inbox (modularity §5b), more code. Spike G5.

**The interact button.** A terminal is a third meaning of the Interact button after tap = grab and hold = pick up (owner, M6 §8). The probe projects exactly one
`Combat.Interact.Target.*` tag per frame, so meanings never compete; the new leaf `Target.Objective` is declared by `GSTNModeVerbs`. *Proposal*: `Command.Mode.Interact`
= `Hold(Interact, HoldFrames)` requiring that tag (a hold avoids accidental activation; data in the slot set, changing it costs no code). Tie priority with grab,
pickup and mount (data `InteractPriority`, existing question M6.22) gets a fourth entry. **Objective availability is not simulated state**: a terminal that is not
yet armed may look interactable on a lagging client; the server refuses and the ability cancels once. Mitigation: objectives arm long before players reach them
and the UI greys them. If the mismatch rate (spike G5) is too high, the probe takes a replicated "armed" mask as a recorded input (more code, not v1).

### 2.5 Simulation caveat applied (modularity §1.3)

- **Sim** toggles (§2.3) and everything the abilities read (ability sets, command bindings, rules assets, content signature) are final before the first NP tick of
  any pawn and are not toggled while a pawn exists; a new level of Levels mode is a map change (seamless travel), so the Experience, and therefore the sim data,
  may differ per level but never inside one.
- **Server** state may change at any time because the rollback never touches it: objective progress, the reinforcement pool, director pressure, generation
  output. Corollary: **nothing a framework does may write simulated state directly** (CLAUDE.md rules 1 and 2). A reward that buffs players, a gravity change
  triggered by an objective, a respawn: the framework asks through an existing channel (a gameplay effect applied through the combat inbox, a spawn of a new pawn,
  a level actor whose own data defines the gravity volume). *Unsure*: whether the inbox already has a generic "apply effect" entry kind (modularity §5b lists
  the registry, not the kinds); if not, the framework only spawns actors and the *actor's* data applies effects at contact. Pawns spawned by respawn are new NP entities created with all features already active.

## 3. Procedural generation on a listen server (Expedition)

### 3.1 What must be identical on every machine

Mover prediction collides against level geometry on the client and on the host; a module placed 1 cm differently makes every reconcile near it fail. So **collision-bearing geometry
and every gameplay slot position must be bit-identical (placement) or come from identical authored assets (content)**. Navigation data is needed on the host only (minions simulate
there; clients draw puppets). Cosmetic, non-colliding decoration may differ.

### 3.2 Options

| | Approach | Pros | Cons / risks | Verdict |
|---|---|---|---|---|
| A | **Same seed everywhere**: replicate a seed and parameters; every machine runs the generator | tiny payload; late join is free | needs bit-identical results on different CPUs, builds and thread schedules: PCG graph execution order with parallel tasks (*Unsure*), float math, CRT math functions that may pick different implementations by CPU feature (reported for FMA, *Unsure* for our toolchain), version mismatch between friends' builds; one divergence = collision mismatch with no way to detect it before play | **reject for collision and slots** |
| B | **Host generates, replicates a compact layout description; clients build it from authored modules** | determinism needed only for *placement math*, which can be exact integers; clients build the same thing by construction; a hash proves it; works for late join; layout is a few KB | the host spends the CPU (off the game thread); clients must have the same module content (content signature already required) | **recommended** |
| C | A plus a replicated hash of the result, fallback to B on mismatch | cheapest bytes | still needs A's solver to be deterministic enough to match most of the time; two code paths | later, only if layouts become large (they will not) |

### 3.3 Recommendation, and how to make B exact

- The solver is **pure C++ in `GSTNProcGenCore`**: integer grid cells, own integer RNG (`FGSTN_Rng`, one stream per phase, derived from the seed with integer mixing;
  not `FRandomStream`, not PCG), no floats in any decision, no container order dependence. Its output is `FGSTN_LayoutDescription`; the seed is kept for replay,
  "daily seed" and tests (GM.7), **not** for client-side regeneration. A `GenerationVersion` integer is in the header so an old seed replays on an old solver.
- **Placement is exact**: module position = integer cell × integer cell size in cm, rotation in 90° steps only (exact quaternions), so `CellToTransform` is exact
  in float on every machine. Free yaw or organic modules are not allowed in the pool (validator), because they would bring the float question back.
- **Slots** (objective props, spawn zones, patrol routes, pickups, extraction) are authored inside each module as data markers. After the build the **host**
  chooses which slots become live and spawns replicated actors for them; clients receive actors and mission state through normal replication, not a layout.
- **Decoration** (foliage, clutter, decals) may be generated per machine from the module's `DecorSeed` through the engine PCG framework (`GSTNProcGenPCG`); it never
  has collision, navigation or gameplay meaning, so a divergence is invisible to the simulation. Whether PCG runtime generation is stable enough even for that is *Unsure*.

`FGSTN_LayoutDescription` (replicated once, with the hash): `GenerationVersion` (u16), `Seed` (u32), `RulesId` (stable id of the generation rules asset), `ContentSignature`
(u32), `SizeX/Y/Layers` (u8), `Modules[]`, `Hash` (u32). Per module: `ModuleId` (u16, index into the rules asset's **stable id table**, so reordering assets cannot
shift ids, same trick as M8's), `X/Y/Z` (i8), `Rotation` (2 bits plus flags), `DecorSeed` (u16): about **8 B**. A 250-module map is about 2 KB; budget ≤ 8 KB.

### 3.4 The solver (*Proposal*)

1. **Skeleton**: a graph landing zone → mission sites → extraction, with required and optional branches, from the mission set's `RequiredSlots`
   (a mission lists slot types and counts; the solver guarantees the pool can satisfy them or rejects the seed).
2. **Placement**: modules on an integer grid (several layers for verticality), matching `Generation.Socket.*` leaves on adjacent faces (a constraint solver,
   wave-function-collapse style or growing-tree; the algorithm is an implementation choice behind the same output).
3. **Traversability check**: each module declares traversal classes (walkable, jump gap up to X, wall-slide climb, trampoline launch, gravity class). The check
   uses the **base moveset only** (kits change mid-match, so the map must be completable with the least capable one) and treats "up" per module because gravity
   may differ (owner §2d). A map that fails any required path is rejected and the next derived seed is tried (cap on attempts, then fail loudly).
4. **Slot plan** (host only, derived from layout and seed, golden-tested): which slot hosts which objective, spawn zones, patrol routes, drops, pickups.
5. **Emit** the layout and its hash.

### 3.5 Streaming and building

| Option for a module | Pros | Cons / *Unsure* |
|---|---|---|
| **Dynamic level instance** (`ULevelStreamingDynamic`-style, one package per module, transform from the layout) | collision, navigation data and stable names come with the package; authored by designers as maps | each machine must load the same list with deterministic instance names; the host must know which clients have a module visible before replicating its actors (level visibility reporting for dynamically loaded levels: *Unsure*); load-time hitches |
| Packed actor or instanced-mesh actor class spawned on all machines | simple to spawn; one actor per module | stable net names of runtime-spawned static actors: *Unsure*; collision and nav are the class's job |
| World Partition + Level Instance actors spawned at runtime | designer tooling, existing streaming | runtime spawning of Level Instance actors in a procedural map: *Unsure* in 5.8 |

*Proposal for v1:* modules are **dynamic level instances holding only static and non-replicated content**; every replicated gameplay actor in a module is a *slot* the
host spawns, which sidesteps net-name stability of sublevel actors. Spike G2 settles which option, with a collision-digest comparison host vs clients as the exit.

- **Who loads what.** Clients load around their own pawn (a remote player is interpolated, no collision needed; standing on an enemy uses the enemy's collider).
  The host must have collision near **every** player and near awake minions: union of radii around all players (World Partition server streaming policy for several
  players: *Unsure*). Rule: `BuildRadius` (modules) ≥ replication relevancy radius (M8: 8000 cm) plus a margin, otherwise actors appear over a void.
- **Navigation** (host only, *Unsure* on cost): runtime navmesh generation around navigation invokers on players and awake pack leaders, versus per-module
  navigation chunks. The navmesh is Z-up, so modules whose gravity differs use an **authored waypoint graph** with M8's lightweight movement (which integrates the
  local gravity vector) instead of the navmesh; T1/T2 walkers may use the waypoint graph everywhere (cheaper). Spike G3.
- **Hitches**: modules are built a few per frame (§7.3), packages prefetched through Asset Manager bundles, landing zone first.

### 3.6 Loading flow and late join

`ExperienceLoading → Generating (host, off the game thread) → LayoutReplicated (reliable, with Hash) → Building (all machines: landing zone and its radius first,
then the rest in the background) → NavReady (host) → ClientsReady (each client reports its layout hash and the landing zone built; one RPC on the player controller,
not simulation) → PawnsSpawn → ServerStateStart (missions, director)`. The host starts the match when every connected client is ready or a timeout drops it with a
message; a hash mismatch kicks that client back to the lobby (never plays on a different map). Progress is `Message.Generation.*` for the loading screen.

**Late join.** A joiner receives the replicated layout (property replication guarantees delivery), builds it, reports ready, receives the full mission state, sees director
output through live actors, and spawns through `GSTNRespawn`'s `JoinInProgress` placement (near teammates at a safe module, or as a reinforcement that may cost the pool, GM.12).
Content signature, `GenerationVersion` and hash are checked first.

**Budgets** are in §7.3 ([E], confirmed by G1–G3, G6). Reference map (*Proposal*, GM.7): 20 × 20 cells of 40 m (about 800 m square), up to 3 layers, 150–250 modules used;
`HostLoadedModulesSoftCap` default 120 [E] (with four spread players the loaded union approaches the whole map: GM.14).

## 4. Missions and objectives (`GSTNMission`)

### 4.1 Objective kinds (all *Proposal*; the owner named none)

| Kind (`Mission.Kind.*`) | Completes when | Events it reads (messages) | Notes |
|---|---|---|---|
| `Eliminate` | N enemies matching a tag query are KO'd | `Message.Combat.Elimination` (republished KO) | query on `Minion.Type.*`, team, or zone |
| `DestroyTarget` | the tracked target entities are KO'd | same, filtered by the target's identity | targets are entities that can raise KO: a stationary minion type or a fighter. **Destructible props are not an entity kind today** (M4 §7b only breaks surfaces): GM.8 |
| `Activate` | N requestable props completed | `Message.Mode.Verb` | the terminal verb of §2.4; hold time is the ability's |
| `Retrieve` | a held object with an identity tag reaches a zone | `Combat.Event.Held.*` republished (names per M10), zone entry | uses M10 held objects and its carry load; item lost (KO zone, carrier dies) follows the rule's `OnItemLost` (respawn at origin after T) |
| `Defend` | a zone or entity survives T seconds | KO of the defended entity, a timer | pairs with a spawn pattern requested through `Message.Spawn.RequestPattern` |
| `Reach`, `Hold` | players enter a zone (any or all alive), or stay T seconds | zone presence from server-side positions | the Levels goal is a `Reach` |
| `Escort` | a moving friendly entity reaches a zone | KO of the escortee, zone entry | the escortee must be actor-backed (a fighter, counted in `MaxFightersFromMinions`), never a swarm minion |
| `Collect` | N collectibles taken | `Message.Mission.Collected` | GM.5 |
| `Extract` | enough players inside the extraction zone when its countdown ends | verb `CallExtraction`, zone presence | §6.2 |

### 4.2 Authoring and state

Designers author in **data and Blueprint** (modularity §3.8, §3.9): a mission is a `UGSTN_MissionDefinition` asset; a new objective kind is a Blueprint child of
`UGSTN_ObjectiveRule` (`BlueprintNativeEvent OnEvent(view) → delta`, plus a server tick at 2–10 Hz; it runs on the server, outside the simulation, so it may read the
world, but latent nodes are discouraged); on-activate and on-complete effects are `UGSTN_ObjectiveAction` children (open a door, request a pattern, spawn a drop,
play a sequence). Level-placed actors carry designer identity tags (`Mission.Objective.<Name>` ini leaves, §8.2): `UGSTN_ObjectiveTargetComponent`,
`AGSTN_ObjectiveZone`, `UGSTN_RequestableComponent`. In Expedition they are slots inside modules, and the generation validator checks every mission's `RequiredSlots`
are available in the pool.

| Type | Key fields |
|---|---|
| `UGSTN_MissionDefinition` (PrimaryDataAsset) | `Mission.Def.*` tag, title/description, `Objectives[]` of `FGSTN_ObjectiveSpec`, order (`Sequential`, `Parallel`, `Graph` of prerequisites), failure query (events that fail it), time limit, `RequiredSlots[]`, rewards (`FInstancedStruct` extensions, data only), `bOptional`, minimum players |
| `FGSTN_ObjectiveSpec` | `Mission.Objective.*` tag, `Rule` (instanced `UGSTN_ObjectiveRule`), params (target count, tag queries, radius, seconds), prerequisites (tags), `OnActivate[]`/`OnComplete[]` actions, UI text, icon and marker tag (UI extension point), `bHidden`, `bOptional` |
| `UGSTN_MissionSet` | the missions of this Experience: a fixed list (Levels) or a weighted pool with counts and tiers (Expedition), chain edges, extraction rule |
| `FGSTN_ObjectiveState` (replicated POD) | `Index` (u8), `State` (3 bits: Locked, Available, Active, Completed, Failed), `Progress` (u16), `Target` (u16), `TimerEndMs` (u32, optional): about 9 B |

### 4.3 Replication, and why it is not in the rollback simulation

`UGSTN_MissionStateComponent` (GameState component) holds a `FFastArraySerializer` of `FGSTN_ObjectiveState` (≤ 32 entries, push model, an item changes at most 4 Hz)
and a small list of marker positions (static ones replicated once, moving ones at 2 Hz). That is under 0.5 KB/s per client and a full resend on late join.

Why not an NP model or a fragment in each pawn's sync state: (1) it is **global**, not per entity, and NP models are per actor; (2) it must **count once**, and
anything in the simulation re-runs on resimulation (combat.md §5.6: quests and statistics never listen to simulation callbacks); (3) no player predicts "terminal
done", a one-RTT delay is invisible; (4) a global field in every pawn's sync state costs bytes on every update of every player. What *is* simulated stays so:
the player's verb (a predicted hold) and an object being carried (the M10 `Held` link). The mission side only **reads their confirmed events**: progress comes from
server-authoritative, once-only events (the outbox republished as messages, §2.4), never from component overlaps evaluated in the simulation. **Dependency D-2:** the republisher of combat outbox events (KO, held-object events) as `Message.*` is the bridge that modularity §5 item 4 says exists "in the project or a small L3 module"; it is not built, and M12 needs it (owner of the bridge: to decide, not a question for the owner). Zone presence is read by
the server from the Mover sim state of each pawn, as KO zones are (M4 §3b), polled at the objective tick.

### 4.4 Events out, chains and extraction

Out: `Message.Mission.*` messages (activated, progressed, completed, failed, mission completed, extraction opened/started) for UI, audio, the director and respawn, which react by
tag and data. A `UGSTN_MissionChainDefinition` orders missions or opens branches ("N of M primaries done unlocks extraction"). Expedition ends in **success** (extraction completed
with the required players) or **failure** (everyone down and the pool empty, a time limit, a key target lost). Results are a message with a small struct; persistence: GM.3.

## 5. Levels mode (`GSTNMode_Levels`)

- **Authored levels** are maps, listed in a `UGSTN_LevelSetDefinition` (ordered or a graph for a selection screen, GM.3); big levels may use World Partition, reusable
  rooms may be level instances. Each map names its Experience in its world settings, so **features can differ per level** (a boss level activates `GSTNPlatform`,
  a plain one does not). Level change = seamless travel. Designers place: spawners and wave triggers (M8 §10), KO zones, trampolines (`GSTNLevelObjects`, M4 H11b),
  checkpoints, objective actors, collectibles, kit pickups (loadout spec), camera sections.
- **Checkpoints.** `AGSTN_Checkpoint` (order index, section id), **shared by the team**: the first player to touch it (host-side position from the NP server state)
  activates it for everyone. Respawn picks the last activated checkpoint; `UGSTN_RespawnRules` says how many lives (unlimited by default, a counter for the
  score, GM.4), delay, invulnerability frames, whether a fallen player waits for a teammate or all restart. KO by `Health` or by `Zone` use the same rule (data by
  `KOReason`); the death presentation (crystal shatter) plays before the new pawn spawns.
- **Goal.** A `Reach` objective (§4.1) at a goal zone: the level completes when any player or all alive players are inside (`Require`, GM.5). Secondary
  objectives (collect, defeat a boss) are optional missions in the same set. Levels therefore need no goal code of their own.
- **Collectibles** (Mario 3D World style stars, stamps): `UGSTN_CollectibleComponent`, taken server-side once, `Message.Mission.Collected`, team or per-player counting
  and persistence are GM.5 and GM.3; v1 counts per level, no save.
- **Camera sections** (*Proposal*, GM.6). The reference uses mostly fixed and rail cameras; the project's style is not stated. `AGSTN_CameraSection` (volume, optional spline)
  holds a `UGSTN_CameraModeDefinition` (follow, fixed, rail, limited orbit, blend time, player rotation allowed, a soft reference to a depth-marker style). Each client picks the
  section from **its own** pawn (a per-player camera is the safe online default; a shared camera that frames all players is a question). Presentation, but **the camera yaw is
  an input** (`CameraYaw` in `FGSTN_InputCmd`, combat.md §6.1): a cut would flip the stick direction mid-run, so the input source latches the world direction until the stick
  returns to neutral (client-side only, the simulation is untouched). Fixed cameras hurt depth reading: a section sets the depth marker's **local style override**
  ([depth-marker.md](depth-marker.md) §6.5, added for this case; presentation only), and the marker is on by default there. `Camera.*` has no owner yet (§8.2). A 2.5D
  plane lock is **not proposed** (not asked; it would be a simulation change). The camera follow stops on KO and resumes on respawn.
- **Scoring** (*Proposal*): `UGSTN_ScoreRules` maps message tag queries to counters (kills, collectibles, deaths, time) shown at the results screen; whether there
  is a score and a rank is GM.5.

## 6. Expedition mode (`GSTNMode_Expedition`)

### 6.1 Flow

Briefing and kit choice (loadout spec) → experience load → generation and build (§3.6) → drop-pod arrival at the landing zone → missions in any order or a chain → extraction →
results → lobby. `UGSTN_ExpeditionDefinition` holds difficulty rows, the mission pool and counts, the extraction rule, the generation rules, the director rules, drop tables and
reinforcement settings.

### 6.2 Reinforcement and extraction

- **Reinforcement** (state in `GSTNRespawn`, verb `Ability.Mode.CallReinforcement`): a shared pool (initial size, regeneration, floor), one call brings back one
  fallen teammate through a pod at the aim point after a delay; invulnerability frames, the new pawn is a fresh NP entity, its kit is the lobby kit unless the
  loadout spec says otherwise (GM.9). v1 uses the aim point; a thrown beacon would be an M10 object (`Held.Type.Beacon`) and is a later option.
- **Extraction**: opens when the chain says so (`Message.Mission.ExtractionOpened`); the console is an `Activate` target; the verb starts a countdown during
  which a `Defend` objective runs and the director raises pressure; success needs the required players inside the zone at the end (GM.8: all alive, or at least one). No downed state exists, so v1 reinforces only, no revive in place (GM.9).

### 6.3 Drops and backpacks (spawn only)

`UGSTN_DropTable` (data): entries `{ soft pickup definition from the loadout spec, weight, count }`, a delay, a pod actor, a landing marker. The drop service of the
mode (a Blueprintable class in `GSTNMode_Expedition`, called by the verb's handler) spawns the pod and then the **loadout spec's pickup actors** by soft class;
what a kit pickup, support weapon or backpack does (attach on top of the kit implant, stats, swapping) is entirely [combat-loadout-mods.md](combat-loadout-mods.md). This file never names a
backpack field. Mid-match change policy is the loadout rules flag (§2.3). Drops also appear as designer-placed pickups in Levels.

## 7. Spawning and budgets (`GSTNSpawnDirector`)

### 7.1 Patterns

| Pattern | What | M8 mapping |
|---|---|---|
| **Wave** | a group spawned at once or in a timed sequence, by trigger (objective state, zone entry, timer) | `UGSTN_MinionWaveDefinition` + `UGSTN_MinionSpawnerActor` (M8 §10); the director adds triggers and budget limits |
| **Patrol** (and ambient sleepers) | a group placed **dormant** (T3) on a route or spot at generation or section load, walking when a player is within relevancy | normal packs; population counts, awake does not (M8 §4d.1) |
| **Breach** | a telegraphed emergence at marked points near the players when alert is high (the Helldivers dropship or bug breach) | spawn timeline in the `Spawning` state, intangible by its `HurtboxModifier` (M8 §10); `Spawn.Zone.*` points; never within the view cone or a minimum distance |

### 7.2 Director

`UGSTN_SpawnDirectorSubsystem` (server): a **pressure** value per team from recent events (hits taken, health, time since the last fight, objective activity, all by messages);
`UGSTN_SpawnDirectorRules` maps pressure and difficulty to patterns; each enemy type has a `SpawnCost` and the director spends a budget that regenerates over time. Hard limits
(§7.3) always win. Its seeded stream exists **only so tests can replay a run**, not for synchronisation (clients see only spawned entities). Objectives request patterns through
`Message.Spawn.RequestPattern { PatternTag, AnchorTag }`, so `GSTNMission` knows no director. An actor-backed enemy (boss, escortee) is a **promotion** (M8 §10) or a fighter, capped.

### 7.3 Budget table, listen host, 3 clients (all *Proposal*, [E]; M8 §8.3 and §11 give the underlying numbers and spikes)

| Metric | Levels | Expedition | Versus |
|---|---|---|---|
| Players | 1–4 (GM.2) | 1–4 | 2–4 (not specified) |
| Minion population (store slots) | ≤ 60 loaded sections only | **≤ 150** (population rule, M8 §4d.1) | 0 (≤ 8 hazard entities) |
| Awake T0–T2, typical / hard cap | 20 / 40 | 40 / 60; 80 only as a peak ≤ 10 s | 0 |
| `MaxActiveT0` | 32 | 48 (M8) | n/a |
| Platform minions (`gstn.Platform.MaxActors`) | ≤ 4 | ≤ 8 (M8 §4d.5) | 0 |
| Promoted fighters, escortees (`MaxFightersFromMinions`) | ≤ 2 | ≤ 4 | n/a |
| Spawns per frame / per second per zone | 4 / ≤ 12 | 4 / ≤ 12 | n/a |
| Host minion batch, mean / p99 | ≤ 1.0 / 2.0 ms (N=40 row) | ≤ 1.6 / 3.0 ms (N=60 row) | none |
| Host upload for enemies (per-client figure × clients) | ≤ 18 KB/s | ≤ 25 KB/s (+ ≤ 6 KB/s platform proxies); `HostUploadBudgetKBps` 40 | none |
| Mission, respawn and marker state | ≤ 0.2 KB/s per client | ≤ 0.5 KB/s per client | ≤ 0.1 |
| Layout description | n/a (authored maps) | ≤ 8 KB once per join | n/a |
| Generation, build, navmesh | n/a | generation ≤ 1.5 s off the game thread and ≤ 1 ms/frame on it; build ≤ 3 modules and ≤ 4 ms/frame on every machine; landing zone playable ≤ 8 s after the layout; runtime navmesh ≤ 2 ms mean (*Unsure*) | n/a |
| Time to first control after "start" | ≤ 5 s | ≤ 20 s including generation and the landing-zone build | ≤ 5 s |

If a limit is missed, the order of M8 §4d.1 applies (T0 cap, T0 rate to 10 Hz, puppet cap) and the director's cap drops before the host's own frame suffers; the host's maximum N is
recorded honestly. The host's own hits are not lag-compensated (host advantage, M8 §4d.1, existing question M4.24); the director adds no further advantage.

## 8. Types, plugins and tags

### 8.1 Types (one row per type; *Proposal*)

| Type | Kind | Plugin | Purpose |
|---|---|---|---|
| `UGSTN_MissionDefinition`, `UGSTN_MissionSet`, `UGSTN_MissionChainDefinition`, `FGSTN_ObjectiveSpec`, `UGSTN_ObjectiveRule`, `UGSTN_ObjectiveAction` | data, Blueprintable | `GSTNMissionCore` | §4.2 |
| `UGSTN_MissionSubsystem`, `UGSTN_MissionStateComponent`, `UGSTN_ObjectiveTargetComponent`, `AGSTN_ObjectiveZone`, `AGSTN_ExtractionZone`, `UGSTN_ObjectiveMarkerComponent` | world subsystem, components, actors | `GSTNMissionRuntime` | server rules, replicated state, markers |
| `UGSTN_GA_ModeVerb`, `FGSTN_VerbSpec`, `UGSTN_RequestableComponent`, `UGSTN_VerbSubsystem` | ability, struct, component, subsystem | `GSTNModeVerbs` | §2.4 |
| `UGSTN_RespawnRules`, `FGSTN_RespawnState`, `AGSTN_Checkpoint`, `IGSTN_PawnSpawnService`, `UGSTN_RespawnSubsystem` | data, POD, actor, interface, subsystem | `GSTNRespawnCore/Runtime` | rules: `Policy {Checkpoint, Reinforcement, Stocks}`, delay, limit, placement (`Checkpoint`, `DropPod`, `NearTeam`), invulnerability, `JoinInProgress`; the GameMode or project implements the service |
| `UGSTN_SpawnDirectorRules`, `UGSTN_SpawnPatternDefinition`, `UGSTN_SpawnDirectorSubsystem`, `AGSTN_SpawnZone`, `AGSTN_PatrolRoute`, `IGSTN_SpawnSink` | data, subsystem, actors, interface | `GSTNSpawnDirectorCore/Runtime` | §7 |
| `UGSTN_ModuleDefinition`, `UGSTN_GenerationRules`, `FGSTN_LayoutDescription`, `FGSTN_Rng`, `UGSTN_LayoutSolver`, `UGSTN_LevelBuilderSubsystem` | data, struct, pure class, subsystem | `GSTNProcGenCore/Runtime` | `UGSTN_ModuleDefinition`: soft level, footprint in cells, sockets, slots (`Generation.Slot.*`), traversal classes, weight, tags. `UGSTN_GenerationRules`: pool, size, attempt cap, mission-site rules |
| `UGSTN_LevelSetDefinition`, `AGSTN_CameraSection`, `UGSTN_CameraModeDefinition`, `UGSTN_ScoreRules` | data, actor | `GSTNMode_Levels` | §5 |
| `UGSTN_ExpeditionDefinition`, `UGSTN_DifficultyDefinition`, `UGSTN_DropTable` | data | `GSTNMode_Expedition` | §6; difficulty rows: map size, mission counts, enemy tier mix, `SpawnCost` multiplier, alert growth, reinforcement pool, time limit, scaling by player count |

Every behaviour type has a Blueprint form with a `BlueprintNativeEvent` entry point and an example Blueprint in the plugin's test content (modularity §3.8, §7.1 item 11);
no hard content references between plugins (modularity §3.9): modes reference module, mission and pickup assets by soft reference and tag.

### 8.2 Gameplay tags (*Proposal*; one owner per root, modularity §4.1, §4.2b)

| Root | Leaves | Owner | Source |
|---|---|---|---|
| `Mission.Def.*`, `Mission.Objective.*` | designer identity of missions and objectives | root `GSTNMission`; leaves the mode or content plugin | root native, leaves **ini** (`Config/Tags/<Mode>.ini`) |
| `Mission.Kind.*`, `Mission.State.*` | `Eliminate DestroyTarget Activate Retrieve Defend Reach Hold Escort Collect Extract`; `Locked Available Active Completed Failed` (names of the replicated enum, derived, never granted) | `GSTNMission` | native |
| `Message.Mission.*` | `ObjectiveActivated Progressed Completed Failed`, `MissionCompleted Failed`, `Collected`, `ExtractionOpened Started` | `GSTNMission` | native |
| `Message.Mode.Verb`, `Ability.Mode.*`, `Command.Mode.*`, `Combat.Interact.Target.Objective` | verbs of §2.4; designer verbs as ini leaves | `GSTNModeVerbs` (`Command.*` root: `GSTNInput`; `Combat.Interact.*` root: `GSTNLink` or the probe's plugin, the leaf is ours) | native + ini |
| `Message.Respawn.*` | `Down Respawned PoolChanged` | `GSTNRespawn` | native |
| `Spawn.Zone.*`, `Spawn.Pattern.*`, `Message.Spawn.*` | designer zone names and pattern identities; `RequestPattern PatternStarted Alert` | `GSTNSpawnDirector` | roots native, leaves ini |
| `Generation.Module.*`, `Generation.Slot.*`, `Generation.Socket.*`, `Generation.Biome.*`, `Message.Generation.*` | module and slot identities, socket types, loading progress (`LayoutReady ModulesBuilt Failed`); native slot leaves `Start Extraction Objective SpawnZone Patrol Drop` | `GSTNProcGen` | roots native, leaves ini |
| `Experience.Mode.*`, `Experience.Trait.*` | `Levels Expedition Versus`; `PvE PvP Coop Procedural` | existing root (modularity §4.2), leaves by the mode plugin or `GSTNExperience` | ini |
| `Camera.*` | `Camera.Mode.<Name>` leaves ini | **owner open**: decided with `depth-marker.md` (a camera plugin, or `GSTNMode_Levels` until a second consumer exists) | ini |

**No new `Mode.*` root.** `Experience.Mode.<Name>` is already the mode identity (modularity §4.2b); a second root for one concept would break one-owner-per-root. `Ability.Mode.*` and
`Command.Mode.*` use "Mode" only as a family segment. Mission state is never a tag granted on a pawn (ASC tags are simulated); others read the replicated state or the messages. Tags
the director or respawn react to are data queries, not framework assumptions (modularity §3.7 rule 4). Each root joins `tag-owners.json` when its plugin is created.

## 9. Testing and milestones

### 9.1 Spikes (record results in combat.md §17; each has an exit)

| Id | Spike | Exit |
|---|---|---|
| G1 | Solver determinism and size: `GSTNProcGenCore` over 10 000 seeds on two platforms (editor and a Linux CI job) | layout hashes identical; all required paths valid after the attempt cap; ≤ 8 KB; solve ≤ 1.5 s |
| G2 | Module instantiation on host + 2 clients: dynamic level instance vs packed actor | collision digest of built modules equal on all three machines; ≤ 4 ms game thread per frame; late join builds the same map |
| G3 | Navigation on the host with runtime modules; waypoint-graph fallback | build time and memory per module recorded; minions reach their targets across a module seam |
| G4 | Experience order: no pawn simulates before the Experience is Active; the GameState component action | zero NP ticks of a pawn before Active; component present on host and clients (this is S-K of modularity §6) |
| G5 | Verb seam under forced rollbacks and 2 % loss: ability → outbox → message | exactly one authoritative event per confirmed verb; refusal leaves no state; interact-probe mismatch rate recorded |
| G6 | Late join during an Expedition | joiner ready ≤ 20 s; state equal to the host's after one update |

### 9.2 Tests

Automation: `GSTNMission.Core.*` (graph, progress, chains, failure queries), `GSTNProcGen.Core.*` (determinism goldens, sockets, traversal, slot plan), `GSTNRespawn.Core.*` (policy by
`KOReason`, pool arithmetic), `GSTNSpawnDirector.Core.*` (pressure, cost budget, caps). Harness (`gasnpp_test.py`, testing.md; listen server host + 2 clients is the reference, dedicated + 2
clients a second run): `scenario_layout_replicate` (digests equal, late joiner equal), `scenario_mission_replicate` (state equal within one update; **an `Eliminate` of 10 kills with forced
rollbacks counts exactly 10**), `scenario_verb_confirm` (predicted hold, server refusal, one cancel), `scenario_respawn_checkpoint` (KO by zone and by health, checkpoint activated by a
remote client, no leaked NP entities), `scenario_director_budget` (caps respected; 3 headless clients as in the M8 spikes), `scenario_experience_order`, `scenario_mode_toggles` (shield
unbound in Levels and Expedition, bound in Versus; a Sim toggle change mid-match is refused).

### 9.3 Milestones (*Proposal*; numbering continues after M10; plans follow the style of plans/README.md)

| Milestone | Content | Needs |
|---|---|---|
| **M12 Mode framework + Levels** | S-J and S-K of modularity §6 if not done, G4, G5; `GSTNMission`, `GSTNModeVerbs`, `GSTNRespawn`, `GSTNMode_Levels`, camera sections, one test level; mission and respawn tests | M3 (ability base, loadout), M4 (KO event and zone, rules asset), M6 (interact probe, defence rules), M5 E10 (validator mechanism); M8 only for waves, so a first level can use fighters and dummies |
| **M13 Expedition** | G1–G3, G6; `GSTNProcGen`, `GSTNSpawnDirector` + `GSTNSpawnDirectorMinion`, `GSTNMode_Expedition`, drops (needs the loadout spec's pickups), extraction and reinforcement | M12, M8 (minions, waves, platforms optional), M10 (retrieve), the loadout spec |
| M14 Versus | an Experience on the same frameworks (stocks, shield on, authored arenas) | M12, M6 |

M12 forces the owner's decision on adopting Game Features and Experiences now (existing question Mod.1). The order Levels then Expedition proves the framework on the
smaller mode first (GM.1).

## 10. Risks

1. **Scope**: two modes and five frameworks. Mitigation: Levels first; each framework is small, tested alone and optional; Expedition starts only when M12 is merged.
2. **Generated maps that are dull or unreachable** for a platformer with wall slides, trampolines and changing gravity: hand-authored modules, a traversability validator,
   seed rejection with an attempt cap, and a visual layout viewer in `GSTNProcGenEditor`.
3. **Runtime level loading over the network** (*Unsure* points of §3.5): spike G2 before building the builder; fallback is a fixed set of pre-assembled map chunks.
4. **Host frame**: generation, module building, navmesh, the director and 60 awake minions share the host's frame; budgets of §7.3, loading screen hides the
   spike, director caps lowered first.
5. **Seams D-1 (ability → outbox event key) and D-2 (outbox → messages republisher)** do not exist yet; without D-1 verbs need the inbox path. Spike G5 first.
6. **Interact probe versus non-simulated objective state**: a rare refused hold; mitigated by early arming and UI, with a recorded-input fallback.
7. **Players far apart**: the host loads the union of regions; a leash or a regroup rule (GM.14) or a higher soft cap.
8. **Content signature and version drift** between friends' builds: refused at join with a clear message (modularity §3.9); `GenerationVersion` protects replays.
9. **Camera and input**: cuts change the stick plane (latch, §5) and fixed cameras need the marker override; **late join** during a mission or extraction has edge cases (GM.12).

## 11. Questions for the owner

The questions GM.1–GM.15 (in Italian, for the owner) are in [open-questions-m4-m10.md](open-questions-m4-m10.md), section "Modalità di gioco". Each states the hypothesis this spec is written on. The ones that shape M12 most are GM.1 (Levels first), GM.4 (respawn in Levels), GM.6 (camera style) and GM.9 (reinforcement instead of revive in place).

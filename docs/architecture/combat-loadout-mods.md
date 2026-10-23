# Combat M11 — Kits in play: pickups, support weapons, mods, special slots, implants, backpacks (specification)

Status: **Proposal**, docs only, for later implementation. Owner direction of 2026-10-23
([game-design-direction.md](game-design-direction.md) §6d, which wins over this file). Builds on the M3 data model
([combat-m3-moves-kits.md](combat-m3-moves-kits.md) §5, §7.7, §7.8, §8–§10) and follows
[modularity-and-tags.md](modularity-and-tags.md) §3.7–§3.10 and §4.

Notation: *Decision* = stated by the owner or decided in another document. *Proposal* = this document's suggestion, to be
confirmed (questions in [open-questions-m4-m10.md](open-questions-m4-m10.md), section "M11"). *Unsure* = not verified against
engine source in this session.

Section numbers are local to this file.

## 1. Scope

In: the owner's terminology mapped onto M3 types; changing kit mid-match by picking one up; support weapons that keep the kit's
stats; swapping special moves among the available ones; kit **mods** (cosmetic, stats, moveset and specials); the kit **implant**
and weapon visuals; Helldivers 2-style **backpacks** that attach on top of the implant; world pickups and drops of kits, support
weapons and backpacks; the seams M3 should prepare now so none of this needs an M3 rewrite.

Out: game modes and where pickups spawn ([game-modes.md](game-modes.md)); the loadout UI layout; economy and unlocks
(question M11.12); held objects ([combat-m10-held-objects.md](combat-m10-held-objects.md), a different system: an
object carried in the hand, not equipment).

## 2. Owner direction (2026-10-23) and the rule behind it

*Decision* (owner): kit = stats + moveset; the kit can be changed mid-match by picking one up; support weapons inherit the kit's
stats; no change of the primary weapon separately from the kit mid-match for now; kits can be modded (cosmetic changes by the
mods chosen) and mods can affect special moves and the base moveset (e.g. a longer sword with slower attacks); special moves can
be swapped among the available ones; the kit is shown by an implant mesh on the back, the weapon can be a sword plus implants
reaching hands or feet; backpacks as in Helldivers 2, modular, attaching **on top of the implant** like the standard port of an
electronic device; PvE has no shield.

**Everything is a pluggable gameplay ability** (*Decision*, owner, 2026-10-23: "praticamente ogni cosa dovrebbe essere un'abilità
pluggabile", as in Fortnite). In this file that means: every verb a kit, weapon, support weapon, mod or backpack gives the player
(attack, special, dodge, jump, weapon switch, jet-pack burst, picking up a kit) is a gameplay ability granted by data, with a
binding to a command; the systems below only decide **which abilities are granted and with which data**. No kit, mod or
backpack behaviour is a code path in a framework class. General rule: modularity-and-tags.md §3.10.

## 3. Terminology (*Decision*) and where it lives in M3

| Owner term | Technical meaning | M3 type (existing) | Notes |
|---|---|---|---|
| **Kit** | The **stats**, kept even while a support weapon is active; also the kit-level verbs (movement, dodge, jump, switch) | `UGSTN_PrimaryKitDefinition`: `Stats (FGSTN_KitStats)`, `InitStatsEffect`, `Meters`, `CommandSet`, `KitBindings`, `Passives`, `HurtboxSet`, `LocomotionAnimLayer` | Unchanged. "Kit" in code always means this asset |
| **Weapon** ("arma") | The **moveset of the primary kit**, without support weapon. May be fists and kicks and show a sword mesh only in some moves | `UGSTN_PrimaryKitDefinition::PrimaryWeapon` → `UGSTN_WeaponDefinition` with `Combat.Weapon.Role.Primary` (`Bindings`, `AllowedAlternatives`, `AnimLayer`, `VisualActor`) | The primary weapon is **owned by the kit**: no loadout field chooses it separately (§5) |
| **Support weapon** | A big extra weapon (Helldivers) to switch to when needed; its moveset replaces the weapon's, the **kit's stats stay** | `UGSTN_WeaponDefinition` with `Combat.Weapon.Role.Support`, in `FGSTN_LoadoutData::AdditionalWeapons`; switch = M3 §7.7 | Default: no stats of its own (§6) |
| **Special move** | A named move in a special slot, swappable among the available ones | Ability bound to a `Command.Special.*` command; alternatives = `UGSTN_WeaponDefinition::AllowedAlternatives` (and kit bindings), choice = `FGSTN_LoadoutData::SlotCustomisations` | Identity tag `Ability.Combat.Special.<Name>` (modularity-and-tags.md §4.2b) |
| **Mod** | A kit modification: look, stats, moves, specials | new: `UGSTN_KitModDefinition` (§8) | New plugin `GSTNKitMods` |
| **Implant** | The visible sign of the kit: a mesh on the back (and, for the weapon, implants reaching hands or feet) | extends M3's presentation (`UGSTN_SkinComponent`, weapon `VisualActor`) | §9 |
| **Backpack** | Helldivers 2 backpack, modular, on top of the implant | new: `UGSTN_BackpackDefinition` (§10) | New plugin `GSTNBackpack` |

M3 already covers most of the data. What it lacks are **seams** (§11), so mods, backpacks and pickups can be added later by
optional plugins without touching `GSTNCombat`'s types again.

## 4. Changing kit mid-match (pickup)

*Decision*: a kit can be changed mid-match by picking one up. Flow (*Proposal*):

1. A **kit pickup** in the world (`AGSTN_EquipmentPickup` with a kit payload, §12) registers an interact candidate. The interact
   probe of [M6](combat-m6-defence-links.md) §8 projects `Interact.Target.Equipment` when it is the best target, and the
   **hold** of the interact button (owner, 2026-10-22: hold = pick up objects) fires the kit's pickup ability, an ordinary timeline
   ability (`UGSTN_GA_EquipPickup`: a short "plug in" animation of the implant) granted to every pawn by the default ability set.
2. At the ability's `RequestFrame` the **server** validates (pickup still there, reach, rules of the mode: `bAllowMidMatchKitChange`)
   and applies the change through M3's loadout component: `UGSTN_LoadoutComponent::ServerApplyLoadoutChange(FGSTN_LoadoutChange)`,
   which edits `Applied`, runs `UGSTN_LoadoutRules`, re-grants and increments `Revision` exactly as M3 §10 does at respawn.
3. Every machine rebuilds the compiled table from the replicated loadout; the **revision gate** of M3 §7.8 keeps presses buffered
   until the new table is present, so no command resolves against the wrong kit. The pickup animation (≈ 20–30 frames) covers the
   round trip on a normal connection; on the host it is instant.
4. **Safe point**: the server cancels the active timeline (combat request `CancelTimeline`) before removing the old grants, so the
   sync state never points at a registry entry that no longer exists; the change is never applied during hitstun or hitstop (the
   pickup ability lists them in its blocked tags).

What carries over (*Proposal*, all open as M11.2–M11.5): `DamagePercent` (health progress) and broken state stay; if the new kit's
`MaxHealth` cap differs, `Progress` is preserved (percent rescaled); meters keep their values, clamped to the new maxima; the
support weapon stays if the new kit's `AllowedAdditionalWeapons` accepts it, otherwise it drops as a pickup; the backpack always
stays (it is independent of the kit); the **old kit drops as a pickup** where the player stood (Helldivers style), with its mods.
Mods and special choices: the picked-up kit brings the **player's saved configuration for that kit** if one exists, else the
pickup's own (question M11.4).

Prediction: the pickup ability and its animation are predicted like any timeline; the kit change itself is **server-applied**
(out of band, M3 §7.8), never predicted, because it re-grants abilities. The client sees its new moveset one round trip after the
host accepted, hidden by the animation. A refused pickup ends the animation with a fumble cue.

**Not supported for now** (*Decision*): changing the primary weapon separately from the kit mid-match. `UGSTN_LoadoutRules`
rejects a `FGSTN_LoadoutChange` that targets the primary weapon alone; the data model keeps the weapon inside the kit, so allowing it
later is a rules change, not a type change.

## 5. Kit and weapon in the loadout

- `FGSTN_LoadoutData::PrimaryKit` selects kit **and** weapon (the kit's `PrimaryWeapon`). No new field.
- A kit may offer **weapon variants** only as mods (§8), not as a free choice of weapon, until the owner decides otherwise.
- The verbs that belong to the kit (dodge, jump, switch, pickup) stay in `KitBindings`; the moveset belongs to the weapon. When a
  support weapon is active only the weapon's bindings are swapped (M3 §9: entries filtered by `ActiveWeaponIndex`), so kit verbs
  keep working with the support weapon, as the owner's definition of "kit" requires.

## 6. Support weapons

- *Decision*: a support weapon **inherits the kit's stats**. Default: `UGSTN_WeaponDefinition::StatsEffect` is empty for
  `Combat.Weapon.Role.Support` and `SwapWeaponStats` (M3 §7.7) changes nothing. The M3 mechanism stays as a designer knob (a very
  heavy weapon could slow the carrier), with a validator warning when a support weapon changes stats (question M11.6). M3's test
  content `WPN_Test_Support` (+40 % walk) exists to test the mechanism only; production support weapons follow the rule above.
- Switching: M3 §7.7 (a timeline ability with a `WeaponSwitch` track, bound in the **kit**). One support weapon at a time
  (`MaxAdditionalWeapons` = 1 in the reference kits, *Proposal*).
- Getting one: chosen in the loadout, or picked up in the world (Helldivers-style drop, spawned by the mode), same flow as §4 with
  a support-weapon payload. Dropped on death as a pickup (question M11.7).
- Ammunition, cooldown or charges, if any, are attributes of the weapon's abilities (GAS cost/cooldown), not a new system
  (question M11.8). A backpack can feed a support weapon (§10).

## 7. Special slots and swapping

- A kit's weapon declares its special commands (`Command.Special.Neutral/.Side/.Up/.Down` in the standard set, more by designer
  ini) and, per command, `AllowedAlternatives` (M3 §8). The player picks one alternative per slot in the loadout
  (`SlotCustomisations`); the compiled table binds it (M3 §9).
- Which alternatives exist is data on the weapon, extended by mods (§8: `UnlockedAlternatives`). The UI lists them by the
  `Ability.Combat.Special.*` identity tag.
- When swapping is allowed: in the loadout (pre-match, between levels) by default; mid-match only where the mode allows it (e.g.
  at a checkpoint or supply point), through the same `ServerApplyLoadoutChange` path (question M11.9).

## 8. Mods (`GSTNKitMods`)

*Decision*: kits can be modded; mods change the look and can affect specials and the base moveset (longer sword, slower attacks).

**`UGSTN_KitModDefinition`** (primary data asset, `Mod.<Name>` identity tag, §13):

| Field | Meaning |
|---|---|
| `FGameplayTag Slot` | Mod slot type (`Mod.Slot.Blade`, `Mod.Slot.Core`, ...; designer ini). A kit lists its slots and how many |
| `FGameplayTagQuery CompatibleKits` | On `Kit.*` / `Weapon.*` identity and classification tags |
| `TArray<FGSTN_ModVisual> Visuals` | Cosmetic changes: mesh swaps or added attachments on the implant and weapon visuals (by socket or by visual slot name), material parameters; presentation only |
| `TSubclassOf<UGameplayEffect> StatsEffect` + `FGSTN_KitStatModifiers` | Stat changes applied with the kit's init effect (SetByCaller, as M3 §5) |
| `TArray<FGSTN_MoveModifier> MoveModifiers` | Changes to moves selected by a filter (below) |
| `TMap<FGameplayTag, TArray<TSubclassOf<UGameplayAbility>>> UnlockedAlternatives` | Extra special alternatives per command |
| `TArray<FGSTN_AbilityBinding> AddedBindings` / `FGameplayTagContainer RemovedCommands` | A mod may add or remove a verb (everything is an ability, §2) |
| `TArray<TSubclassOf<UGameplayAbility>> Passives` | Passive abilities granted while the mod is equipped |

**`FGSTN_MoveModifier`**: `FGameplayTagQuery MoveFilter` (on the ability's identity tags and the timeline's tags, e.g. "every
`Ability.Combat.Attack.*` of this weapon", or one special), `float TimeScale` (1 = unchanged; > 1 slower), `FVector
HitboxScale` (in each hitbox's local space), `float ReachScale` (along the hitbox's authored `ReachAxis`, for "longer sword"),
`float DamageScale`, `float KnockbackScale`, `float StaggerForceScale`, `float HitstopScale`, `FGameplayTagContainer AddedTags`.

**Baked at compile time, never evaluated in the simulation** (*Proposal*, the key design choice). The loadout compiler (M3 §9)
calls the mods plugin through the compile extension (§11) with the base timeline and the ordered list of equipped mods; the
plugin returns a **derived timeline** (`UGSTN_CombatTimeline`, transient, built on every machine from the same inputs). The
registry stores it like any timeline override, so the rollback simulation sees only static data and pays nothing for mods.

- **Determinism.** Same inputs, same order, same integer rounding on every machine. Registry order (M3 §9 sorts by path) uses a
  stable id for derived timelines: `hash(base timeline path, mod ids in slot order)`. The loadout revision already covers a change
  of mods; the content signature of modularity-and-tags.md §3.9 includes mod definitions.
- **Time scaling keeps integer frames.** Every key frame `f` of a phase becomes `round(f × TimeScale)`, with order preserved (no two
  keys collapse; a 1-frame window stays ≥ 1 frame). Cancel windows and the cancellable tail scale with their keys unless the key is
  marked `bFixedTime` (e.g. a parry window that must not grow). The montage is played at `1 / TimeScale` so animation and frames
  agree (M3 syncs montages to the combat clock). Movement tracks scale in time and keep their distance.
- **Reach scaling** moves and stretches hitboxes along `ReachAxis` (authored per hitbox in M4 hit data, new field, default none:
  such a hitbox ignores `ReachScale`). Baked socket tracks (M5) are transformed the same way. M5 validates that a mod's longer
  blade mesh and the scaled hitboxes agree (bounds check, warning).
- **Order and stacking.** Modifiers apply in slot order; scales multiply; results are clamped by the kit's `ModLimits`
  (`MinTimeScale`, `MaxTimeScale`, `MaxReachScale`) so no combination breaks a move. One mod per slot (question M11.13).
- **Specials**: a mod changes a special through the same `MoveModifiers` (filter on `Ability.Combat.Special.<Name>`) or swaps the
  alternatives list. Owner's example: the "long blade" mod has `ReachScale 1.3` on every sword attack and `TimeScale 1.15` on the
  heavy ones, plus a longer blade mesh.

Mods on support weapons and backpacks: not asked; the definition allows `CompatibleKits` to match a support weapon's tags, so it is
a data choice later (question M11.10).

## 9. Implants and weapon visuals

*Decision*: the kit is shown by an implant mesh on the back (or anything that shows the kit); the weapon can be a sword plus
implants reaching hands or feet; a "weapon" may use the sword mesh only in some animations.

- **`FGSTN_EquipmentVisual`** (on kit, weapon, mod, backpack): `TArray<{ FName Socket; TSoftObjectPtr<UStaticMesh or
  USkeletalMesh> Mesh; FTransform Offset; FName VisualSlot; }>`. Standard sockets on the character skeleton (*Proposal*):
  `Implant_Back`, `Implant_Arm_L/R`, `Implant_Hand_L/R`, `Implant_Leg_L/R`, `Implant_Foot_L/R`, `Weapon_Hand_R/L`.
- **Back implant with a standard connector.** Every back implant mesh carries a socket `Connector_Backpack`; backpacks attach to
  it (owner: on top of the kit, like a standard port), so **any backpack fits any kit** and the implant stays visible. The
  validator (M5) checks that every back implant has the connector socket.
- **Weapon mesh shown only in some moves**: a timeline track `FGSTN_VisualTrack` (presentation only) shows or hides named
  visual slots (e.g. `Sword`) over frame ranges; outside it the weapon slot follows the weapon's default (`bShownByDefault`).
  Fists-and-kicks weapons simply have no default-shown mesh.
- **`UGSTN_EquipmentVisualComponent`** (`GSTNCombat`, presentation): extends M3's `UGSTN_SkinComponent`; composes skin, kit
  implants, weapon visuals, mod visuals and registered attachments from other plugins (backpack) through
  `RegisterAttachmentProvider` (an interface, so `GSTNCombat` never names backpacks). Applied on spawn and on every loadout
  revision; no simulation code reads it. Async-loads the soft references; shows the previous look until the new one is ready.

## 10. Backpacks (`GSTNBackpack`)

*Decision*: backpacks as in Helldivers 2, modular, attaching on top of the kit implant through a standard connector.

**`UGSTN_BackpackDefinition`** (primary data asset, `Backpack.<Name>` identity): `FGSTN_EquipmentVisual Visual` (attached to
`Connector_Backpack`), an **ability set** (`Abilities` with command bindings, `Passives`, `Effects`; everything is an ability, §2:
a jump-pack burst, a deployable shield dome, a supply drop for teammates, a guard drone), `FGameplayTagQuery CompatibleSupportWeapons`
(a backpack that feeds a support weapon, e.g. team reload), `bDropOnDeath`.

- **Entry source.** The backpack's bindings enter the compiled table as their own source (M3 §9 `EGSTN_EntrySource`, extended
  through the compile extension of §11: a registered source id, not an enum value M3 must know). They stay bound whatever weapon
  is active.
- **Command.** `Command.Backpack.Use` (native, `GSTNBackpack`), bound by default to a designer button; which button is open
  (question M11.11).
- **Getting one**: loadout field (through the loadout extension, §11), or a world drop (§12). One backpack at a time; picking up
  another drops the current one. A kit change keeps the backpack.
- **"No shield in PvE"** concerns the PvP shield action of M6 (`GSTNDefence`). A backpack that projects an energy dome is a
  different, item-based ability; whether such backpacks exist in PvE is the owner's call (question M11.11).

## 11. Seams M3 should prepare now (small, no new feature)

These keep M11 additive. They are cheap while M3 is being written and costly afterwards.

1. **Loadout extensions**: `FGSTN_LoadoutData::Extensions` (`TArray<FInstancedStruct>`, base `FGSTN_LoadoutExtension`), serialised
   in `NetSerialize` and the save game; unknown types dropped with a warning (plugin disabled). Mods (`FGSTN_LoadoutModsExt`) and
   backpacks (`FGSTN_LoadoutBackpackExt`) live there, so `FGSTN_LoadoutData` never names them.
2. **Compile extension**: `IGSTN_LoadoutCompileExtension` registered at startup, called by `Compile` with the loadout and the
   assets: may add entries with a **registered source id**, replace a timeline override by a derived one (stable id), add grants
   and effects. Pure, deterministic, ordered by an explicit priority.
3. **Rules extension**: `UGSTN_LoadoutRules` calls registered validators (mods compatibility, backpack/support pairing).
4. **Mid-match apply**: `ServerApplyLoadoutChange(FGSTN_LoadoutChange)` with the safe-point rule of §4 (cancel the active timeline,
   then re-grant), reusing the M3 §10 apply path and the §7.8 revision gate. M3 §10 already says "apply now if the pawn is alive
   and the game allows it": this item makes that path explicit and tested.
5. **Visual composition**: the skin component accepts attachment providers (§9), so kit implants and backpacks plug in.
6. **Support weapon stats default**: support weapons ship without `StatsEffect` (§6).

**What M3 takes** (answer of the M3 implementation thread, 2026-10-23; C12 in progress, C13–C15 to come):

| Seam | In M3 | Left to M11 |
|---|---|---|
| 1 Loadout extensions | a reserved, versioned, empty extension block in `NetSerialize` and the save game, so the format can grow without a redirect | the typed `TArray<FInstancedStruct>` with base `FGSTN_LoadoutExtension` and its wire type ids |
| 2 Compile extension | interface and registry only, called by `Compile` (C15, if it fits a short commit) | implementations; if it does not fit C15, the whole seam |
| 3 Rules validators | yes (C12/C15) | — |
| 4 Mid-match apply | yes: it is M3's `scenario_loadout_change` (C12/C15) | the pickup ability and actor (§4, §12) |
| 5 Attachment providers | interface only on the skin component (C15, if it fits) | implementations; if it does not fit C15, the whole seam |
| 6 Support weapon stats default | yes, data default (C12) | — |

M3 also records that the kit's primary weapon never changes mid-match, so its weapon-switch track means primary ↔ support only.

## 12. World pickups and drops (`GSTNPickups`)

- `AGSTN_EquipmentPickup` (actor) with a payload `FInstancedStruct` (base `FGSTN_PickupPayload`): `FGSTN_KitPickupPayload { kit id,
  mods, special choices }`, `FGSTN_SupportWeaponPickupPayload`, and the backpack plugin's `FGSTN_BackpackPickupPayload` registered
  by `GSTNBackpack` (the pickups plugin never names backpacks). Applying a payload produces a `FGSTN_LoadoutChange`.
- Interaction: an interact candidate provider (M6 §8) projecting `Interact.Target.Equipment`; bound to the **hold** of the
  interact button. The probe, its focus on what the player points at (so one weapon among many on the ground is clearly the one
  the button takes) and the `Interact.*` root live in the plugin `GSTNInteract` (*Decision*, owner answer to Mod.20, 2026-10-23;
  [interaction.md](interaction.md)); `GSTNPickups` is one of its candidate providers.
- Server-authoritative actor (replicated, dormant until touched); the pickup ability is predicted, the grant is not (§4).
  Simultaneous pickups: earliest `RequestFrame`, then lower player id (same rule as M10).
- Spawning: by the mode (supply drops, level placement), [game-modes.md](game-modes.md).

## 13. Gameplay tags

| Tag / root | Owner | Source | Notes |
|---|---|---|---|
| `Kit.*`, `Weapon.*`, `Ability.Combat.Special.*`, `Combat.Weapon.Role.Support` | existing (`GSTNCombat`, M3; §4.2b) | root native, leaves ini | unchanged |
| `Mod.*` (identity), `Mod.Slot.*` (slot types) | `GSTNKitMods` | roots native, leaves ini (designers) | §8 |
| `Backpack.*` (identity) | `GSTNBackpack` | root native, leaves ini | §10 |
| `Command.Backpack.Use` | `GSTNBackpack` | native | §10 |
| `Interact.Target.Equipment` | `GSTNPickups` | native, projected | §12 |
| `Combat.Event.Loadout.KitChanged`, `.SupportWeaponChanged`, `.BackpackChanged`, `.Picked`, `.Dropped` | `GSTNCombat` (`Loadout.*` events of M3's component; `Picked/Dropped` by `GSTNPickups`) | native | presentation, UI, missions |

## 14. Simulation and network summary

| Thing | Where | Rollback |
|---|---|---|
| Active weapon (primary / support) | combat sync state `ActiveWeaponIndex` (M3) | yes |
| Kit, support weapon, backpack, mods, specials | replicated `FGSTN_AppliedLoadout` + `Revision`, server-applied out of band (M3 §7.8) | gate only; never predicted |
| Mod effects on moves | derived timelines baked at compile on each machine | static data |
| Backpack ability state (charges, cooldown) | GAS attributes/effects in the NP fork, like any ability | yes |
| Visuals (implants, mods, backpack mesh) | presentation component | no |
| Pickups | replicated actors, server-authoritative | no |

Listen host: applying a loadout change re-grants abilities on the host's ASC and rebuilds one table per player; budget per change
≈ one compile (ms, off the fixed tick) and one revision write. Late join receives `Applied` and builds the table before simulating.

## 15. Impact on other milestones and plans

| Milestone / plan | Impact |
|---|---|
| **M3** (in progress) | The six seams of §11 (small). Terminology already matches. Test content `WPN_Test_Support` keeps its stat change only as a mechanism test (§6). |
| **M4** / `plans/m4-hits-plan.md` | Hit data gets an optional `ReachAxis` per hitbox (H1), and the reaction/hitstop rules already take the per-hit scales a derived timeline writes; no simulation change. |
| **M4b** / `plans/m4b-parts-plan.md` | None: players stay one part; equipment is not a part. |
| **M5** / `plans/m5-editor-plan.md` | Preview a timeline with a mod list applied (derived timeline), validate reach vs mesh bounds, connector socket on back implants, mod limits; loadout validator covers extensions. New editor tooling can live in `GSTNKitModsEditor` (client of M5 hooks, like `GSTNPartsEditor`). |
| **M6** | Interact probe consumers grow (equipment pickups, mission terminals): it moves to `GSTNInteract` (owner, Mod.20; [interaction.md](interaction.md)). PvP shield only, unchanged. |
| **M10** | Different system (held objects); shares the hold-to-pick-up input and the simultaneous-pickup rule. |
| Game modes | Spawn pickups and drops; toggle `bAllowMidMatchKitChange`, mid-match special swaps, backpack drops ([game-modes.md](game-modes.md)). |

## 16. Plugin and module boundaries

| Plugin | Modules | Depends on | Notes |
|---|---|---|---|
| `GSTNCombat` (M3) | Core, GAS, NP | — | Kits, weapons, supers, loadout, compile and the §11 seams, equipment visual component |
| `GSTNKitMods` (optional) | `GSTNKitModsCore` (definitions, pure baking of derived timelines, tested without a world), `GSTNKitModsGAS` (compile extension registration, stat effects), `GSTNKitModsEditor` | `GSTNCombatCore`, `GSTNCombatGAS` | Disabled = kits without mods; saved mod data dropped with a warning |
| `GSTNBackpack` (optional) | `GSTNBackpackCore`, `GSTNBackpackGAS` | `GSTNCombatCore`, `GSTNCombatGAS` | Registers loadout extension, entry source, attachment provider, pickup payload |
| `GSTNPickups` (optional) | `GSTNPickupsCore`, `GSTNPickupsRuntime` | `GSTNCombatCore`, `GSTNInteractCore` | Payloads are open structs; knows no backpack or mod |

No plugin above depends on another optional plugin; backpack pickups are registered by `GSTNBackpack` into `GSTNPickups`' payload
registry only if both are loaded. That registration is the one allowed contact; if it grows, it becomes a bridge plugin
(modularity-and-tags.md §3.7 rule 3).

## 17. Testing

- Core Automation: derived timeline baking (rounding, order preserved, `bFixedTime`, clamping, stable id, two machines' inputs
  produce byte-identical timelines), reach scaling of hitboxes and baked sockets, loadout extension round trip (save game and net),
  rules validators, payload → loadout change.
- Harness (listen server, host + 2 clients): `scenario_kit_pickup` (client picks a kit mid-match: presses during the round trip
  stay buffered, no command resolves against the old table, old kit drops, damage progress preserved), `scenario_support_switch`
  (kit stats unchanged after switching), `scenario_mod_reach` (a modded long blade hits at the scaled reach on every machine,
  slower attack frames match), `scenario_backpack_use` (backpack ability works with primary and support weapon, survives a kit
  change).

## 18. Milestone placement and risks

M11 after M3 (seams), M4 (hit data, `ReachAxis`) and M5 (preview, validators); pickups need `GSTNInteract` ([interaction.md](interaction.md)).
Order: M11a seams and mid-match apply (in M3 if cheap), M11b support weapons and special swapping (pure data on M3), M11c mods,
M11d backpacks, M11e pickups.

Risks: re-granting abilities mid-match on the NP fork (spike: grant/remove while other abilities are mid-timeline, rollback around
the revision change); derived timelines multiply registry size (one per modded move per player: cap and share identical ones by
stable id); mod combinations that break moves (limits and validator); visual load hitches on kit change (async load, keep the old
look until ready).

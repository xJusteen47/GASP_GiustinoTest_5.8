# M11 implementation plan: kits in play (pickups, support weapons, mods, special slots, implants, backpacks)

Plan only, no code (2026-11-06). Spec: [combat-loadout-mods.md](../combat-loadout-mods.md) (owner direction 2026-10-23: kit = stats +
moveset, changed mid-match by picking one up; everything is a pluggable ability). Owner rules of 2026-11-06 (sequenza D1): **swap your kit
with one lying on the ground** (stats follow the kit, yours stays on the ground in its place); **a player who dies drops their kit**, and
that is a **per-mode rule** (M12). Ground rules: [resto-m3-m4b-m5-t.md](resto-m3-m4b-m5-t.md) §0.

## 1. What is already there

- M3 C17/C18: the compiled loadout table swaps at a `LoadoutRevision` boundary; mid-match apply = `scenario_loadout_change`;
  `UGSTN_LoadoutRules` validators; a reserved, versioned extension block in `NetSerialize` and the save game; `ReachAxis` on hitboxes (M4 H1).
- TH-1b (branch `claude/project-thread-0t6tfg`, in `kizkcl`, **not on feature yet**): `AGSTN_KitPickup` in `GSTNCombatGAS/Pickups/`
  (replicated, holds a full `FGSTN_LoadoutData`), `UGSTN_KitSwapSubsystem` (server: hold interact → swap with the nearest kit in radius
  via `AcceptLoadout`), `DropKit(...)` hook (uncalled). Swap is out-of-band (no sim code), reaches the sim as a new revision.
- TH-2: 7 ground kits spawned at Play on the test hub pads, kit-coloured back box (placeholder implant).
- `GSTNInteract` (experimental 0.1): probe, focus and `Interact.*` root; not yet the path TH-1b uses (TH-1b listens to `Command.Interact.PickUp`).

## 2. Plugin layout (spec §16, adjusted)

`GSTNPickups` (Runtime + GAS bridge; the TH-1b pickup moves here out of `GSTNCombatGAS`, with a redirect), `GSTNKitMods` (Core pure +
Runtime), `GSTNKitModsEditor` (client of the M5 hooks), `GSTNBackpack` (Runtime). None of them is named by `GSTNCombat*`; each registers
its loadout extension, compile extension and validators at startup. Disable any of them and the rest still builds and plays.

## 3. Gates

| Gate | Content | Machine / model | Cost | Needs |
|---|---|---|---|---|
| **G0** | Audit of TH-1b on feature + spike: grant/remove abilities while another ability is mid-timeline, forced rollback across the revision change, 2 % loss; record reconciles per swap (expect ≤ 1) | laptop, Opus medium | $4-7 | TH-1b merged |
| **G1** | M3 seams left to M11 (spec §11): typed `FGSTN_LoadoutData::Extensions` (`TArray<FInstancedStruct>`), compile-extension registry with priority and stable source ids, attachment-provider interface on the skin component | laptop, Opus | $6-10 | G0 |
| **G2** | `GSTNPickups`: `AGSTN_EquipmentPickup` with payloads (kit, support weapon), TH-1b actor becomes its kit child; candidate provider for `GSTNInteract` (focus on the pointed pickup, hold interact); race rule (earliest request frame, then lower player id); drop on death through `DropKit` gated by a rule flag (`bDropKitOnDeath`, a project setting until M12 G3 moves it into the mode rules) | desktop, Opus | $10-18 | G1 |
| **G3** | Support weapons and special-move slots (spec §6-7): data on M3, predicted support switch keeps kit stats, special swapping among the available ones | laptop, Sonnet | $5-8 | G1 |
| **G4** | `GSTNKitMods`: mod data, derived timelines (reach scale on `ReachAxis`, frame slowdown), stable ids shared between players, registry cap, mod validators | laptop, Opus | $10-18 | G1, M5 E10 (done) |
| **G5** | Implants and weapon visuals, placeholder only: providers for kit implant, weapon implants on hands/feet, async load keeps the old look until ready | desktop, Sonnet | $4-7 | G1 |
| **G6** | `GSTNBackpack`: backpack loadout extension, attaches on top of the implant, grants its ability (example: jet-pack burst), survives a kit change, backpack pickup payload | laptop, Sonnet | $6-10 | G2, G5 |
| **G7** | `GSTNKitModsEditor`: mod preview in the timeline editor, validators in the M5 validation panel | laptop, Sonnet | $4-7 | G4 |
| **G8** | Sweep: `scenario_kit_pickup`, `scenario_support_switch`, `scenario_mod_reach`, `scenario_backpack_use`, kit drop on death; test hub gets a mods/backpack pad; docs | desktop, Sonnet | $4-7 | G2-G7 |

**Total: $53-92.** Order: G0 → G1 → G2 → (G3, G4, G5 in any order) → G6 → G7 → G8. G1 and G4 touch `GSTNCombatGAS`/Core loadout code:
not in parallel with M10 G3 (credit) or M8 G1.

## 4. Blueprint and C++

- **C++**: loadout extensions, compile extensions, derived timelines, pickup race and replication, swap/apply path (everything that changes
  the compiled table or the sim).
- **Blueprint / data (designer)**: every kit, support weapon, mod and backpack is a data asset; their verbs are abilities (example
  abilities in Blueprint: one support weapon shot, the jet-pack burst, one mod that lengthens the sword); pickup actors are Blueprint
  children (`BP_Pickup_Kit_*`, `BP_Pickup_Backpack_*`) with mesh and payload set in the details panel; placed in the test hub by the
  startup script, never by hand-run Python.

## 5. Stop and ask

G0 shows > 2 reconciles per swap or a mispredicted move after the swap → stop (the swap path needs a sim-side revision request).
G4: derived timelines push the registry over its int16 limit with 4 players → stop, propose a per-kit cap.

## 6. Defaults applied (owner may change, all data)

Pickup = hold interact 15 frames (as TH-1b); kit on the ground never despawns in Levels; dropped kit keeps its mods; backpack survives
death only if the mode says so (M12 rules, default no); support weapon dropped with the kit.

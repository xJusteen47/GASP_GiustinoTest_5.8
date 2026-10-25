# GSTNParts

Destructible body parts for [GSTNCombat](../GSTNCombat/README.md): part sets, the penetration table, break effects as data, integer-quantised hit
math. Optional: `GSTNCombat` builds and passes its tests without it, and the combat seams parts use (hurtbox `Priority`, hit `ExtensionData`, the
outcome applier registry, the snapshot extension, sync fragments) stay inert when the plugin is off. Design: `combat-parts.md` and the plan
`m4b-parts-plan.md` on the docs branch; the spikes behind it: [docs/notes/m4b-spike-report.md](../../docs/notes/m4b-spike-report.md).

Status: **experimental (0.1), skeleton only (P1)**. `EnabledByDefault` is false: a project enables it in its `.uproject`.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNPartsCore` | exists | Pure, no GAS, Network Prediction or Mover; links `GSTNCombatCore` only. Today: the native tags, the settings and the module that registers the plugin's tag ini folder. Covered by `GSTNParts.Core.*` tests. |
| `GSTNPartsGAS` | planned (P5) | Break-effect appliers that need a gameplay effect, event emission, tag projection helpers. |
| `GSTNPartsNP` | planned (P6, P7) | The parts sync fragment, tick extension, snapshot contributor, outcome applier. Needs the fork. |
| `GSTNPartsPresentation` | planned (P10..P12) | Damage material, debris pool, shatter; not loaded on a dedicated server, loaded by a listen host. |
| `GSTNPartsEditor` | planned | Validators, Physics Asset to part set helper. |

## Tags

Native, in `GSTNPartsCore` (`GSTN_PartsTags.h`, `namespace GSTN_PartsTags`), one doc line each:

| Tag | Kind |
|---|---|
| `Combat.Part.Broken`, `Combat.Part.GrabLocked`, `Combat.Part.AllBroken` | projected from the broken mask, never granted |
| `Combat.Part.Effect` | root of the tags a break effect grants (leaves are designer content) |
| `Combat.Entity.Destructible` | marker: the entity has a part set |
| `Combat.Event.Part.{Damaged,WeakPointHit,Ricochet,Broken,AllBroken}` | outbox events |
| `Message.Combat.PartBroken` | UI message channel |

Ini only (`Config/Tags/GSTNParts.ini`, registered by the module at startup): `Part.Type.{Head,Torso,Arm,Leg,Weapon,Armour,Crystal}` and
`GameplayCue.Combat.Part.{Hit,Break,Ricochet,Shatter}`. Parts never declare `Combat.State.*`, `Combat.Mover.*`, `Combat.Lock.*` or `Combat.Link.*`
(stagger is the combat plugin's `Combat.State.Hitstun`), and no tag goes into sync state or a snapshot (part indices and masks only).

## Settings

`UGSTN_PartsSettings` (Project Settings > Plugins > GSTN Parts, `GSTNParts.ini`): the default penetration table and weight curve (soft references,
empty until P2/P3 ship defaults) and `FGSTN_PartsPresentationBudget` (48 chunks, 4 s lifetime, 600 shard particles, 2 Geometry Collections, 1.5 s
sleep stop). Defaults in `Config/DefaultGSTNParts.ini`; a project overrides them in its own `Config/DefaultGSTNParts.ini`. The simulation-read
content must resolve identically on the host and every client.

## Tests

Headless: `UnrealEditor-Cmd.exe <project>.uproject -ExecCmds="Automation RunTests GSTNParts; Quit" -unattended -nopause -NullRHI` (editor closed).
Skeleton tests: `GSTNParts.Core.Tags.AllRegisteredAndDocumented`, `GSTNParts.Core.Tags.NoStateTagDeclared`.

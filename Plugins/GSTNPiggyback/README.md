# GSTNPiggyback

Piggyback and pair moves for the GSTN combat (milestone M7): an ally **rides** on a carrier, and the two can play **pair moves** together
(the carrier throws its rider, the rider jumps off with a boost). A mount is one more `GSTNLink` link type: the rider is the link's
initiator, parked on the carrier and still acting through its own bindings; the carrier is the target and keeps acting. Everything
piggyback-specific (mount rules, carry load, pair moves, the mount hit step) is data and code of this plugin; `GSTNLink` and `GSTNCombat`
know nothing of it.

Status: **skeleton** (checkpoint G4 of the M7 plan). The pure part is done: native tags, mount data, carry and anchor math, exit candidates,
credit split, with tests. The Runtime and Editor modules are empty until G5 and G11.

Spec: `docs/architecture/combat-m7-piggyback.md`. Plan: `docs/architecture/plans/m7-piggyback-plan.md` (branch `claude/project-thread-wzb12r`).
Where the spec and the shipped code differ, the shipped names win; the differences are listed in `docs/notes/m7-doc-deltas.md`.

## Modules

| Module | Type | Contents | Depends on |
|---|---|---|---|
| `GSTNPiggybackCore` | Runtime | Native tags (`GSTN_PiggybackTags.h`), mount data (`GSTN_MountTypes.h`), pair-move enums (`GSTN_PairMoveTypes.h`), `UGSTN_MountUtils` | `GSTNCombatCore`, `GSTNLinkCore` |
| `GSTNPiggybackRuntime` | Runtime | Empty. G5+: mount link rule and type, mount/dismount/drop and pair-move abilities, carry load effect, pair-move registry, mount hit step | `GSTNPiggybackCore`, `GSTNInputCore`, `GSTNCombatGAS`, `GSTNCombatNP`, `GSTNLinkGAS`, `GSTNLinkNP` |
| `GSTNPiggybackEditor` | Editor | Empty. G11: pair-move factory and validators, anchor debug panel | `GSTNPiggybackRuntime` |
| `GSTNPiggybackTests` | DeveloperTool | Automation tests (`GSTNPiggyback.*`); never ships | all of the above |

The tags are declared in the Core module because it is a Runtime module: a native tag defined in an `UncookedOnly` module ensures in a
cooked game.

## Dependencies and isolation

- The `.uplugin` requires `GSTNInput`, `GSTNCombat`, `GSTNCombatGAS`, `GSTNCombatNP` and `GSTNLink` (the plugins its modules use). Nothing
  requires `GSTNPiggyback`, and no module outside it names it.
- Usable alone: no `/Game/` path, no `ConstructorHelpers`, no project tag or ini. Its designer tags live in its own
  `Config/Tags/GSTNPiggybackCommands.ini`, which the Core module registers at startup.
- Off by default (`EnabledByDefault: false`) and **not listed in the committed `.uproject`** until G5 passes. Disabled, none of its tags
  exists, a binding naming `Command.Link.Mount` is just unbound, and grabs, latches and every other feature are the M6 configuration.
- Grep gate: `python Tools/Modules/check_modules.py` (add `--allow-uproject` while the plugin is enabled locally). It fails when anything
  outside the plugin names `GSTNPiggyback`, when the plugin's source has a `/Game/` path or `ConstructorHelpers`, or when a module
  edge of the optional plugins is crossed.

To build and test locally: list `{ "Name": "GSTNPiggyback", "Enabled": true }` in the `.uproject` next to `GSTNLink` (never commit that line;
touch the file after swapping it, or UBT may say the target is up to date), build, then `Automation RunTests GSTNPiggyback` (or `GSTN`).

## Tags

Native (`GSTN_PiggybackTags.h`, 21 tags; `GSTNPiggybackTags::GetDeclaredTags` lists them):

| Tag | Meaning |
|---|---|
| `Combat.Link.Type.Mount` | The mount link type (reserved for this plugin by `GSTNLink`) |
| `Combat.Link.Role.Carrier`, `Combat.Link.Role.Rider` | The two roles, projected from the link state |
| `Combat.HitType.Interaction.Mount` | Hit type of the mount ability's hitbox (a mount request) |
| `Combat.State.Riding`, `Combat.State.MountPending` | Rider state, projected from the link state (removed by the same projection) |
| `Combat.State.Carrying` | Carrier state, an ordinary tag of the carry load effect |
| `Combat.PairMove.ThrowRider`, `Combat.PairMove.BoostJump` | Identities of the two reference pair moves |
| `Combat.Event.Link.Mounted`, `Combat.Event.Link.MountRefused` | Authoritative mount events |
| `Combat.Event.PairMove.Started`, `Combat.Event.PairMove.Aborted` | Authoritative pair-move events |
| `Combat.Link.BreakReason.{Dismount, Drop, Outcome, CarrierHit, RiderHit}` | This plugin's break reasons (indices from `EGSTN_LinkBreakReason::FirstPlugin` on) |
| `Combat.Link.Credit.Partner` | Event tag: a pair move's partner is credited |
| `Combat.SetByCaller.CarryLoad.Ratio` | Set-by-caller key of the carry load effect |
| `Interact.Target.Mount` | A mountable ally is the nearest interactable (the interact button's mount meaning) |

Ini (`Config/Tags/GSTNPiggybackCommands.ini`): `Command.Link.Mount`, `Command.Link.Dismount`, `Command.Link.Drop`, the three meanings of the
interact button.

Reused from `GSTNLink`, never redeclared: `Combat.Link.Linked`, `Combat.Link.Pending`, `Combat.Lock.Link`, `Combat.Mover.Linked`,
`Combat.Event.LinkFormed`, `Combat.Event.LinkBroken`, `Combat.Event.Link.PartnerMoveAborted` and the generic break reasons (`Timeout`,
`Distance`, `OwnDeath`, `PartnerDeath`, `Hit`, `PartnerState`, `Release`, `Throw`, `Forced`...).

## Core data (`GSTNPiggybackCore`)

- `FGSTN_MountKitExtension` (a `FGSTN_KitExtension`, in a kit's extension array): `FGSTN_CarrierProfile Carrier` (can be mounted,
  disallowed commands, socket and offset of the rider's anchor, mount range, `FGSTN_CarryTuning`, default exit mode and distance, remount
  cooldown) and `FGSTN_RiderProfile Rider` (can ride, IK sockets, facing arc). A kit without it can neither carry nor ride.
- `FGSTN_MountRules` (a `FGSTN_RulesExtension`, in a combat rules asset): ally only, pending frames, break thresholds for hits on the
  carrier and on the rider, rider flinch scales, cover arc, shield cover, teleport break distance, pending timeout, default credit. A rules
  asset without it refuses every mount.
- `EGSTN_MountVerdict`, `EGSTN_ExitMode`, `FGSTN_MountEligibilityInput`, `FGSTN_CreditSplit`; pair-move enums `EGSTN_PairRole`,
  `EGSTN_PairCredit`, `EGSTN_PairPartnerMode` (the link outcome is GSTNLink's `EGSTN_LinkActionOutcome`).
- `UGSTN_MountUtils` (stateless, doubles, identical on every machine):
  - `EvaluateEligibility`: the one mount rule, run by the rider's prediction and the carrier's resolution alike. Checks in order: carrier
    kit, rider kit, already linked, cooldown, hitstun, busy or locked, range, weight; equal weight, equal range and the cooldown's end frame
    are accepted.
  - `ComputeCarryRatio` (rider weight / capacity, clamped, quantised to a byte, round half up), `CarryRatioToUnit`, `ScaleCarryTuning`.
  - `ComposeAnchor`: carrier, socket, offset, then the rider's own turn (the same composition as `UGSTN_LinkMath::ComposeLocalYaw`).
  - `ExitCandidates`: ordered places to put a rider down, for GSTNLink's exit placer.
  - `MakePartnerShare`, `SplitCredit`: a pair move's credit between the hitbox owner and the partner.

## How to extend

- **New pair move** (from G8): author the initiator's and the partner's timeline abilities, create a pair-move definition (roles, offset,
  outcome frame, exit, credit), add it to the kit's pair-move set, bind the initiator's ability under an existing command with the role
  requirement (`Combat.Link.Role.Rider` or `.Carrier`), validate the loadout. No code.
- **New carrier restriction**: add the command to the carrier profile's `DisallowedCommands`.
- **New exit mode**: an `EGSTN_ExitMode` entry and its case in `UGSTN_MountUtils::ExitCandidates` (pure, with a test in
  `GSTNPiggyback.Core.Exit.*`).
- **New mount condition**: a `UGSTN_LinkRule` subclass on the mount type's definition, or a new verdict and check in `EvaluateEligibility`
  (one place for prediction and resolution).
- **Different weight rules**: the kit's `FGSTN_CarryTuning`, or `ComputeCarryRatio`/`ScaleCarryTuning`.

## Tests (`GSTNPiggyback.*`)

- `Core.Eligibility.EveryVerdict`: each check failing alone gives its verdict; with every check failing, the documented order.
- `Core.Eligibility.WeightBoundary`: weight equal to the capacity accepted, the next double above refused; zero capacity; NaN; equal range
  and the cooldown's end frame accepted.
- `Core.CarryRatio.ClampAndQuantise`: clamping, round half up, zero and negative capacity, NaN; tuning scaled linearly, air jumps toward zero.
- `Core.Anchor.Compose`: socket and offset in the carrier's frame; the turn does not move the anchor; equal to GSTNLink's composition.
- `Core.Exit.CandidateOrder`: the order of every mode, a turned carrier, an upside-down carrier, a negative distance.
- `Core.Credit.Split`: the ends, half, a third, a negative amount.
- `Core.Tags.Registered`: every native tag registered, explicit, documented and not one of GSTNLink's; the generic reasons exist; the
  three command slots come from the plugin's ini.

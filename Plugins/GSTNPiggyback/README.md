# GSTNPiggyback

Piggyback and pair moves for the GSTN combat (milestone M7): an ally **rides** on a carrier, and the two can play **pair moves** together
(the carrier throws its rider, the rider jumps off with a boost). A mount is one more `GSTNLink` link type: the rider is the link's
initiator, parked on the carrier and still acting through its own bindings; the carrier is the target and keeps acting. Everything
piggyback-specific (mount rules, carry load, pair moves, the mount hit step) is data and code of this plugin; `GSTNLink` and `GSTNCombat`
know nothing of it.

Status: **G5** of the M7 plan. G4 built the pure part (native tags, mount data, carry and anchor math, exit candidates, credit split). G5 adds
the mount itself: the mount link type (made by code), its rule and exit placer, the mount, dismount and drop abilities, their bindings and the
carrier's disallowed commands, the plugin's test kits and the harness scenarios. Carry load (G6), hits while mounted (G7) and pair moves (G8+)
are not built yet; the Editor module is empty until G11.

Spec: `docs/architecture/combat-m7-piggyback.md`. Plan: `docs/architecture/plans/m7-piggyback-plan.md` (branch `claude/project-thread-wzb12r`).
Where the spec and the shipped code differ, the shipped names win; the differences are listed in `docs/notes/m7-doc-deltas.md`.

## Modules

| Module | Type | Contents | Depends on |
|---|---|---|---|
| `GSTNPiggybackCore` | Runtime | Native tags (`GSTN_PiggybackTags.h`), mount data (`GSTN_MountTypes.h`), pair-move enums (`GSTN_PairMoveTypes.h`), `UGSTN_MountUtils` | `GSTNCombatCore`, `GSTNLinkCore` |
| `GSTNPiggybackRuntime` | Runtime | G5: `FGSTN_MountLinkType` (the mount type, registered at startup), `UGSTN_LinkRule_Mount`, `UGSTN_Placer_Mount`, `UGSTN_GA_Mount` / `_Dismount` / `_Drop`, `FGSTN_MountLoadout` (bindings and disallowed commands), `UGSTN_PiggybackContentLibrary` (test kits, kit scan). Later: carry load, pair moves, mount hit step | `GSTNPiggybackCore`, `GSTNInputCore`, `GSTNCombatGAS`, `GSTNCombatNP`, `GSTNLinkGAS`, `GSTNLinkNP`, `GameplayAbilities`, `AbilitySystemSimulation`, `NetworkPrediction` |
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

## The mount (G5, `GSTNPiggybackRuntime`)

- **Type**: `FGSTN_MountLinkType::Build(Rules)` makes the definition, `Register()` adds it to GSTNLink's registry once the engine exists (code, not
  an asset: its instanced rule and placer would need the editor to author, and it is fully decided by `FGSTN_MountRules` and a few constants).
  Body channel; role 1 **Rider** (initiator: parked on the carrier's root + `RiderOffset()` (-20, 0, 95) cm, acts, no collision with the carrier,
  turns freely, Release reason `Dismount`); role 2 **Carrier** (target: acts, Release reason `Drop`). `bInitiatorPredicts`, `PendingFrames` 2.
  Rules: GSTNLink's `UGSTN_LinkRule_Team` (AlliesOnly) then `UGSTN_LinkRule_Mount`. Hit breaks: carrier above `CarrierBreakKnockback`, rider above
  `RiderBreakKnockback`, partner hits never (reason `Hit` until G7). Deaths break it; leash 300 cm. Exit `UGSTN_Placer_Mount` (BehindCarrier, 90 cm).
  Interact offer `Interact.Target.Mount`, priority 50 (the grab is 100: with an enemy and an ally in reach the grab wins), reach 150 cm, kits
  with `FGSTN_MountKitExtension`. The carrier's `Combat.State.Hitstun` is named in the request requirements (a carrier in hitstun refuses).
- **Rule**: `UGSTN_LinkRule_Mount` runs `UGSTN_MountUtils::EvaluateEligibility` on values of the request context: both kits' mount data
  (`Carrier.CarryCapacity`, `Rider.Weight`, `bCanBeMounted`, `bCanRide`), link states, request tags, the rider's link cooldown. Range is the
  hitbox's and the offer's reach. Its payload is the carry ratio (`ComputeCarryRatio`), held by both sides for G6's carry load.
- **Moves** (timeline abilities built in their constructors, Blueprintable): `UGSTN_GA_Mount` (24 frames, one ally-only interaction hitbox
  on frame 3 only, one target (a longer window re-requested a refused carrier on the rider's client after its rollback): the Ally variant is `bForceHittable` (GSTNCombat), Enemy and Neutral ignored), `UGSTN_GA_Dismount` and `UGSTN_GA_Drop`
  (a Release key: the role names the reason). A kit can name its own classes in its mount data.
- **Bindings** (`FGSTN_MountLoadout`, a loadout extension): Mount = Press(Interact) requiring `Interact.Target.Mount` (slot offset -1, below the
  grab's 0); Dismount = Press(Interact) requiring `Combat.Link.Role.Rider` (101, above the Release's 100); Drop = Press(Interact) + Hold(Crouch)
  requiring `Combat.Link.Role.Carrier` (102). The loadout validator refuses two slots with one input at one priority, whatever their requirements.
  All fall through when unavailable. The carrier profile's `DisallowedCommands` add `Combat.Link.Role.Carrier` to the ignored tags of every kit
  and weapon binding of those commands.
- **Exits are server-given**: the watchdog on the authority places the rider (dismount, drop, every break) and the rider's client takes it as a
  correction. `FGSTN_MountRules::bPredictRiderExits` (false) is the switch for the predicted rider exit with the server rewinding the carrier
  (owner decision 2026-11-01, spec 6.4): not built yet, true is logged and treated as false.
- **Test content**: `Content/Test/KIT_Test_Mount` (rider weight 100, capacity 150) and `KIT_Test_MountHeavy` (rider weight 200), copies of the
  project's `KIT_Test_Link` with a mount entry, written by `Tools/GASNPPTest/mount_scenarios.py` (`prepare_editor`). The plugin adds its content
  to the asset manager's kit scan (`UGSTN_PiggybackContentLibrary::ScanPluginKits`). Harness: `mount_scenarios.SUITE_MOUNT`.

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
- `Runtime.Rule.AcceptsAllyWithMountData`, `Runtime.Rule.EveryRefusal`, `Runtime.Rule.TypeRegisteredAsSpecified`,
  `Runtime.Rule.MountHitboxTargetsAlliesOnly`: the rule on hand-built request contexts, the registered type, the mount move's hitbox.
- `Runtime.Bindings.RiderHasNoGroundContext`, `Runtime.Bindings.DisallowedCommands`: the loadout extension's bindings and disallowed commands.
- `Runtime.Break.EveryReasonClearsStates`: every generic and plugin reason, from either side, clears both states and places the rider (fake host).
- `Runtime.Disabled.UnboundCommandIsLegal`: a kit without mount data is unchanged; the mount bindings bring their own slots.
- `Core.Tags.Registered`: every native tag registered, explicit, documented and not one of GSTNLink's; the generic reasons exist; the
  three command slots come from the plugin's ini.

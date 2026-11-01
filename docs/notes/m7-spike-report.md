# M7 spike report (checkpoint G0)

Plan: `docs/architecture/plans/m7-piggyback-plan.md` (branch `claude/project-thread-wzb12r`), sections 0, 1, 3 G0 and 4.
Machine: desktop, worktree `GASP_Giustino2_feature`, branch `feature/gas-npp-5.8` at 365a0e0 (spin-drill D3). Date 2026-11-01.

**Result: S0 STOPS.** Two seams the mount needs are worse than the plan's section 1 says: the interact offers of the link probe are
hard-wired to the grab (Y8), and a request rule cannot see the team or its own kit data (behind Y1/Y2). Plan section 4, first bullet:
stop and ask. **S1 and S2 were not run** (no build, no editor, no scratch code). Both gaps have a small generic fix in `GSTNLink` that
fits the G1 follow-up (proposal below); S1/S2 do not depend on them and can run once the owner decides.

## S0 seam audit

Changes in the audited area since the plan's audit commit 9dcaeb2: `git diff --stat 9dcaeb2..HEAD -- Plugins/GSTNLink Plugins/GSTNCombat
Plugins/GSTNExtensionTest` = one file, `GSTN_SyncFragment.h` (+3: `GSTNFeatureIds::SpinDrill = 19`). So no row moved since the plan was
written; the two WORSE rows below were already wrong at 9dcaeb2.

Paths below are under `Plugins/GSTNLink/Source/` unless they name another plugin.

| Id | Plan says | On HEAD | Verdict | Evidence |
|---|---|---|---|---|
| Y1 | Present | Present (channel, two role defs with `bParked`, `bActsWhileLinked`, `bCollisionWithPartner`, `SocketOnPartner`, `SocketOffset`, `ReleaseBreakReason`; break rules, rules, placer, `ExitCollisionIgnoreFrames`, `ForbiddenWithBody`, `MaxFrames`). But see **R1**: what a rule can read is narrower than the plan assumes | as written (data); **R1 worse** | `GSTNLinkCore/Public/Data/GSTN_LinkTypeDefinition.h:59,66,70,74,78,86,159,163,167,170,181,188,198,205,220,224` |
| Y2 | Present, proven by EXT | Present | as written | `GSTNLinkGAS/Public/GSTN_LinkRegistry.h:115` (`AddDefinition`), `:179` (`RegisterPluginReason`); `GSTNLinkCore/Public/GSTN_LinkSettings.h:43`; `GSTNLinkCore/Public/State/GSTN_LinkState.h:48` (`FirstPlugin = 16`); `Plugins/GSTNExtensionTest/Source/GSTNExtTestLink/Private/GSTN_ExtTestLinkTests.cpp:57` |
| Y3 | Present | Present | as written | `GSTNLinkNP/Public/Pipeline/GSTN_HitStep_LinkRequest.h:16`; `GSTNLinkNP/Public/Sim/GSTN_LinkInboxKinds.h:150`; `GSTN_LinkState.h:164,167` (`LastRefusedSerial`, `RefusedFrame`); `GSTNLinkNP/Public/GSTN_LinkContentLibrary.h:46-52` (`FormTestLink`, the S1 Form command) |
| Y4 | Partial: `bInitiatorPredicts` read by nothing | Same. Note: the `Predicted` mark is put on **every** registered link type's hit, not only grab-like ones; `bInitiatorPredicts` would gate only G1's new optimistic accept | as written | `GSTN_LinkTypeDefinition.h:220` (only reference in the source); `GSTNLinkNP/Private/Sim/GSTN_LinkTickExtension.cpp:433-446` (`MarkPredicted` per attacker hit), `:59-66` (timeout) |
| Y5 | Present | Present; veto and substitution read the role def, no type name | as written | `GSTNLinkNP/Public/Movement/GSTN_LinkedMovementMode.h:20`; `GSTNLinkNP/Private/Movement/GSTN_LinkedMovement.cpp:23-29`; `GSTNLinkNP/Private/Sim/GSTN_LinkSnapshotContributor.cpp:32`; `GSTNLinkNP/Private/Pipeline/GSTN_LinkHitSteps.cpp:288`; `GSTN_LinkTickExtension.cpp:489` |
| Y6 | Present | Present | as written | `GSTN_LinkTypeDefinition.h:101,105,116,124,128,135` (break rules); `GSTNLinkNP/Public/GSTN_LinkWatchdog.h:125,150`; `GSTNLinkNP/Public/Movement/GSTN_LinkExitRequest.h:64`; `GSTNLinkNP/Public/Sim/GSTN_LinkExitCollision.h:31` |
| Y7 | Present (attach while Pending with G1) | Present; no type name in the component | as written | `GSTNLinkNP/Public/GSTN_LinkPresentationComponent.h:18,47`; `GSTNLinkNP/Private/GSTN_LinkPresentationComponent.cpp:48` |
| **Y8** | **Present**: "mount = one more offer under `Interact.Target.Mount`" | **Missing seam.** The probe the link tick runs in the simulation builds its offers in `MakeGrabOffers`: the grab offer plus the grab definition's `Alternatives`, all under the grab offer's meaning. The meaning table is a static table with `Interact.Target.Grab` only, and the result returns only that tag. Another plugin cannot add an offer, a meaning or a projected tag without editing `GSTNLinkNP`; the only data route (adding the mount to the grab's `Alternatives`) projects `Interact.Target.Grab`, needs `GSTNLink`'s grab asset to name the mount (plan section 4 stop: "make GSTNLink name piggyback") and lets the grab's weight rules decide. Also: only a kit with a `FGSTN_LinkKitExtension` runs the probe, so a mount-only kit never looks; and the pre-filter cannot keep allies only (see R1). The README's "How to extend" line for an interact-formed type ("one more offer in the source the link tick extension hands the probe") is an edit of the tick extension, not an extension point. The INT.5 order that lists Mount last is in `GSTNInteract`'s meaning table, which the link probe does not use | **WORSE** (stop) | `GSTNLinkNP/Private/Sim/GSTN_LinkTickExtension.cpp:275-292` (`MakeGrabOffers`), `:294-306` (`GetLinkMeanings`: Grab only), `:316-322` (gate on the link kit extension), `:374` (returns `Interact_Target_Grab` only); `GSTNLinkGAS/Public/GSTN_LinkInteractProvider.h:31-33,101`, `GSTNLinkGAS/Private/GSTN_LinkInteractProvider.cpp:115-118`; `Plugins/GSTNInteract/Source/GSTNInteractCore/Public/GSTN_InteractTypes.h:434` (INT.5 order) |
| Y9 | Present | Present (`Build()` and `DefaultBindings()` are defaults naming grab/latch roles; a plugin adds its own slots and bindings) | as written | `GSTNLinkCore/Public/Input/GSTN_LinkSlots.h:49,58,61`; `GSTNLinkCore/Private/Input/GSTN_LinkSlots.cpp:116-123` |
| Y10 | Present | Present | as written | `Plugins/GSTNCombat/Source/GSTNCombatCore/Public/Hits/GSTN_HitData.h:16-23,142`; `GSTNLinkNP/Private/Pipeline/GSTN_LinkHitSteps.cpp:188` |
| Y11 | Present | Present (attributes). Not visible to a link rule: see R1 | as written | `Plugins/GSTNCombatGAS/Source/GSTNCombatGAS/Public/Attributes/GSTN_CombatAttributeSet.h:59-66` |
| Y12 | Partial: enum values, no executor for `ApplyLinkEffect`/`RemoveLinkEffect`/`LaunchLinked` | Partial, and **different**: no executor for **any** of the five link kinds (`EnterLinked`, `ExitLink` too), and there is no "link GAS executor" for G1 to extend: the model's request switch ends in `default: break` and `GSTNCombatNP` has no seam that lets a plugin execute a request kind. Not a stop: the link tick extension already applies a gameplay effect itself from a hook with `Context.AbilitySystem` (the exit teleport), so G1 can apply and remove link effects the same way, inside `GSTNLink`, without the request kinds and without a `GSTNCombatNP` edit | different, not worse | `Plugins/GSTNCombatNP/Source/GSTNCombatNP/Public/Simulation/GSTN_CombatActionTypes.h:73-79`; `Plugins/GSTNCombatNP/Source/GSTNCombatNP/Private/Simulation/GSTN_CombatSimComponent.cpp:1375` (`default: break`); only other use is a test (`GSTN_CombatNPTests_M6Follow2.cpp:591,640`); the existing path: `GSTN_LinkTickExtension.cpp:467` → `GSTNLinkNP/Private/Movement/GSTN_LinkExitRequest.cpp:80-100` |
| Y13 | Partial: serialized, no writer | Same | as written | `GSTNLinkCore/Public/State/GSTN_LinkState.h:98,101,180-184`; `GSTNLinkCore/Private/State/GSTN_LinkState.cpp:16,59-60,82`; `GSTNLinkNP/Private/Sim/GSTN_LinkFragment.cpp:45-46,150`; no other reference |
| Y14 | Missing | Missing (no `Credit`/`PartnerShare` anywhere in `GSTNCombat*`) | as written | grep `-i "CreditPartner\|PartnerShare\|Credit"` over `GSTNCombat*/Source`: no hit |
| Y15 | Present | Present | as written | `Plugins/GSTNCombat/Source/GSTNCombatCore/Public/Hits/GSTN_HitResolution.h:381`; `.../Rules/GSTN_CombatRules.h:77` (`HitPipeline`); `GSTNLinkNP/Public/Pipeline/GSTN_HitStep_LinkBreakOnHit.h:23`; `Plugins/GSTNExtensionTest/Source/GSTNExtTestDefence/Public/GSTN_ExtTestStep_Brace.h:26` |
| Y16 | Present (`FGSTN_KitExtension::Compile`, loadout registry, rules extensions) | Present under other names: `FGSTN_KitExtension` is data only (no `Compile`); a plugin compiles through `FGSTN_LoadoutExtensionRegistry::ContributeCompileInput` | naming only | `Plugins/GSTNCombat/Source/GSTNCombatCore/Public/Rules/GSTN_KitExtension.h:17-22`; `Plugins/GSTNCombatGAS/Source/GSTNCombatGAS/Public/Loadout/GSTN_LoadoutExtensions.h:30-75`; `.../GSTNCombatCore/Public/Rules/GSTN_CombatRules.h:38,187` |

### R1 (behind Y1/Y2): what a link rule can read — WORSE

The plan's G5 rule (`UGSTN_LinkRule_Mount` wrapping `EvaluateEligibility`: ally only, kit `bCanBeMounted`/`bCanRide`, weight against the
carrier's capacity, cooldown, `Combat.Lock.Link`, carrier not in hitstun) assumes the request context carries the team, the kits' own
extension data and the capacity. It does not:

- `FGSTN_LinkRequestSide` (`GSTNLinkCore/Public/State/GSTN_LinkState.h:258`) holds the id, a tag container, `EffectiveWeight` (from the
  snapshot's weight scale), immunity and cooldown frames and the link states. **No team** (the snapshot entry has one,
  `Plugins/GSTNCombatNP/Source/GSTNCombatNP/Public/World/GSTN_StepSnapshot.h:216`, but `MakeSide`, `GSTN_LinkTickExtension.cpp:38-51`, drops
  it), **no attribute** (no `MaxCarryWeight`).
- The tags are a 32-bit mask over a fixed table (`GSTNLinkNP/Public/Sim/GSTN_LinkSnapshotExt.h:84-109`): only tags some registered
  definition's `RequestRequirements` (or the grab/latch default rules) name. Workable for capability tags and `Combat.State.Hitstun` if the
  mount definition names them, but the table is capped at 32 and shared by every type.
- The kit rules in the context are typed to the two shipped types: `InitiatorGrabRules`, `InitiatorLatchRules`
  (`GSTN_LinkState.h:299-327`, fields `:323-324`; filled at `GSTNLinkGAS/Private/GSTN_LinkRequirementLibrary.cpp:61-62` and
  `GSTN_LinkInteractProvider.cpp:51-52`). A third type has no slot for its kit data, so per-kit capacity (M7.7) and `bCanBeMounted` /
  `bCanRide` cannot reach the rule. The EXT carry rule worked around it with a per-type property (`GSTN_ExtTestCarryRule.h:30`), which is
  per type, not per kit.

On the hit path the team is handled before the link step (the hitbox's target filter and `TeamFilter`), so a mount *hit* can be made
ally-only. On the interact path (Y8) the pre-filter runs the same rules with no team, so an enemy in reach would be offered as mountable.

## Proposed fix (generic, no piggyback name; fits G1 as a third item)

1. **Interact offers as a registry** (Y8): `UGSTN_LinkRegistry::RegisterInteractOffer(TypeTag, MeaningTag, Priority)` (or two definition
   fields, `InteractMeaning` + `InteractReachCm`, read by the registry), with `MakeGrabOffers` → `MakeOffers` (the grab offer and its
   alternatives first, then registered offers in a stable order), `GetLinkMeanings` built from the registered meanings with their priorities
   (INT.5: Grab before Mount), the result returning whatever `Interact.Target` leaf was projected, and the gate widened from "kit has a link
   extension" to "some offer applies to this kit". Test: grab sequences byte-identical, a test-only second meaning projected.
2. **Rule context** (R1): add the team to `FGSTN_LinkRequestSide` (from the snapshot entry) and a generic per-side
   `const FGSTN_KitExtensionList*` (the kit's extension array, already resolved for the body data) so a rule finds its own kit struct with
   `Find<T>()`; keep the typed grab/latch pointers. The capacity then lives in the piggyback kit extension (per kit) rather than the
   attribute, or the attribute is copied into that struct at compile time.

Both are in `GSTNLink` only (no `GSTNCombat` edit), touch `GSTN_LinkTickExtension.*`, `GSTN_LinkInteractProvider.*`, `GSTN_LinkState.h`,
`GSTN_LinkRequirementLibrary.cpp`, `GSTN_LinkRegistry.*`: the spin-drill thread must be told before and after (plan section 7), and its
drill latch (an interact-formed type, D9/D10) needs item 1 as well.

Plan corrections for `m7-doc-deltas.md` (not blocking): Y12 (no link executor; apply link effects from the link tick extension as the exit
effect is applied), Y16 (no `FGSTN_KitExtension::Compile`; the loadout extension registry), Y4 note (the predicted mark is per registered
type).

## S1, S2

Not run: S0 is a stop condition. No build, editor, PIE, standalone process or scratch code was started, so there are no logs to name.
Planned as written once the owner answers (the mount-like scratch type and the `FormTestLink` command do not need the fix above).

## Logs

None: the audit is source-only (`git diff --stat`, greps on HEAD 365a0e0).

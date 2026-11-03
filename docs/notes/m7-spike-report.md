# M7 spike report (checkpoint G0)

Plan: `docs/architecture/plans/m7-piggyback-plan.md` (branch `claude/project-thread-wzb12r`), sections 0, 1, 3 G0 and 4.
Machine: desktop, worktree `GASP_Giustino2_feature`, branch `feature/gas-npp-5.8` at 365a0e0 (spin-drill D3). Date 2026-11-01.

**Result: S0 STOPS.** Two seams the mount needs are worse than the plan's section 1 says: the interact offers of the link probe are
hard-wired to the grab (Y8), and a request rule cannot see the team or its own kit data (behind Y1/Y2). Plan section 4, first bullet:
stop and ask. Both gaps have a small generic fix in `GSTNLink` (proposal below), **approved by the lead on 2026-11-01 as a G1 item**
(plan commit fb4bd36 on `claude/project-thread-wzb12r`); the lead then asked for S1 and S2 anyway (they do not depend on the fix).

**S1 (second pass, same day): SERVER-GIVEN for every pairing whose rider is a client** (host carrier, client-client); host rider is
PREDICT only because the rider simulates on the authority. On a client rider's own machine its anchor is the authority's anchor of
**17-21 frames earlier** (about 0.3 s): p95 96-110 cm walking, 123-139 cm running, 364-398 cm with dashes, against a limit of 10 cm.
**S2: no reconcile-induced pop** at link start or end on the carrier's own client; two presentation findings (a fixed 74 cm second step
after every attach, and the rider's mesh not riding on the rider's own client). Details, logs and two harness findings below.

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

## S1 anchor agreement

### Setup (throwaway, nothing committed)

- **Mount-like type**: the plugin's own latch test type `DA_LinkType_Latch_Test` (initiator = rider parked and acting, target = carrier
  acting: the mount's shape), formed by the `LinkCommand` Form debug command (`UGSTN_LinkContentLibrary::FormTestLink`, which runs no
  request rule, so weights and the latch rule do not matter). No new code path and no copy of the EXT carry plugin was needed.
- **What is compared**: the rider's **anchor** = its entry's transform in the combat **step snapshot** (where it is hit and where its hitboxes
  start; the parked capsule itself never moves, `UGSTN_LinkedMovementMode`), on the rider's own client at snapshot frame F (the first
  record of F: what that client predicted with) against the authority's anchor at the same F. Frame numbers are the authority's on both
  sides. A lag search (authority frame F + k, k = -40..3) shows what the client's anchor actually equals. Presentation (the
  `GetLinkedTransform` location, the carrier's shown frame on both sides) is a note.
- **Tooling**: an untracked scratch plugin `Plugins/GSTNM7Spike` (one `UFUNCTION` for the client mirror: `GetMirrorText` + snapshot frame,
  anchor, mesh and actor locations; deleted after the runs) and scripts in `Saved/m7spike/` (`m7spike.py` patches `link_mp` with the
  scenarios `s1_*`, `s2_*`, `s2n_*`; `m7_standalone.ps1` / `m7_host.py` = copies of the F14 standalone launcher). Standalone listen host +
  2 client processes, all `-nullrhi`, 60/15 ms, fresh processes per batch; host 284-296 fps mean, p95 3.4-4.6 ms.
- **Carrier motion** (authority speeds, cm/s): idle 0; walk (stick 0.35) mean 293-295; run (stick 1.0) 391-394, max 399; dash (run + a
  dash tap every 1 s) mean 484-543, peaks 1400. Circles at 2.5 rad/s; each window 6 s, judged from 0.7 s after it starts.
- **Pairings**: host carrier = carrier host, rider client 1; host rider = rider host, carrier client 1; client-client = rider client 2,
  carrier client 1 (client 2 as carrier ran into the harness level's travel trigger in the first batch).

### Results (rider's own client, anchor vs authority at the same frame, cm; rollback = the client's rollback counter steps in the window)

| Pairing | Loss | Walk p50 / p95 / max | Run p50 / p95 / max | Dash p50 / p95 / max | Rollback (all windows) | Lag that explains it (p95 there) | Verdict |
|---|---|---|---|---|---|---|---|
| host carrier | 0 % | 95.0 / 95.9 / 96.4; 103.4 / 104.3 / 105.5 | 122.2 / 123.4 / 123.9; 131.5 / 133.2 / 133.6 | 123.1 / 364.4 / 418.4 | 0 | k = -17..-20 (2.1-7.2) | **SERVER-GIVEN** |
| host carrier | 2 % | 107.0 / 108.7 / 112.3; 106.3 / 109.6 / 111.0 | 132.8 / 139.1 / 140.0; 134.0 / 138.1 / 140.3 | 131.4 / 379.5 / 432.5 | 0 | k = -20..-21 (2.7-13.3) | **SERVER-GIVEN** |
| client-client | 0 % | 97.5 / 98.5 / 99.2; 95.4 / 97.1 / 98.1 | 127.0 / 128.5 / 130.4; 123.4 / 124.9 / 125.6 | 124.1 / 375.6 / 426.0 | 0 | k = -19..-20 (1.8-4.3) | **SERVER-GIVEN** |
| client-client | 2 % | 101.9 / 105.6 / 106.3; 103.2 / 107.4 / 108.5 | 133.7 / 136.8 / 137.8; 130.3 / 134.6 / 137.5 | 131.6 / 398.1 / 433.4 | 0 | k = -20 (3.9-6.6) | **SERVER-GIVEN** |
| host rider | 0 / 2 % | 0 by definition | 0 | 0 | - | - | **PREDICT** (rider on the authority) |

Two runs per cell where two numbers are given (`s1-0pct-2` / `-3`, `s1-2pct-1` / `-2`); dash only in the runs after the dash fix (`s1-0pct-3`,
`s1-2pct-2`; the earlier "dash" windows were plain runs, see harness finding 2). Idle: p50 0, p95 0-19.3, max up to 46 cm (the carrier settling
after the placement). Observer clients read the same lag (k = -17..-23). In host rider, the **carrier's** own client holds the rider's anchor
exactly (0.0 cm walking and running: it predicts the carrier); in its dash windows p95 12.7 / 26.5 cm with 4 / 6 rollback steps, the carrier's
own dash corrections (`s1-0pct-3`, `s1-2pct-2`), not the link.

**Why**: on a client, a pawn that does not simulate there contributes "the pose it shows" to the step snapshot
(`Plugins/GSTNCombatNP/Source/GSTNCombatNP/Private/Simulation/GSTN_CombatSimSnapshot.cpp:79-95`): the carrier, remote on the rider's client,
is its interpolated proxy, about 18-20 frames behind the frame the rider's client predicts; the link contributor puts the rider on that pose's
socket. So the rider's anchor equals the authority's anchor of ~0.3 s earlier (p95 2-13 cm at that lag), which at running speed is 1.1-1.4 m
off. **Rollbacks were 0** in every held window because nothing in the spike reads the anchor in a predicted outcome; a predicted rider hit or
rider-initiated exit placed from it would be off by that distance, hence the verdict on the p95 rule. What the rider's player sees is
consistent with its anchor (shown vs the authority's shown at the same carrier frame: p95 3.8-5.1 cm walking/running, 10.4-11.9 cm with dashes).

**Consequence for M7** (plan section 4, second bullet: "S1 fails for every pairing"): every pairing with a client rider fails, which is
every real case except a host riding. Per the plan, rider-initiated exits and rider hitboxes take the **server-given** fallback (M7 §6.4) for
client riders, unless the anchor is built from a forward-predicted carrier (an NP/fork change) or from the rider's predicted copy of the
carrier. This is the stop-and-ask case of the plan: **owner decision**.

## S2 presentation at link start and end (carrier predicted on its own client)

Pairings host rider (carrier client 1) and client-client (carrier client 1, rider client 2), carrier running in circles. Forced reconciles
on the carrier's client (`np.ForceReconcileExtraFrames 10` + `np.ForceReconcile 1`) at form + 0 / 0.1 / 0.2 / 0.35 s and at break + the same
(8-11 rollback counter steps per window on that client: they landed). Baseline `s2n_*` without forced reconciles. Mesh positions per
engine frame from the mirror.

- **No reconcile pop**: on the carrier's own client, exactly one link phase change and one shown-parked change at start (+145..+184 ms after
  the host's command) and one at end (+145..+174 ms), the rider mesh's attach or detach step, and no other step above 15 cm beyond the
  carrier's own motion, except the next finding. Same with and without the forced reconciles (`s2_*` vs `s2n_*`, `s1-2pct-1`).
- **Finding A, a fixed 74 cm second step**: 3-4 ms (the next engine frame) after every attach, the rider's mesh moves 74.0-74.3 cm once more,
  on the carrier's client and on the observer, in every run, with or without forced reconciles: the attach places the mesh, then a second
  placement corrects it by a constant. A presentation detail of `UGSTN_LinkPresentationComponent` (or the mesh offset), not prediction.
- **Finding B, the rider's mesh does not ride on the rider's own client**: on client 2 (rider) in client-client the mesh steps 332 cm to the
  carrier and 332 cm back 3 ms later, and no detach step at the end. Over the held link the rider's mesh sits on its own parked capsule
  (mesh to own actor xy p50 0.0 cm) and p50 259 / p95 415 cm away from its linked transform; on the carrier's client the same mesh rides at
  a constant 42 cm from the linked transform (mirror `Saved/Logs/mirror/P258.txt` and `P257.txt`, run `s2_client_client` of `s1-2pct-2`).
  Probably the locally controlled pawn's mesh placement overrides the presentation component. Not checked with rendering on (all runs
  `-nullrhi`), and not checked for a host rider. This likely affects M6's latcher on its own client too (M6 measured `GetLinkedTransform`,
  not the mesh). For G1 (attach during Pending) / G10.

## S3 optimistic mount cost (checkpoint G5, laptop, 2026-11-01)

Measured by `Tools/GASNPPTest/mount_scenarios.py` (G5) on the real mount (a hit of `UGSTN_GA_Mount` through GSTNLink's LinkRequest step, the
optimistic accept of G1 at `PendingFrames` 2), listen host + 2 clients in one editor process, 60/15 ms, rider = client 1, carrier = client 2.
Rollbacks are the rider pawn's Mover post-simulation rollbacks (the harness counter); the visible snap is the largest jump of the rider's shown
position between two editor ticks on its own screen (the parked presentation while it shows one, else the actor).

| Case | 0 % (`mount-listen-0pct.log`) | 2 % (`mount-listen-2pct.log`) |
|---|---|---|
| Accepted mount: rollbacks on the rider's client | 0 | 0 |
| Accepted mount: corrections on the carrier's client (it learns the link from the authority) | 1 | 1 |
| Refused mount the rider cannot see (the server links the carrier on the tick of the tap): rollbacks on the rider's client | 3 | 3 |
| Same: visible snap on the rider's screen | 117 cm (3-4 ticks shown on the carrier's back, then back to its feet) | 117 cm |
| Refused mount the rider can see (`KIT_Test_MountHeavy`: the probe offers no mount) | no request, 0 rollbacks | 0 |
| Dismount (server-given exit): rollbacks on the rider's client | 3 | 3 |
| Dismount: rider's place, own client vs authority after it settles | 0.0 cm (90 cm behind the carrier) | 0.0 cm |

**Verdict.** An accepted mount is free on the rider's machine, as G1 designed (the flag-only difference is not a reconcile). An unseen refusal
is NOT one rollback as the plan assumed but about three at 60/15 ms: the rider's client re-predicts the accept against its interpolated copy of
the carrier, which still shows the carrier free, until that copy catches up; each authoritative packet in between corrects it again (the log
shows the linked modifier started and ended "after a rollback" on consecutive engine frames). The snap is the full attach offset (117 cm), shown
for 3-4 ticks: without the presentation blend of G10 it is visible. Unseen refusals need the carrier's state to change on the server within the
interpolation delay of the tap (another link, a hit, a lock), so they are rare; a refusal by data (kits, teams, weights) is predicted and costs
nothing. Options for later (OPEN): a refusal memory on the rider (do not re-predict a serial the authority refused), or no optimistic attach
until the carrier's copy is newer than the request frame.

## Other findings

1. **Unity build clash in `GSTNLinkNP` tests (blocks a clean build)**: `GSTN_LinkNPTests_ExitCollision.cpp` (F15) and
   `GSTN_LinkNPTests_Watchdog.cpp` both define `GrabberId`, `GrabbedId`, `Serial`, `LinkFrame` and `MakeSide` in an anonymous namespace; in the
   module's one unity file they collide (C2374/C2086/C2084), and the global `Serial` makes `GSTN_LinkNPTests_Fragment.cpp:15` fail C4459.
   It passed until now only because new or modified files are compiled outside the unity file (adaptive unity). `build-m7g0-spike-1.log`
   failed on HEAD's sources when an added plugin regenerated the unity files; `-DisableUnity` did not reach the plugin module
   (`build-m7g0-spike-2.log`). Fix (verified in a unity build, `build-m7g0-spike-3.log`, on a scratch local branch, deleted): wrap the
   ExitCollision file's content after its includes in `namespace GSTNLinkExitCollisionTest { ... }`. **For the G1 session, first thing.** The
   worktree's `GSTNLinkNP` DLL is the scratch build (test code differs only); `build-m7g0-head.log` on HEAD is a no-op ("up to date").
2. **Standalone harness never dashed**: in a `-game` process `gasnpp_test._tag_text` cannot read a tag's name (editor-only property), so
   `button_mask` raises and every `tap(Dash)` of `link_standalone`/`link_mp` silently failed: the F14/F15 standalone spikes' walks had no dash
   (`f15-sweep/after0-1-host.log` etc. contain "is not a button of"). The spike tapped raw bits 3 and 8 (Dash, Sprint of `IC_GASNPP_Test`, order
   from `Tools/GAS/setup_gas_assets.py`). Fix: resolve names with `unreal.BlueprintGameplayTagLibrary` or cache the indices from the host.
   (The library call alone did not resolve them in `-game` either.)
3. Harness level: a pawn running far enough reaches a travel trigger to `RagdollLevel` (first batch, `s1-0pct-1-c1.log` "Browse ...
   RagdollLevel"): keep walks in circles near the spawns.

## Logs (all under `Saved/Logs/m7-g0/`)

| Log | What |
|---|---|
| `build-m7g0-spike-1.log` | Failed: the unity clash on HEAD's sources (finding 1) |
| `build-m7g0-spike-2.log` | Failed: same with `-DisableUnity` |
| `build-m7g0-spike-3.log` | Succeeded: scratch branch with the namespace fix, scratch plugin |
| `build-m7g0-head.log` | Succeeded, no action: back on HEAD, scratch plugin deleted |
| `s1-0pct-1.log` (+ `-host`, `-c1`, `-c2`) | First batch: lag search too short, client-client hit the travel trigger; not used for verdicts |
| `s1-0pct-2.log`, `s1-0pct-3.log` | 0 %: S1 three pairings (+ warm-up), S2 two pairings; `-3` with dashes |
| `s1-2pct-1.log`, `s1-2pct-2.log` | 2 %: S1, S2, and in `-1` the S2 baselines `s2n_*`; `-2` with dashes |

Counts come from the reports' per-window lines (the rider client's mirror records against the host's anchors); each report names its
scenarios, sample counts and the host frame statistics.

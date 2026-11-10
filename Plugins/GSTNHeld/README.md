# GSTNHeld

Held and thrown objects for the GSTN combat (milestone M10, plan [m10-held.md](../../docs/architecture/plans/m10-held.md) on the plan
branch, spec [combat-m10-held-objects.md](../../docs/architecture/combat-m10-held-objects.md)). Built on
[GSTNProjectile](../GSTNProjectile/README.md): a throw **is** a projectile shot, so the host sweep, the hit into the victim's inbox, the
credit to the thrower and the predicted ghost come from that plugin unchanged ("deterministic first",
[entita-dinamiche review](../../docs/notes/reviews/entita-dinamiche-2026-11-09.md) §2, §9 category (a)).

Status: **experimental (0.1), slice 5**, `EnabledByDefault: false`, enabled in `GASP_Giustino2.uproject`. Nothing else knows it.

## Modules

| Module | Type | What it is |
|---|---|---|
| `GSTNHeldCore` | Runtime | Pure (engine + `GSTNCombatCore` + `GSTNProjectileCore`). `EGSTN_HeldObjectState` (Idle, Held, Flying), `FGSTN_HeldObjectTuning` (pickup radius, hold offset, rest half height, the throw as `FGSTN_ProjectileParams`; every default a test value), `FGSTN_HeldRules::ResolvePickups` (the race: earliest frame, then lower holder id, nearest object in range, lower object id on a tie), `HoldTransform`. Tests `GSTNHeld.Core.*`. |
| `GSTNHeldRuntime` | Runtime | World side. `UGSTN_HeldObjectDefinition` (data asset with the tuning), `UGSTN_HeldObjectComponent` (replicated state, holder, flight, rest location; places its actor on every machine), `AGSTN_HeldObjectActor` (ready-made object, engine cube placeholder), `UGSTN_HeldSubsystem` (host: pickups, holder gone, flight landing), abilities `UGSTN_GA_HeldPickup` / `UGSTN_GA_HeldThrow` and their ServerOnly children, dev commands `gstn.Held.Spawn`, `Pickup`, `Throw`, `Report`. |

Slice 2 adds to the core: `FGSTN_HeldBreakRules`, `FGSTN_HeldThrowInput`, `EGSTN_HeldBreakReason`, `EGSTN_HeldThrowAim` and their rules
(`BreaksOnState`, `BreaksOnEvent`, `ChooseThrowAim`, `DropPoint`), native tags `Combat.Held.Carrying` and `Command.Held.Pickup/Drop/Throw`,
`UGSTN_HeldSlots` (default slots and bindings) and `UGSTN_HeldSettings` (`Config/DefaultGSTNHeld.ini`, every value a TEST VALUE). To the runtime:
`FGSTN_HeldFragment` (sync fragment, feature id `Held` = 20), the `Held.Command` inbox kind (48), `FGSTN_HeldTickExtension` (projects
`Combat.Held.Carrying`), `UGSTN_GA_HeldDrop`, the hold-to-aim throw, the object definition's `CommandActions` / `RangedAction` hooks and
`gstn.Held.Drop`. Tests `GSTNHeld.Core.BreakRules`, `ThrowAim`, `DefaultSlots`, `GSTNHeld.Runtime.HandFragment`.

## How it works

1. **At rest** the object is a replicated actor with nothing dirty: 0 bytes. Movement replication is off; every machine places it.
2. **Pickup**: `UGSTN_GA_HeldPickup` asks the host (`RequestPickup`). After the fixed tick of that frame the host resolves every request of
   the frame with `ResolvePickups` against the holders' combat snapshot positions and sets the winner's object `Held`.
3. **Hold**: every machine draws the object at its holder with `HoldOffset` in the holder's actor space (the plan's spike S2 fallback,
   root + offset; a hand socket later). If the holder disappears the host drops the object to the floor.
4. **Throw**: `UGSTN_GA_HeldThrow` is a `UGSTN_GA_FireProjectile` whose shot template is the object's `Throw` tuning (hook `PrepareShot`).
   On the host the shot is fired and the object becomes `Flying` with it (hook `OnShotFired`); the throwing client draws the projectile
   ghost. Nothing held: no shot.
5. **Flight and landing**: the object is drawn at its projectile. The host traces each frame's step of the segment against WorldStatic: on
   contact it removes the shot (`UGSTN_ProjectileSubsystem::RemoveProjectile`) and lands the object there; when the shot ended (hit or
   lifetime) the object drops to the floor below its last position. Landed objects are `Idle` and can be picked up again.
6. **The link (slice 5, the plan's G1)**: the hold is a GSTNLink **Hand link** of type `Combat.Link.Type.Held` (made in code,
   `FGSTN_HeldLinkType`: holder = role 1 `Combat.Link.Role.Holder`, object = role 2, a partner of kind `GSTNEntityKinds::Object` with no
   snapshot entry). The host forms it with a LinkCommand `Form` on the holder's inbox (stamped with the holder's latest frame; serial newer than
   the holder's Hand serial) and ends it with a `Break` of that serial (drop = Release, throw = Throw, holder state = Hit, KO = OwnDeath). The link
   fragment is sync state, so `Combat.Held.Carrying` (projected from the Hand link every frame) is predicted and rolled back; a client gets it by
   correction, as a grab. GSTNLink's own break rules stay off for this type: the held rules below end a hold; the link watchdog ends it only when
   the object is destroyed, through the held subsystem's partner resolver (`IGSTN_LinkPartnerResolver`, kind Object). Slice 2's own fragment
   (`FGSTN_HeldFragment`, inbox kind `Held.Command`) is gone. Log `HeldLink hold|release entity= object= serial=`.
7. **Drop and breaks**: `UGSTN_GA_HeldDrop` asks the host to put the object down; after the frame the host also ends the hold when the holder
   has a tag of the object's break rules (default: `Combat.State.Hitstun`, `Combat.State.Tumble`, `Combat.Link.Role.Grabbed`, matched by name,
   no dependency on GSTNLink) or gets an authoritative event of them (default `Combat.Event.KO`). The object lands `DropForwardCm` in front of the
   holder. Log line `HeldBreak object= entity= reason= frame=`.
8. **Buttons (owner's design 2026-11-10, defaults, subject to playtesting)**: grab button (Interact, B) = `Command.Held.Pickup` with nothing in
   hand (one below the grab, so a grab target wins), `Command.Held.Drop` with an object; strong button (RT) = `Command.Held.Throw` above the strong
   attacks while carrying. The throw waits for the release: a tap (shorter than `AimHoldFrames`) goes along the left stick (`EGSTN_ProjectileAim::Stick`,
   the facing when centred); held, the reticle shows (`UGSTN_PrecisionAimComponent::SetHoldAimShown`) and it goes along the camera; with the reticle
   toggled on (`bPrecisionAim`) a tap aims with the camera too. **Slice 3: the listed kits get them automatically**: the runtime module
   registers a loadout extension (`GSTNHeld`) that appends the three bindings with their slots (`UGSTN_HeldSlots::AppendKitBindings`) on every
   machine that compiles a kit in `KitsWithDefaultBindings` (test values: `KIT_Test_Hits`, the hub's default, and `KIT_Test_Link`), or every kit
   with `bAddDefaultBindingsToEveryKit`; a kit that binds a held command itself keeps its own. Not every kit by default: the other kits' tables
   (and the combat tests that count their rows) stay as authored. Other abilities with
   `PickupAbility` / `DropAbility` / `ThrowAbility` (`DefaultGSTNHeld.ini`). The module also contributes the tier `Command.Held` = 360 (as
   `Command.Link`) to the input settings: without it every strong attack (200) took RT from the throw. Offsets: pickup -2 (below grab 0 and
   mount -1), drop and throw 110 (above release 100, dismount 101, carrier drop 102), so no slot is ambiguous with a link or mount slot.
9. **Pickup target and prompt (slice 4, the plan's G2)**: an object at rest is a **stamped record** in GSTNInteract's registry
   (`FGSTN_HeldRecordStamp` on the component, replicated: handle, whole-cm anchor, server frames, and the end of the record before it). The host
   opens one when the object comes to rest, live from the frame after the last fixed tick plus the interact rules' `MinAgeFrames` (40, 0.64 s:
   a client holds the record before it simulates that frame), and closes it at the frame after the pickup or throw resolved; every machine adds
   or stamps the same record (`ApplyRecord`), so a frame and its re-simulation see the same set. **GSTNLink's interact probe** reads the records
   beside the snapshot entities and projects the one `Interact.Target.*` of the frame (for any kit, also one without link offers); this plugin
   no longer projects the tag. The meaning `Interact.Target.Pickup` is added to the GSTNInteract settings at module startup (unless the project
   lists it), priority `PickupMeaningPriority` (90, below the grab's 100; TEST VALUE), reach = the object's `PickupRadius`. The pickup binding
   requires the tag, so with nothing focused B falls through to what the kit binds below. **Slice 5**: the host gives a request the object
   whose record the probe focused for that holder on the request's frame (`UGSTN_InteractSubsystem::NoteProbeFocus`/`FindProbeFocus`, host data,
   not simulation state; `FGSTN_PickupRequest::TargetObjectId`) when it is free and in reach; else (taken by an earlier request of the frame, no focused
   record, a console pickup) the nearest free object in radius, as before; nothing = fumble. Log `HeldPickup entity holds object (focus id)`.
   Prompt: a placeholder text "B" above the local player's nearest object while it has the tag (`UpdatePrompt`, presentation only, never on a
   dedicated server). `HeldHandReport` logs `target=`.
10. **Thrown hits (slice 4, the plan's G3)**: the throw's shot carries `SourceTag = Combat.Source.Thrown`; GSTNProjectile puts it on the hit
   inbox entry with a **detached instigator** (the shooter at the release, else its player state), so the hit and KO events carry the source tag
   and name the thrower even when its entity id no longer resolves (death, disconnect). Slice 5: a minion's KO event too (GSTNMinion). Log `Projectile .. hit .. source= credit=`.
11. **Hooks, no content**: `UGSTN_HeldObjectDefinition::CommandActions` (what the other attack buttons do with this object) and `RangedAction`
   (its own ranged action) are data only; a later slice routes commands to them.
## Not yet (later slices of the plan)

Carry load and size classes (G6: the Held type's role effect is the hook), aim policy, friendly fire by mode, impact behaviours
Break/Bounce/Stick, ghost blend of the object itself (the projectile's ghost is used), thrown enemies (G9), editor preview. Minion hit delegates
(`OnMinionHit`) carry no source tag (only the KO event does).

Harness: `Tools/GASNPPTest/held_scenarios.py` (`SUITE_HELD`: `held_pickup_throw_hit`, `held_drop_and_break`).

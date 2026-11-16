# Handover — BP state queries + typed events (bp-exposure P1), 2026-11-16

Thread "Expose combat state and events to Blueprint". Cloud, Opus. Branch `claude/project-thread-brrdze` from `claude/integ-playtest-1116` @ b528f9a.
Owner decisions: 1A (P1 now), 2A (online: events reach clients). Design: `docs/architecture/combat-bp-state-events.md`.

## Done (C++ only, no asset, no map, no movement/attack-direction/run fields touched)
- `GSTNCombatNP/Presentation/GSTN_CombatStateTypes.h/.cpp`: `FGSTN_CombatStateView` (BP struct), `EGSTN_CombatStatus`, `EGSTN_CombatAirState`,
  `FGSTN_CombatHitEventData`, `FGSTN_CombatKOEventData`, pure `FGSTN_CombatStateWatcher`.
- `Presentation/GSTN_CombatStateLibrary.h/.cpp`: 17 BlueprintPure nodes by actor (Combat|State).
- `UGSTN_CombatSimComponent`: Get Combat State, Get Frames Since Landed, Get Current Move Tags, Get Last Hit Received/Landed; confirmed events
  On Hit Received / On Hit Landed / On Knocked Out / On Combat Event (NetMulticast Reliable from the authority's FinalizeFrame); state events
  On Move Started/Ended, Hitstun, Tumble, Landed, Left Ground, Run Stage Changed (edges of presented state). New file `Simulation/GSTN_CombatSimState.cpp`;
  hooks: `FinalizeFrame` (2 calls before `PublishOutbox`), `AppendVictimEvents` (typed hit copy into `HitEventOutbox`, local data like `EventOutbox`).
- Tests `GSTNCombat.NP.StateEvents.{ViewFromCore,WatcherEdges,Payloads}` (pure, no world). README of GSTNCombatNP updated.

## NOT verified (cloud has no UE)
- Not compiled, tests not run. First step on desktop: build + `GSTNCombat.NP.StateEvents` + full headless suite (baseline 734/734 on integ-playtest-1116).
- Online check still to do: PIE dedicated server + 2 clients, bind On Hit Received on a client, hit: it must fire once on each client.

## Choices applied (rule 6)
- Confirmed vs state split (table in the design doc); predicted hit stays on HitPresentationComponent.
- Reliable multicast per event, switch `bSendConfirmedEventsToClients` (default on). Input events not sent.
- Launch angle measured against world up (not the planetoid gravity); "damage" = hitbox damage, like the outbox event magnitude.
- Knocked down / teched / shield broken: no typed event (Defence is an optional plugin CombatNP cannot depend on); they arrive in On Combat Event.

## Da confermare (to owner via coordinator)
- None blocking. P2 of the audit (BP timeline track, validator, knockback model exposure) remains queued.

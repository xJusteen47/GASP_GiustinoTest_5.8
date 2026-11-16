# Combat state and events for Blueprints

Owner decisions 2026-11-16: 1A (build P1 of the BP exposure audit), 2A (the public plugin supports online: Blueprint events reach the clients).
Code: `Plugins/GSTNCombatNP/Source/GSTNCombatNP/` — `Presentation/GSTN_CombatStateTypes.h`, `Presentation/GSTN_CombatStateLibrary.h`,
`Simulation/GSTN_CombatSimState.cpp`, the "Blueprint state and events" block of `Simulation/GSTN_CombatSimComponent.h`.

## Rule: nothing here is simulation

Every query and event runs outside the Network Prediction tick: queries read the **presented** state (`ReadSyncState` presentation, Mover's
`GetSyncState`), events fire from `FinalizeFrame`. Nothing writes sync state, so rollback and determinism are untouched. An ability that decides
inside the simulation keeps using the simulation reads (`GetCombatClock`, `IsInTimeline`, `GetActivationContext`...). A Blueprint listener must
not change simulation state (start a move through a command, not from an event).

## Two kinds of events

| Kind | Events | Source | Where it fires | Timing | Guarantee |
|---|---|---|---|---|---|
| Confirmed | On Hit Received, On Hit Landed, On Knocked Out, On Combat Event | the authority's outbox (`EventOutbox` + the typed `HitEventOutbox`), read in `FinalizeFrame` before `PublishOutbox` | server (incl. listen host) and every client, via `NetMulticast, Reliable` | server: the frame it happens; client: one trip later | exactly once per machine; never a predicted or re-simulated copy (clients drop their own outboxes) |
| State | On Move Started/Ended, On Hitstun Started/Ended, On Tumble Started/Ended, On Landed, On Left Ground, On Run Stage Changed | `FGSTN_CombatStateWatcher` comparing the presented state of one finalized frame with the previous one | every machine, from its own presented state | owning client: predicted (immediate); server: authoritative; others: interpolated | one fire per change seen; a re-simulation that ends in the same state fires nothing; a correction that changes the presented state shows as a change (and its undo) |

The attacker's own predicted hit (no round trip, may be cancelled) stays on `UGSTN_HitPresentationComponent` (On Hit Predicted / On Hit Prediction
Cancelled). Recommended use: confirmed for game flow, score, UI, kill feed; state for animation and camera; predicted for hit-feel.

On Hit Landed is fired on the **attacker's** component (the victim's tick makes the hit; the server routes it). A ricochet gives On Hit Landed
(bRicochet) and no On Hit Received, like the outbox. The credit partner's copies of HitLanded/KO are only in On Combat Event (tag
`Combat.Event.Hit.CreditPartner`). `bSendConfirmedEventsToClients = false` keeps the confirmed events on the server.

Cost: one reliable multicast per confirmed event (a hit = 2: received + landed, plus On Combat Event copies). Input events
(`Input.Event.CommandExecuted`) are not sent.

## Queries

`Get Combat State` (component) / `UGSTN_CombatStateLibrary::GetCombatState(Actor)` return `FGSTN_CombatStateView`; single-value nodes exist for
the common fields. Airborne comes from Mover's `Mover_IsOnGround` in the presented Mover state when the pawn has Mover (a hit launch is airborne),
else from the combat jump phase. Run stage from `FGSTN_RunStageSyncState` (read only; no run field changed). Last hit received/landed is the last
confirmed event seen on this machine.

## Not done (P2 of the audit)

Custom BP timeline track, validator for BP steps/models, `ResolveRules` exposure, debug API cleanup, runtime hurt state.

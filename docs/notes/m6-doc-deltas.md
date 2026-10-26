# M6 doc deltas (defence and links)

Corrections and additions to the design documents that the M6 checkpoints found, collected per checkpoint and folded into `docs/` by F14 (one docs commit). Plan: `docs/architecture/plans/m6-defence-links-plan.md` (on `claude/project-thread-mvizq6`). Test counts come from named logs under `Saved/Logs/`.

## F0 S0: seam audit (4092e4f)

Details in `docs/notes/m6-spike-report.md`. No row of plan section 2 is worse than audited. Doc deltas for the plan and for combat.md:

- X18 (`GSTNInteract`) and X19 (`GSTNCombatDebug`) are on `feature` since the merge of the desktop branch: the plan's "not on feature" rows and the F9 fallback condition no longer apply.
- X20: the combat request array has `Capacity = 8` slots (wire: 4-bit count, 5-bit type, packed frame, 16-bit argument), not 4; the 5-bit type field holds 32 values and 17 are used (`EndReaction` = 17). F2's "grow to 6" is already satisfied.
- X6: timeline tracks are open through `TInstancedStruct<FGSTN_TimelineTrack>` (a plugin can derive a track struct) but there is no registry object; the clock hold-stall and parry-window stamp hooks are F2 work as planned.
- X11: `EGSTN_CancelCondition` is a closed enum (`Always, OnHit, OnBlock, OnWhiff`); `OnParry` therefore needs a neutral enum value in F2, not a registration in F7.
- X2: the tick-extension hook points are `AfterClocks, BeforeCandidates, AfterExecute, BeforeTagProjection`; there is no "after inbox drain" point. F1's inbox kind handlers run at the drain, before the clocks.

## F1: open inbox kinds, entity ref, hit-data fields

Full entry: `docs/notes/m4-doc-deltas.md`, "M6 follow-up 1 (F1)". For combat.md and modularity-and-tags.md (folded by F14):

- The combat inbox is `FGSTN_CombatInbox` of `FGSTN_InboxEntry` (kind, frame, sender, instanced-struct payload); the hit is kind 0. Kinds register in `FGSTN_InboxKindRegistry`; drain order is (SenderId, Kind, the kind's key, arrival) and the frame stamp is only the delivery gate (the plan's "(Frame, SenderId, Kind, order)" would contradict the pinned `Hits.Inbox.DrainOrder` test).
- Central ids in `Sync/GSTN_SyncFragment.h`: inbox kinds, entity kinds, feature ids `Defence = 17` and `Link = 18`.
- `FGSTN_EntityRef` (Kind, Id, Serial) in `GSTNCombatCore`.
- `FGSTN_HitData` gained `bParryable`, `bDisableDI`, `LinkFilter`, `LinkType`; `IsBlockable(Rules)` reads `UGSTN_CombatRules::NonBlockableHitTypes`. Not yet readable by any step; not yet overridable by a hit variant.
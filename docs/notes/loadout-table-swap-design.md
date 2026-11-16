# Loadout table swap: design for the combat simulation (M3 leftover 1)

Branch feature/gas-npp-5.8 at 4fe0187. Scope: feed the loadout's compiled table into `UGSTN_CombatSimComponent` and swap it
deterministically at a loadout revision boundary, so `command_set`, `super_code`, `ex_meter`, `wait_frames` can follow.
Paths are relative to the repo root. Nothing in this document is implemented yet.

## 1. Current state

**Two table shapes.** Core (`Plugins/GSTNCombat/Source/GSTNCombatCore/Public/Loadout/GSTN_CompiledBindingTable.h`):
`FGSTN_CompiledBindingTable` owns `Slots` (one `FGSTN_InputSlotSpec` per command, first-use order), `Entries` (`FGSTN_CompiledEntry`:
`Source` Kit/Weapon/Super, `SourceIndex`, `Command`, `Requirements`, `SlotIndex`, `Ability` as `FSoftObjectPath`, `TimelineIndex`, per-binding
`PriorityOffset` / `BufferFramesOverride`), `Registry` (ability + override path pairs, sorted, deduped), `GrantedAbilities`, `Revision`, and a
deterministic `WriteBytes`. Input NP (`Plugins/GSTNInputNP/Source/GSTNInputNP/Public/Bindings/GSTN_CommandBindingSet.h:44-117`):
`FGSTN_CommandTable` has `Commands` (sorted by tag name, `Slot` is a raw pointer into the asset, `FirstBinding`/`NumBindings`, `LockTags`),
`Bindings` (`FGSTN_CompiledBinding`: `CommandIndex`, `Requirements`, loaded `TSubclassOf<UGameplayAbility>`, no source), `CandidateSlots` /
`CandidateCommands` (fixed at build), `WatchedTags`, `ResetRules`. Limits: 255 bindings (request stores a byte), 64 watched tags, registry int16.
The combat component builds the NP table once from the Blueprint's `SlotBindings` (`GSTN_CombatSimComponent.cpp:103-205`, `CompileInput`) and, in the
same pass, `TimelineRegistry`, `TimelineTier`, `TimelineDefaultOverride`, `BindingTimeline`, `BindingJumpRole`, `TableJumpData`,
`TimelineContent`. They are read-only members, so they are not rolled back; `AdoptableTimelines` are appended after the bound ones.

**Revision and gate.** `FGSTN_CombatSyncCore::LoadoutRevision` (uint16, `GSTN_CombatSimTypes.h:54`) is serialised, compared in `ShouldReconcile`
(`:156`) and written only out of band by `ServerWriteLoadoutRevision` (`GSTN_CombatSimComponent.cpp:247`). `IsLoadoutCurrent(TableRevision)`
(`GSTN_CombatSimTypes.h:72`) is defined and unit-tested (`GSTNCombat.NP.State.*RevisionGate`) but nothing in the tick reads it: the table never changes,
so the gate is moot. `UGSTN_LoadoutComponent` (GSTNCombatGAS) already compiles `FGSTN_CompiledBindingTable` on every machine in
`ApplyAppliedLoadout` (server directly, client `OnRep_Applied`, `GSTN_LoadoutComponent.cpp:222`), keeps the resolved assets and broadcasts
`OnLoadoutApplied(Component, Revision)` (`:255`); its `Revision` replicates in the same update as `Applied`. `ServerApplyLoadoutChange`
(`GSTN_CombatSimComponent.cpp:259`) validates, cancels the running ability and clears the timeline out of band, `AcceptLoadout` (re-grant, kit stats),
then writes the revision. The harness (`GSTN_GASNPPTestComponent.cpp:960-983`, `Tools/GASNPPTest/gasnpp_test.py:2766`) compares the loadout
component's `tableCrc` (CRC of `WriteBytes`) and `loadoutRevision` of the combat state on host and owner; it never sees the NP table.

**WeaponStats swap, the pattern to copy.** Step 4 of the tick (`:571-600`) resolves the new index (`FGSTN_WeaponSwitchRules::Resolve`), writes
`ActiveWeaponIndex` and a frame-stamped `SwapWeaponStats` request (Arg = old index) into the sync state; the ASC pre-tick
(`HandleAbilitySimulationPreTick`, `:725`) executes it with `FGSTN_WeaponStatsSwap::Execute` (effects found by class, idempotent) before the
frame's activations. So the rule is: decide in the combat tick from rolled-back state only, record in sync state, execute in the pre-tick of the same
frame. `WeaponStats` itself is an `EditDefaultsOnly` array fixed at BeginPlay and never filled from the loadout.

## 2. Design

### Principle
The active table is a pure function of rolled-back state: `Tables = Lookup(InSync.Core.LoadoutRevision)`. No component member says "the current table",
so a rollback or re-simulation of frame F always meets the table that frame had the first time. The swap is a state transition (new field
`TableRevision`, cleared buffers, cancelled timeline) plus that lookup; a client that reconciles to a post-swap server state adopts the transition
without running it. Nothing branches on role.

### (a) Converter Core table -> NP command table
New in GSTNCombatNP (it sees both Core and GSTNInputNP; it uses no GAS type, only `GameplayAbilities` classes it already links):
`Simulation/GSTN_CombatTableSet.h/.cpp`.
- `struct FGSTN_CombatTableConverter { static bool Convert(const FGSTN_CompiledBindingTable&, const UGSTN_InputSettings&, TFunctionRef<UClass*(const FSoftObjectPath&)> LoadClass, FGSTN_CombatTableSet& Out, FGSTN_CompileReport*); }`.
  Pure: the class loader is injected (the component passes a synchronous loader, a test passes `UGameplayAbility` subclasses from the transient package),
  no world, no ability system instance. Game thread only at registration, never inside a tick.
- `FGSTN_CombatTableSet` (a `UObject`, Transient, so it can hold `TObjectPtr`s and the timeline content) owns: a copy of the compiled table (the
  `Slots` the NP table points into; heap-stable, never resized after conversion), `FGSTN_CommandTable Table`, `TimelineRegistry` (classes in
  `Compiled.Registry` order, then the component's `AdoptableTimelines`), `TimelineContent`, `TimelineTier`, `TimelineDefaultOverride`, `BindingTimeline`
  (`= Entry.TimelineIndex`), `BindingJumpRole`, `TableJumpData`, `WeaponStats`, `Revision`, `Checksum`. This is exactly the set `CompileInput` builds today;
  the refactor moves the members into the set and `CompileInput` produces revision 0 from the Blueprint binding set through the same registry code.
- Mapping rules (all deterministic): Commands = distinct entry commands sorted by tag name (LockTags as today); Bindings sorted by (command name, entry order),
  entry order is kit, weapon 0..N, super (the Core order). A binding's slot is `&Compiled.Slots[SlotIndex]`; a binding whose effective slot (offset or buffer
  differs, `MakeEffectiveSlot`) is not the command's slot gets its own copy owned by the set and a `Slot` pointer on the binding (see (b)). Entries whose class
  does not load, or past 255 / 32767, are dropped with a report error; the loadout validator below makes that a refusal before it can happen. Entries whose
  `TimelineOverride` is set keep the registry slot but the override is NOT honoured in this session (non-goal, see section 4).
- Revision 0 = the Blueprint binding set (a pawn with no loadout behaves as today). Once a loadout is accepted, its table replaces revision 0 entirely
  (assumption: the test kits bind jump and dash themselves, `KIT_Test_Alpha/Beta`).

### (b) Source filtering in the input resolver
"Source" stays out of GSTNInputNP's vocabulary (it must not know Core's `EGSTN_EntrySource`). It is an opaque bit mask:
- Core adds `Loadout/GSTN_EntrySourceMask.h` (pure): `Bit(EGSTN_EntrySource, SourceIndex)` with Kit = bit 0, Super slot s = bit 1+s (`MaxSuperSlots` = 8),
  Weapon w = bit 9+w (up to 23 weapons), and `ActiveMask(ActiveWeaponIndex)` = Kit | all supers | weapon bit. Supers are always active; a super with no
  entry simply has no row. Primary weapon = index 0 = the default `ActiveWeaponIndex`, so a switch (primary <-> support) only flips the weapon bit.
- GSTNInputNP: `FGSTN_CompiledBinding` gets `uint32 SourceMask = 0` (0 = always active, so every existing binding set is unchanged) and
  `const FGSTN_InputSlotSpec* Slot = nullptr` (null = the command's). `FGSTN_InputTickArgs` gets `uint32 ActiveSourceMask = ~0u`.
  `SelectBindingFor` skips inactive bindings (same tie rule: declaration order). The candidate list becomes per frame: when the table has any
  non-zero mask, `CandidateSlots` / `CandidateCommands` are rebuilt into stack `TInlineAllocator<48>` arrays from the commands that have at least one
  active binding, slot = the slot of the command's first active binding. A command with no active binding is NOT a candidate, so an inactive weapon's
  or a removed command's input neither fires nor reserves a press (the `command_set` requirement, combat-m3 section 9). Tables without masks use
  the precomputed arrays: zero change for the harness suite.
- The combat tick passes `Args.ActiveSourceMask = EntrySourceMask::ActiveMask(InSync.Core.ActiveWeaponIndex)`. The index comes from the
  sync state of the previous frame, so the weapon switch of frame F filters from F+1, the same frame the stats swap takes effect after the pre-tick.

### (c) The swap step in the combat tick
New pure helper `FGSTN_LoadoutSwapLogic::Step(FGSTN_CombatSyncCore& Out, uint16 DemandRevision, bool bTablePresent, int32 Frame)` returning
`Closed | Unchanged | Swapped` (file `Simulation/GSTN_LoadoutSwapLogic.h/.cpp`, no UObject, so it is unit-testable).
New sync field `uint16 TableRevision` next to `LoadoutRevision` (`IsDefault`, `NetSerialize`, `ShouldReconcile`, `ToString`, +2 bytes):
`LoadoutRevision` is the demand (server's out-of-band write), `TableRevision` is what the simulation has swapped to.

Position: in `SimulationTick`, right after `Requests.PruneBefore(Frame)` and before the hitstop and clock steps (so every later step of the frame,
including clock, adoption, jump bookkeeping, input and step 8, sees one table). Sequence:
1. `Tables = FindTableSet(InSync.Core.LoadoutRevision)`; `FrameTables = Tables` (member pointer valid for this frame only, the pattern of `GateCore` /
   `FrameAim`; the ASC pre-tick of the same frame and the gate read it; presentation callers such as `GetCommandTable()` fall back to the set of the
   latest `TableRevision`).
2. **Gate closed** (no set for the demanded revision, e.g. the owning client has the sync state but not yet the `OnRep` of the loadout; this is
   `IsLoadoutCurrent` with the table lookup behind it): `Step` returns Closed. The tick runs with an empty table: the input sample is still appended
   and aged (buffers keep running), no command resolves, no adoption (step 3 skipped), `TableRevision` untouched, a diagnostic counter increments. This
   realises "no predicted activation or adoption until OnRep" (combat-m3 scenario_loadout_change). Closed frames are only ever a client prediction;
   the next correction overrides them.
3. **Boundary** (`TableRevision != LoadoutRevision` and the set exists): `Step` returns Swapped and, on `Out.Core`, (i) ends a running timeline
   (`ClearTimeline` after writing a `CancelTimeline` request with the old registry index, so the ASC pre-tick of this very frame cancels the old ability on
   every machine; the server's out-of-band cancel in `ServerApplyLoadoutChange` stays only so the server does not keep the ability alive until the next tick,
   and is idempotent); (ii) `ActiveWeaponIndex = 0` and the pre-tick weapon-stats state is not touched (the loadout component's accept path already removed
   the old weapon effect and applied the kit stats; verify in step 6 test); (iii) consumes every buffered press and stick anchor up to `Frame - 1`
   (`History.ConsumeSticksUpTo`, button consume; presses made for the old kit must not fire a new kit's command), by direct history calls, not a data rule;
   (iv) `TableRevision = LoadoutRevision`; (v) a `LoadoutSwapped` outbox event (tag `Event.Loadout.Swapped`, data field) written on every machine, published by the
   authority only. The frame's `Cmd.ForcedCommand` is dropped on the swap frame (a forced index is table specific and carries no revision; see risk 2).
4. **In-flight requests**: requests live exactly one frame and were pruned in the line above, so none refers to the old registry; the only registry-indexed
   state is `ActiveTimeline` and (i) handles it. Jump state (air jumps left, coyote) is kept; per-kit jump numbers switch with `TableJumpData` of the new set on
   the same frame. Held `ReleaseEnds` / stance abilities die with the cancel (they are granted abilities of the old kit).
5. Everything after reads `FrameTables->...` instead of the old members (`CommandTable`, `TimelineRegistry`, `TimelineTier`, `TimelineDefaultOverride`,
   `BindingTimeline`, `BindingJumpRole`, `TableJumpData`, `WeaponStats.Num()`, `Args.Table`, `Args.GlobalResetRules` unchanged). The ASC pre-tick uses the set
   of `Sync->Core.TableRevision` (the frame's output), `FGSTN_InputHostUtils::ExecuteRequest(..., Tables->Table, ...)` and `Tables->WeaponStats`.

Retention: `TArray<TObjectPtr<UGSTN_CombatTableSet>>` on the component, revision 0 pinned plus the newest `UGSTN_CombatSettings::MaxRetainedTableRevisions`
(new, default 4). A rollback never reaches further back than a few frames; `ServerApplyLoadoutChange` additionally refuses a change within
`MinFramesBetweenLoadoutChanges` (new setting, default 30) of the previous one so two sets never expire inside the rollback window. A lookup miss is the
closed gate, never a crash.

**How both machines reach the same frame.** Server: the out-of-band write lands between ticks, so frame F is the first tick whose `InSync` carries the new
demand; it swaps there. Owning client: it predicted old-table frames; the correction delivers a server state at some frame >= F with `TableRevision`,
`LoadoutRevision`, the cleared timeline and the cleared input history already in it, and the client resimulates from there with the lookup returning the
new set. If the loadout `OnRep` has not arrived the lookup misses (closed, never a wrong table). When it arrives, `TableRevision` already equals the demand,
so no swap step is repeated. Simulated proxies that do not run the combat simulation never need the set. The ASC reconcile is for the part the combat
tick must not do: the re-grant of the new kit's abilities and the kit stats effect are out-of-band ASC writes, delivered by the ASC's own sync state
(one ASC reconcile per accepted change, as `loadout_change` already measures). The combat reconcile (one per change) carries `LoadoutRevision` plus the
cleared timeline. Both are written in the same game-thread call and describe the same server frame; if the client gets them one correction apart, the
window shows as `CanExecuteBinding` false for the new abilities (no request), which is benign.

### (d) Checksum for the harness
`FGSTN_CombatTableSet::ComputeChecksum()` = CRC32 over a byte buffer written with `FMemoryWriter`: `Compiled.WriteBytes` (Core, already stable) + NP part:
per command (tag name, slot key), per binding (command name, `SourceMask`, class path, registry index, jump role), registry (class path, tier, default
override frames, `TotalFrames`), `WeaponStats` (effect class path, stat tag + magnitude) + the CRC of `ReservedExtension` when present. No pointers, no
indices that depend on load order. The harness `GetTestState` adds `combat.tableRevision`, `combat.tableCrc`, `combat.tableBindings`,
`combat.tableGateClosed` (frame counter, diagnostic only) next to the existing `loadout.tableCrc`; `loadout_change` requires host == owner for the
combat CRC and `tableRevision == loadoutRevision` on both.

### (e) Seam for M11 mods / backpacks
No new mechanism, reuse what C15 added. (1) `FGSTN_LoadoutData` reserved extension block (layout byte 0, <= 1024 bytes, kept unchanged by older builds) stays the
carrier; its CRC enters the checksum so two machines with different mod payloads cannot look equal. (2) Registered validators (`UGSTN_LoadoutRules::RegisterValidator`):
GSTNCombatNP registers `GSTN.NP.TableLimits` at module startup (binding count <= 255, registry <= 32767, watched tags <= 64) so an over-large table is refused
before it reaches the sim; M11 registers its own ids. (3) A converter extension list, same shape as the validators:
`FGSTN_CombatTableConverter::RegisterExtension(FName Id, TFunction<void(const FGSTN_CompiledBindingTable&, FGSTN_CombatTableSet&)>)`, run in id order after
the base conversion, may append bindings or registry entries and are part of the checksum. Typed mod data (backpack slots) is a reserved block in the set, not a
field (`TArray<uint8> ReservedExtension`). Any change they cause rides the same revision gate because it changes the loadout revision.

## 3. Implementation steps

### Session 1 (the swap, no new scenarios)
1. **Core source mask.** Files: new `GSTNCombat/Source/GSTNCombatCore/Public|Private/Loadout/GSTN_EntrySourceMask.h/.cpp`; test file
   `Private/Tests/GSTN_CombatTests_Table.cpp`. Tests `GSTNCombat.Core.Table.SourceMaskBits`, `...SourceMaskActiveWeapon`, `...SourceMaskLimits`.
   PIE: none (pure).
2. **Input NP filter.** Files: `GSTNInputNP/.../Bindings/GSTN_CommandBindingSet.h/.cpp` (binding `SourceMask`, `Slot`; `BuildCandidates(ActiveMask, Out)`),
   `Simulation/GSTN_InputSimulator.h/.cpp` (`ActiveSourceMask`, filtered `SelectBindingFor`, per-frame candidates), `Private/Tests/GSTN_InputNPTests_Simulator.cpp`.
   Tests `GSTNInput.NP.Table.SourceFilterActiveOnly`, `...InactiveCommandDoesNotReservePress`, `...ZeroMaskAlwaysActive`, `...FirstActiveBindingSlotRanks`,
   `...LegacyTableUnchanged`. PIE: `run_suite()` 22/22 listen + dedicated (no behaviour change; mask 0 everywhere).
3. **Table set + converter + checksum.** Files: new `GSTNCombatNP/.../Simulation/GSTN_CombatTableSet.h/.cpp`, `Private/Tests/GSTN_CombatNPTests_Table.cpp` (new; register in the
   module as the other test files). Tests `GSTNCombat.NP.Table.ConvertOrder` (commands by name, bindings by entry order), `...ConvertSlotPointersStable`,
   `...ConvertRegistryMatchesCompiled`, `...ConvertTierAndJumpRole`, `...ChecksumDeterministic`, `...ChecksumDiffers`, `...DropsUnloadableClass`, `...ExtensionHookOrder`.
   PIE: none.
4. **Sync state field + pure swap logic.** Files: `GSTN_CombatSimTypes.h` (`TableRevision`), `GSTN_CombatTimelineLogic.h/.cpp` or new
   `GSTN_LoadoutSwapLogic.h/.cpp`, `Private/Tests/GSTN_CombatNPTests_State.cpp` (extend), `..._Swap.cpp`. Tests `GSTNCombat.NP.State.TableRevisionRoundTrip`,
   `...TableRevisionReconciles`, `GSTNCombat.NP.Swap.BoundaryOnce`, `...ClosedWithoutTable`, `...CancelsTimeline`, `...ConsumesBuffer`, `...ResimSameResult`
   (run the step twice on copies of one state), `...ForcedCommandDroppedOnSwapFrame`. PIE: none.
5. **Component refactor to table sets, revision 0 only.** Files: `GSTN_CombatSimComponent.h/.cpp` (`TableSets`, `FindTableSet`, `FrameTables`, `CompileInput` builds set 0,
   every reader switched, `GetCommandTable()` kept), `GSTN_CombatSimJump.cpp`, `GSTN_GA_Base.cpp` (reads `GetCommandTable()`), `UGSTN_CombatSettings`
   (`MaxRetainedTableRevisions`, `MinFramesBetweenLoadoutChanges`, defaults in `Plugins/GSTNCombat/Config/DefaultGSTNCombat.ini`). Existing tests unchanged and green
   (headless 201+). PIE: `run_suite()` 22/22 listen (3 pawns) and dedicated (acceptance: no behaviour change without a loadout).
6. **Loadout feeds the sim.** Files: `GSTN_CombatSimComponent.cpp` (`BindLoadoutComponent(UGSTN_LoadoutComponent*)`: subscribes `OnLoadoutApplied`, converts
   `GetCompiledTable()` on every machine, loads classes, fills `WeaponStats` from the loadout's resolved weapons (support weapons ship without a stats effect;
   weapon 0 none), registers the set; auto-binds in BeginPlay / on pawn set when a loadout component exists; `ServerApplyLoadoutChange` keeps validate -> out-of-band cancel -> accept
   -> `ServerWriteLoadoutRevision` and adds the min-frames refusal), `GSTNCombatNP` module (`GSTN.NP.TableLimits` validator), harness
   `GSTN_GASNPPTestComponent.cpp` (`EnsureLoadoutComponent` calls `BindLoadoutComponent`; JSON `combat.tableRevision/tableCrc/tableBindings/tableGateClosed`),
   `Tools/GASNPPTest/gasnpp_test.py`. Tests `GSTNCombat.NP.Table.LimitsValidatorRefuses`, `GSTNCombat.NP.Table.WeaponStatsFromLoadout`.
   PIE: `scenario_loadout_change` extended (checks 16-20: `combat.tableRevision == loadout revision` on both; combat table CRC equal host/owner; after Alpha the dash
   command does not exist (input does nothing, no reserved press), after Beta it dashes; the gate closed counter stays 0 on the host; the existing 15 checks and the
   one-combat + one-ASC reconcile budget unchanged). Run listen (host + 2 clients) and dedicated, fresh PIE each, `loadout_change` last (it leaves the loadout applied).
7. **Docs and handover.** `docs/notes/m3-doc-deltas.md` (C17), `takeover-state.md` HANDOVER 12: tables, gate, limits, the forced-command risk.

### Session 2 (the four scenarios; test content in `setup_gas_assets.py --combat-content`, tags in `Config/Tags/GSTNCombatTest.ini`)
Expected scenario text from `docs/architecture/combat-m3-moves-kits.md:460-495`; all as `_Run` scenarios inside `run_suite`, judged on `run.sim`.
1. `scenario_command_set`: kit `KIT_Test_Gamma` (command set base minus the dash command plus one added command with its own slot and ability). Checks on host and owner: the removed
   command's input does nothing and the attack pressed with it is not swallowed; the added command activates its ability; the same press under Beta (has dash) dashes;
   a forced reconcile (`server_correction`) on the swap frame. Needs: a kit with `CommandSet` changes, one ability (existing `GSTN_TestCombatAbilities`).
2. `scenario_super_code`: kit with a `PerSuperCode` scheme and two `SUP_Test_*` definitions (codes as motion patterns). Enter each code, a wrong code does nothing, a forced
   reconcile mid-code (history is sync state, so the code resumes). Needs the super definitions plus abilities (assets from the content script).
3. `scenario_ex_meter`: kit with a special and an EX variant (own slot, fall-through authored in the slot data so an EX press without meter falls to the plain special, see the
   resolver comment "unless it falls through"); EX ability has a meter cost. Without meter the plain special comes out; the harness gives meter (server GE, existing
   debug GE path); EX comes out and the meter is spent; both machines agree.
4. `scenario_wait_frames`: a kit-bound timeline ability whose timeline waits `WaitCombatFrames` before a strike, a hitstop window (M0 debug button) opened during the wait
   delays the strike by exactly the window length on host and owner, forced reconcile inside the wait. Premise to verify first: it exercises the registry/override path of
   the swapped set (class in `TimelineRegistry` index of the loadout set, not the Blueprint's).

## 4. Risks, assumptions, owner decisions

Assumptions stated and decided (no owner input needed):
- Revision 0 = Blueprint binding set; a loadout's table replaces it wholesale. Revisions are uint16 and wrap after 65535 changes in a match; ignored.
- `TimelineOverride` and moving `AdoptableTimelines` into kit data are out of this change (override needs the ability to read its timeline from the registry entry).
- Per-binding slot differences resolve by "first active binding's slot ranks the command".
- Risk 1 (known, accepted): while the owning client lacks the loadout `OnRep`, it plays "no commands" for at most that latency instead of a wrong table.
- Risk 2 (known, accepted): `FGSTN_InputCmd::ForcedCommand` is a table index without a revision; a command forced exactly across the swap frame is dropped. Only AI / scripts / the harness force
  commands. If this turns out to matter, stamp the table revision into the command in a later wire change.
- Core stays pure (the mask helper is plain values); GSTNInputNP gains no Core dependency. Note the Build.cs of GSTNCombatNP already lists GSTNCombatGAS; the converter and swap logic
  deliberately use only Core and InputNP types, GAS is touched only in the component's `BindLoadoutComponent` glue.

Open owner decisions: none required to start. Optional: confirm that a pickup within 30 frames of the previous one may be refused (`MinFramesBetweenLoadoutChanges`), the alternative being
a larger retained window.

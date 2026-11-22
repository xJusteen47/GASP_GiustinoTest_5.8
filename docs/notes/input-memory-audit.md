# Input memory and frame time audit (D3b, 2026-11-01)

Laptop session, branch `feature/gas-npp-5.8`. Asked by the owner: the game should run on very weak hardware, so the 32-event
stick ring (D3b) must not cost more memory or copying than the 16-event one, and the input simulation's time per frame is
what matters. Part 1 is what D3b changed and measured; part 2 is an audit of the copies (no code changed for it); part 3
ranks proposals. Nothing in part 3 is done.

## 1. What D3b changed, measured

### Size

| | before (16 + 16 events) | after (16 button + 32 stick events) |
|---|---|---|
| button event in the ring | 8 B (int32 frame + 1 B, padded) | 3 B (uint16 frame + 1 B) |
| stick event in the ring | 12 B (int32 frame + 6 B, padded) | 5 B (uint16 frame + uint16 packed stick + aim byte) |
| `sizeof(FGSTN_InputHistory)` | 348 B | 232 B |
| `FGSTN_InputSimState` (history + classifier + request), computed from the layout | ~384 B | ~264 B |
| that state in one entity's 64-frame NP ring | ~24.0 KB | ~16.5 KB |
| wire, per event | 2 B button, 4 B stick | unchanged |

The 348 / 232 come from `GSTNInput.Core.Input.History.StorageSize` (it prints the real size and a mirror of the old
layout). The input state and ring sizes are arithmetic on the struct layouts, not printed. The full-ring wire size is
printed by `...History.SerializationFullStickRing`.

### Time (Unreal Insights)

Run: the M6 standalone harness (`Tools/GASNPPTest/link_standalone.ps1` logic, a scratch copy that adds
`-trace=cpu,frame -tracefile=...` to the host and client 1), listen host + 2 client game processes, `-nullrhi`, net
(60 ms, 15 ms, 2 % loss), the four link spike scenarios (warm-up + 3) with a 20 s idle window first. Same machine (laptop,
16 GB, Rider open), same run, once on a build with only the trace scopes (before) and once with D3b (after). Timer
statistics exported headless with `UnrealInsights -ExecOnAnalysisCompleteCmd="TimingInsights.ExportTimerStatistics ..."`,
game thread only. "Per frame" divides by the whole trace's `FEngineLoop::Tick` count (loading and idle included), so
read per-call times and calls per frame first. Traces are under `Saved/D3b/traces` on the laptop (not versioned).

Listen host (3 pawns simulated, serves 2 clients):

| scope | calls/frame | us/call before | us/call after |
|---|---|---|---|
| `NetworkPrediction::FixedTick` (all models) | 1.00 | 573.9 (excl 346.7/frame) | 563.1 (excl 340.9/frame) |
| `GSTN_CombatSimComponent_SimulationTick` (whole combat tick) | 1.21-1.23 | 59.8 | 57.6 |
| `GSTN_InputSimulator_Tick` (the input part of it) | 1.21-1.23 | 9.37 | 9.88 |
| `GSTN_SlotResolver_Resolve` | 1.21-1.23 | 5.73 | 6.23 |
| `GSTN_InputMatcher_Match` | ~19.5 | 0.26 | 0.30 |
| `GSTN_InputHistory_NetSerialize` | 1.32-1.41 | 0.40 | 0.61 |
| `GSTN_InputHistory_Append` | 1.21-1.23 | 0.13 | 0.14 |

Client 1 (owns one pawn, interpolates the others):

| scope | calls/frame | us/call before | us/call after |
|---|---|---|---|
| `NetworkPrediction::FixedTick` | 1.00 | 412.3 (excl 177.9/frame) | 391.6 (excl 168.7/frame) |
| `NetworkPrediction::FinalizeFrame` | 1.97 | 223.3 | 226.0 |
| `NetworkPrediction::Interpolation` | 1.00 | 330.1 | 332.1 |
| `GSTN_CombatSimComponent_SimulationTick` | 0.50 | 110.0 | 109.3 |
| `GSTN_InputSimulator_Tick` | 0.50 | 14.2 | 15.7 |
| `GSTN_InputMatcher_Match` | ~8 | 0.40 | 0.49 |
| `GSTN_InputHistory_Equals` (reconcile check) | 0.31-0.33 | 0.43 | 0.33 |
| `NetworkPrediction::Rollback` (whole resimulation) | 30 / 27 in the run | 6493 avg, 17.1 ms max | 8946 avg, 31.3 ms max |
| `NetworkPrediction::RestoreFrame` | 81 / 75 in the run | 104 | 125 |

Reading:
- The input simulation is small: about 10-16 us per simulated combat frame, 1-2 % of a 62.5 Hz fixed tick's budget, and
  about a sixth of the combat tick. NP's own work (FixedTick exclusive, FinalizeFrame, Interpolation, Mover, the
  animation Blueprints) is where the frame goes.
- After D3b the input scopes cost about 5-15 % more per call (+0.5 us per combat tick on the host, +1.5 us on the
  client): the matcher and the conditions now unpack events on every read and the stick-sequence conditions copy up to
  32 events instead of 16. NetSerialize per call went from 0.40 to 0.61 us on the host (unpacking on save). These are
  sub-microsecond scopes: the trace scope itself costs a few tenths of a microsecond, so part of this is measurement.
- Against that, the parts that copy the whole state got slightly faster: the combat tick -2 us per call on the host,
  NP FixedTick exclusive -6 us per frame on the host and -9 us per frame on client 1, and the reconcile compare -0.1 us
  per call (it compares the packed words). Both runs are single runs on a laptop with Rider open, so differences under
  ~5 % are within noise.
- Rollbacks: too few (27-30 per run) and too dependent on how many frames each one resimulates to compare; the averages
  are dominated by Mover and the animation graph, not by input.
- Net: frame time unchanged within noise, memory and copied bytes down by a third for the input state, wire unchanged.

## 2. Copies of the input history and of the input/combat sync state per simulated frame

Per entity running the combat model (the input state is embedded in `FGSTN_CombatSyncCore`):

1. **The NP frame ring** (`TInstanceFrameState::Buffer`, `NpInstanceFrameBufferSize` = 64): every frame slot holds a
   full `FGSTN_CombatSyncState` (and aux and input command). This is the storage the history size multiplies.
2. **`OutSync = InSync`** at the top of `UGSTN_CombatSimComponent::SimulationTick`: one full copy per simulated frame,
   resimulated frames included. Same in `UGSTN_InputSimComponent::SimulationTick` for the standalone model.
3. **Smoothing** (`NetworkPredictionService_Smooth.inl`, fixed smoothing on, about half the frames here): up to two
   `CopyTo`s of the output sync state into the smoothing instance plus delta/smoothed temporaries built by the driver's
   default smoothing hooks.
4. **Interpolation** of simulated proxies: `FGSTN_CombatSyncState::Interpolate` copies `To->Core` and the fragments into
   the presentation state.
5. **Network**: the server serializes the history once per send per connection (`GSTN_InputHistory_NetSerialize`, 1.3-1.4
   calls per host frame here); the client deserializes into its receive buffer and runs `ShouldReconcile`
   (`GSTN_InputHistory_Equals`, 0.3 per client frame).
6. **Rollback**: `RestoreFrame` plus one copy per resimulated frame (item 2 again).

Findings:
- **No duplicated history.** With the combat model the input state lives only inside the combat sync state;
  `UGSTN_InputSimComponent::SimulationTick` never appears in either trace, so no character runs the standalone input
  model next to the combat one.
- **Per-frame heap allocations in the combat sync state copies (biggest finding).** `FGSTN_CombatSyncState::Fragments`
  is a `TArray<TInstancedStruct<FGSTN_SyncFragment>>`: every copy of the sync state (items 2, 3, 4, 6) allocates the
  array and one block per fragment once any fragment exists (parts, defence, link, spin-drill fragments are added once
  and never removed). That is several allocations per entity per simulated frame, more during a rollback.
- **The default `FGSTN_SyncFragment::ShouldReconcile`** serializes both fragments into two `TArray<uint8>` (heap) on every
  compare; it runs per fragment per received frame on clients.
- **The input simulation itself does not allocate**: the simulator, resolver, matcher and conditions use inline
  allocators (48 slots, 16 candidates, 8 used events, `MaxDirectionEvents` stick events). The stick-sequence conditions
  (`Spin`, `Charge`, `Motion`) copy the whole stick ring into a stack array on every call, and the matcher calls the
  anchor condition once per candidate, so a Spin slot rebuilds its node list up to 32 times per match.
- **Debug strings**: `FGSTN_InputScript::Describe` and the `GetDebugString` functions build `FString`s but only run on
  demand (Blueprint debug calls, logs); the NP user-state trace calls `ToString` only while the NetworkPrediction trace
  channel is on. Nothing builds strings per frame in a normal run.
- **Trace scopes added by D3b** (`GSTN_InputHistory_Append/NetSerialize/Equals`, `GSTN_InputMatcher_Match`,
  `GSTN_SlotResolver_Resolve`, `GSTN_InputSimulator_Tick`, `GSTN_InputSimComponent_SimulationTick`,
  `GSTN_CombatSimComponent_SimulationTick`): `TRACE_CPUPROFILER_EVENT_SCOPE`, compiled out with the trace system
  (Shipping by default); in other builds they cost only while the cpu channel is traced. The matcher scope runs ~20 times
  per host frame; if it ever shows as noise it can be removed and left to the resolver's.

## 3. Proposals, ranked by expected time per frame saved

Estimates are from the traces above and the code; none is measured.

1. **Fixed-capacity fragment storage in the combat sync state** (GSTNCombat core, needs its owner): replace the
   `TArray<TInstancedStruct>` by inline storage (a small fixed array of type id + inline byte buffer, sized by the
   registered fragments, or `TArray<..., TInlineAllocator<N>>` with an inline-allocating instanced struct). Removes all
   per-copy heap allocations; with 2-4 fragments per entity that is roughly 5-15 allocations per entity per simulated
   frame, likely a few us per entity per frame and much less allocator contention on weak CPUs. Biggest win, also the
   biggest change (fragment API, all M4b/M6/spin-drill fragments).
2. **Override `ShouldReconcile` in every fragment, or compare into an inline buffer** (`TArray<uint8,
   TInlineAllocator<64>>` writers): removes 2 heap allocations per fragment per received frame on clients. Small change in
   GSTNCombatCore; per-fragment overrides are cheaper still.
3. **Smoothing copies** (NP fork): the default smoothing hooks build full delta and smoothed temporaries of the sync
   state; providing combat-model hooks that touch only the presentation fields (or turning fixed smoothing off for the
   combat model, which has no continuous state to smooth) saves 2-4 full state copies on half the frames. FixedSmoothing
   is 27 us/frame on the host, 13 us on the client in these traces (all models).
4. **Stick-sequence conditions without the copy**: iterate the history in place (the accessors are cheap now) instead of
   collecting the ring into `FStickEvents`, and let `Spin` build its node list once per match instead of once per
   candidate (cache it in the condition context for the step). Saves most of the matcher's ~5 us/frame on the host when
   Spin/Charge/Motion slots are bound; this is GSTNInput only and the natural next input checkpoint.
5. **Delta-copy the sync state in `SimulationTick`**: the history only changes on a frame with an event, but
   `OutSync = InSync` copies it every frame. With item 1 done the remaining copy is a ~0.5 KB memcpy per entity per frame:
   a few hundred nanoseconds. Not worth a change today; listed so it is not redone.
6. **Shrink the NP ring for the combat model** (fork setting): 64 frames (~1 s) bounds the deepest rollback; if the game's
   rollback budget is lower (e.g. 32 frames, 0.5 s) the per-entity sync storage halves. A design decision (max latency),
   not a free win.

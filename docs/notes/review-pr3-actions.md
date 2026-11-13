# Review of PR #3 at 37975fd: actions (coordinator, 2026-10-22 18:15)

Read-only review (not on this PC) plus draft PR #7 with fixes. Fold into M3. One subagent per item group; each fix gets a unit test where possible and its own commit. Report when A is verified and at the end of each group.

## A. Pull in PR #7 at the next build
`git fetch origin claude/project-thread-5aothp` and MERGE it (not cherry-pick, so GitHub marks PR #7 merged on push). Contains: NetworkPredictionService_Rollback.inl QueryRollback skips server states that map past the local pending frame (likely cause of the 6-15 startup mismatches), DesignerValues keys sorted with FName::LexicalLess instead of FastLess (GSTN_LoadoutData.cpp), PATCHES.md catch-up. Resolve PATCHES.md conflicts with the trigger patch (restore-phase guard in MonitoredTagChanged). Then build and run abilities_startup(runs=3): the startup burst should be gone; abilities, forced_rollback, combat_spike still CMP OK.

## B. High findings
1. UGSTN_GE_MeterCost (GSTNCombatGAS Effects): the set-by-caller magnitude is never set on the standard CheckCost/ApplyCost path, so cost = 0, "not enough meter" never blocks, every CanActivate logs an Error. Fix: make the cost effect data (FScalableFloat / Cost property used by RebuildModifiers), fix header comment, add a test through the real CheckCost/ApplyCost path.
2. FGSTN_InputHostUtils::CanExecuteBinding (GSTNInputNP, from the simulator's FindExecutableBinding) calls CanActivateAbility, which checks ShouldActivateAbility(GetLocalRole()): ServerOnly abilities make press consumption diverge client/server. Fix now: reject/log bindings to ServerOnly/ServerOnlyExecution abilities in BuildTable and IsDataValid; document that CanActivateAbility overrides used by bindings must not read role or local control; fix the false comment in GSTN_InputSimulator.h / IGSTN_InputSimHost. Later item: a role-free fork check.
3. Stick-only terminal commands (Motion/Spin, e.g. double-tap dash) consume nothing and re-fire for up to BufferFrames+1 frames (GSTN_InputMatcher IsTerminalAccepted, Motion/Spin Matches, SlotResolver consumption). Fix in the core: track the last consumed stick anchor frame in FGSTN_InputHistory (serialized, in operator==), matcher ignores stick anchors at or before it, simulator updates it with Out.History.Consume. Test: the motion command fires exactly once.
4/5. Wall movement (Source/.../WallMovement): MemoryResetTimeMs and FGSTN_WallBaseInfo live in the Mover SimBlackboard (not rewound on rollback); Evaluate (const) writes the blackboard and runs twice per tick; ResetWallMemory uses GetLastTimeStep and is not predicted. Fix: MemoryResetTimeMs into FGSTN_WallMovementSyncState (+ShouldReconcile), BaseInfo into a rollback blackboard entry InvalidatedOnRollback (or rebuilt from sync state), Evaluate pure, reset as a sim input or queued instant movement effect. ShouldReconcile must also cover PullAwayTime, TimeWithoutWall, bIsSkidding, bHasWallMemory, LastWallHeight/TimeMs (with tolerances). Re-run the wall scenarios afterwards.

## C. Policy: the input sim state IS authoritative
FGSTN_InputSimState::ShouldReconcile returns true on mismatch via UE_NP_TRACE_RECONCILE but both hosts (FGSTN_CombatSyncCore, standalone component) discard the result. Return it (`if (Input.ShouldReconcile(Auth.Input)) return true;`), fix comments, README, input.md 5.1, combat.md 5.5. Measure reconciles under 60/15 ms and 2% loss before and after; if unexplained reconciles rise noticeably, report the numbers before committing.

## D. Medium findings
- Sync fragments that cannot be sent (unregistered, duplicate, >16, >512 bytes) are dropped on write but kept locally, so ShouldReconcile is true every frame: validate at FindOrAdd/registration and compare only sendable fragments.
- Tag roots Weapon/Kit/Super/Loadout/Skin (and Combat.State.*) declared in GSTNCombatGAS but used by GSTNCombatCore; modularity-and-tags.md 4.2 assigns them to the Core: move them. Test tags and types compiled only with WITH_DEV_AUTOMATION_TESTS (or a test module).
- Runtime string-built tags in UGSTN_CommandLocks and the loadout (de)serializer: replace with native tags or cached lookups (no string tags rule).
- GSTNInputNP depends hard on Mover and the GAS fork; check against modularity-and-tags.md 3.7: document as allowed bridge or split the Mover tracker into its own bridge. ConstructorHelpers with /Game paths in the game module and UGSTN_GA_Jump hard-coding /Game/Input/IA_Jump: soft refs/data. Test harness (GASNPPTest) compiled in the runtime game module: move to a dev-only module or guard it.
- FGSTN_LoadoutData::NetSerialize mutates the source object and does not clamp counts on write.

## Other decisions from the coordinator
- Trigger spike: option (a), fork patch guarding only the restore phase of MonitoredTagChanged; verify GetIsRestoringFrame() true in RestoreFrame, false in resim ticks; document in PATCHES.md; live probe (forced rollback across a tag-triggered activation: same active abilities on server, owning client, other client; no double activation; none during restore); GameplayEvent triggers unchanged. Folded into the live spike batch (L1-L9).

## Deferred
- D-Harness: move the GASNPPTest harness to a separate module of Type DeveloperTool/UncookedOnly with CoreRedirects (Lyra pattern), do at a stable point with a Shipping build check. Not done by wrapping in defines: UHT parses the reflected headers whatever a Build.cs define says, so only a module split excludes the generated code.
- Done instead (8607390): no /Game paths in C++ for ability input actions and mapping contexts (Config properties, DefaultGame.ini).

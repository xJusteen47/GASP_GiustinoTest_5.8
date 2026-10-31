# M3 implementation plan (draft for user review)

Plan only. Sources: `docs/architecture/combat-m3-moves-kits.md` (spec, "S" below), `combat.md`, `docs/notes/modularity-and-tags.md`
(MT; §8.1 placement, §8.2 tag roots, §8.5 extension prerequisites are binding), CLAUDE.md.
`docs/architecture/game-design-direction.md` (DD; commit d02312b on `origin/claude/project-thread-cb006o`, read in full) wins
over the spec where they disagree (DD header). Its M3-relevant points are applied in §1, §3 and §3b "Design-direction check".

## 0. Ground rules for every checkpoint

- One checkpoint = one commit, short imperative subject + reason in body. Gate before each commit: full build (Rider, not
  Live Coding, for header/reflection changes); `Automation RunTests GSTNCombat+GSTNInput` green; the listed PIE scenarios
  clean (60/15 ms, 2 % loss) once NP code is touched; M1/M2 tests and scenarios still green.
- Never edit `docs/` in these commits unless the user lifts that rule; each checkpoint appends its doc delta to
  `docs/notes/m3-doc-deltas.md` (combat.md §17 results, §10/§11 corrections) for one docs commit at the end.
- Dependency order is strict: `GSTNCombatCore` (no GAS/NP/Mover) < `GSTNCombatGAS` (no NP/Mover) < `GSTNCombatNP`.
  Each new dependency is mirrored in the owning `.uplugin`; project module gets only PRIVATE deps (MT §8.3.7).
- Native tags: `UE_DEFINE_GAMEPLAY_TAG_COMMENT`, one doc line each (who adds/removes, projected or granted), declared in
  the module that owns them (MT §8.2). No tag in any sync/aux state (compiled indices only). No new `State.*` /
  `Combat.Context.*` tags; new code uses `Input.Button/Context/Lock.Command/BufferEvent.*` names.
- Test prefixes: `GSTNCombat.Core.*`, `GSTNCombat.GAS.*`, `GSTNCombat.NP.*`. Harness scenarios named as in S §11.
- Live status: working tree has uncommitted M2 work (GSTNInputNP Private/Public dirs, combat sim component). M3 starts only
  after M2 is committed; C0 is allowed to proceed in parallel since it writes nothing in the plugins.

## 1. Network target: LISTEN server (host also plays)

Spec parts that assume dedicated-only, and what to do:

0. DD §6b: listen server is the REFERENCE topology, dedicated stays supported as secondary. Wherever the plan says "both
   topologies", listen (host + 2 clients) is the primary run and dedicated the secondary check.
1. Harness "dedicated server + 2 clients" (S §11/§12): the working-tree harness ALREADY has listen as primary (see C0b);
   every NP scenario runs in BOTH topologies from C9 on (listen first). The host pawn has no prediction: scenarios
   must also assert "host = authoritative, zero reconciles".
2. `OnRep` is not called on a listen host (S §7.8 `FGSTN_AppliedLoadout`, §10 skins, applied loadout): the server applies
   locally through the same function the OnRep calls (`ApplyAppliedLoadout()`), table rebuilt on the host immediately.
3. Authoritative outbox events (combat.md §5.6, `TimelineStarted/Finished/WeaponSwitched/SuperUsed`) are server-only by
   design: the host's local UI/audio must listen to the same outbox exactly once (no double fire from "server" and "local").
   Cosmetic cues: host sees them from `ExecuteGameplayCue` once; test it.
4. Skins/presentation applied "on spawn and OnRep": host needs the spawn path as well (see 2).
5. Revision gate OOB write (S §7.8): on the host the writer and the consumer are the same machine; the gate must pass
   immediately (test: revision equal the frame it is written, no one-frame hole).
6. Spike 1/2/3 wording "server and client positions equal": add "listen host vs remote client" runs.
7. Host-specific budgets (DD §6b): the host machine also renders, so server-side cost shares the frame with the host's game, and
   the host's home upload is the bandwidth cap. M3 consequences: report per-pawn sync-state bytes/s for N clients in C0a/C0c/C3a
   (zero-extension case stays one flag bit); timeline player and tick extensions stay O(tracks), no per-frame allocation (C2);
   no hit resolution or lag compensation in M3 (M4). Host advantage (zero latency vs dodge/startup frames) is a design note for
   M4/M6, not an M3 fix. Host leaving ends the session: no loadout/session migration code.
8. Sim code must keep having no `IsServer()` branches (CLAUDE.md rule 1): the listen host changes nothing there, only the
   non-simulated glue (loadout component, skins, event listeners) needs the host path.

## 2. Checkpoints (in order)

Legend: [files/module] - [tags] - [tests] - [acceptance].

### C0 Spikes (no plugin code; scratch branch or `Saved/` only) - 3 sub-steps
- C0a: the 3 spikes of S §2 (runtime grants, stats modifier on `UCommonLegacyMovementSettings`, GAS triggers in the fork) in
  dedicated AND listen topologies. Output `docs/notes/m3-spike-report.md` (to become combat.md §17). Acceptance: each spike
  has pass/fail + numbers (sync-state bytes for ~40 specs); if triggers unsupported or settings object shared, STOP and ask
  (fork patch vs trigger table; per-mode `MaxSpeedOverride`). Gates C9 (triggers), C10 (stats), C7/C12 (grants).
- C0b: harness listen mode. STATUS: already in the uncommitted working tree (`gasnpp_test.py`, `GSTN_GASNPPTestComponent.cpp`,
  testing.md: listen primary, dedicated secondary, `host-local vs simulated` comparison). So C0b = commit that work (after M2),
  verify M0-M2 scenarios pass in listen AND dedicated, then add the gaps in §3b.C (host zero-reconcile assert, bytes/s report,
  outbox single-fire, OnRep-less apply). Acceptance: M0-M2 scenarios clean in both topologies.
- C0c: spike for the extension surface wire format: fragment array serialization size with 0/1/2 fragments (feeds C3).

### C1 `GSTNCombatGAS` skeleton
- Files: `GSTNCombat.uplugin` (+module), `GSTNCombatGAS/{Build.cs, GSTNCombatGASModule, Public/GSTN_CombatTags.h, Private/GSTN_CombatTags.cpp}`,
  README module table row. Deps: Core, CoreUObject, Engine, GameplayTags, GameplayAbilities, GameplayTasks, GSTNCombatCore.
  No NP, no Mover (checked by a small Automation test that loads the module's `.Build.cs` text? no: review by grep in the gate).
- Tags (`GSTNCombatGAS`, MT §8.2): `Combat.State.{Busy,InTimeline,SuperInput,CancelWindow,OverrideWindow,Falling}`,
  `Combat.Ability.{Timeline,AwaitingAdoption}`, `Combat.Lock.Movement`, `Combat.Weapon.Role.{Primary,Support}`,
  `Combat.Event.{TimelineStarted,TimelineFinished,WeaponSwitched,SuperUsed}`, `Combat.SetByCaller.{MeterCost,Stat.*}`,
  roots `Weapon`, `Kit`, `Super`, `Loadout`, `Skin`, `Combat.Stat`, `Combat.Meter` (roots only; leaves in ini).
  `Combat.Mover.*` is NOT here (NP). `Command.Movement.*`, `Command.Weapon.Switch.*`, `Command.Super.Slot<N>`, `Input.Lock.Command.*`,
  `Input.BufferEvent.{AbilityActivated,Landed,WeaponSwitched}` belong to the input vocabulary: declared in GSTNInputCore tags
  (command family leaves of Combat as native in `GSTNCombatCore` next to the existing `Command.Attack…`). [S §4 puts all in
  `GSTN_CombatTags.h`: conflict, see §4 Q1.]
- Tests: `GSTNCombat.GAS.Tags.AllRegisteredAndDocumented`. Acceptance: builds with `GSTNCombatNP` and project untouched;
  editor loads; tags visible in the picker.

### C2 Pure timeline: data + `FGSTN_TimelinePlayer` (`GSTNCombatCore`) [MT §8.5.5]
- Files: `Core/Public/Timeline/{GSTN_TimelineTypes.h (FGSTN_CombatTimelineData, FGSTN_TimelineTrack + CancelWindow/Movement/Tags/Event/WeaponSwitch/Hitbox/HurtboxModifier declared), GSTN_TimelinePlayer.h, GSTN_TimelineValidation.h}`,
  `UGSTN_CombatTimeline` asset. Core may reference `UAnimMontage`/`UCurveVector` (Engine); no GAS type, no `UGameplayAbility`.
- Player API: input = timeline + frame (+ paused-frame offset); output = active windows (cancel/tags/override), frame keys
  (events, swap, movement start/end). O(tracks), no allocation per frame (preallocated output struct).
- Tests: `GSTNCombat.Core.Timeline.{WindowsAtFrame,EventsOnce,NoAllocation,Validation(FAF<=Total, deferred-commit rules, montage length warning)}`.
- Acceptance: tests green; zero include of GAS/NP/Mover headers in Core (grep gate).

### C3 Extension-surface foundations [MT §8.5.1, .2, .3, .4, .6]
- C3a wire format (`GSTNCombatCore` types, `GSTNCombatNP` state): `FGSTN_SyncFragment` base + registry of fixed small type ids
  (central file), `FGSTN_CombatSyncCore` + `TArray<fragment>` with deterministic `NetSerialize/ShouldReconcile/Interpolate/ToString`;
  zero fragments = one flag bit. Wire the existing combat sync state to it WITHOUT changing behaviour. Tests:
  `GSTNCombat.NP.Fragments.RoundTrip{0,1,2}`, `...UnknownTypeId`, byte count printed. Acceptance: M2 scenarios identical.
- C3b rules + hooks: `UGSTN_CombatRules` (UObject/data asset, `GSTNCombatCore` or GAS if it needs GAS types) with
  `Pipeline` = array of step CLASSES (`UGSTN_CombatStep`), ONE built-in step now; `IGSTN_CombatTickExtension` with the fixed
  hook points of S §7.2 called through empty loops; `FGSTN_KitExtension` base + `ExtensionData` (`TArray<FInstancedStruct>`) declared
  on the future kit/weapon assets (placeholder base in Core; the assets land in C5); `FGSTN_HitVictimView` plain struct in Core
  (fields from combat-m4, no GAS type). Tests: `GSTNCombat.Core.Rules.PipelineOrder`, `...UnknownStepSkipped`,
  `GSTNCombat.NP.TickExtension.OrderByPriority` (ties by type id), `...EmptyHooksNoEffect`. Acceptance: all prior scenarios clean.

### C4 Attributes, meters, GEs (`GSTNCombatGAS`)
- Files: `Attributes/{GSTN_MovementAttributeSet,GSTN_MeterAttributeSet}.h` (clamps in `PreAttributeChange`), `Effects/{GSTN_GE_MeterCost,GSTN_GE_WeaponStats}.h`,
  `FGSTN_KitStats`, `FGSTN_MeterSetup`. Name: S says `UGSTN_MovementAttributeSet`, MT §8.1 says `UGSTN_CombatAttributeSet` (Q2).
- Tests: `GSTNCombat.GAS.Attributes.{Clamp,KitStatsInitEffect,MeterCostSetByCaller,WeaponStatsApplyRemoveByClass}` with a transient stock ASC.
- Acceptance: standalone GAS tests green on stock-ASC (no fork needed in the test).

### C5 Kit / weapon / super / skin / loadout DATA
- Files: `Kits/{GSTN_PrimaryKitDefinition,GSTN_WeaponDefinition,GSTN_SuperDefinition,GSTN_SuperInputScheme,GSTN_SkinDefinition}.h`
  and `FGSTN_AbilityBinding`, `FGSTN_LoadoutData` (+ NetSerialize/Serialize) in `GSTNCombatGAS` (they hold `TSubclassOf<UGameplayAbility>`;
  pure pieces - `FGSTN_TagRequirements` use, scheme structs, `FGSTN_LoadoutData` ids - in Core per MT §8.1). Each kit/weapon has `ExtensionData`.
  Kit `CommandSet` = `{BaseSet, AddedSlots, RemovedCommands}`. Binding inheritance (parent then child by command+requirements).
  `UGSTN_InputSlotSet` and `FGSTN_InputSlotEntry` are used from GSTNInputCore (not re-declared).
- Primary asset types + `IsDataValid` (binding to removed command = error; one stats GE per weapon). Tags: leaves via ini only.
- Tests: `GSTNCombat.GAS.Data.{BindingInheritance,CommandSetComposition,LoadoutNetSerialize,LoadoutSaveSerialize,UnknownExtensionIgnored}`.

### C6 Compiled binding table + registry (pure) + `UGSTN_LoadoutRules`
- Files: `Loadout/{GSTN_CompiledBindingTable,GSTN_LoadoutRules}.h/.cpp`. `Compile()` stays pure over interfaces/structs (no ASC).
- Tests (S §11 list): `GSTNCombat.GAS.Table.{CompileOrder,CommandSetBaseMinusRemovedPlusAdded,RegistryDedupeAndSort,Customisation,SuperCodeSlots}`,
  `GSTNCombat.GAS.Rules.<each rule +/->`, ambiguous conflicts via the M1 validator. Acceptance: deterministic bytes on two compiles.

### C7 Loadout component, subsystem, skins (`GSTNCombatGAS`) - needs C0a(spike 1)
- `UGSTN_LoadoutComponent` (PlayerState; `ServerRequestLoadout`, `Applied` replicated, `ApplyAppliedLoadout()` called by OnRep AND by the server
  directly, listen-safe), grants/GE application OUTSIDE simulation, revision counter (value only; OOB write is NP, C8),
  `UGSTN_LoadoutSubsystem` + `UGSTN_LoadoutSaveGame` (unknown ids dropped with warning), `UGSTN_SkinComponent`.
- Tests: `GSTNCombat.GAS.Loadout.{SaveRoundTrip,UnknownIdDropped,RulesRejectInvalid}`, `...ApplyOnHostWithoutOnRep` (PIE-less simulated).
- Acceptance: grants visible on a transient ASC; no NP dependency.

### C8 Combat sync state additions + requests + revision gate (`GSTNCombatNP`, with GSTNInputNP types)
- ActionState, ActiveTimeline, ActiveSource, Timeline/Paused frames, PausedFramesTotal, ActiveWeaponIndex, LoadoutRevision, fixed `Requests[4]`;
  all serialized only when `Frame == state frame`. OOB revision write. Camera aim fields: `AimYaw/AimPitch` in `FGSTN_InputCmd` live in
  `GSTNInputNP` (input plugin, ask: input.md owns the command); `GetAimRotation()` reads from the sim host.
- Tests: `GSTNCombat.NP.State.{RoundTrip,RequestsStaleDropped,Interpolate,ShouldReconcile}`, `...RevisionGate`.
  Scenario: `scenario_loadout_change` (gate half). Acceptance: no behaviour change when no loadout is applied.

### C9 `UGSTN_GA_Base` + activation context + triggers (`GSTNCombatNP`) - needs spike 3
- `GA_Base` (EndOnTriggerRelease, `GetCombatClock`, `WaitCombatFrames` task, `InterruptPolicy`); `FGSTN_ActivationContext`/`EGSTN_ActivationSource`
  belong to the input side (MT §8.1: activation context in `GSTNInputNP`), not CombatGAS as S §3 lists (Q1).
- Scenarios: `scenario_stance_lock`, wait-frames half of `scenario_triggered_timeline` (fuse delay, hitstop M0 debug), `scenario_camera_aim`.
- Tests: `GSTNCombat.NP.GABase.{WaitFramesRollback,InterruptPolicy,ReleaseEnds}`.

### C10 Timeline abilities: `UGSTN_GA_BaseCombat`, tick order 7.2, ASC pre-tick requests
- BaseCombat wraps the C2 player; steps 2, 5-10 of S §7.2 with C3b hooks; tag projection of `Combat.State.*`; cancel windows
  (OnHit/OnBlock false until M4); startup override window with deferred commit; `DefaultOverrideFramesByTier` in `UGSTN_InputSettings` (input plugin).
- Scenarios: `scenario_timeline_cancel`, `scenario_startup_override`. Tests: `GSTNCombat.NP.Timeline.{CommitDeferred,ReplaceKeepsFacing,OverrideInto}`.
- Acceptance: replaced timeline pays no cost and publishes no event; clean at 2 % loss; both topologies.

### C11 Adoption (S §7.4) - `scenario_triggered_timeline` (full) - exactly one `TimelineStarted(Adopted)`, host and client.

### C12 Stats modifier, weapon stats and switch - needs spike 2
- OWNER RULE 1: the modifier's gravity phases (variable-jump, fast fall) scale/replace gravity along the pawn's local gravity
  direction (from the Mover sim state), never world Z. Test `GSTNCombat.Core.Gravity.PhasesFollowLocalGravity` (see ORG).
- `FGSTN_StatsMovementModifier` (rewrites shared settings or per-mode override per spike), movement track layered move
  (`FGSTN_LayeredMove_TimelineMovement`, own frame counter, M4 freeze-ready), weapon-switch track, stats GE swap by class+source tag.
- Scenarios: `scenario_weapon_switch`, `scenario_ex_meter`, `scenario_super_code`, `scenario_command_set`, `scenario_loadout_change` (full).
  Tests: `GSTNCombat.NP.Stats.{MaxSpeedFollowsAttribute,LockMovementBrakes,SwapDeterministic}`.
- Acceptance: speeds with the support weapon match the configured numbers on host, server-authority and remote client.

### C13 Jump (variable height, air jump, fast fall) - `UGSTN_GA_PlatformJump`, jump phase sync state, gravity phases in the modifier
- PLUGGABLE CONTENT (note 2026-10-22): the jump is a gameplay ability supplied as default kit / ability-set DATA, not framework code. Framework
  code provides only the mechanism (gravity phases, sync state, reset rules); tuning and which jump ability a kit uses are data, so changing
  the jump later is a data edit, never a framework change.
- OWNER RULES 1+2 (see "Owner rules 2026-10-22"): "up", apex height, hold-time phases and fast fall are computed with Up = -GravityDir from
  the sim state (prerequisite: tracker exposes it, ORG-1). Jump/air-jump/coyote are NOT assumed to be the only upward launch source:
  `FGSTN_JumpResetRules` DATA (per kit/jump definition) defines how air-jump count and coyote time react to an external launch.
- Extra tests: `GSTNCombat.Core.Jump.TiltedGravityApex` (pure), `GSTNCombat.Core.Jump.ExternalLaunchResets`.
  Scenarios: `scenario_tilted_gravity_jump` (PIE: gravity tilted, e.g. 45 deg / sideways; jump, fast fall, air jump go along local up),
  `scenario_external_launch_jump` (a test launch layered move through the GE path mid-air/at coyote).
- Project `UGSTN_JumpImpulseMoveCreator` reads `JumpVelocity` (project module change, no plugin to project dependency).
- Scenarios: `scenario_variable_jump`, `scenario_air_jump_fastfall`. Acceptance: apex heights strictly increase with hold time,
  host/remote within 1 cm.

### C14 Movement abilities and defensive-movement shells (kit-dependent) - see section 3
- PLUGGABLE CONTENT (note 2026-10-22): dashes, dodges (ground and air) and similar moves are pluggable gameplay abilities supplied as default
  kit / ability-set DATA, kept out of framework code. The plugin versions are default content, not framework; a kit can replace them and
  changing their timing, invincibility window or blocked tags later is a data edit only.
- OWNER RULES 1+3: dash direction, "horizontal" plane and air dodge/dive roll use the local gravity frame (Up = -GravityDir), never world Z.
  Ground and air dodge abilities carry blocked tags `Combat.State.Hitstun` and `Combat.State.Hitstop` (DATA on the ability/kit; a kit can drop
  them for a special escape; no hard-coded check). Scenario `scenario_dodge_blocked_in_hitstun` (blocked by default; kit override allows it)
  and `scenario_dash_tilted_gravity`.
- Dash (`scenario_dash`), ground dodge, air dodge as plugin timeline abilities with default timelines; kit data decides (§3).
- Scenarios: `scenario_ground_dodge` (long-jump/dive arc, lands standing, no lying state; the feet `HurtboxModifier` window defaults to the INITIAL part of the airborne frames, from take-off to 40%, and is edited as data only), `scenario_air_dodge` (dive roll,
  `bCancelOnLand`), `scenario_kit_without_dodge` (extends `scenario_command_set`), `scenario_interact_tap_hold` (default set: tap = grab, hold = pick up; swapped data set flips them with no code change; tap not swallowed while hold pending). Acceptance: arc and
  landing position match host vs remote client within 1 cm; no `Combat.Mover.*` tag and no new Mover mode added.

### C15 Test content and closing
- `/Game/GASNPPTest/Combat/*` (KIT_Test, WPN_*, GA_Test_*; tell the user before touching `BP_GASNPP_TestCharacter`),
  Blueprint-only authoring check (acceptance 4 of S §12), docs delta file, README/uplugin/CLAUDE.md-table suggestions.
  Run the whole harness (both topologies) plus all `GSTNCombat.*` tests.

## 3. Game-design direction applied to M3 (DD §3, §6, §8)

- Dodge and shield are KIT-dependent (DD §6 "no standard implementation"): nothing in the plugin core assumes them. The default
  command set carries the command tags (`Command.Defence.*`), bindings live in kits (S §8), a kit removes or replaces them via
  `CommandSet.RemovedCommands`/`KitBindings`, and a kit may ship a different dodge/shield ability. The plugin dodges are only the
  DEFAULT kit data. Shield mechanics are M6; M3 ships only the binding path + `scenario_kit_without_dodge` (command does nothing
  and does not reserve the press).
- Base GROUND dodge (PvE) = Mario long jump / Helldivers dive, but the character does NOT end lying down: timeline ability,
  `Movement` track (forward velocity curve, low arc), landing returns straight to Actionable (no prone/getup state, no extra
  `EGSTN_ActionState`). Invincibility is FEET ONLY and only in the INITIAL part of the dodge (default: from take-off to 40% of the airborne frames;
  "User answers (c)" 2): the `HurtboxModifier` track addresses hurtboxes by group/part name or tag (feet group), never a whole-body
  flag and never a hard-coded part/window/state, to match DD §1 per-part hurtboxes; declared now, ignored until M4. NOT a roll.
- AIR dodge = Mario 3D World dive roll: air timeline ability, movement track (momentum mode per feel), `bCancelOnLand` + landing
  lag, `HurtboxModifier` declared (who is invincible: open question 14, assumption feet only; windows/parts are designer data, see "User answers (c)" 2).
- DD §8: NO rolling movement mode now. Dodges are layered moves only; no Mover mode, no `Combat.Mover.*` tag, and no stub, enum
  value or tag for a rolling mode.
- Grab doubles as interact (DD §3, §2g): Interact button TAP = grab, HOLD = pick up objects (heavy grab on hold is dropped).
  The tap/hold bindings are DATA in the slot set (`Press` slot for tap, `Hold(n)` slot for hold, requirements decide), never code,
  resolved by the existing M1 conditions/resolver (hold/tap in GSTNInput). M3 test: two placeholder abilities on the same command
  with tap and hold slots, no ambiguity, tap not swallowed while a hold is pending; the shipped default slot set uses tap = grab,
  hold = pick up, and swapping them is a data edit (test with the swapped set). Grab ability, pick-up ability, interact target query,
  grabbability by size/broken parts, throwing enemies = M6/M10. TODO(M6) "interactable in reach" tag stays.
- Dash/dodge/air-dodge are plugin timelines overridable per kit: none adds a Mover mode or a tag outside the §8.2 roots.

## 3b. Design-direction check

A. Conflicts DD vs spec / earlier plan
1. DD §6 (dodge/shield per kit) vs S §8 listing them as weapon-independent defaults: resolved as "default kit data, overridable";
   soften the S wording in the final docs commit.
2. DD §6 (feet-only invincibility) vs S `HurtboxModifier` track: needs per-part/group targeting. Check S field definition before C2
   and fix the declaration in `FGSTN_TimelineTrack`.
3. DD §6b (listen reference) vs S §11/§12 (dedicated reference): plan and working-tree testing.md follow DD; S still says dedicated.
4. DD §1/§2 (capped percent, per-part health) vs M3 attributes: DD §7 lists "M3 attributes" as affected by combat-parts.md; plan
   keeps `UGSTN_CombatAttributeSet` = stats/movement only (open question 13).
B. DD points that are NOT M3 work (recorded so they are not lost)
- Parts/hardness/penetration/transfer, part break, weight from remaining health, stagger/juggle, part-dependent hitstun/hitstop,
  wall splat/bounce/destruction, crystal shattering/chips, grabbability, thrown enemies: M4/M6/M8/M10 + new combat-parts.md.
  M3 obligations only: (a) `FGSTN_HitVictimView` and the extension surface must not assume one health value or one hurtbox
  (C3a/C3b); (b) timeline hitbox/hurtbox tracks carry a part/group id so assets authored now stay valid; (c) no health attribute
  hard-wired into the meter set.
C. Harness gaps vs DD §6b (harness not edited here; follow-up in C0b): host zero-reconcile assertion, bytes/s per client, outbox
   single-fire on host, OnRep-less apply, host-frame budget probe.



## 4. Spec conflicts / open decisions to ask the user

1. Tags and activation context placement: S §3/§4 puts `Command.Movement.*`, `Input.Lock.Command.*`, `Input.BufferEvent.*` and
   `FGSTN_ActivationContext` in CombatGAS; MT §8.1/§8.2 say input vocabulary/context live in GSTNInput. Plan follows MT. OK?
2. Attribute set name: S `UGSTN_MovementAttributeSet` vs MT `UGSTN_CombatAttributeSet`. One set with stats + meter + (M4) damage,
   or the S split (Movement + Meter)? Which?
3. Pipeline in M3: there is no hit pipeline until M4. MT §8.5.1 wants `Pipeline` as step classes from the first commit. Proposal: create
   `UGSTN_CombatRules` now with the built-in activation steps as the only step list (candidate -> resolve -> execute order stays fixed in code). OK, or defer to M4?
4. Where the M3 doc deltas go: `docs/` is off limits for this plan; OK to do one final docs commit after C15?
5. `Combat.Lock.Command*` (S §4) vs `Input.Lock.Command*` (input.md §3): confirm renames are applied in the tree before C1; otherwise a rename checkpoint C0d first.
6. Spec stats with listen host: host pawn is authoritative and unpredicted; confirm the target topologies for CI: dedicated (kept) + listen (new), 1 host + 1 client minimum.
7. `EGSTN_ActionState` / hitstun etc.: S defines only `Actionable/InTimeline` (M4 adds). Fine.
8. Dash "run" model (looping timeline vs kit run speed) is left "by feel" in S: choose later, pick default = plain walk + kit run speed.
9. Where `AimYaw/AimPitch` of `FGSTN_InputCmd` lands: `GSTNInputNP` (input plugin). Needs the input plugin owner's approval since M2 files are uncommitted.
10. Jump: keep project `GA_Jump` (wall movement) alongside `UGSTN_GA_PlatformJump`, or retire? Wall movement stays untouched in M3.
11. Interact tap/hold: is `Command.Link.Grab` the single command, and which tag signals "an interactable is in reach"? (tap/hold ANSWERED 2026-10-22 (c): tap = grab, hold = pick up; command-tag name and in-reach tag still open.)
13. Capped-percent and per-part health (DD §1/§2): reserve anything in M3 attributes, or land entirely with combat-parts.md in M4?
    Plan: latter, no health attribute in M3.
14. Air dodge (dive roll): invincible at all (whole body, feet only, none)? Plan: declare the track, default empty, decide in M4.
15. Ground dodge: ANSWERED 2026-10-22 (c): feet invincible only in the initial part (default from take-off to 40% of airborne frames), window is designer data. Jump-/air-cancel still open (assumption: no cancel).
16. Interact tap vs hold: ANSWERED 2026-10-22 (c): tap = grab, hold = pick up objects; bindings are data.
17. Listen host: accept host advantage in M3 scenarios (plan: yes, M4 topic)?
12. Spike failure policy: if spike 2 shows a shared settings object, go to per-mode override (touches `Plugins/EngineOverrides/Mover`) - OK to touch the override?

## 5. Blueprint / shared-asset edits (tell the user BEFORE doing them)

- `BP_GASNPP_TestCharacter` (`/Game/GASNPPTest/`): add loadout/skin components, test input config (C15, maybe C9 for camera aim).
- `SandboxCharacter_Mover` (shared pawn): needs the combat component (exists), `UGSTN_LoadoutComponent` lives on PlayerState instead,
  so a PlayerState Blueprint/game mode change: probably `BP_*GameMode`/`PlayerState` (C7 wiring), and the Mover modifier queue (C12).
- Input assets under `Content/Input`: new mapping context actions for Switch/Dodge/Grab/FastFall, listed in
  `Config/DefaultGame.ini` `AbilitySystemMappingContexts` (C13/C14).
- `Content/Levels/DefaultLevel.umap` already modified in the working tree (not mine): no edit planned; test maps new under `Content/GASNPPTest/Combat/`.
- New assets (kit/weapon/timeline/GE data) are new files, not edits, but go through Git LFS without locks; created via
  `Tools/GAS/setup_gas_assets.py` (extends it; it is currently modified/uncommitted) or the editor MCP.
- `Config/Tags/GSTNCommands.ini` (+ plugin tag ini for `Weapon./Kit./Super.` leaves): config files, not Blueprints.
- `Config/DefaultGameplayTags.ini` redirects if renames from Q5 are pending.

## Decisions from the coordinator (2026-10-22)
1. Activation context and every `Input.*` tag live in the input plugin (GSTNInputCore/NP), modularity §8.1. Combat-only tags go in GSTNCombatGAS (§8.2).
2. Follow combat-m3-moves-kits.md; where open, one attribute set per concern: `UGSTN_CombatAttributeSet` (stats/movement) and `UGSTN_MeterAttributeSet` (meters).
3. Pipeline as a list of step classes from the first M3 commit that has a pipeline (§8.5.1).
4. Verify the input.md §8 renames in the tree; if not applied, do them in their own commit before M3, with redirects in `Config/DefaultGameplayTags.ini`.
5. `AimYaw/AimPitch` go in `FGSTN_InputCmd` (input plugin), only after the M2 S2 commits land.
6. Keep the project `GA_Jump` in the project module (mark "to be replaced later", do not delete); add `UGSTN_GA_PlatformJump` in GSTNCombatNP.
7. No interact-range tag now. TODO(M6): tag marking "interactable in reach" for grab/interact.
8. No Mover override change without the user's ok; if spike 2 fails, stop and report with a proposal.
9. One final docs commit is fine.
M3 code waits for coordinator confirmation. game-design-direction.md read at d02312b (2026-10-22); owner answers to follow in open-questions-m4-m10.md.

## User design answers (via coordinator, 2026-10-22)
- Per-part health, percent-with-cap HP, weight tied to remaining HP, stagger/juggle and part destruction are M4: keep the M3 attribute set free of them, but do not block adding them later.
- Grab = interact: tap/hold is data-configurable through the input system's hold conditions (no hard-coded behaviour).
- Still open with the user, working assumptions kept: air dive-roll invulnerability = feet only; ground dodge not jump-/air-cancellable; host advantage accepted in M3. (The earlier "whole ground dodge" and "tap = interact, hold = grab" assumptions are superseded by "User answers 2026-10-22 (c)" below.)
- Soften spec S §8 for the feet-only HurtboxModifier: yes, do it in the M3 docs (final docs commit).

## Owner rules 2026-10-22 (from game-design-direction.md 2d/2c and combat-m4-hits.md 7b, read at origin/claude/project-thread-cb006o 908e7dd)
1. **Arbitrary gravity.** Gravity can change arbitrarily. Variable-jump gravity phases, fast fall, air jumps, dash and every "up"/"horizontal"
   notion use the pawn's local gravity direction from the Mover sim state: Up = -GravityDir, never world Z. Pure helpers in Core take
   `GravityDir` as an input (no UWorld). Tests: `GSTNCombat.Core.Gravity.PhasesFollowLocalGravity`, `...Jump.TiltedGravityApex` (pure, tilted
   and inverted gravity) + PIE `scenario_tilted_gravity_jump` / `scenario_dash_tilted_gravity` (host and remote client within 1 cm).
   **ORG-1 (needed addition):** grep shows no gravity in `Plugins/GSTNInput/Source` today: the Mover sim-state tracker
   (`UGSTN_MoverSimStateTracker`, GSTNInputNP) / sim state must expose the gravity direction (from the Mover `FMoverDefaultSyncState`/
   UpDirection or the active gravity source, read at the same frame as the other tracker fields, quantised, in the NP-safe snapshot). Do it as
   a small additive commit in GSTNInputNP before C13 (needs the coordinator ok, M2 S2 files are still uncommitted).
2. **Trampolines / external launches.** A separate `GSTNLevelObjects` plugin (M4 7b proposal) will request a launch layered move
   (`FGSTN_LayeredMove_Launch`) via the normal GE path, direction = trampoline normal, not gravity up. Jump abilities must not assume they are
   the only upward launch source (no "vertical velocity > 0 means I jumped"). Reaction is DATA: `FGSTN_JumpResetRules` { `AirJumpsOnExternalLaunch`
   (Keep / Refill / Set N), `CoyoteOnExternalLaunch` (Clear / Keep), `CancelHoldPhaseOnExternalLaunch` (bool, ends variable-jump gravity phase) },
   triggered by a launch tag / sim-state flag (`ExternalLaunchedThisFrame`, to be derived from the tracker, ORG-1b) and defaulting to
   Refill air jumps + clear coyote. Deterministic, no `IsServer` branch.
3. **No dodging in hitstun or hitstop by default.** Implemented as blocked tags on the dodge abilities' data (`Combat.State.Hitstun`,
   `Combat.State.Hitstop`; names to match M4 once defined), not as hard rule: a kit may override for a special escape. Covered in C14.

## Ricochet room (coordinator, 2026-10-22; game-design-direction §2e, combat-m4-hits §3b item 3c)
A ricochet = a hit on a part too hard for the attack: no hitstop; the attacker's ability ends its timeline into a recoil authored per attack. M3 reserves room only if cheap (no logic, M4 fills it in):
- Optional per-attack `FGSTN_RicochetResponse` in `FGSTN_CombatTimelineData` (recoil montage, recoil frames, optional movement track, cue), with a weapon/kit fallback. Pure data, GSTNCombatCore (C2 for the struct, C5 for the fallback).
- `UGSTN_GA_BaseCombat` (C10): a path that ends the timeline into the response on an attacker-outcome flag (flag is set by M4; M3 only reads it, default never set). Test: pure timeline-player test that an outcome flag switches to the recoil data.
- Super armor = infinite stagger threshold, stored as a bool flag, never a float sentinel (M4; keep the M3 data free of it).
Plan only, no code.

## Owner rules 2026-10-22 (b) (from game-design-direction.md 6c and modularity-and-tags.md 3.7, read at origin/claude/project-thread-cb006o 41eb27e)
### R-A Cancellable animation tails (DD 6c) - belongs to C2 (data) and C10 (UGSTN_GA_BaseCombat)
- After `FirstActionableFrame` (the start of the cancellable tail; the designer sets it as DATA in `FGSTN_CombatTimelineData`: tail-start
  frame, defaults to FirstActionableFrame, unset = no cancellable tail) the timeline can be cancelled by ANYTHING: any command, plain
  movement input (stick past deadzone), jump input. The montage blends out (blend-out frames are data, deterministic). No cost
  refund, no new request type: the cancelling input is simply the next request/movement and the ability ends with EndReason=TailCancel.
- Before the tail start only the existing cancel windows / override rules apply.
- Reuse in M4: stagger is the same pattern (about 0.5 s unskippable, then a cancellable tail), so the split "locked part / cancellable
  tail" is a generic Core struct (`FGSTN_TimelineTail`, evaluated by `FGSTN_TimelinePlayer::IsInTail(Frame)`), not attack-specific.
- C2 pure tests: `GSTNCombat.Core.Timeline.TailCancelFlagAfterStart`, `...TailCancelNotBeforeStart`, `...TailCancelUnsetMeansNone`,
  `...TailStartClampedToLength`. C10 NP test: `GSTNCombat.NP.Timeline.TailCancelByMovementInput`.
- C10 PIE scenario `scenario_tail_cancel`: tail cancelled by movement input, by jump, by a new command; host and remote client within
  1 cm; no cancel one frame before the tail start; clean at 2 % loss.
### R-B Independence rules (MT 3.7): acceptance line for EVERY checkpoint
Acceptance addendum "Independence": new plugin code (a) has no `/Game/` reference, no include of the project module, no project ini key
and no project tag; (b) settings live in the plugin's own UDeveloperSettings with defaults in `Plugins/<X>/Config/Default<X>.ini`;
(c) depends on another plugin only through its `*Core` module; (d) optional integrations are bridge plugins, never `#if WITH_<PLUGIN>`;
(e) game-specific tags are FGameplayTag/query fields in settings or data; (f) tests carry the plugin prefix.
Per checkpoint: C1 (GSTNCombatGAS deps Core only; fork requirement declared in .uplugin/README); C2, C5, C6 (pure Core, tags in data);
C4, C7 (settings in a combat UDeveloperSettings, defaults in the plugin Config/); C8-C10 (resolve or list the GSTNCombatNP ->
GSTNInputNP edge, see docs/notes/independence-audit.md item 1); C12-C14 (jump/stat tags as data fields); C15 (canary build per
plugin if `Tools/Canary/make_canary.py` exists).

## User answers 2026-10-22 (c) (from game-design-direction.md 2g, modularity-and-tags.md 3.8, 3.9, 4.2b; origin/claude/project-thread-cb006o 7ce3ad7)
1. **Interact button: TAP = grab, HOLD = pick up objects.** Flips the earlier assumption (tap = interact, hold = grab). The heavy grab
   on hold is dropped. Bindings are data in the slot set (`Press` for tap, `Hold(n)` for hold), never code. Applied: section 3 grab
   paragraph, C14 `scenario_interact_tap_hold`, Q11/Q16, "User design answers" assumption line. M3 still ships mechanism + placeholder
   abilities only; grab/pick-up abilities are M6/M10.
2. **Ground dodge: feet invincible only in the initial part** of the dodge (default: from take-off to 40% of the airborne frames).
   Which hurtbox parts, windows and states are invulnerable is ALWAYS designer data (`HurtboxModifier` track addressing parts by name
   or tag), never hard-coded in C++ or in the ability. Applied: section 3 ground dodge, C14, Q15. C2: the `HurtboxModifier` declaration
   takes a part name/tag (+ frame range); the fraction default lives in the default dodge timeline asset, not in code. Still open,
   assumptions kept: air dive roll = feet only; no jump-/air-cancel of the ground dodge; host advantage accepted in M3 (M4 topic).
3. **Designer tags: roots native, leaves ini** (MT 4.2b). Roots are declared in C++ by their owner; leaves are created by designers in
   the editor into a tag ini (plugin `Config/Tags/<Owner>.ini`, game `Config/Tags/GSTNContent.ini`). **Code never names a leaf.**
   C5 `IsDataValid` rule: `KitTags` and `WeaponTags` hold EXACTLY ONE identity leaf under `Kit.` / `Weapon.` (other tags classify only),
   unique per asset type; the validator reports zero or several. Specials use `Ability.Combat.Special.<Name>` (identity in the ability's
   `AbilityTags`, MT 4.2b), not a second root. Behaviour comes from data and tag queries (`Weapon.Sword` matches `Weapon.Sword.Longsword`).
   Kit and weapon identity tags are granted through the existing init/stats effects so bindings can require them deterministically.
   Tests: `GSTNCombat.GAS.Data.{KitIdentityExactlyOne,WeaponIdentityExactlyOne,IdentityUniquePerType}`; grep gate: no `"Kit.` / `"Weapon.`
   leaf string and no `UE_DEFINE_GAMEPLAY_TAG` for a leaf under these roots in plugin C++ (C1 declares roots only, as planned).
4. **MT 3.8 Designer extension in Blueprint.** Most gameplay logic must be extensible by designers in Blueprint. Extension classes
   (pipeline steps, rules, outcome/knockback hooks, activation helpers, anything with behaviour added in M3) are
   `UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)` with ONE `BlueprintNativeEvent` entry point: input is a const
   view struct (BlueprintReadOnly fields only), output is a result struct, C++ behaviour is `_Implementation`. Hooks inside the simulation
   are pure (no world/actor/Mover reads, no latent nodes, no timers, no Blueprint variables kept between calls, seeded random only).
   Per-frame per-entity hooks (tick extension, timeline player tick) stay C++; a Blueprint variant is opt-in and counted in the budget.
   Tuning structs are `USTRUCT(BlueprintType)` with `EditAnywhere, BlueprintReadOnly`; tag fields carry `meta=(Categories=...)`; stateless
   math in `*Utils` `BlueprintPure` libraries. Events reach Blueprint through gameplay messages/cues, not multicast delegates on sim
   components. Applies to C3b (`UGSTN_CombatStep`, rules; `IGSTN_CombatTickExtension` stays C++ per-frame), C2/C5 structs, C6
   `UGSTN_LoadoutRules`, C9/C10 `GA_Base` hooks. Each extension class gets an example Blueprint in test content (C15) and a README
   "Extending in Blueprint" section.
5. **MT 3.9 Replaceable content.** Content is replaceable through soft references and data indirection only (soft class/object pointers
   or Primary Asset IDs in data assets, selected by tag/experience). No `ConstructorHelpers`, no literal asset paths in plugin C++, no
   hard reference from one plugin's assets to another plugin's content. Plugin defaults come from a `UDeveloperSettings` with soft
   references. Simulation-read content (kits, timelines, rules) must resolve before the pawn's first NP tick and match on host and
   clients; the content-signature check at join is an MT proposal, noted for C8/C15 and not built in M3 unless the coordinator asks.
   Applies to C5 (definitions use `TSoftObjectPtr`/`TSoftClassPtr` for montages, effects, abilities), C7 (loadout/skins), C15.

### Acceptance additions, EVERY checkpoint (from answers 3-5, next to R-B Independence)
- **Blueprint extensibility (3.8):** every new extension class is Blueprintable with a BlueprintNativeEvent entry point and const view in /
  result out (per-frame per-entity hooks excepted, documented); new tuning structs are BlueprintType; tag fields have `Categories`;
  stateless helpers are BlueprintPure. Grep gate in the commit.
- **Replaceable content (3.9):** grep gate: no `ConstructorHelpers`, no `"/Game/`, no `TEXT("/` asset path, no hard `TObjectPtr`/`TSubclassOf`
  to content in plugin data where a soft reference works (justify each exception in the commit body).
- **Designer tags:** no leaf of `Kit.`/`Weapon.`/`Ability.Combat.Special.` named in C++; identity-validator test where an asset type is added.
Per checkpoint: C2 (HurtboxModifier part name/tag, BlueprintType structs), C3b (BP-capable steps/rules), C5 (identity validator, soft refs),
C6 (rules BP-extensible), C7 (soft refs, skins), C9/C10 (GA_Base hooks pure), C14 (dodge window as data, interact slots as data), C15
(example BPs, README sections).

## Early M3 item (2026-10-22): exclusive context tag groups for binding validation
Problem: `BS_GASNPP_Test` reports 8 "Bindings X and Y can both hold and require the same number of tags: choice is arbitrary" errors (Grounded/Airborne pairs of Attack/Special commands). `FGSTN_TagRequirements::CanHoldTogether` only knows "one requires what the other ignores"; it cannot know that Grounded and Airborne never coexist. Not silenced with test-data tie-breakers (the bindings are correct).
Design (needs a header change in GSTNInputCore, so a full rebuild with the editor closed):
- `USTRUCT FGSTN_ExclusiveTagGroup { FGameplayTagContainer Tags; }` ("a context holds at most one of these"), and `TArray<FGSTN_ExclusiveTagGroup> ExclusiveContextGroups` on `UGSTN_InputSettings` (config, `Categories = "Input.Context"` not applied: groups may hold any state tag). Plugins contribute through the existing `SetContribution` mechanism (extend it with a groups argument); `Config/DefaultGSTNInput.ini` ships the plugin's own pair (`Input.Context.Grounded` / `Input.Context.Airborne`, native tags of this plugin, so naming them in its own default config is fine). No project tag is ever named in the validator.
- `CanHoldTogether(Other, TConstArrayView<FGSTN_ExclusiveTagGroup> Groups)`: additionally false when a group contains a tag required by this and a different tag required by Other. `FindBindingConflicts` gets the same optional parameter (default empty = today's behaviour); `UGSTN_CommandBindingSet::IsDataValid` (GSTN_CommandBindingSet.cpp:233) passes `UGSTN_InputSettings::Get()`.
- Tests: `GSTNInput.Core.Input.Bindings.*`: grounded vs airborne with the group -> no conflict; same pair without the group -> conflict; three-member group; a binding requiring two tags of one group is itself impossible (report it as an error of the binding).
- Docs: input.md (settings table), plugin README (Settings section).
Status: written on disk, not built; the 8 errors stay until it is built.

## Early M3 item (2026-10-22, coordinator): split GSTNCombat the same way
GSTNCombat.uplugin hard-depends on GSTNInputNP (non-optional), so using only the combat core drags in the NP fork. Likely fix: GSTNCombat (core) + GSTNCombatNP as separate plugins, like GSTNInput/GSTNInputNP. Check §8.1 here and modularity-and-tags.md §3.7 first.

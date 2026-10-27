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

## M6 follow-up 2 (F2): extra hurtboxes, clock track hook, action veto, outgoing entries

Full entry: `docs/notes/m4-doc-deltas.md`, "M6 follow-up 2 (F2)" (it touches M4's hurtbox selection and the request serialization). Deviations from the plan text and deltas for combat.md, modularity-and-tags.md and the M6 spec (folded by F14):

- **`FGSTN_InboxEntry` moved to the core module** (`GSTNCombatCore/Public/Sync/GSTN_InboxEntry.h`, header only, no behaviour). The plan puts `OutgoingEntries` (`TArray<FGSTN_InboxEntry>`) on `FGSTN_HitResolutionContext`, which is core; the struct was in `GSTNCombatNP`. The inboxes, the kind registry and the delivery stay in NP; `GSTN_CombatInbox.h` includes the new header.
- **The track hook is registered per track struct, not per kind enum.** `EGSTN_TimelineTrackKind` is closed (player, validator and editor switch over it), and a closed enum cannot name a defence track. `FGSTN_TimelineTrackHookRegistry::Register(const UScriptStruct*, IGSTN_TimelineTrackHook*)`: the defence track derives `FGSTN_TimelineTrack`, reports kind `None` (the player skips it) and its struct is the key. The clock calls the hook of every such track on the running timeline every tick, with the frame the timeline is on (a stall holds it) and `FGSTN_InputSimState`. `EvaluateStart` runs the hooks too, for stamps only: a stall of frame 0 starts with the next tick, because the previous-frame arithmetic of `AdvanceClock` assumes no pause was counted between the two. Hooks are not run on a recoil.
- **A stall is a pause of the timeline only.** `bStallThisFrame` adds one to `TimelinePausedFrames`, once per tick even when the tick is also a hitstop frame; `PausedFramesTotal` (the combat clock) is untouched. The stamped window (`FGSTN_StampedWindow { StartFrame, EndFrame, Profile }`, simulation frames, two at most per tick) is reported on `FGSTN_TimelineOutput::StampedWindows` and, for an extension, through the new `FGSTN_CombatTickContext::Timeline`; the plugin writes it into its own fragment at `AfterClocks`. Nothing is added to a sync state.
- **`BlocksActions` closes the gate the candidates ask.** Asked of every extension (OR, no short-circuit) once a frame after the `BeforeCandidates` point; a true answer makes `UGSTN_CombatSimComponent::IsCommandAllowed` return false for every command (even the "ignores hitstun" abilities), as `bFrameGateClosed` does for the loadout swap, but only for the commands: clocks, adoption and the attacker step still run. The component path is not unit tested (it needs a world); the registry is (`GSTNCombat.NP.Tick.BlocksActionsVeto`) and F7's `scenario_shield_*` will exercise the gate.
- **`OutgoingEntries` go to the attacker of the hit.** A step appends to `FGSTN_StepResult::OutgoingEntries` (C++ steps only: an inbox entry is not reflected), the runner merges them into the context, and `FGSTN_CombatVictim::Resolve` stamps `Frame` = the victim's frame and `SenderId` = the victim and moves them to `FGSTN_VictimResult::Outgoing` with `TargetId` = `Entry.AttackerId`, for hits that stand (not a ricochet, not refused by a step). `RunVictimStep` posts them with `PostToInbox(TargetId, Entry)` after `Resolve`; the attacker drains them on the next frame. A target that does not tick on the machine refuses them, so on a client the reply reaches an attacker the client predicts and on the host every attacker (S3 will measure the correction count).
- **Extra hurtboxes.** Priority scale shared with the body, a tie goes to the extra (the bubble at priority 0 still sits above the body it covers), ignore tags match hierarchically, an unset hit type is never ignored, two extras per entity (`GSTNHurtboxIndex::MaxExtraHurtboxes`), cut at the snapshot build. The set holds 128 hurtboxes at most now (validator). `AppendExtraHurtboxes` is called by the snapshot build only (not by the rebuild between inbox entries: the victim step resolves against the extras of the end of the previous frame, like the hurt states). `FGSTN_HitVictimView::ExtraHurtboxes` is a copy, empty for almost every hit.
- **Request slots.** The array had eight slots already (plan: four to six); no capacity change. `EnterLinked`, `ExitLink`, `ApplyLinkEffect`, `RemoveLinkEffect`, `LaunchLinked` = 18 to 22, data only. `EGSTN_CancelCondition::OnParry` = 4, data only (`ConditionHolds` returns false for it, as for `OnBlock`).
- **Tests**: `GSTNCombat.Core.Hurtbox.{ExtraWinsByPriority, ExtraIgnoredByHitType, NoExtraIdenticalToM4, IndexRangeReserved}`, `GSTNCombat.NP.Snapshot.ExtraHurtboxInHistory`, `GSTNCombat.NP.Clock.{TrackHookStallsTimelineNotClock, HookStampsWindow}`, `GSTNCombat.NP.Tick.BlocksActionsVeto`, `GSTNCombat.NP.Victim.OutgoingEntriesPosted`, `GSTNCombat.NP.Requests.EightSlotsRoundTrip` (named after the real slot count).

## Shield decision (owner, 2026-10-26)

After spike S5 (tilt failed on an interpolated victim) the owner fixed the shield as a **spherical bubble of fixed size, as in Rivals of Aether 2**: no stick tilt, no shrink with the shield's health, no directional arc variant, no poke mechanic and no `ShieldPoked` event. Shield health only decides shield stun, pushback and the break. The spec and plan were updated on `claude/project-thread-mvizq6` (commit b77b878); S5b is dropped; the bubble stays an extra hurtbox as F2 built it. For combat.md and the M6 spec (folded by F14): `ShieldCentreAt`/`MakeShieldBubble` take the rules and the entity transform only; the fields `ArcDegrees`, `ElevationLimit`, `TurnRate`, `RadiusByFraction` and `Tilt*` do not exist; the plan's F4 tests `RadiusFixedAtAnyValue` and `CentreArbitraryUp` replace the tilt and shrink tests.

## F3: GSTNDefence skeleton, data, tags, tracks

- All defence tags are declared in `GSTNDefenceCore` (accepted layout, stated in the README); there is no `ShieldPoked` tag and a test asserts it.
- Added beyond the plan's field list: `FGSTN_ShieldRules::MaxShield` (the attribute default needs a home), `FGSTN_SDIRules::bSDIAllowedWhenArmored` and `bSDIInStaggerInPlace` (spec 5.5, needed by `SDI.ArmoredBlocked`), `FGSTN_ParryProfile::Name` (what a parry window track names), `EGSTN_ParryAnchor`, `EGSTN_TechType`, `GSTNDefenceLimits::MaxParryProfiles = 16`.
- `Build.cs` lists Engine, DeveloperSettings and (private) Projects besides the plan's modules: engine modules the data assets, settings and the tag-folder registration need. No GAS, NP, Mover, Link, Editor or project module.
- Ability classes in `FGSTN_DefenceKitExtension` (`ShieldAbility`, tech set, get-up set) are `TSoftClassPtr<UObject>` because Core has no GameplayAbilities dependency; F6 checks the class when it resolves them (and may add a `MetaClass` filter).
- The two tracks derive `FGSTN_TimelineTrack` and report kind `None`; F2's hook registry keys them by struct. `FGSTN_ParryWindowTrack::ProfileIndex` is `Transient` and baked by `Bake(Rules)`; `Validate` reports an unbaked or unknown profile.
- `FGSTN_Curve1D`: X in 1/4096, Y in Q2.14, round half up; an empty curve is the identity. Quantisation means the factor at 0.8 reads 3277/4096 = 0.800048828125.
- `Config/Tags/GSTNDefence.ini` holds the five cue leaves only.

## F4: GSTNDefenceCore math

- `ResolveParry(Context, Rules)` takes the rules asset as well as the context (the projectile flag lives on the profile, which depends on the source); the context carries flags, no tags. The move source reads a precomputed `bMoveCoversHurtbox` (from `FGSTN_ParryWindowTrack::Affects`).
- `MakeShieldBubble(Rules, Transform)` returns the world sphere; `MakeShieldBubbleHurtbox(Rules, IgnoreHitTypes)` returns the root-relative extra hurtbox (priority 255, source `GSTNFeatureIds::Defence`).
- Shield value: a released view regenerates from `max(ReleaseFrame + RegenDelayFrames, StampFrame)`, so a damage re-stamp after regeneration began restarts it from the hit; a view never raised regenerates from `StampFrame + RegenDelayFrames`.
- `EvaluateTech` takes the history's press frames, the newest contact, the hitstop end and whether the entity tumbles; `RegisterTechPress` is the single press rule. **Picking the tech kind from the stick and the contact normal is deferred to F8** (it needs the contact record).
- SDI: `bScaleByHitstopFrames` is implemented as a speed cap of `MaxDistance / (HitstopFrames x FrameSeconds)` (spreads the maximum over the hitstop); residue below 1e-9 cm ends the drift. `FrameSeconds` defaults to 0.016.
- `UGSTN_DIRulesMath::ApplyDI` rescales the stick to `min(1, |stick| x curve(|stick|) x victim scale)` and scales `MaxDIElevationDot` by the elevation curve, then calls M4's `ApplyDI`.
- Tests: Shield 6, Parry 11, Tech 7, DI 3, SDI 3, Bounce 1 (31) plus F3's 8 = 39 `GSTNDefence.Core.*`.

## F5a: GSTNLink skeleton, definitions, rules data, tags

- `Combat.HitType.Grab` is declared by `GSTNCombatCore` (`GSTNCombatHitTags::HitType_Grab`) already, so the plugin reads it and does not redeclare it; the plan's tag list and the spec's owner column should say "read" for it. 38 native tags in `GSTN_LinkTags.h`, cues ini-only.
- The state tags of the other optional plugins (`Combat.State.Tumble`, `.Knockdown`, `.ShieldBroken`, `.Intangible`) are `FGSTN_GrabRules::IgnoreTagNames` (`TArray<FName>`), resolved with `RequestGameplayTag(..., false)` when the request is decided, not `FGSTN_TagRequirements` entries: a constructor may run before the declaring plugin registered its tags, and a name no loaded plugin declares is skipped. `Combat.Part.GrabLocked` is read the same way behind `bRespectGrabLocked` (grab default on, latch default off).
- Primary asset type `GSTN_LinkType` comes from `GetPrimaryAssetId`; the project has not yet listed it under `PrimaryAssetTypesToScan` in `Config/DefaultGame.ini` (a root ini, owner edit: F9, when the registry first scans).
- `Build.cs` lists Engine, DeveloperSettings and (private) Projects besides the plan's modules, as `GSTNDefenceCore` does; the module class registers `Config/Tags` with the tag manager. No GAS, NP, Mover, Editor, GSTNDefence or project module.
- `FGSTN_LatchRules` (requirements, `MinLatchWeight` 250, `bRespectGrabLocked`, `ImmunityFramesAfterLatch`) sits next to the grab rules, and `FGSTN_LinkKitExtension` carries both.
- `GSTN_LinkKitExtension.h` and the outcome track are header-level data only; nothing consumes them before F9.

## F5b: GSTNLinkCore rules, placers, state, math, fake host

- **One latcher per partner is the shared busy-slot check**, not a rule of its own: `UGSTN_LinkRule::CheckCommon` refuses a target whose channel already holds a link (`TargetBusy`), and a latch occupies the partner's Body slot, so `Rule.Latch.OnePerPartner` passes through it. A pending request occupies the slot too.
- `EGSTN_LinkRefusal` (NoDefinition, SelfLink, InitiatorBusy, TargetBusy, CoexistForbidden, TargetLocked, RequestRequirements, Requirements, GrabLocked, TooHeavy, TooLight, Immunity) is for logs, tests and UI; it never reaches the wire. The kit's rules in `FGSTN_LinkRequestContext` (`InitiatorGrabRules` / `InitiatorLatchRules`, raw pointers, not reflected) win over the rule's own copy.
- `FGSTN_LinkState` is plain data (type index, role 1/2, phase, partner `FGSTN_EntityRef`, serial, link frame, end frame, flags, break reason, payload, quantised yaw); idle serialises to one byte. Serials are an 8-bit circle, `IsSerialNewer` counts half the circle as later, `NextSerial` is one after the newer of the two and never 0. The break reason enum reserves `FirstPlugin = 16` for a plugin's own reasons.
- `UGSTN_LinkMath::EvaluateBreak` order: pending (Forced if unnamed), own death, partner death, partner state, hit (per own role; a partner's hit breaks only with `bPartnerHitsBreak`), distance, escape (mash end), timeout. `MashProgress` never pulls the end before the current frame.
- The parked transform is `SocketOffset * SocketRootRelative * PartnerTransform` for whichever role `bParked` names (`TryGetParkedTransform` refuses the other role), so a latch parks role 1 with the grab's code.
- Placers: `FGSTN_ExitQuery` carries a non-reflected `IGSTN_ExitCollisionQuery*` (null = no collision check, first candidate wins); the ring is the preferred direction then seven 45-degree turns counter-clockwise about the local up, then above, then the fallback reported invalid; the at-socket placer keeps the throw's knockback as velocity even on the fallback. A Blueprint placer cannot reach the query.
- `IGSTN_LinkHost` / `FGSTN_FakeLinkHost` (entity views as values, link slots, definitions by index, immunity stamps, placement, one event per formation or break) are the host contract the NP module implements in F10.
- Tests: the plan's 31 names, unchanged (`GSTNLink.Core.*`: Tags 2, Data 3, State 4, Mash 3, Break 6, Placer 4, Rule.Grab 4, Rule.Latch 3, FakeHost 2).

## F6: GSTNDefenceGAS attributes, effects, shield ability data, loadout contribution

- **The ability classes are not in the GAS module.** `UGSTN_GA_BaseCombat` (and `UGSTN_GA_Base`) live in `GSTNCombatNP`, so a class of
  `GSTNDefenceGAS` cannot derive from them without the forbidden NP edge (spec 1b, plan ground rules). F6 ships what the abilities are made
  of, NP-free and tested: the shield timeline as data (`FGSTN_ShieldAbilitySpec::BuildTimeline`: startup, ONE `FGSTN_HoldWindowTrack` on
  `Input.Button.Shield` with `bEndOnZeroShield`, release, one cancel window per out-of-shield option over the hold window), the shield
  `CanExecute` decision (`UGSTN_DefenceLibrary::CanExecuteShield`: available, value > 0, not breaking, grounded; `EGSTN_ShieldRefusal`),
  the get-up requirement (`GetupNeutralRequirements`: `Combat.State.Knockdown`), the class-checked tech and get-up set resolution, and the
  loadout contribution. `UGSTN_GA_Shield`, `UGSTN_GA_Tech`, `UGSTN_GA_GroundBounce`, `UGSTN_GA_GetupRoll` and `UGSTN_GA_GetupNeutral` are
  thin `UGSTN_GA_BaseCombat` children of **F7** (`GSTNDefenceNP`) that plug these in. The spec's 1b table ("`GSTNDefenceGAS`: ...
  `UGSTN_GA_Shield`, `UGSTN_GA_Tech`") and the plan's F6 file list should move those classes to the NP row (folded by F14).
- **Loadout builder hook added to `GSTNCombatGAS`** (the plan's F6 names one; none existed): `FGSTN_LoadoutExtensionRegistry`
  (`Loadout/GSTN_LoadoutExtensions.h`), the same shape as `UGSTN_LoadoutRules::RegisterValidator`: `FGSTN_LoadoutExtension` with three
  optional callbacks, `ContributeCompileInput` (run at the end of `FGSTN_LoadoutAssembly::BuildCompileInput`, every machine), `OnGranted`
  (after `UGSTN_LoadoutComponent::GrantToAbilitySystem` gave the table's abilities, authority) and `OnCleared` (before `ClearGranted` removes
  them), run in lexical id order. `FGSTN_ResolvedLoadout::WorldContext` (the loadout component, null in tools and tests) lets an extension find
  the match's rules component. Test `GSTNCombat.GAS.Loadout.ExtensionHooks` (+1 to the GSTNCombat count). The combat-m3 loadout text
  (combat.md 10, modularity-and-tags 5b) gains this seam (F14).
- The defence contribution (`UGSTN_DefenceLibrary::ContributeToLoadout`, id `GSTNDefence`): the `Command.Defence.Shield` binding only where
  `IsShieldAvailable(rules, kit has a shield)`, as a **slot override** `Press(Shield)` + `Hold(Shield, 0)` requiring `Input.Context.Grounded`
  (the standard slot set has no shield slot, and the slot must stay free for kits where the shield is unavailable, spec 1d); the neutral get-up
  as a `Command.Movement.Jump` binding requiring `Combat.State.Knockdown` on the kit's own jump slot; the Forced-started classes (ground
  bounce, roll, get-up attacks, the tech set in kind order) as kit triggered abilities, so they are granted and enter the timeline registry.
  `GrantDefence` adds `UGSTN_DefenceAttributeSet` initialised from the rules where the shield is available and removes it otherwise. The rules
  resolve kit extension first, then the match's `FGSTN_DefenceRulesExtension` through `UGSTN_CombatRulesComponent::Find(WorldContext)`, then
  `UGSTN_DefenceSettings::DefaultRules`.
- `FGSTN_GetupSet::GroundBounce` added (Core): the spec's `UGSTN_GA_GroundBounce` had no slot in the kit extension; the landing bounce is
  kit data like the roll and the neutral get-up.
- Effects. `UGSTN_GE_ShieldDamage`: instant, additive on `ShieldHealth` through `UGSTN_ShieldDamageMagnitude` (reads the set-by-caller
  `Combat.SetByCaller.ShieldDamage`) with a -1 coefficient; `ApplyMirror(ASC, Value)` writes an absolute mirror value through the same
  modifier (loss = base - value), so one class serves the damage and the sparse mirror. `UGSTN_GE_ShieldRestore`: instant, additive by the
  set-by-caller **name** key `GSTN.Defence.ShieldRestore.Amount` (no new tag, as F6 allows none); `RequestedDelta(ASC, LastMirrored)` is the
  request the model reads (mirror base value minus the last value it mirrored; positive = restore, negative = a hand-applied loss, 0 = nothing).
  `UGSTN_GE_ShieldBreak`: instant and empty, a Blueprint child adds the cue. The attribute set clamps `ShieldHealth` in `[0, Max]` on base
  and current value and pulls the health down when the maximum drops (`PostAttributeChange`).
- `LogGSTNDefence` is now exported from `GSTNDefenceCore` (`GSTNDEFENCECORE_API`), for the GAS and NP modules.
- Tests: `GSTNDefence.GAS.{AttributeClamp, ShieldBindingOmittedWhenUnavailable, ShieldAbilityRefusesWhenUnavailable, TechSetPerKit,
  GetupNeutralRequiresKnockdown, ShieldRestoreSeenAsRequest}` (the plan's six names; the ability tests exercise the decisions and data the
  F7 ability classes call) plus `GSTNCombat.GAS.Loadout.ExtensionHooks`.

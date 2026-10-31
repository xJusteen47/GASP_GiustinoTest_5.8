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

## F7: GSTNDefenceNP part 1 (F7a fragment, snapshot, clocks, abilities; F7b steps, applier, punish)

- **Module deps**: `GSTNDefenceNP` also lists `AbilitySystemSimulation` (the `UNpGameplayAbility` base of `UGSTN_GA_BaseCombat`, needed to link
  the five ability classes) and `Mover` (the push back move); the uplugin lists both plugins. No GSTNLink module.
- **Action states are derived, not stored** (spec 5.1 adds `ShieldStun`, `ShieldBroken`, `Punished`, `Knockdown` to `EGSTN_ActionState`):
  the core enum is untouched; the states are the frame against the fragment's end stamps and the `Breaking` flag, `BlocksActions` (F2) vetoes
  every command while any holds, and the tags are projected from them. A window that runs out needs no clearing.
- **Combat seams** (GSTNCombatNP/Core): the snapshot entry and the victim view carry one blob per contributor (`FGSTN_SnapshotExtensionSet`,
  two at most, ascending type id; parts and defence both contribute; `MaxContributors` 2); `FGSTN_TrackHookEnvironment` (the button map and
  the entity's fragments) and the track index reach the track hooks; `IGSTN_CombatTickExtension::HeldCancelConditions` (OnParry) feeds the
  cancel gate; `FGSTN_ReactionRequest::bNoTumble`; `FGSTN_ResolvedHit::Frame` (the hit's stamped frame, what every defence window is
  tested against); `FGSTN_SnapshotContributorContext::ActionState` (no bubble in a hit reaction).
- **Hold stall**: the hold hook stalls the timeline ON the hold frame (the frame the previous tick evaluated) while the button is held or
  the shield is stunned, so the out-of-shield cancel windows of that frame stay open; the button is read from the history at the end of F-1
  (the combat tick samples input after the clock step): a release is seen one frame late, the same on every machine.
- **Push back is not a reaction request**: the request seam always staggers (a launch or a hitstun floor), which would end the shield. The
  block's push back is queued in the fragment and given at the hitstop end as `UGSTN_GE_PushBack` (the victim side push back move: a slide,
  no tumble). The break's upward launch does use the request (`bNoTumble`).
- **Shield value at the hit**: the snapshot's value at the snapshot frame minus the shield damage, re-stamped at the hit frame (at most one
  frame of drain apart from the exact value; identical on every machine, which read the same snapshot).
- **Restore request**: `RequestedDelta` is applied by the tick extension at the frame it is seen (after clocks), not by the applier at F+1.
- **Meter**: a blocked hit gives the attacker the combat rules' per-damage meter; the defender's small gain (block and parry) is not applied,
  the victim side has no meter request path yet (F14 or a combat seam).
- **Parry immunity**: only the victim's `Combat.Immunity.Parry` (snapshot bit); `FGSTN_HitData` has no tag container for the hit side.
- **`bCloseOnParry`** of the parry track rides in bit 7 of `MoveParryProfile` (fragment and snapshot); profiles stay below 16.
- **Perfect shield advantage**: the tick extension ends the shield timeline at `ParryRecoveryEndFrame` (the spec's "actionable early"), so no
  cancel window content is needed. A move parry keeps playing; its `OnParry` cancel windows open from `LastParryFrame`.
- **Punish**: inbox kind `GSTNDefenceInboxKinds::Punish` = 16 (the first defence id); `PunishEndFrame = max(hitstop end, frame) + stun`,
  the running timeline cancelled, the hitstop extended (with a freeze request) only where the attacker's own prediction did not open it.
  A profile with no attacker stun sends no entry ("advantage only"). A parried hit no longer sets the attacker's hit confirm (OnHit
  cancels); a blocked one does (OnBlock stays unimplemented).
- **Events**: `Combat.Event.Parried` (magnitude = `EGSTN_ParrySource`), `Combat.Event.Blocked`, `Combat.Event.ShieldBroken`, from the
  applier's `AppendEvents`; the core's `HitLanded`/`HitReceived` still fire for a blocked or parried hit.
- **Step placement**: `UGSTN_DefenceContentLibrary::AddDefenceStepsToRules` puts Parry then Shield after `HurtboxReaction` (the core's
  placeholder note), so before `HealthWeight`/`PartResolve` and `ComputeKnockback`. Both steps live in `GSTNDefenceNP` (they read the NP
  snapshot extension), not in Core like `PartResolve`.
- **The bubble is any extra hurtbox** of the entity (only the defence contributes extras today); a second contributor of extras would need
  the bubble's position in the extension.
- **Snapshot build reads the ASC** (immunity tags, the attribute set's rates and multiplier) at the end of F-1, as the spec states; the
  steps and the applier read only the snapshot and the fragment. `ResolveParryTrack` reads the combat component's table set (content) to
  find the running track.
- **The shield ability's frames** are class defaults (`StartupFrames`, `ReleaseFrames` on `UGSTN_GA_Shield`, timeline built on the CDO),
  not built from the rules at grant: a kit whose rules differ names a child class with the matching frames.
- **Found by the harness and fixed in F7**: (1) the F6 shield binding `Press(Shield)` + `Hold(Shield, 0)` in one step was refused by the loadout
  validator (its canonical input pressed a held button), so no kit with a shield loaded: now `Press(Shield)` only (the hold is the timeline's
  hold window), test `GSTNDefence.GAS.ShieldBindingMatchesPress`; (2) `UGSTN_GA_Shield::CanActivateAbility` read `Input.Context.Grounded` from
  the ability system, where it never is (it is a context tag of the input provider): the binding's requirement owns it now; (3) the mirror was
  written unquantised and any difference >= 0.01 read as a restore request, so the regenerating shield was re-stamped at machine-dependent
  frames: the mirror gets the quantised value and a request is at least one quantum (0.25 point).
- **Test content**: two data assets, `/GSTNDefence/Test/DA_DefenceRules_Test` (PvP) and `DA_DefenceRules_TestPvE`, from
  `Tools/GAS/setup_defence_link_content.py` (`.gitignore` gains `!Plugins/GSTNDefence/Content/`). No `GA_Shield_Test` / `GA_CounterTest`
  Blueprints: the native `UGSTN_GA_Shield` matches the test rules' frames and `UGSTN_GA_CounterSample` (new, `GSTNDefenceNP`: parry window over
  frames 2..14, profile "Counter", `OnParry` cancel into `Command.Attack`) is the counter move. The bubble radius of the test rules is 95 cm
  at the capsule middle (the test pawn's origin), so it covers `HBS_Test_Pawn`.
- **Harness** (`defence_scenarios.py`): the kit, the input config (`Input.Button.Shield` appended, before PIE) and the combat pipeline are
  changed in memory only; the parry scenario widens the perfect-shield window and the bubble's release frames to 30 frames in memory so a
  scripted release survives 60/15 ms (and 2 % loss); the break scenario uses a 7 point shield draining 0.02 a frame. `scenario_grab_shield`
  is a stub (pure grab-type checks) until F12. A hit the attacker lands on a just-broken bubble through lag compensation resolves on the victim
  as a body hit (the victim's own snapshot has no bubble): accepted behaviour.
- `UGSTN_DefenceContentLibrary` gains `AddDefenceStepsToRules`/`RemoveDefenceStepsFromRules` and a per-world defence event log
  (`StartDefenceEventLog`, `GetDefenceEventLog`, `StopDefenceEventLog`; debug only).
- **`part_break_launch` with GSTNDefence enabled (HANDOVER 52 open item, closed in HANDOVER 53)**: not a defence regression. Every defence
  piece (tick extension, contributor, hooks, applier, punish handler) early-outs for an entity with no defence data, which is the parts test
  pawn. The failing quantity was the owner's peak vertical speed, a latency reading: the owner learns of the hit after the launch frame and
  re-simulates the rise, and the test recorder samples Mover velocity and mode in `PreSimulationTick`, where a re-simulated tick still reports
  the pre-rollback state (Walking, 0 cm/s) while its position already matches the authority's to 0.1 cm. The owner's peak was therefore the
  speed of its first forward tick, 540 - 15.68 per tick of lead (9 to 16 ticks across the P14 and F7 sweeps; > 300 fails past 15). The check
  now keeps the speed threshold on the authority and asks the owner to follow the authority's first 16 rising ticks within 1 cm
  (`_owner_follows_rise`); owner windows are compared on the hitstop only, as `_HitRig` does (a correction after the hitstun end arrives
  canonical, hitstun end -1). Any harness check that reads owner velocity or mode from re-simulated sim ticks has the same blind spot.
- Tests (17 NP + 1 GAS): `GSTNDefence.NP.Fragment.{RoundTripIdleOneBit, ActiveBytesWithinTable, Interpolate, ForcedRollbackTakesAuthority}`,
  `.Snapshot.{BubbleContributed, NoBubbleWhenUnavailableOrHitstunOrBroken, RulesIndexPerEntity}`, `.Tick.{HoldStallsTimelineNotClock,
  VetoWhileStunned, TagsProjected}`, `.Steps.{ParryBeforeShieldOrder, ShieldBlockNoDamage, BodyHitWhileRaisedContinues, BreakLaunchUp,
  GrabPassesThroughBubble, ModeOffSkipsShieldNotParry, OutgoingPunishPosted}`.

## F8: GSTNDefenceNP part 2 (landing, downed, get-ups, optional tech, DI rules, SDI drift)

- **Core seams added to GSTNCombatNP (neutral, no defence knowledge)**:
  - `FGSTN_LayeredMove_Freeze::DriftVelocity` (OverrideAll velocity of a freeze, one bit on the wire when zero) and
    `UGSTN_FreezeMoveCreator::bDriftFromSetByCaller` (the drift from `Combat.SetByCaller.Move.Velocity.X/Y/Z`, priority 1 above the hitstop
    freeze, finish keeps the velocity).
  - `FGSTN_HitSyncState::SDICount` (uint8, reserved) renamed `SDIDistanceUsed` and widened to uint16 in 1/8 cm (`SDIUnitsPerCm`); the
    victim step resets it on every staggering hit. Test `GSTNCombat.NP...HitState` updated for the rename only.
  - `FGSTN_CombatTickContext` gains `StickOnPlane`, `Stick` (the frame's command), `Up`, `Buttons` and `Services`
    (`IGSTN_CombatTickServices::StartForcedTimeline`, implemented by the combat component: step 8's start with source Forced plus an
    `ActivateEntry` request). The pre-tick now executes `ActivateEntry` (it existed as a type with no executor), under the command
    activation scope: a non-command activation is refused by `UGSTN_GA_Base`'s interrupt policy while a timeline runs, and the forced
    timeline is already the running one when its ability activates (found in PIE: the bounce timeline ended after one frame).
  - `IGSTN_CombatTickExtension` gains `AdmitsOnlyBindingsRequiring` (the gate passes only bindings whose requirements hold the tag),
    `ConsumeTumbleContact` / `OnTumbleContactResolved` (called by the victim step around `StepImpact` for a new contact, same conditions as
    the impact; the hooks run before the frame's tables are chosen, so their context has no services) and `OverrideDI` (wired through
    `FGSTN_ReactionStepInput::DIOverride` into `StepReaction`, the existing hit-state path at the hitstop end). `RunVictimStep` takes the
    frame's input state and reads the contact whenever an extension is registered, not only with an impact table.
- **Downed state is not a veto** (F7 had Knockdown in `BlocksActions`): it admits only the bindings that require
  `Combat.State.Knockdown`, so the neutral get-up (jump slot) and the get-up attacks can start; F7's test `Tick.VetoWhileStunned` now
  checks this. The downed movement lock is an infinite effect `UGSTN_GE_DownedLock` (`Combat.Lock.Movement`, read by the stats movement
  modifier), applied and removed on the tag projection from `IsKnockedDown`.
- **No contact modifier**: the spec's `FGSTN_TumbleContactModifier` is not added; M4's knockback probe already records floor and wall
  contacts (`FGSTN_TumbleContactSyncState`), and F8 reads that record through the hooks. `Combat.Mover.Tumble` stays unused.
- **Landing rules**: no tech set: `KnockdownEndFrame` = contact + `MaxDownedFrames`, or never (MAX_int32) for 0; a kit with a tech set
  that missed: contact + `KnockdownFrames` + `NoTechKnockdownBonusFrames` (spec 6 algorithm). The bounce stick is the command of the
  frame the model sees the contact (contact + 1). The bounce is a knockback move (flight time from the rise), the ground bounce ability
  starts Forced at AfterClocks of the same frame (the decision is carried in the fragment's pending start, empty at the end of the tick).
  Holding the stick (|stick| >= 0.5) once the bounce timeline is over starts the get-up roll Forced; the roll's direction is content
  (the thin class has no movement track).
- **Tech**: presses come from the history up to the end of the previous frame, so `LateFrames` presses after the contact cannot count
  (evaluated at contact + 1; the default is 0). The kind: wall -> WallTech, or WallJumpTech with the stick away from the wall (dot > 0.5);
  floor -> InPlace with a neutral stick, else RollRight/RollLeft by the physical stick's x; a kind without an ability falls back to
  InPlace/WallTech. Tech moves: in place and wall techs hold still (the core's at-rest freeze) for their frames, a roll slides
  `RollDistance` over `RollFrames`, the wall jump tech launches off the wall at the same speed (no rule fields for its motion yet). A wall
  tech failure with no impact outcome keeps M4's behaviour (no extra reflect).
- **SDI**: drift frames are the victim's hitstop frames but the last (the hitstop freeze hands back its velocity on the frame after; a drift
  finishing with it would fight it); the used distance is stored rounded up to 1/8 cm, so the quantised total never passes the cap.
  `SDIBlocked` (throw, armored unless allowed, stagger-in-place tier per the rules) and `DIDisabled` are recorded at every hit in the
  fragment's `HitFlags` by the outcome applier: the Parry step now appends a pass-through outcome (Result None) for every hit it lets go
  on, so the applier sees it (the step tests read the deciding outcome, the last one).
- **Fragment**: + `HitFlags`, `PendingStartFrame/Kind` (presence bits 10 and 11; everything live 2 + 91 bytes).
- **Attach data**: + the kit's ground bounce, roll and tech classes (`FGSTN_DefenceAttach::SetKitMoves`, loaded where the loadout compiles).
- **Content and harness**: `UGSTN_DefenceContentLibrary::SetKitDefenceMoves` (the native landing and tech classes on a kit, in memory) and
  `GetSDIDistanceUsed`; the state string gains `techlock techpress techend techtype down hitflags`; the defence event log records Teched and
  KnockedDown. The four thin classes get a plain `TotalFrames` timeline. Scenarios `landing_bounce` and `landing_getup` (the plan's one
  landing scenario split in two: its second shot found the attacker turned by the first), `tech_ground`, `tech_wall` (the impact_wall
  layout: a Wall test surface the wall mode slides on), `sdi` (the asset's SDI values); the tech scenarios widen the window to 60 frames in
  memory and force a reconcile of the victim's client after the press. The get-up press is the combat button (`combat_tap` of Jump): the
  `press('jump')` helper drives the movement input action only.
- Tests (12 NP): `GSTNDefence.NP.Landing.{BounceSteered, DownedThenGetupRoll, GetupAttackFromSlot, JumpGetsUp, MaxDownedFrames,
  TechSuccessCancelsImpact, TechFailureResolvesImpactFirst, NoTechSetNoLockout}`, `.DI.RulesOverloadApplied`,
  `.SDI.{DriftSweptByMover, MaxDistancePerHit, ThrowBlocksSDI}`. The move effects check the avatar has a Mover first (a test rig has none).

## F9: GSTNLinkGAS registry, requirements, interact provider, anims by role

- **Asset scanning (Step 0, 0942df4) is plugin-owned, not a root ini line.** `GSTNLinkCore`'s module registers the primary asset type
  `GSTN_LinkType` when the asset manager is created (`UAssetManager::CallOrRegister_OnAssetManagerCreated`,
  `ScanPathsForPrimaryAssets` over `/GSTNLink` when that mount exists plus the new setting `UGSTN_LinkSettings::LinkTypeDirectories`,
  base class `UGSTN_LinkTypeDefinition`, no Blueprint classes, not editor only, `CookRule = AlwaysCook`). Reason: a
  `PrimaryAssetTypesToScan` line in `Config/DefaultGame.ini` naming `/Script/GSTNLinkCore.GSTN_LinkTypeDefinition` makes the engine
  `ensure` ("Failed to load class ... for Primary Asset Type", `FPrimaryAssetTypeInfo::FillRuntimeData`) in every build where GSTNLink
  is disabled, which is the default. No root ini changed; `/Game/GASNPPTest` is not scanned yet because no link type asset lives there:
  the project adds it with one `+LinkTypeDirectories=(Path="/Game/GASNPPTest")` line in its own `Config/DefaultGSTNLink.ini` when the
  first one does (F11/F12). Closes HANDOVER 50 open item (3) and the F5a delta "owner edit: F9". If a project also lists the type in its
  ini it must use the same base class and flags, or the asset manager reports a mismatch.
- **`GSTNInteractCore` exists on the branch**, so the provider implements `IGSTN_InteractCandidateProvider` and the plan's fallback
  projection is not built. `GSTNLink.uplugin` now depends on the `GSTNCombatGAS` and `GSTNInteract` plugins (GSTNInteract is enabled in
  the committed `.uproject` already). Provider id `GSTNInteractProviders::LinkEntities = FirstConsumer` (16) is allocated in the central
  id list of `GSTN_InteractTypes.h` (one line in GSTNInteract, the file its comment names as the only place for ids).
- **`UGSTN_LinkRegistry` is an engine subsystem**, not a world one: the table is content, shared by a listen host and its PIE clients in
  one process. It builds when the asset manager completes its first scan (`CallOrRegister_OnCompletedInitialScan`) and on demand
  (`Get()` builds if needed, `RebuildFromAssetManager`), never by itself afterwards, so it cannot change under a match. Code-made
  definitions join through `AddDefinition` / `RemoveDefinition` (the acceptance path: a test module registers a type without touching
  GSTNLink); `Build(Definitions)` is the pure entry point. Wire index = 1-based rank of the type tag's name (case-sensitive), so it depends
  on the set, never on load order; duplicates keep the definition whose path sorts first; more than `MaxLinkTypes` (255) are reported.
  `GetSignature()` (CRC of the table and the plugin reasons) tells two machines with different sets apart; nothing compares it yet (F10's
  content signature can).
- **Skipped definitions**: the registry runs `UGSTN_LinkTypeDefinition::Validate`, the same function the asset's `IsDataValid` reports
  (F5a), logs `Link registry: Skipped <path>: <problem>` as a warning and keeps the lines in `GetBuildErrors()`. No editor-only validator
  was added: the definition's own `IsDataValid` already reports unregistered tags.
- **Reason table** `FGSTN_LinkReasonTable`: generic reasons keep the `EGSTN_LinkBreakReason` indices (through `UGSTN_LinkMath::BreakReasonTag`
  / `BreakReasonFromTag`, so the core's break math needs no lookup); plugin reasons take `FirstPlugin` (16) on, sorted by tag name, so
  the registration order does not matter. Plugins register by tag NAME from their module startup (`UGSTN_LinkRegistry::RegisterPluginReason`,
  a static list, because no registry exists that early); each build appends the names that resolve. An append re-sorts, so indices are
  fixed only once the registry is built (before a match).
- **Requirement library** (`UGSTN_LinkRequirementLibrary`): `EvaluateRequest` = `UGSTN_LinkRule::CheckCommon` then the definition's rules in
  order (first refusal wins; a definition with no rule is decided by the common checks), `ResolveDefinitions` (definition from the
  registry, each side's `OtherChannelDefinition` from its other channel's type index), `ApplyKitRules`, `FindKitExtension`, `IsLinkLocked`
  (`Combat.Lock.Link`, which `CheckCommon` already refuses as `TargetLocked`), `MeetsRequirements`, and `GetActionBlockingRoleTags`: the roles
  that are parked AND do not act while linked (the grabbed; not the latcher, not the grabber whose pummels and throws are abilities), for the
  base ability's activation block in F10/F11, so a plugin's parked role is blocked without being named. Everything per link type is data:
  the drill's latch variant (spin-drill-plan 2b) is a definition plus its own `UGSTN_LinkRule` subclass, with no change here.
- **Interact provider** (`FGSTN_LinkInteractProvider`, plain C++ implementing the interface): reads a `FGSTN_LinkInteractSource` through
  `FGSTN_InteractQuery::HostContext` (type name `GSTNLinkInteractSource`): the interactor as a `FGSTN_LinkRequestSide`, the entities
  (request side, location, radius) and the OFFERS (definition, `Interact.Target` meaning, reach, kit rules) in priority order. An entity is
  offered under the first offer whose `EvaluateRequest` accepts it, so the pre-filter is exactly the target's LinkRequest decision
  (`GrabLocked` by name, `Combat.Immunity.Grab`, immunity frames, weight, busy channels, `Combat.Lock.Link`). Candidate id
  `{LinkEntities, entity id}`, reach override = the offer's reach (the kit's `InteractReachCm` for a grab), radius = the capsule, live for
  every frame (the snapshot is the stamp). Arc, scoring, ties and the focus stay the probe's (`UGSTN_InteractMath`). `MakeGrabOffer` builds
  the standard offer; a latch offer is the same struct with another definition and meaning. `ProjectTargetTag` (static) removes every
  `Interact.Target` tag and adds the focus meaning only when actionable: the "one tag per frame" rule as a pure function the F10 link tick
  extension calls (the plan put the projection in the tick extension; the function lives here so it is tested without NP).
- **Anim selection** (`UGSTN_LinkAnimSelection`): `FGSTN_KitLinkAnims` per role tag already lived in `GSTNLinkCore` (F5a's
  `FGSTN_LinkKitExtension::AnimsByRole`), so `GSTN_LinkAnimSelection.h` holds the selection only: weapon extension first, then kit (the M3
  weapon anim layer being the more specific), soft classes never loaded; `SelectForLinkState` maps a state's type index and role through the
  registry; `GatherRoleAnimLayers` lists them for a client preload. No runtime consumer of M3's kit/weapon anim layers exists yet
  (`LocomotionAnimLayer` / `AnimLayer` are read by the editor preview only), so "consumed by the M3 anim pattern" means the same soft-class
  data shape; the presentation link is F10/F11.
- `LogGSTNLink` is now exported from `GSTNLinkCore` (`GSTNLINKCORE_API`) for the GAS module, as F6 did for `LogGSTNDefence`.
- **Tests: six, one more than the plan's five**: `GSTNLink.GAS.Registry.{IndicesStableAcrossLoadOrder, UnknownTagSkipped, ReasonTableAppend}`,
  `GSTNLink.GAS.Interact.{ProviderFiltersUngrabbable, OneTargetTagPerFrame}` and `GSTNLink.GAS.Anim.LayerByRole` (the anim selection had no
  test name in the plan). The acceptance (a test-only type gets an index and roles without touching GSTNLink) is inside
  `IndicesStableAcrossLoadOrder`: the test file declares `Combat.Link.Type.GSTNTestOnly`, `Combat.Link.Role.GSTNTestOnlyA/B` and
  `Combat.Link.BreakReason.GSTNTestOnlyA/B` with `UE_DEFINE_GAMEPLAY_TAG_STATIC` under `WITH_DEV_AUTOMATION_TESTS` (test builds only; "no new
  tags" holds for production). `ProviderFiltersUngrabbable` checks the `Combat.Part.GrabLocked` case only when GSTNParts registered the tag.
- Gates (named logs in `Saved/Logs`): `build-f9-1` (Step 0), `build-f9-2`, `build-f9-link-off`, `build-f9-final` Succeeded;
  `headless-f9-on-1.log` 1133 / 0 (1127 + 6, GSTNLink 37, GSTNDefence 75, no asset-manager ensure); `headless-f9-link-off.log` 1096 / 0,
  no GSTNLink line, GSTNDefence 75, no ensure. PIE sweep skipped: nothing registers the provider or reads the registry at runtime before F10.

## F10: GSTNLinkNP part 1 (F10a fragment, snapshot, tick, parked mode, presentation; F10b inbox kinds, steps, harness)

- **Neutral combat seams** (GSTNCombatCore / GSTNCombatNP; with nothing registered they change nothing): `FGSTN_SnapshotExtensionSet` holds
  THREE blobs (parts, defence, link; `MaxBlobs` 3, slots packed in ascending type id), `FGSTN_HitVictimView::MaxSnapshotExtensions` 3;
  `IGSTN_SnapshotContributor::FinalizeSnapshot(Frame, Entries)` and `FGSTN_SnapshotContributorRegistry::Finalize`, called by the snapshot
  build after the sort and before the grid and the history record (a parked entity is placed from its partner's entry); the resolved hit
  carries the ATTACKER's blobs (`FGSTN_ResolvedHit::AttackerExtensions`, `FindAttackerExtension`; the victim step finds the attacker's
  entry through the new `FGSTN_VictimTickInput::Snapshot`, the attacker's prediction its own entry); `FGSTN_CombatTickContext` gains
  `Snapshot`, `AttackerHits` (from AfterExecute on, on a frame the attacker step ran) and `EntityId`.
- **Action state Linked is derived** (as F7's defence states): no `EGSTN_ActionState` value; a parked role that does not act
  (`bActsWhileLinked` false) is vetoed by `BlocksActions`, which also leaves it no candidates. Its running timeline is cancelled by the link
  tick extension at AfterClocks on the frame the veto starts (an applier cannot reach the core). The `EnterLinked` request slot is not used:
  the tick extension queues `FGSTN_LinkedModifier` on the parked edge and cancels it by tag on the edge out (the combat tick runs before
  Mover, so Mover simulates it that frame).
- **The parked mode is entered by the modifier's tag through a transition**: `UGSTN_LinkedModeTransition` (entry: any mode to the linked
  mode while `Combat.Mover.Linked` is in Mover's state; exit, the linked mode's own: to Falling once it is gone). A project authors the
  entry as a global transition; `FGSTN_LinkAttach` adds the mode and an entry transition to every mode of a pawn's Mover at attach. The
  mode carries no tag of its own (a mode's tags count in the state query, so the exit would never fire).
- **Baked socket** = the partner's skeletal mesh REFERENCE POSE socket (socket offset x bone component-space reference pose x the mesh's
  offset from the actor root), baked at attach for every `SocketOnPartner` the registry knows (`FGSTN_LinkBodyData`, append-only table, the
  snapshot carries its index). No per-frame bake of animated sockets; presentation follows the live socket (`bUseLiveSocket`).
- **Every combat pawn is attached** (not only kits with a link extension): any entity can be grabbed and the sockets are the partner's.
  Attach runs from the loadout extension `GSTNLinkNP`, and for pawns whose loadout never compiled (the harness pawns at PIE start) from
  `UGSTN_LinkContentLibrary::RebuildRegistryAndReattach`.
- **Request tags in the snapshot** are a 32-bit mask over `FGSTN_LinkRequestTags` (the immunity and lock tags, `Combat.Part.GrabLocked` by
  name, then every tag the registered definitions' `RequestRequirements` and their grab and latch rules' defaults name, sorted by name). A
  tag only a kit's own rules name is not in the table and reads as absent. The request weight is 100 x the snapshot's weight scale, for the
  provider pre-filter and the step alike (the parts plugin's effective weight is not read).
- **A refused request posts no response**: it does not stand (`bHit` false: the victim step posts nothing for it), which is what makes it
  whiff with no hitstop on both sides. The common refusal (target busy, immune, locked) is predicted identically by the initiator; a
  disagreement (simultaneous grabs) leaves the loser's predicted request to `RequestTimeoutFrames` (12), identical on every machine.
  `LinkResponse{refused}` and its handler exist for later kinds. An accepted request Stops with `bNoDamage` and no knockback: no
  grab-confirm hitstop in F10 (F12's grab content decides it).
- **Not built (no link command exists before F12)**: the grabber's cancel window into `Command.Link.*` (spec 7.2 item 3, the
  `LinkEstablished` buffer event) and PendingBreak on Release; `FGSTN_LinkTickExtension::MarkPendingBreak` is the hook F12's release uses.
- **`LinkCommand` has a test and debug kind `Form`** (type, role, partner): the S1 spike forms its link by a server-side command posted to
  both inboxes (`UGSTN_LinkContentLibrary::FormTestLink`), the plan's "linked by a server-side test command (no pipeline)". The simulation
  never posts it. `Break` and `Place` are handled for F11; a break's exit record is dropped the frame after its own (otherwise an idle entity
  carries 55 bytes of exit forever: measured 78 B per sync state after a break before the fix, 23 B after).
- **`Interact.Target.Grab` is projected only for a kit with a link extension.** The probe reads other entities' snapshot positions, which a
  client knows as interpolated proxies, so its answer is not prediction-stable: projected on every pawn it made the ability system model
  reconcile (found by the spike: `Interact.Target.Grab : 1` in the server's state, absent in the client's). The probe is stateless (no
  sticky focus in the fragment) and has no line of sight. A prediction-stable grab focus is an F12 item.
- **Collision between the two capsules** is ignored from the presentation component's tick (both roots' `IgnoreActorWhenMoving`), not from
  the simulation, so a frame late at the start; F11's exit request is where the restore belongs.
- **Camera and aim**: the plugin exposes `GetLinkedTransform` and `IsLinkedParked`; the camera follow is the consumer's code (GSTNCamera is
  outside this checkpoint).
- **Connect refusal by the fragment registry (acceptance)**: no such rule exists. The fragment list is length-prefixed and a machine without
  the plugin SKIPS fragment 18 (`FGSTN_SyncFragmentList::NetSerialize`); it does not refuse the connection. Not tested; the registry's
  `GetSignature` compared at login would be the place for a refusal.
- **S1 as built** (`link_scenarios.py`): presentation error = the grabbed pawn's shown transform on each world against the server's, after
  removing the best constant delay of that world (-500..500 ms: the grabber's own client runs ahead, its partner's behind), p50 / p95 / max
  over the walk's first 7 s; reconciles per client from the harness counter (one forced reconcile adds 2 to it), judged outside the forced
  windows; the uninvolved observer judged over the held link only (the PIE start-up corrections land in a batch's formation window). Bytes
  per second are not measurable from Python: a live grab's fragment is 23 B per sync state on each side, idle 0 B. No watchdog yet (no host
  frame time with it); the latch pairing of S1 is covered by the unit tests only.
- **Test content**: `/GSTNLink/Test/DA_LinkType_Grab_Test` (grab, the grabbed parked 90 cm in front of the grabber's root facing it, no
  socket, the grab rule) from `Tools/GAS/setup_defence_link_content.py` `run_link()`; `.gitignore` gains `!Plugins/GSTNLink/Content/`. The
  harness turns the test weapon's `Command.Attack.Neutral` into a grab in memory (`SetKitMoveHitType`) for `link_rollback`.
- **Tests (17)**: `GSTNLink.NP.Fragment.{RoundTripIdleOneBit, TwoChannelsBytes, Interpolate}`, `.Formation.{AcceptedAtF1ResponseAtF2,
  RefusedWhiffsNoHitstop, SimultaneousFirstWins, ChainRefused, PredictedClearedOnTimeout, SerialIdempotentAfterRollback, LatchParksInitiator}`,
  `.Parked.{ModeEnteredByTag, HurtboxFromPartnerSocket, NoInputNoGravity, InitiatorFollowsMovingPartner}`, `.Tick.{GrabbedHasNoCandidates,
  LatchedKeepsCandidates, TagsFromRole}`. Test-only types `Combat.Link.Type.GSTNTestOnlyNPGrab` / `NPLatch` and their roles live in the
  test fixture (automation builds only).

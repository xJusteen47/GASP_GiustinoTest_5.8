# M6 spike report (defence and links)

Results of the F0 spikes of `docs/architecture/plans/m6-defence-links-plan.md` (kept on the branch `claude/project-thread-mvizq6`). Numbers
only come from named logs; a row without a log is not a result.

## S0 seam audit

Reads only. Branch `feature/gas-npp-5.8` at 4092e4f (the merge of the desktop branch into `feature`, 2026-10-26), checked against the table of
plan section 2 (audited there at 0848345). Method: `git grep` of the names of the plan's S0 bullet plus a read of the header that owns each
seam. "Plan" is the state the plan claims, "State" what this HEAD has.

| Id | Capability | Plan | State at 4092e4f | Location |
|---|---|---|---|---|
| X1 | Sync fragment registry, ids in `GSTNFeatureIds` | Present | Present (`Parts = 16`, game ids from 128, test ids 240..242) | `GSTNCombatCore/Public/Sync/GSTN_SyncFragment.h` |
| X2 | Combat tick extension hooks | Present, `BlocksActions` missing | As the plan: hook points are `AfterClocks`, `BeforeCandidates`, `AfterExecute`, `BeforeTagProjection`; `Owner`, `AbilitySystem`, `Outbox` are on the context; no `BlocksActions`. There is **no separate "after inbox drain" point**: the inbox is drained in `RunVictimStep` before the clocks, which F1's kind handlers cover (a handler runs at the drain) | `GSTNCombatNP/Public/Simulation/GSTN_CombatTickExtension.h` |
| X3 | Open inbox kinds | Missing | Missing (hit-only `FGSTN_HitInbox`, `FGSTN_HitInboxEntry`) | `GSTNCombatNP/{Public,Private}/Simulation/GSTN_HitInbox.*` |
| X4 | `PostToInbox` | Missing | Missing: `UGSTN_CombatSimComponent::PostHit`, `UGSTN_CombatWorldSubsystem::PostHit`, the virtual `PostHit` of `IGSTN_SnapshotSource` only | `GSTN_CombatSimComponent.cpp`, `GSTN_CombatWorldSubsystem.cpp`, `World/GSTN_StepSnapshot.h` |
| X5 | Pipeline steps as data, `bBlocked`/`bParried` placeholders | Present, `OutgoingEntries` missing | As the plan (`OutgoingEntries` grep: no match) | `GSTNCombatCore/Public/Hits/GSTN_HitResolution.h`, `GSTN_HitSteps.h` |
| X6 | Timeline track kinds registrable by an extension | Verify | Open by construction: `Tracks` is `TArray<TInstancedStruct<FGSTN_TimelineTrack>>`, so an extension plugin can derive a track struct; there is **no registry object** (the player and the validator switch over the known struct types). The hold-stall hook on the clock step and the parry-window stamp hook are missing, as the plan says. Both fall to F2 | `GSTNCombatCore/Public/Timeline/GSTN_TimelineTypes.h:128-482`, `GSTN_TimelinePlayer.h` |
| X7 | Snapshot extension contributors | Present | Present | `GSTNCombatNP/Public/World/GSTN_StepSnapshot.h` |
| X7b | Extra hurtbox seam | Missing | Missing (`FGSTN_ExtraHurtbox`: no match). `PickHurtboxIndex` exists and is the place F2 extends | `GSTNCombatCore/Public/Hits/GSTN_HitShapeUtils.h` |
| X8 | `ExtensionData`; `bParryable`, `bDisableDI`, `LinkFilter`, `LinkType` | Present, four fields missing | As the plan (`bParryable` no match) | `GSTNCombatCore/Public/Hits/GSTN_HitData.h` |
| X9 | `FGSTN_EntityRef` | Missing | Missing | - |
| X10 | `FGSTN_KitExtension`, `FGSTN_RulesExtension` | Present | Present | `GSTNCombatCore/Public/Rules/GSTN_KitExtension.h`, `GSTN_CombatRules.h`; used by `GSTNParts` |
| X11 | Tag requirements, `ActiveSource`, `UGSTN_StandardSlots`, cancel windows | Present, `OnParry` and `LinkEstablished` missing | `UGSTN_StandardSlots` present; `EGSTN_CancelCondition` is a **closed enum** (`Always, OnHit, OnBlock, OnWhiff`), not a registry, so the plan's "through the existing condition registry if it is open" does not apply: `OnParry` goes through F2 (neutral enum value). `LinkEstablished` missing as the plan says | `GSTNCombatCore/Public/Timeline/GSTN_TimelineTypes.h:36` |
| X12 | Reaction request seam, layered moves `Freeze`/`Knockback` | Present, no-tumble variant and `DriftVelocity` to add in F8 | As the plan (`DriftVelocity` no match) | `GSTNCombatCore/Public/Hits/GSTN_HitExtensions.h`, `GSTNCombatNP/.../Movement/GSTN_LayeredMove_*` |
| X13 | `UGSTN_HitMathUtils::ApplyDI` | Present | Present (owner file found by the grep of `ApplyDI`) | `GSTNCombatCore` |
| X14 | `SDICount` in the hit state | Present | Present | `GSTNCombatNP/Public/Simulation/GSTN_CombatHitState.h` |
| X15 | Tumble contact record and probe | Present | Present as `FGSTN_TumbleContactSyncState` (record name for F0 S2) with `FromHit`, carried by the knockback layered move; surface impacts in `GSTN_ImpactTypes.h`/`GSTN_ImpactMath.cpp` | `GSTNCombatNP/Public/Movement/GSTN_TumbleContact.h`, `GSTN_LayeredMove_Knockback.h:57` |
| X16 | Outcome applier registry (`AppendEvents`), attacker outcome seam | Present | Present (`FGSTN_OutcomeApplierRegistry`, `FGSTN_AttackerOutcomeRegistry`) | `GSTNCombatCore/Public/Hits/GSTN_HitExtensions.h:157,228` |
| X17 | `Combat.Part.GrabLocked`, `EffectiveWeight` | Present | Present (`GSTNParts` projection and `UGSTN_PartMath`; `EffectiveWeight` also in the knockback model) | `GSTNPartsCore/Public/Math/GSTN_PartsProjection.h` |
| X18 | `GSTNInteract` | Not on `feature` | **Now on `feature`** after the merge: `IGSTN_InteractCandidateProvider`, `GSTN_InteractMath` in `GSTNInteractCore`, plus `GSTNInteractRuntime` | `Plugins/GSTNInteract/Source/` |
| X19 | `GSTNCombatDebug` | Not on `feature` | **Now on `feature`** after the merge | `Plugins/GSTNCombatDebug/` |
| X20 | M3 request array | Present, 4 slots | Present with **`Capacity = 8`** (better than the plan's 4; F2's "grow to 6" is already covered). Wire: 4 bits count, 5 bits type, packed frame, 16 bits arg. Request types go to `17` (`EndReaction`); `EnterLinked`, `ExitLink`, `ApplyLinkEffect`, `RemoveLinkEffect`, `LaunchLinked` do not exist yet. The 5-bit type field holds 32 values | `GSTNCombatNP/Public/Simulation/GSTN_CombatActionTypes.h:42-100` |
| X21 | Lag-compensation history | Present | Present (`RewindActors` in the fork's targeting library, used by the M4 history) | `Plugins/GASNPP/AbilitySystemSimulation`, `GSTNCombatNP/World` |
| X22 | `UGSTN_WallQueryUtils`, `UGSTN_MoverSimStateTracker` | Present | Present (project module and `GSTNInputNP`); the M4 §7b probe stays the one F8 uses | - |

Verdict: **no row is worse than the plan says.** Differences, all in the plan's favour or already covered by its fallbacks: X18 and X19 are
now on `feature`; X20 has 8 slots, not 4; X6 has no registry object but is open through instanced structs; X11 needs `OnParry` in F2 because
the cancel condition is a closed enum (the plan names F2 as the fallback); X2 has no separate "after inbox" hook point (F1's kind handlers
run at the drain, which is the same moment).

## Baseline (untouched 4092e4f)

Build: `Saved/Logs/build-baseline.log` (Development Editor Win64, `Result: Succeeded`, 400.84 s, fresh Intermediate). Headless `Automation RunTests GSTN`: `Saved/Logs/headless-f1-baseline.log` (opened 11:37:51, exit 11:38:37 on 2026-10-26): **1002** `Result={Success}`, **0** `Result={Fail`. This is the reference for every later M6 count; `GSTNParts` is enabled in the committed .uproject.


## S5 shield bubble as an extra hurtbox (F2 code, 2026-10-26)

**Verdict: FAIL on the interpolated-victim threshold for tilting. STOP: owner decision (fallback is the contact-point test); no fallback was implemented.** The other thresholds pass, with one caveat on cost.

**Method (read this before the numbers).** A throwaway automation test (kept outside git in `Saved/s5-spike/`, never committed) runs the REAL F2 code: the snapshot contributor registry with a contributor that returns the analytic bubble (sphere, radius 18 to 60 cm from the shield value, centre shifted up to 20 cm by the tilt, priority 255, ignores Grab) from a shield sync fragment of integers (value 0..1000, tilt -1000..1000); the fragment wire (`FGSTN_SyncFragmentList::NetSerialize`) and its `Interpolate`; the lag compensation history (`RecordHistory`, `BuildOverlay`); and the real `FGSTN_CombatAttacker::Tick` (overlap, `PickHurtboxIndex`, pipeline) to classify each hit as miss, poke (body) or block (bubble). **It is an in-process model, not a network run**: the shield timelines are generated (400 frames at 60 Hz), and the observers are: the SERVER, which rewinds to the fractional time the attacker's client showed the victim at and takes the history record at or before it (what `SampleHistory` does; it interpolates only the transform); the EXACT observer (the host, or the victim's owner client), which has the same integer frame after the wire; and the INTERPOLATED observer (the attacker's client), which has the victim blended between two server frames by the fragment's `Interpolate`. 20000 random hitbox positions per scenario (sphere of radius 25 cm, uniform in a ball of 130 cm around the victim's chest; body = one sphere of radius 45 cm), a random fractional time each. No packet loss, no jitter model: the fraction is uniform in [0, 1). Log: `Saved/Logs/s5-spike2.log` (`LogGSTNSpikeS5: S5RESULT`), headless `Automation RunTests GSTNSpike`, build `build-f2-s5d.log` "Result: Succeeded". The first run `s5-spike.log` was invalid (the fragment was stored as the base type, so no bubble was ever emitted and the wire dropped it: `wire_mismatches=400`, no cost difference); it is not used.

| Scenario | Connecting hits | Owner/exact disagreements | Interpolated disagreements | Per 100 hits | Radius error (cm) | Centre error (cm) |
|---|---|---|---|---|---|---|
| (a) standing | 5501 | 0 | 0 | 0.000 | 0 | 0 |
| (b) tilting, full circle every 0.8 s | 5764 | 0 | 119 | **2.065** | 0 | mean 1.30, max 2.63 |
| (b) tilting, flick in 3 frames | 5605 | 0 | 75 | **1.338** | 0 | mean 0.96, max 13.34 |
| (c) draining 125/s while held | 3625 | 0 | 4 | 0.110 | mean 0.044, max 0.126 | 0 |
| (d) minimum radius | 3231 | 0 | 0 | 0.000 | 0 | 0 |

Wire round trip of the fragment: 0 mismatches in every scenario. The disagreements of (b) are block against miss at the edge of the bubble (circle: 56 block->miss and 56 miss->block, 6 poke->block, 1 block->poke). Cause: the server's `SampleHistory` copies the record at or before the rewound time, so it tests the bubble of frame k; the attacker's client shows the blended state k + alpha, and a tilting bubble has moved by up to 2.6 cm (13 cm for a flick) between the two. Draining changes the radius by 0.09 cm per frame, which is why (c) is almost clean.

**Pass criteria.** Zero disagreements for (a), (c), (d) on the owner: **pass** (0 in all). At most 1 per 100 on an interpolated victim: **pass for (a), (c), (d)**, **fail for (b)** (2.07 and 1.34 per 100). A way out that this spike did not build: have the history keep the victim's fragments and re-contribute with `Interpolate` at the rewound fraction (so the server tests the same blended bubble the client showed); that is the decision to take with the owner together with the contact-point fallback.

**M4 hurtbox selection with no bubble.** 20000 random hits through the real attacker step: contributor registered but the shield down vs no contributor, 0 differences in outcome and hurtbox index; against the M4 rule written out (a hit lands iff it overlaps the single body hurtbox), 0 differences. Together with `GSTNCombat.Core.Hurtbox.NoExtraIdenticalToM4` (every subset of seven candidates): unchanged.

**Cost of 8 shielding entities** (Development Editor build, 20000 iterations after 1000 warm-up, three rounds, microseconds per frame; noise about 0.3 us):

| Geometry | Snapshot build, extra | Attacker step (4 hitboxes against 8 victims), extra | One attacker, total extra | Eight attackers, total extra |
|---|---|---|---|---|
| Out of reach, broadphase only (ring 120 cm, 0 hits) | +0.93 (2.26 vs 1.33) | +0.7 (4.6 vs 3.8) | 1.7 | 6.5 to 7.2 |
| In contact (ring 55 cm, 32 hits resolved by the pipeline) | +0.94 | +20.4 to 20.6 (64.3 vs 43.7) | 21.3 to 21.6 | 164 to 166 |

Pass is < 100 us per frame: **pass for one attacker in any geometry and for eight attackers with no contact; 165 us for the artificial case of eight attackers each resolving 32 hits in the same frame** (a landed hit opens hitstop, so this does not repeat frame after frame). Most of the 20 us is the copy of the extras into the victim view of each candidate hit (`MakeVictimView`); a pointer instead of a copy would remove it if it ever matters.

**STOP conditions evaluated:** S5 (interpolated rate for tilting above 1 per 100): hit. Owner decision needed. Not evaluated here: S1 to S4 (other checkpoints), a real listen-server or lossy run of S5 (the model has no jitter or loss), and what players feel at 2 hits in 100 on a tilting shield.

## S5 outcome (2026-10-26)

The owner removed tilt and shrink on 2026-10-26 (shield: fixed-size bubble, Rivals of Aether 2 style, no arc, no poke). S5 therefore passes for the remaining scenarios (standing, draining while held: 0 disagreements on the owner and the exact observer, within the 1 per 100 limit on an interpolated victim), the scenarios that failed (tilting) and (d) minimum radius no longer exist, and the bubble stays an extra hurtbox. S5b (history re-contribution) is dropped.

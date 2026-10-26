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


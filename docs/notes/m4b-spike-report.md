# M4b spike report (P0), laptop, 2026-10-25

Plan: `docs/architecture/plans/m4b-parts-plan.md` (branch `claude/project-thread-cb006o`) P0; spec `combat-parts.md` 5.1, 3.4, 8.4. Repository at 4624fa7.
Output is versioned here like the M4 notes; it becomes combat.md 17 when the docs checkpoint (P14) runs. No plugin code was needed for the spikes
(the P1 skeleton was built beside them and the temporary spike test removed before the commit).

| Spike | Verdict | One line |
|---|---|---|
| P0a overlap to part | **STOP condition met by the letter** (1 of 343 hits = 0.29 %, above 0.1 %); the authority's value always wins and the one disagreement is cosmetic as built | needs the lead's decision on how the 0.1 % line is read |
| P0b presentation cost | **inconclusive, no cost signal** | the proxy shows no measurable mean cost; 16 ms cannot be judged on this setup (baseline 44 ms); GC option not measurable here |
| P0c fragment wire format | **PASS** | 16 parts = 19 B payload, 23 B in the list; zero fragments = exactly 1 bit |
| P0d seam audit | **PASS** | D1..D9 all present, no reshaped seam |

## P0a Overlap to part

Setup: listen server (host + 2 clients), 60 ms lag, 15 ms variance, 2 % loss, `HBS_Test_Pawn` replaced in memory (never saved) by six hurtboxes:
`torso` (priority 1), `head` (2), `feet` (0), `chest` (3, overlaps torso and head), `weak` (5, small, inside torso and chest), `shoulder` (2, ties with
`head`, so the lowest index wins). Harness: `Tools/GASNPPTest/parts_scenarios.py` `spike_overlap_part` (not in `SUITE_DEFAULT`), 3.2 s cycles: victim
teleported to a random spot 110 to 170 cm ahead of the attacker (lateral +-35 cm), a random stick held for 1.2 s from the attack tap (the victim keeps
moving), the attack tapped, the hit rows of the authority's copy of the attacker and of the attacker's own client copy read each cycle (the logs keep
512 frames). A hit is matched on (frame, victim, group, hitbox); the compared field is the hurtbox index.

| Attacker | Cycles | Hits on the authority | Matched on the attacker's client | Index disagreements | Missing on either side |
|---|---|---|---|---|---|
| Remote client 1 (run A, partial: the PIE was sent to another map by an unexplained `ServerTravel` after 73 cycles) | 73 | 55 | 55 | 0 | 0 |
| Remote client 1 (run B, partial, same cause after 37 more cycles; stick held for the whole cycle) | 37 | 42 | 42 | 0 | 0 |
| Remote client 1 (run C, final, stick released 1.2 s after the tap) | 120 | 246 | 246 | **1** | 0 |
| Host (listen server) | 50 | 108 | no second computation (see below) | 0 | 0 |
| **Total predicting client** | 230 | **343** | 343 | **1 (0.29 %)** | 0 |

Chosen index histogram of run C: torso 28, chest 71, weak 146, shoulder 1 (head and feet are never reached by the test move at these spots). The
priority rule is deterministic: the weak point wins whenever the hitbox covers it, otherwise the chest.

The one disagreement: frame 2496, group 0, hitbox 0, authority `chest` (3), predicting client `weak` (4). The victim was walking: the client reads the
victim through its own view (interpolated plus the rewound view), the authority through the rewound snapshot, and the 12 cm weak point sits at the edge
of the hitbox in one of the two, which is the expected failure mode and not a bug of the rule.

Findings:

1. **The server value wins in every case.** All 238 victim-inbox rows of run C carry the same hurtbox as the authority's attacker row of the previous
   frame (0 differences, 0 inbox rows without an attacker row). The client's index is used for its own flash and for its prediction only. So no
   disagreement can change the victim's state or the damage as M4 stands today.
2. **It is cosmetic only until the parts plugin uses the client index for prediction.** The plan has the attacker's client predict the ricochet
   from the part's hardness (`IGSTN_AttackerOutcomeExtension`, D5). A wrong index between a hard and a soft part would then predict a recoil that the
   authority does not confirm (or the reverse), a visible correction. At 0.29 % of hits (and only when two overlapping parts differ in hardness) that is
   rare but not zero. The spec's wording ("the expected result is: client may flash a different hurtbox for one frame") covers the flash, not this.
3. **Host as attacker**: the host resolves authoritatively, the remote client does not run the host pawn's attacker step (the host pawn is a proxy
   there) and the victim's client inbox log stays empty, so there is no second computation to disagree: all 100 inbox rows of the host run equal the
   host's own rows. Between host and remote client the index exists on the host only; the remote client learns the outcome through the replicated
   state (for parts: the fragment).
4. The sample is small: 343 hits cannot prove a rate under 0.1 % (that needs about 3000 hits per configuration for a rule-of-three bound). One
   disagreement in 343 is consistent with a true rate between 0.01 % and 1.6 % (95 %).

STOP evaluation: the plan's condition reads "above 0.1 % of hits **or** damage-changing disagreement not resolved by the inbox". Measured 0.29 % (above
by the letter, statistically uncertain); damage-changing: none (the inbox resolves it). Recorded as hit, not acted on. Options for the lead: (a) accept
that the rate is an upper-bound of a cosmetic effect and keep the rule; (b) predict the attacker's ricochet only when all overlapped parts agree on the
band (needs the attacker step to see the candidates, not only the winner); (c) run the 3000-hit campaign (about 3 h of PIE).

Process notes: the PIE went to `RagdollLevel` / `NPCLevel` twice (`Cmd: ServerTravel /Game/Levels/RagdollLevel?listen` in the log) while a victim
walked on a random stick for 2.2 s per cycle; I did not find the trigger (a level transition volume or an input mapping of the sample content
reachable from the harness level is the likely cause), shortening the stick to 1.2 s removed it. The runs before the fix are listed above as partial.

### P0a re-measure with the ricochet prediction (M4b P8, laptop, 2026-10-26)

Same setup (listen host + 2 clients, 60/15/2 %, the six overlapping spike hurtboxes of `HBS_Test_Pawn` in memory, remote client 1 attacks client 2, 2.8 s cycles, stick held 1.2 s), but now with the real prediction: the pawns carry an in-memory part set with one part per hurtbox, hard ones (`head`, `chest`, `shoulder`, hardness 5: the test move's penetration 0 bounces) alternating with soft ones (`torso`, `feet`, `weak`, hardness 0), the rules converted to parts, `FGSTN_PartsAttackerOutcome` registered. So a wrong overlap choice between a hard and a soft neighbour flips the ricochet the attacker predicts. Harness: `scenario_p0a_ricochet` (`parts_batch(['p0a_ricochet'] * 14)`, a fresh PIE per chunk of 130 cycles), numbers from `spike_report()`.

| Cycles | Predicted hits (authority) | Matched on the attacker's client | Index disagreements | Ricochets (authority) | Ricochet-flag disagreements |
|---|---|---|---|---|---|
| 1820 | **3014** | 3014 | **1 (0.033 %)** | 926 | **1** |

Chosen index histogram: torso 490, chest 925, weak 1598, shoulder 1 (head and feet are not reached at these spots). The one disagreement: frame 12347, group 0, hitbox 0, authority `chest` (3, hard: ricochet), the attacker's client `torso` (0, soft: a normal hit with an attacker hitstop of 6) - the same kind of edge case as the one of the first run (a moving victim seen through two views). It DID change the attacker's predicted ricochet, and it shows as the correction the plan expects: in that cycle the attacker's client rolled back twice (the rest of the cycles: see below). The authority's value won (its inbox row carries the ricochet flag and the chest), so no damage or part state was affected; the visible effect is one wrong prediction (a normal hit with a hitstop and no recoil) corrected into the recoil.

Attacker-client rollbacks: 270 over 1820 cycles in 214 cycles. They are not caused by the ricochet: 93 of 926 cycles with a ricochet (10.0 %) against 121 of 894 cycles without (13.5 %); a walking victim and 2 % loss produce them anyway. The recoil of a correctly predicted ricochet did not cost a rollback beyond that baseline.

Reading against the 0.1 % line: 1 in 3014 = 0.033 % (95 % upper bound about 0.18 %, rule of three on a count of one), below the line. The earlier 0.29 % was a small-sample overestimate. Option (b) of the earlier STOP evaluation (predict only when all overlapped parts agree) is not needed.
(`missing on the victim's client` reads 100 % in this run: the harness reads the victim's inbox on the victim's client copy, which only drains on the authority in this configuration; the authority's inbox rows had 0 hurtbox differences against the attacker's rows.)


## P0b Presentation cost

Not run as specified. The two assets the spike needs cannot be authored from the editor Python of this project: a Niagara system with a mesh
renderer (no emitter graph editing from Python; the engine templates are sprite bursts) and a fractured Geometry Collection (the fracture tools are
not exposed). The Python API also has no deferred actor spawn in a PIE world, so the content was placed in the editor level (unsaved) and toggled in PIE.
What was measured is a **proxy**, honestly labelled:

- 40 dummy bodies (cylinders, pawn sized), 48 chunks (cubes, `SimulatePhysics` switched on in one frame, Chaos rigid bodies falling onto the floor),
  600 shards as 600 separate small cube actors (a pessimistic proxy for 600 mesh particles: 600 components and draw calls instead of one instanced
  draw), and 40 Niagara `DirectionalBurst` systems spawned in the same frame (a sprite burst, so it measures the per-system cost, not the mesh cost).
- The listen configuration with two clients runs **three worlds in one process**, so each number below carries the load three times (one real host
  carries it once). Frame time is the wall time between editor ticks, 280 samples per stage, `t.MaxFPS 0`, `r.VSync 0`.

| Stage | Mean | p95 | Max |
|---|---|---|---|
| Baseline (3 PIE viewports plus the editor, nothing placed) | 44.4 ms | 49.6 ms | 54.3 ms |
| + 40 dummy bodies | 46.1 ms | 60.6 ms | 93.6 ms |
| + 48 simulating chunks, 600 shards, 40 Niagara bursts | 40.4 ms | 70.8 ms | **157.3 ms** |

Reading: no mean increase beyond the noise of the baseline (the third mean is below the first: the machine is the noise); the p95 and the maximum rise
with the load, and the 157 ms maximum is the burst frame (spawn of 40 systems, first use of the template and the physics wake-up of 48 bodies in one
frame, the three worlds each doing it). The STOP condition (host frame time above 16 ms) **cannot be evaluated on this setup**: the baseline alone is
44 ms because the editor renders three viewports. What the numbers do say: the steady cost of this budget is under the noise floor, the cost is a **hitch
at the burst frame**, and that frame is exactly what P12 must spread (stagger the spawn over frames, pre-warm the Niagara system and the chunk pool at
level load). The pre-fractured-pieces-versus-GC comparison was not made.

Recommended re-measure: in P12 with the real shatter content, on a standalone listen host (one world, one viewport, `-game`), with `stat unit` and
`stat niagara`; the Geometry Collection option needs an artist-authored asset.

## P0c Fragment wire format

A throwaway fragment (`FGSTN_SyncFragment` subclass, id `GSTNFeatureIds::TestC`, registered in the spike test and removed before the commit) in
two encodings, measured through `FGSTN_SyncFragmentList::NetSerialize` into an `FBitWriter` (the list framing is: 1 flag bit, packed count, 8 bits id,
packed byte length, payload bytes). "Bytes": `uint16` mask, N damage bytes, one serial byte. "Packed": N mask bits, N damage bytes, 4 serial bits, padded
to a byte.

| N | Spec 5.1 raw bits | Payload bytes (bytes / packed) | One fragment in the list, bits (bytes) bytes / packed |
|---|---|---|---|
| 1 | 14 | 4 / 2 | 57 (8) / 41 (6) |
| 6 | 59 | 9 / 8 | 97 (13) / 89 (12) |
| 8 | 77 | 11 / 10 | 113 (15) / 105 (14) |
| 16 | 149 | 19 / 19 | **177 (23)** / 177 (23) |
| 0 fragments | | | **1 bit** (the flag) |
| N = 16 plus a second 5 byte fragment | | | 233 (30); the second adds 56 bits |
| N = 6 packed plus a 5 byte fragment | | | 145 (19) |

Verdicts: a 16-part fragment is 23 bytes in the list (19 payload), under the 24 B line (by 1 byte: a second feature next to it costs 7 B more);
zero fragments cost exactly the flag bit: **PASS**. The spec's table counts the payload only (~7, ~10, ~19 B); the framing adds the type id and the
length (2 B) plus the count (1 B for the first fragment), so the realistic figures are 12 to 23 B, and the "packed" encoding saves at most 1 to 2
bytes (nothing at N = 16), so **use the byte encoding** (simpler, equal at the maximum).

Findings that change the design text:

1. **N cannot be an instance field read by `NetSerialize`.** The registry builds a default fragment from the type id and calls `NetSerialize` on it
   when reading, so the part count of the set is not available there (a first version of the spike lost the fragment on read for exactly this reason).
   The payload length is available (`FArchive::TotalSize()` of the `FMemoryReader`) and determines N unambiguously (bytes: N = length - 3; packed:
   invertible too), so N stays off the wire as the spec wants. The spec's "a set mismatch is refused at connect" stays valid for gameplay, but the
   wire itself is self-describing through the length prefix.
2. `ShouldReconcile` compares serialized forms by default, which is the exact-integer equality the spec asks for.

## P0d Seam audit of M4

All present as merged code in `Plugins/GSTNCombat*` at 4624fa7.

| Seam | Present | Location |
|---|---|---|
| D1 `UGSTN_HitResolutionStep`, `FGSTN_HitResolutionContext`, `FGSTN_HitVictimView` | yes | `GSTNCombatCore/Public/Hits/GSTN_HitResolution.h:175,345`, `GSTN_HitVictimView.h:68`; shipped steps in `GSTN_HitSteps.h` |
| D2 `FGSTN_HitReactionModifiers` in the context and on hurtboxes; `OutcomeExtensions` | yes | `Hits/GSTN_ReactionTypes.h:68`, `GSTN_HitResolution.h:183,222`, `GSTN_HurtboxTypes.h:55` |
| D3 `HurtboxIndex` (inbox, context, events) and `FGSTN_Hurtbox::Priority` | yes | `Hits/GSTN_HurtboxTypes.h:51`, `GSTN_HitResolution.h:225`, `GSTNCombatNP/Public/Simulation/GSTN_HitInbox.h:37`, `GSTNCombatGAS/Public/Events/GSTN_HitEvents.h:38` |
| D4 `IGSTN_OutcomeExtensionApplier` and Blueprintable `UGSTN_OutcomeApplier` | yes | `Hits/GSTN_HitExtensions.h:76,94` |
| D5 `IGSTN_AttackerOutcomeExtension`, `EGSTN_AttackerHitOutcome`, `FGSTN_RicochetResponse` | yes | `Hits/GSTN_HitExtensions.h:146,158`, `Timeline/GSTN_TimelineTypes.h:377`; recoil path in `GSTNCombatNP` (H12) |
| D6 `PercentCapped` (`EGSTN_DamageMode`) | yes | `Rules/GSTN_CombatRules.h:30` (`MaxHealth` = cap, `GSTN_HealthAttributeSet.h`, `GSTN_DamageExecution.h`) |
| D7 hit `ExtensionData` (`TArray<FInstancedStruct>`) and per-field override | yes | `Hits/GSTN_HitData.h:148`, `Hits/GSTN_HitExtensions.h` (`FGSTN_HitExtension` base), kit `ExtensionData` `Rules/GSTN_KitExtension.h:34` |
| D8 tick extension with `HitGroupMask` | yes | `GSTNCombatNP/Public/Simulation/GSTN_CombatTickExtension.h:51,68`, `FGSTN_HitGroupMask` `Hits/GSTN_HitExtensions.h:30`, used by `GSTN_CombatAttacker.h:63` |
| D9 snapshot extension | yes | `GSTNCombatNP/Public/World/GSTN_StepSnapshot.h:22` (`FGSTN_SnapshotExtension`) and `FGSTN_SnapshotEntry::Extensions` (`:73`, `TArray<FInstancedStruct>`) |
| D10 sync fragment registry (M3) | yes | `Sync/GSTN_SyncFragment.h:41,75,115` |

Deviations from the plan's wording, none blocking: the snapshot extension base is a USTRUCT with a virtual destructor and the entry holds
`TArray<FInstancedStruct>` (not a fixed POD slot), so the "20 B per entity in the lag history" budget of P7 needs the history to copy
`FInstancedStruct`s (P7 checks); `FGSTN_HitReactionModifiers::StaggerThreshold` is an `FGSTN_StaggerThreshold` (`Value`, `bInfinite`), as m4-doc-deltas H1 says.

## Merge note (not a spike)

The parallel branch `claude/project-thread-v1plhd` (fetched, never merged) already contains a `Plugins/GSTNParts` with a larger `GSTNPartsCore`
(data, math, validation, tests) written against the pre-M4 tree. The P1 skeleton here and that tree define the same files (uplugin, Build.cs, tags,
settings, tests), so merging the two branches will conflict in `Plugins/GSTNParts` and the `Plugins/README.md` rows. Whoever merges decides which
side wins; the plan's P2 to P3 would otherwise redo the work that tree already holds.

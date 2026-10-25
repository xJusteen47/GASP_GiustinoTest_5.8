# M4b doc deltas (for the final docs commit, P14)

Same use as `m4-doc-deltas.md`: what the code did differently from the M4b plan (`m4b-parts-plan.md`) and the spec (`combat-parts.md`, both on the
docs branch), one entry per checkpoint, folded into the docs in the closing commit. The plan says `Saved/Notes/`; `docs/notes/` is the current rule.

## P0 spikes (report: `m4b-spike-report.md`)

- P0a ran as a harness scenario kept in `Tools/GASNPPTest/parts_scenarios.py` (`spike_overlap_part`, not in `SUITE_DEFAULT`). Measured 1 index
  disagreement in 343 predicted hits (0.29 %, above the plan's 0.1 % line, statistically uncertain at this n); the inbox carried the authority's index
  in every case (238/238), so as built nothing but the attacker's own flash differs. It becomes damaging only if the parts plugin predicts the
  attacker's ricochet from the winner part alone (D5). Spec 3.4 should say so, and say that the host as attacker has no second computation.
- P0b could not be run as specified (no Niagara mesh emitter and no fractured Geometry Collection can be authored from Python; no deferred actor spawn
  in a PIE world). A proxy was measured: no mean cost, a hitch at the burst frame (max 157 ms in three worlds). Spec 6.4 should add "stagger the burst
  and pre-warm the pool"; the 16 ms line is to be re-measured in P12 on a standalone listen host.
- P0c: spec 5.1's table counts the payload only. The list framing adds the type id (1 B), the packed length (1 B) and the packed count (1 B for the
  first fragment): N = 16 is 19 B payload, 23 B in the list; zero fragments are exactly the flag bit. The wire is self-describing through the length, and
  N cannot be an instance field read in `NetSerialize` (the registry reads into a default instance): derive it from `FArchive::TotalSize()`. Use the
  byte encoding (bit packing saves 0 to 2 bytes).
- P0d: D1..D10 all present. The snapshot extension is `TArray<FInstancedStruct>` of a USTRUCT with a virtual destructor (not a fixed POD slot): the
  20 B/entity history budget of P7 has to be checked against that.
- Assumption ids used: A7 (highest Priority wins, ties lowest index; resolved attacker-side), A15 (no host mitigation), A16 (budgets 48/600/2).

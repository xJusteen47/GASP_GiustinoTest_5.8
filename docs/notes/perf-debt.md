# Perf debt

Choices suspected of not being the most performant. Target hardware is very low end; revisit at the end of M8. One line each:
what, where, cheaper alternative.

- X4 lag-comp rewind iterates all minions: `GSTNLagComp::BuildOverlay` samples every provider entry per rewinding attacker (O(N)); the cheap
  `GatherRewoundCandidates` query exists but is not wired into GSTNCombatNP. Alternative: provider hook in BuildOverlay using the hash + swept bound.
- Pooled SkeletalMesh actors for minion visuals (`AGSTN_MinionActor`): alternative, instanced/vertex-animated meshes beyond a distance tier.
- Full per-minion net state each frame, no delta (`GSTNMinion` replication, G6 packs): alternative, delta/quantised packs with dormancy.
- History ring copies full arrays per tick (`GSTN_MinionHistory`, world subsystem ring): alternative, write only moved entities / ring of deltas.
- G5 `EnsureSnapshot(F)` from the post-tick hook builds S(F) with all minion entries even when no fighter ticked: skip when no fighter is registered.
- G5 drain cost 8.9 us per hit (spike 5): victim entry and view rebuilt per hit; batch per victim and reuse the view.
- Bench Attack tick (`GSTN_MinionBench.cpp`): `RequestGameplayTag` and a combat-world lookup every world tick; cache both (test tooling only).
- Harness: scenarios wait fixed wall-clock seconds; poll a condition instead (test time only).

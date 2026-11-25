# GSTN World Sim (rollback plan F0 spike)

A world entity simulated by Network Prediction next to the pawns: no player, no owning connection, forward predicted on every client
(its driver allows only `ForwardPredict` for simulated proxies, so the project's `SimulatedProxyNetworkLOD` does not apply to it). It is
the base the rollback plan builds held objects, projectiles, KO and lives, and missions on
(`docs/architecture/tag-verbi-simulazione-rollback.md`).

The spike measures what a re-simulation of world state costs per frame. The tick does synthetic work sized by the authority:

| Console | Meaning |
|---|---|
| `gstn.WorldSim.Spawn 1` | spawn the entity on the authority at the next world begin play (off by default) |
| `gstn.WorldSim.Entities N` | fake entities updated per tick, vector math only (CPU part of a minion) |
| `gstn.WorldSim.Traces N` | line traces per tick on static geometry (level-query part of a minion) |
| `gstn.WorldSim.Report` | log ticks, re-simulated ticks, average cost of each, worst game frame, for every world sim of the process |
| `gstn.WorldSim.ResetStats` | start a new measurement |
| `gstn.WorldSim.LogEverySeconds S` | log the report every S seconds (for the harness) |

The load travels in the model's input, so every machine simulates the same work; the sync state folds every result into an accumulator,
so a machine that computed anything differently is reconciled. Force corrections with `np.ForceReconcile` to measure the worst case.

Allowed edges: engine and NetworkPrediction only. Enabled in the project, inert unless `gstn.WorldSim.Spawn` is 1.

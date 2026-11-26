# Handover rollback-f1-f0-1: input handshake (F1), world sim spike (F0), query facade (F0b) (2026-11-26)

Branch `claude/project-thread-n7diie` from `main` 152d25e6. Thread "Portare tutto il gameplay nel rollback". Plan doc (owner's choice B,
all gameplay in rollback): https://claude.ai/artifact/SfrTpktgxySeSCUc2gWkPp. Desktop worktree `GASP_Giustino2_n7diie` (never `_ue59`).

## What
- F1 (71e91dc6, 0d7c94e8): `FGSTN_InputProfile` (device, button overrides of remappable functions, command picks among the layout's
  `CommandChoices`) travels as loadout extension `GSTN.InputProfile` (`ServerSetInputProfile`, new revision); the compiled binding table
  applies it (`FGSTN_InputSlotSpec::ApplyPlayerInput`). Device detection in `UGSTN_InputOptionsSubsystem`. Wall jump / wall detach read
  `FGSTN_MoverButtonInputs` written from the input command (no more GASP jump bit, crouch-intent read or debug globals in the sim).
- F0 (68073317, 7b9f3710): plugin `GSTNWorldSim`, NP model `GSTN_WorldSim` (priority PreKinematicMovers-5) forced to ForwardPredict on
  simulated proxies by its driver's capabilities. Synthetic load `gstn.WorldSim.Entities/Traces/Parallel`, `gstn.WorldSim.Report`.
  Inert unless `gstn.WorldSim.Spawn 1`.
- F0b (f7fbb6f8): `FGSTN_SimQuery` (GSTNCore) for every simulation collision query; `gstn.SimQuery.Hash 1`, `gstn.SimQuery.Dump N`.
  Mover's internal sweeps not routed yet.
- First desktop build on f7fbb6f8: 2 errors, both of this branch, fixed by the RC session (f6c42a98 unity name clash, 17e6961c
  TestEqual on UScriptStruct*). No errors outside the branch.

## Next (desktop, one build at a time, respect IN-USO.txt)
1. Pull 17e6961c in the worktree, incremental `Run-Timed.ps1 -Preset Build`.
2. Tests: `GSTNInput.Core.Profile.*`, `GSTNCombat.Core.Data.InputProfileNetSerialize`, then all `GSTNInput.*`, `GSTNCombat.*`.
3. Harness (host + 2 clients), `gstn.WorldSim.Spawn 1`, `gstn.SimQuery.Hash 1`: loads 0/0, 8/8, 40/40, 40/40 parallel; report avg tick,
   avg resim tick, worst frame, acc match, SimQuery hash match (first mismatching frame).
4. Then F8 (input-only replay desktop vs laptop), rename comando/comando virtuale after #53/#54 merge, F2 after #53.

## Owner decisions in this thread
Minions C + B (F9); per-match switch Rollback completo / Scalato; physics 1A (facade first, Jolt only if Chaos diverges).

ESITO: PARZIALE (F1, F0, F0b written; first build fixed; tests and F0 measures not run yet)
SUBAGENTI: 0
COSTO: ~20 $ (estimate, cloud thread + RC build)

# Calibration (before trusting verdicts)

Run the skill once on each of these, from the CP2 composite (`animpipe review --visual`), and compare. Max 2 edits of
`rubric.md` to make all three hold; if they still do not, stop and write what is ambiguous.

| Move | Card | Expected esito | Must find / must not find |
|---|---|---|---|
| jab v12 (accepted by owner) | `cards/pilot/jab_v12.json` | `ok` | no `blocca`; fists closed; segments all `ok` |
| ftilt v3.1 (accepted, snap 9.8/10) | `cards/overlord/overlord_ftilt_v31.json` | `ok` | no `blocca`; at most a `nota` on niente_scatto (thin margin) |
| dair v2 (rejected by owner) | `cards/overlord/overlord_dair_v2.json` | `correggi` | `sinergia` blocca (arms fixed on the chest f6-12); `niente_scatto` blocca (head pops after active); `compenetrazione` blocca (forearm in the head) |

Record the result of each run here (date, esito, misses), one line per move. Not yet run: needs the CP2 composite
(desktop). The verdicts in this table are what the owner already said about these versions.

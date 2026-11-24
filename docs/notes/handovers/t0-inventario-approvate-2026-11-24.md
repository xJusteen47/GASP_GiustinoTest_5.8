# T0 inventario clip approvate (2026-11-24)

Scopo: inventario delle animazioni approvate dall'owner, verifica asset e tag. Nessuna approvazione nuova inventata.

## Blocchi bloccati (docs/notes/anim-approvate.md + tag approved/*)
| mossa | versione | asset (tracked) | tag | commit tag | stato |
|---|---|---|---|---|---|
| run start | generic w1 | AS_GSTN_RunStart_Generic_w1 | approved/runstart-w1 | 1c1c4ab0 | OK, bloccata (owner 2026-11-21 14:35Z) |
| run start | generic w2 | AS_GSTN_RunStart_Generic_w2 | approved/runstart-w2 | 5e585aa8 | OK, bloccata (owner 2026-11-21 14:57Z) |
| run start | v4r5 | AS_GSTN_RunStart_v4r5 | approved/runstart-v4r5 | 9fe083bd | OK, bloccata (owner 2026-11-20 20:17Z) |
| run start | v4r7 | AS_GSTN_RunStart_v4r7 | approved/runstart-v4r7 | 83e6f1f3 | OK, bloccata (owner 2026-11-20) |

Verifica: `git rev-list -n1 approved/<nome>` e `git ls-files Content/GSTN/Animation/RunStart/`.

## Da confermare per l'owner (nessuna riga di lock, nessun tag)
- **run start w3**: asset tracked (AS_GSTN_RunStart_Generic_w3), commit 967de748 ha solo passo-03 "PASS" dei gate (esito.json, GATES: OK), nessuna nota di approvazione owner. NON approvata.
- **jab v12**: card presente (Tools/AnimPipeline/cards/pilot/jab_v12.json), nessuna riga in anim-approvate.md, nessun tag.
- **ftilt v3.1**: card presente (Tools/AnimPipeline/cards/overlord/ftilt/overlord_ftilt_v31.json), nessuna riga, nessun tag.
- **dair**: asset AM_OverlordDAirV7..V10 e KIT_Test_OverlordDAirV7..V10R4 in Content/GASNPPTest/Combat, nessuna riga, nessun tag. Quale versione sia quella da bloccare non è scritto da nessuna parte.

Azione: l'owner decide quali di queste bloccare. Non ho creato tag o righe.

## Note operative
- Tag: i tag approved/* NON sono stati creati né pushati (il guard blocca i tag). Comandi per l'owner, solo se conferma, sulla versione giusta:
  `git tag approved/runstart-w3-<ver> <commit> ; git push origin approved/runstart-w3-<ver>`
- Worktree _ue59 non toccato in commit. Modifiche locali dell'owner (DA_Locomotion_Default.uasset, pie_capture_move.py, altri .uasset RunStart) NON committate.
- Il worktree ha file untracked di altre sessioni (Mixamo/, docs/notes/reviews/runstart-*): non inclusi.

SUBAGENTI: 0 (lavoro fatto in questo thread)
COSTO: 0.30 $ | 6 min desktop | 0 token review

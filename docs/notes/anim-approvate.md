| run start | v4r5 | AS_GSTN_RunStart_v4r5 (Saved/AnimPipeline/runstart/RunStart_v4r5.casc) | owner 2026-11-20 20:17Z: versione di sicurezza, nuova variante piu' alzata | approved/runstart-v4r5 |
| run start | v4r7 | AS_GSTN_RunStart_v4r7 (Saved/AnimPipeline/runstart/RunStart_v4r7.casc) | owner 2026-11-20 (approvata, bloccata; v4r8 e' variante) | approved/runstart-v4r7 |
| run start | generic w1 | AS_GSTN_RunStart_Generic_w1 (Saved/AnimPipeline/runstart/RunStart_Generic_w1.casc), rows Variant 20 / 30 (Handoff) | owner 2026-11-21 14:35Z: "Buona mi piace. Nuovo standard" (bloccata) | approved/runstart-w1 |
| run start | generic w2 | AS_GSTN_RunStart_Generic_w2 (Saved/AnimPipeline/runstart/RunStart_Generic_w2.casc), rows Variant 21 / 31 (Handoff) | owner 2026-11-21 14:57Z: "La w2 va bene, nuovo default" (bloccata; le inversioni B/LL/RL sono righe sorelle nuove sullo stesso valore) | approved/runstart-w2 |
| dropkick (calcio) | K7 | AS_OverlordDropkick_StartupK7 + AM_OverlordDropkickK7 (carta cards/overlord/dropkick/overlord_dropkick_startup_k7.json, settle/loop K6; commit ac8d6201) | owner 2026-11-23 21:00Z: "approva e parti con il k8" (bloccata; K8 e' variante) | approved/dropkick-k7 (tag da creare su ac8d6201) |
| run start | generic w3 (29w3) | AS_GSTN_RunStart_Generic_w3 (branch claude/approved-runstart-v29w3, commit 967de748; istruzioni docs/notes/reviews/runstart-w3-c1/c2) | owner 2026-11-23 10:08Z (29w3), confermata 2026-11-24 al coordinatore (T3) | approved/runstart-w3 (tag da creare su 967de748) |
| jab | v12 | AS_JabV12_Build (/Game/AnimPipeline/Rig/Pilot/Jab/, asset NON tracciato in git: solo copia locale del fisso) carta cards/pilot/jab_v12.json | owner 2026-11-24 al coordinatore (T3): approvata = bloccata, non rifinita; FAIL vero di fermata (statua f3-9, coda morta f18-37, life_calibration.json) | approved/jab-v12 (tag da creare sul commit di questa riga) |
| ftilt (F.L) | v3.1 | AS_GanonFTiltV31_Build (/Game/AnimPipeline/Rig/Overlord/FTilt/, asset NON tracciato in git) carta cards/overlord/ftilt/overlord_ftilt_v31.json; tenuta viva del calcio f10-15 dichiarata | owner 2026-11-24 al coordinatore (T3) | approved/ftilt-v31 (tag da creare sul commit di questa riga) |

## Esenzioni dei controlli
Solo l'owner (INV-10, `Tools/AnimPipeline/contract/invarianti-review.md`): un blocco di `animpipe check` che fallisce anche
sulla base approvata blocca la variante, salvo una riga qui con le parole dell'owner. Una riga vale per quella base, quel
blocco e quella riga esatta (frame e numeri): se la variante li cambia, blocca di nuovo. `stop_and_go` e `hold_reason` mai.
| base | blocco | riga esatta di animpipe check | owner (data e parole tra virgolette) |
|---|---|---|---|

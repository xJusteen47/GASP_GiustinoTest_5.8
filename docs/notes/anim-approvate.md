# Clip approvate e bloccate (vista generata)

<!-- GENERATA da `python -m animpipe registro view` da Tools/AnimPipeline/registro-clip.csv: non modificarla a mano
(owner 3A, 2026-11-24). Solo la sezione Esenzioni in fondo si scrive a mano, con le parole dell'owner. -->
approvata = parole dell'owner + tag `approved/<mossa>-<versione>`; bloccata = nessuno la sovrascrive, ma non e' approvata
(niente tag). Si lavora sempre su una variante nuova. Stato di tutte le altre versioni: il registro.

| mossa | versione | stato | asset UE (carta; commit) | owner | tag |
|---|---|---|---|---|---|
| runstart | v4r5 | approvata | AS_GSTN_RunStart_v4r5 (Saved/AnimPipeline/runstart/RunStart_v4r5.casc; commit af857cff) | owner 2026-11-20 20:17Z: versione di sicurezza, nuova variante piu' alzata (branch claude/approved-runstart-v4r5 (9fe083bd) da eliminare: il tag esiste) | approved/runstart-v4r5 |
| runstart | v4r7 | approvata | AS_GSTN_RunStart_v4r7 (Saved/AnimPipeline/runstart/RunStart_v4r7.casc; commit 83e6f1f3) | owner 2026-11-20 (approvata, bloccata; v4r8 e' variante) | approved/runstart-v4r7 |
| runstart | w1 | approvata | AS_GSTN_RunStart_Generic_w1 (Saved/AnimPipeline/runstart/RunStart_Generic_w1.casc; commit 1c1c4ab0) | owner 2026-11-21 14:35Z: "Buona mi piace. Nuovo standard" (rows Variant 20 / 30 (Handoff)) | approved/runstart-w1 |
| runstart | w2 | approvata | AS_GSTN_RunStart_Generic_w2 (Saved/AnimPipeline/runstart/RunStart_Generic_w2.casc; commit 5e585aa8) | owner 2026-11-21 14:57Z: "La w2 va bene, nuovo default" (rows Variant 21 / 31 (Handoff); le inversioni B/LL/RL sono righe sorelle nuove sullo stesso valore) | approved/runstart-w2 |
| runstart | w3 | approvata | AS_GSTN_RunStart_Generic_w3 (docs/notes/reviews/runstart-w3-c1, runstart-w3-c2 (istruzioni); commit 967de748) | owner 2026-11-23 10:08Z (29w3), confermata 2026-11-24 al coordinatore (T3) (run start GENERICO; branch claude/approved-runstart-v29w3 (967de748) da eliminare: il tag esiste) | approved/runstart-w3 |
| dropkick | k7 | bloccata | AS_OverlordDropkick_StartupK7 + AM_OverlordDropkickK7 (cards/overlord/dropkick/overlord_dropkick_startup_k7.json (settle/loop K6); commit ac8d6201) | owner 2026-11-23 21:00Z: "approva e parti con il k8" (NON approvata: owner 2026-11-24 niente tag (stop-and-go, T3). Resta il caso rovinato da confrontare; K8 e' variante) | nessun tag |
| jab | v12 | approvata | AS_JabV12_Build (cards/pilot/jab_v12.json; commit c699d46a) | owner 2026-11-24 al coordinatore (T3): approvata = bloccata, non rifinita (asset NON in git (solo /Game/AnimPipeline/Rig/Pilot/Jab/ del fisso): publish da C4. FAIL vero di fermata (statua f3-9, coda morta f18-37, life_calibration.json)) | approved/jab-v12 |
| ftilt | v31 | approvata | AS_GanonFTiltV31_Build (cards/overlord/ftilt/overlord_ftilt_v31.json; commit c699d46a) | owner 2026-11-24 al coordinatore (T3) (F.L Overlord = ftilt v3.1; asset NON in git (solo /Game/AnimPipeline/Rig/Overlord/FTilt/ del fisso): publish da C4; tenuta viva del calcio f10-15 dichiarata) | approved/ftilt-v31 |
| dair | v9 | approvata | AS_OverlordDAirV9 + AM_OverlordDAirV9 (cards/overlord/dair/overlord_dair_v9.json (build 0f0bfe96); commit 916338ad) | owner 2026-11-24 01:26Z: "dovrebbe essere v9 la dair giusta" (AM con la modifica locale dell'owner (11-17 23:08) preservata in 916338ad; KIT_Test_OverlordDAirV9 / GA / TL / WPN in Content/GASNPPTest/Combat. Review livello 2 cieca 11-24: FAIL vita e fermate a17-26 (la v9 resta approvata; il miglioramento e' una variante nuova)) | approved/dair-v9 |

## Esenzioni dei controlli
Solo l'owner (INV-10, `Tools/AnimPipeline/contract/invarianti-review.md`): un blocco di `animpipe check` che fallisce anche
sulla base approvata blocca la variante, salvo una riga qui con le parole dell'owner. Una riga vale per quella base, quel
blocco e quella riga esatta (frame e numeri): se la variante li cambia, blocca di nuovo. `stop_and_go` e `hold_reason` mai.
| base | blocco | riga esatta di animpipe check | owner (data e parole tra virgolette) |
|---|---|---|---|

# Review: jab 1, jab 2, dropkick r1 — FAIL / FAIL / FAIL (cattura non valida) — 2026-11-15
Regola 10 + 13, cloud, sola lettura. Catture `base/` e `variant/` di questo folder (branch desktop c9c860f5).
Griglie: `rv_*.jpg` (righe: base az90, base az180, variante az90, variante az180; stessi frame v).

## Dropkick — FAIL, catena di cattura NON valida (dati montage / build)
- La variante è IDENTICA alla base r1: gambe aperte, braccia lanciate, v46-v90 (`rv_dropkick_identity.jpg`). Il ripristino r0 non è in gioco.
- Causa (verificata con `git diff --stat 36dcdb44 c9c860f5 -- Content`): la build ha scritto `Content/AnimPipeline/Rig/Overlord/Dropkick/{AM,Startup,Loop,Land}`
  (file nuovi), ma il gioco suona `Content/GASNPPTest/Combat/Overlord/AS_OverlordDropkick_*`, NON toccati. Per i jab è stato fatto il passo
  `_Build` -> consolidate -> rename; per il dropkick no.
- Identità (gambe unite e dritte, braccia conserte): non giudicabile. Penetrazione braccia: non giudicabile.
- Serve (fisso, economico): pubblicare le 3 clip nel percorso di gioco come per i jab, ricatturare solo `dropkick_whiff`, nuova review.

## Jab 1 — posa OK, FAIL solo per `gioco`
Catena valida: la variante differisce dalla base da v24, etichette m/a avanzano ogni frame.
- Identità: 1 schiaffo da sinistra a destra SÌ (v26 mano alla mascella sinistra, v27 contatto, v29 braccio fuori a destra, az180);
  2 mano sinistra sul fianco SÌ (v27-v34); 3 arco completo SÌ: pixel cambiati az180 per frame v27-v31 = 4448, 2465, 2608, 1783, 836,
  poi fermo v32-v34 (120, 32, 27). Base: morta v28-v33 (168..27). La tenuta ora è a fine arco e dura 3 frame.
- Confronto con base: MEGLIO. Proposta `good/jab1-1` = card a19e16a1 (tag da creare sul fisso).
- Limite `posa` (minore, non blocca): orientamento dorso/palmo non convertito (`hand_rot`), non leggibile a 360 px.
- `gioco`: accovacciata a fine montage ancora presente (v90, `rv_jab_end.jpg`), uguale alla base. Già in coda codice.

## Jab 2 — FAIL (posa + da confermare con l'owner)
- Identità 1 (palmo verso sinistra): direzione SÌ (v40-v44 il braccio passa da destra a sinistra, `rv_jab2_contact.jpg`).
- Scarto 1, `posa` (intercalari del thread, f0-f7): nessun caricamento. Da jab 1 a O17 il corpo ruota in modo uniforme v34-v40
  (yaw bacino 0 -> -114° lineare nel piano), senza anticipo: si legge come una giravolta, non come un colpo.
- Scarto 2, da confermare: le chiavi dell'owner in `owner_r3/overlord_jab2_r3.plan.json` hanno yaw bacino -143° al contatto (f9) e
  -155° con bacino -26 cm a O21 (f11). In gioco v43-v49 il personaggio è accovacciato e quasi di spalle al bersaglio per 6 frame
  (`rv_jab2_follow.jpg`). Il follow-through del thread aggiunge solo 4° e 4 cm: la posa viene dalle chiavi. O è voluta, o è un errore
  di conversione (`casc_to_author.py`, ratios 1.0): va confrontata con la scena Cascadeur sul fisso prima di altri giri.
- Scarto 3, da confermare: nessun passo avanti visibile (scheda: 15-20 cm).
- Confronto con base: diversa dal jab 1 da dietro (bene), ma peggio come lettura del colpo.
- `gioco`: stessa accovacciata finale.

## Da confermare (per il coordinatore -> owner)
1. Jab 2: la fine di spalle e accovacciata (yaw -155°, -26 cm) è la posa voluta? (prima controllo conversione sul fisso)
2. Jab 2: passo avanti 15-20 cm ancora voluto con le nuove chiavi?
3. Jab 1: mano sul fianco, 29°, pestone (domanda 3 già aperta).
Hitbox e frame attivi: non giudicati (owner). Check numerici `checks.txt`: non rifatti.

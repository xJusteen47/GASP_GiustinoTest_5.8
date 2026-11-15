# Handover: review regola 10/13 di jab-dropkick r1 (cloud, Fable high, 2026-11-15)
Branch `claude/project-thread-h5ke0e` (da `claude/desktop-jab-dropkick-r1` c9c860f5), solo docs. Review: `docs/notes/reviews/jab-dropkick-r1/review.md` + `rv_*.jpg`.

## Esito
- Dropkick: FAIL, cattura non valida. La variante in video è la r1 (gambe aperte). `git diff --stat 36dcdb44 c9c860f5 -- Content`
  mostra clip nuove solo in `Content/AnimPipeline/Rig/Overlord/Dropkick/`; il percorso di gioco `Content/GASNPPTest/Combat/Overlord/AS_OverlordDropkick_*` è invariato.
  Etichetta: dati montage/build. Non conta come giro di posa.
- Jab 1: posa OK e meglio della base (arco completo: conteggio pixel per frame nella review, script inline ffmpeg+numpy su az180);
  FAIL solo `gioco` (accovacciata finale v90, già in coda). Proposta `good/jab1-1` = card a19e16a1.
- Jab 2: FAIL. `posa`: nessun caricamento v34-v40. Da confermare: chiavi owner con yaw bacino -143°/-155° e -26 cm
  (`owner_r3/overlord_jab2_r3.plan.json`, chiavi 9 e 11): voluta o errore di `casc_to_author.py`?

## Prossimo (consigliato, applicabile senza owner)
1. Fisso: pubblicare le clip dropkick nel percorso di gioco (stesso passo dei jab), ricatturare `dropkick_whiff`, review nuova (~$8-10 + $5).
2. Fisso, stesso thread: aprire la scena Cascadeur dell'owner e confrontare O19/O21 con la clip in gioco (anteprima affiancata).
3. Solo dopo il punto 2: giro 2 di posa del jab 2 (caricamento f0-f7).
## Per l'owner (via coordinatore)
Jab 2 fine di spalle e accovacciata: voluta? Passo 15-20 cm ancora valido? Jab 1 domanda 3 (mano sul fianco, 29°, pestone).
## Non fatto
Nessuna clip per l'owner (nessun PASS). Primo frame e FAF non guardati a fondo sul jab 2. Check numerici non rifatti.
**Costo** stimato ~$6 (budget $8).

## Seconda review, solo dropkick (16:30Z, desktop 07b0ab3a) — PASS posa
`docs/notes/reviews/jab-dropkick-r1/review-dropkick-2.md` + `rv2_*.jpg`. Tre tratti d'identità SÌ, meglio della base r1, compenetrazione non visibile a occhio.
Resta `gioco` già noto: salto di mezzo metro v102->v103 (capsula, domanda 5), nessuna rialzata v126->v133 (domanda 4). Proposta `good/dropkick-2` (tag sul fisso).
Clip owner in project files `reviews/jab-dropkick-r1/dropkick_r1_owner{,_quarter}.mp4` (ffmpeg crop az90|az45|game). Costo stimato ~$3 (totale thread ~$9).

# Handover HIT-REACTIONS-1: additive di colpo direzionali (fisso, _ue59, 2026-11-18)
Branch `claude/project-thread-98dml5`. Opus + review Fable (subagente). Lock `_ue59/IN-USO.txt` preso/liberato a ogni passo (vedi sotto l'errore iniziale).
## Fatto
- 8 clip additive torso (terra/aria x fronte/retro/sinistra/destra) in `Content/GSTN/Animation/Reaction/HitAdditive/AS_GSTN_HitAdd_*`,
  generate da numeri con `Tools/GAS/build_hit_additives.py` (60 fps, 41 frame = 0,667 s; scatto in 3 frame, picco dentro l'hitstop a 0,07 s,
  molla smorzata con overshoot, testa/collo/braccia in ritardo). Log `Saved/Reaction/build_hit_additives.log` (righe "head moves": verso giusto).
- Copia variante `Content/GSTN/Animation/Reaction/DA_ReactionAnims_HitAdd` (`Tools/GAS/set_hit_additives_only.py`). `DA_ReactionAnims` NON toccato
  (lo modifica l'owner): il gioco usa ancora i segnaposto finché l'owner non approva e scambia il set.
- Cattura: `pie_capture_react_ingame.py` accetta `PIE_CAPTURE_REACTION_SET` (set solo in memoria sull'host); boot VD2 `react_ingame_boot_vd2.py`.
## Review Fable (rule 10), file in `docs/notes/reviews/hitadd-v2..v4/REVIEW-fable.md`
- v2 FAIL su tutto; v3 Front PASS, Right PASS, Left FAIL; v4 (giro 3) Right PASS, Air front PASS, Left PASS (cattura a colpo singolo `left_single_*`).
- Video: `hitadd-v3/front_v3.mp4`, `hitadd-v4/right_v4.mp4`, `left_single_v4.mp4`, `air_front_v4.mp4`.
- Front visto in v3: in v4 è cambiato solo lo smorzamento dei gomiti (k*0,75 -> k*1,0), non ricatturato.
- NON visti (4 di 8): Ground_Back (la sequenza back_hitstand non colpisce mai: 4 catture su 4), Air_Back, Air_Left, Air_Right.
## Da confermare (owner, regola 13d)
1. Scambiare DA_ReactionAnims con la variante (o copiare le 8 clip nei suoi slot)?
2. Posa base AS_React_HitStand: braccio teso orizzontale verso l'attaccante e avambraccio alla spalla per ~20 frame dopo il ritorno
   (Fable: abbassare ~25° o piegare il gomito 30°). È una clip esistente: non toccata.
3. Collo molto esteso in aria: Air amp 1,35 -> 1,15 se troppo.
## Coda codice ("gioco", non animazione)
Additivo che non riparte su un secondo colpo in hitstun; uscita brusca dall'hitstun (v069-071) e pop v058->059; taglio secco all'ingresso
Tumble; nessuna traccia bacino (la maschera del nodo parte da spine_01) e nessuna vibrazione in hitstop visibile; sequenza back_hitstand.
## Errore di processo
Ho controllato `IN-USO.txt` alla radice invece di `_ue59/IN-USO.txt`: 6 commandlet brevi e 2 catture (~15:35-15:50 locali) mentre il lock era del
thread toolset AnimBP. Segnalato al coordinatore. Resta un file `IN-USO.txt` alla radice con scritto "NON E' UN LOCK" (cancellazione negata in auto mode).
## Prove
Build/catture: `Saved/Logs/ci/hitadd_*.log(.esito)`, tutte ESITO OK. Hit per sequenza: `capture.log` in `Saved/AnimReact/hitadd_v3|v3b|v4|v4b` (`done:` con `'hit': True`).
## Costo
Stima 25-35 $ (Opus ~150k contesto, 4 giri di review Fable ~160k, 9 commandlet, 6 catture).

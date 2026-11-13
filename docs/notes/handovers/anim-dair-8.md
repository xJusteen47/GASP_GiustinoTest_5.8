# Handover: down-air dell'Overlord v8, parte senza editor (ANIM-DAIR-8, 2026-11-12)
Branch locale `claude/desktop-anim-dair-8` (= jab-4 `b46421c8` + merge di `experiment/3d-silhouette-anim`), su origin come
`claude/project-thread-lwhro8`. Cartella: il checkout principale `GASP_Giustino2` (m7 era occupato dal jab-5). **Prima c'era
`feature/gas-npp-5.8` (92e9b029): l'owner ci torna con `git checkout feature/gas-npp-5.8`.** Non toccati: `stash@{0}`,
`NewBlueprint1/2.uasset`, mappe, `_feature`.
## Stato: NON pronto per l'owner. Nessuno ha ancora visto la v8 (né build né video). Verdetto regola 10: nessuno.
Fatto: card v8, estensione delle curve, C++ compilato in questa cartella, script del kit e della cattura (mai eseguiti).
Da fare nel prossimo thread (serve l'editor; ora girano quello dell'owner su `_feature` e m11v, poi tocca a jab-5).
## Card `Tools/AnimPipeline/cards/overlord/dair/overlord_dair_v8.json` (handoff `dair-v7-rereview.md`, decisioni owner 09:09Z)
- 15: chiave Cascadeur ruotata dell'owner (v4) spinta ~25%: spine [34,40,0], pelvis rot [11,-9,11], head [-18,-28,-6], gomiti
  fuori dal profilo, punte -45/-38, ginocchio destro sfasato (piede 5 cm più alto).
- Colpo in 2 frame: `release_start` 14→13, breakdown a 14 (gambe, bacino, busto; braccia e testa passano), piedi z -37/-56/-82.
- Peso: partenza lenta 10→13 (`ease_from`), arresto secco a 15, `part_delays` 0. Dopo: un overshoot di testa e mani a 17,
  rimbalzo piedi +3,5 cm a 18, tenuta in movimento 21→26 (il corpo affonda 3 cm sui piedi). `hold_end` 31→26.
- Recovery in sequenza con `drop`: bacino/gambe 26, busto 28, braccia 30, testa 32; 37 e 42 dell'owner invariate; arrivo
  morbido a 42 (`ease_into`). Carica 3→8→10 continua (8 stretta: busto +6°, mani +5 cm, bacino +2 cm). Frame data invariata.
- `animpipe/ue_rig.py`, `card.py`: due profili a un lato (`ease_from`, `ease_into`), così una key non ferma tutto il corpo.
- Scelte mie da verificare a video: quanto è spinta la 15; la 8 ritoccata (le gambe erano già al limite di piega); niente
  key a 39 per "80% entro f39" (avrebbe accelerato dopo la 37): misurare la velocità f41→42 dopo il build.
## Prossimo thread, in ordine (dettagli e ipotesi dello script nel rapporto qui sotto)
1. Da `Tools/AnimPipeline`: `python -m animpipe author|build|check cards/overlord/dair/overlord_dair_v8.json`, poi griglia pose.
   `sheets/raw` copiato da m7 (non in git). `fidelity`/lock: le key 8, 13, 15, 26 hanno `source: null` + `origin` (ok owner).
2. `Tools/GAS/setup_overlord_dair_kit.py` headless con `OVERLORD_DAIR_ANIM=<V8>,<V7>` → `KIT_Test_OverlordDAirV8/V7`.
3. `Tools/GASNPPTest/pie_capture_move.py` (sequenze `dair_whiff`, `dair_land`, `dair_hit`) per V8 e V7, uscita in
   `docs/notes/reviews/overlord-dair-v8/`; poi revisione frame per frame contro i due doc di teoria, PASS/FAIL per punto.
- Script scritti da un subagente Sonnet, solo `py_compile`: binding `Command.Attack.CrouchNeutral` + `Input.Context.Airborne`
  (giù in aria fa anche FastFall), hitbox sfera sul piede campionata dall'animazione, altezze del salto stimate (140/55 cm).
- **PROVVISORI, decide l'owner:** danno 11, knockback 450, angolo 75, raggio 40 cm, hitstop 7/5.
- In gioco (dopo i passi 1-2): `L_Test_Hub`, kit `KIT_Test_OverlordDAirV8`, salto poi giù + attacco in aria.
- Build C++ con un editor aperto altrove: `Build.bat ... -NoHotReloadFromIDE` (il blocco "Live Coding" è condiviso tra progetti).
Tentativi su blocchi: 1 (build). Costo stimato: ~160k token di contesto Fable + 136k di un subagente Sonnet.

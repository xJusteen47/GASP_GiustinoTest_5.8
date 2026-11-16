# Handover: down-air dell'Overlord v9, ultimo giro di pose; resta un difetto lato gioco (ANIM-DAIR-10, 2026-11-13)
Branch locale `claude/desktop-anim-dair-8` nel checkout principale, su origin come `claude/project-thread-se53y5`. Niente C++ di gioco, nessuna
mappa salvata, `stash@{0}` e `NewBlueprint1/2.uasset` non toccati. Editor rilasciato. Catture fatte su 5.8, prima di combat-sync-1 e del passaggio a 5.9.
## Verdetto: FAIL di misura. Le pose passano da dietro e di lato; un difetto lato gioco si nota. Revisione: `docs/notes/reviews/overlord-dair-v9/review.md`
(11 griglie v8 sopra / v9 sotto, 4 mp4). **Non mostrare all'owner come pronta.** Giri di pose usati: 1 su 2; l'owner ha chiesto che sia l'ultimo.
- Dopo l'impatto (a16-a27): PASS. Non è più un fermo immagine (da dietro 830 px cambiati per frame contro 328 della v8, di lato 927 contro 465).
  Il rimbalzo dei piedi si vede anche in camera di gioco (circa 10 px), una sola oscillazione, poi il corpo affonda: pesante, non nervoso.
- Fine tenuta (a24-a28): PASS, nessun frame morto. Carica, colpo in 2 frame, frame di hitstop (chiave a15 pulita), sagoma di lato: come v8, nessun regresso.
- Recovery: la clip ora rallenta dentro il blend-out, ma nel caso a vuoto a **f33→f34 le gambe passano da raccolte dietro a seduta avanti in un frame**
  (4769 px di lato). Succede due frame dopo l'inizio del blend-out (in v8, blend-out di 4 frame, succedeva a f38-39): non è nelle chiavi.
## Cosa è stato fatto sulla v9 (card nuova `cards/overlord/dair/overlord_dair_v9.json`; chiavi 0, 3, 13, 14, 15, 37, 42 intatte)
- Rimbalzo: rigido fino a 17, piede sinistro +9,6 cm a 19, destro +8,5 cm a 20, ginocchia che cedono avanti e in fuori, giù entro 23.
- Tenuta in movimento 17→26: bacino -9 cm, piedi che premono altri 4 cm, arco che si chiude di 10°; inversione sfalsata: bacino 26, busto 28, braccia 30, testa 32.
- Carica 8→10: palla più stretta del 22% (ginocchia e gomiti 7-8 cm verso il centro, testa più bassa). Recovery: ogni parte arriva morbida sulla 37 (87% entro a34).
  `animpipe check`: stessi tre bloccanti della v8 (`legibility`, `source_fidelity`, `co_motion`); `clearance` e `hitstop_snap` verdi.
## Pipeline e kit
- **`animpipe build` e `roundtrip` ora cuociono `ik_foot_l/r` dai piedi dopo l'export** (`_bake_ik_feet` in `animpipe/ue_rig.py`; il risultato `ik_feet`
  è nel json della build). Il thread del jab deve solo rifare `animpipe build` e rigenerare il kit: lo "squat" sparisce senza altri passaggi.
- `setup_overlord_dair_kit.py`: `OVERLORD_DAIR_REFRESH=1` e `OVERLORD_DAIR_BLEND_OUT_FRAMES` (V9 fatto con 8; V8 e V7 non toccati).
- Il mio editor va chiuso senza `Stop-Process` (negato): mappa vuota nuova senza salvare, poi `quit_editor` dal bridge. `ue.py` senza `port.json`
  locale ripiega sull'editor dell'owner in `_feature`: impostare sempre `UE_MCP_PORT` sulla porta del proprio editor.
## Cosa resta imperfetto da dietro e con cosa coprirlo (proposte, niente altre pose: la chiave a15 dell'owner è chiusa)
- Colonna nei 3 frame attivi: scia verticale sotto i piedi a13-a15, anello d'impatto e shake sul colpo a segno.
- Contatto grigio su grigio: scintilla e flash sul bersaglio, reazione del bersaglio (oggi resta in piedi).
- Rimbalzo a vuoto senza causa visibile: piccolo anello d'aria a15-a17. Carica debole in miniatura: bagliore che converge a8-a10, flash di un frame a12.
## Prossimo thread (lato gioco, breve; va fatto su 5.9 dopo il passaggio)
1. Capire perché in aria l'uscita verso la posa di caduta scatta in un frame all'inizio del blend-out invece di distribuirsi (ipotesi: IK gambe o
   foot placement dell'ABP Blueprint legati al peso del montage; in `Plugins/GSTNAnimation` C++ non c'è nulla del genere). Provare blend-out che parte dopo il FAF.
2. Ricatturare solo `dair_whiff` (`PIE_CAPTURE_SEQUENCES=dair_whiff`) e rimisurare v69-v77: nessun frame sopra 2400 px di lato. Poi PASS e all'owner.
In gioco oggi (non approvata): `L_Test_Hub`, kit `KIT_Test_OverlordDAirV9`, salto e poi giù + attacco in aria.
Tentativi su blocchi: clearance 2 (risolto); rilettura finale di `author` 2 (fallisce dopo il salvataggio, take verificata con `sample_pose`). Costo stimato: ~190k token di contesto Fable + revisore Fable 171k.

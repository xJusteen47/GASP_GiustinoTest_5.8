# Handover anim pipeline: fase B, jab v12 e Ganondorf v3.1 sul rig (2026-11-02, branch claude/project-thread-s125cq)

Esito: **le due mosse sono bakate dal motore unico** (`python -m animpipe build cards/<mossa>.json`, ~10 s ciascuna):
`/Game/AnimPipeline/Rig/AS_GanonFTiltV31_Build` (11 pose, 10659 chiavi) e `AS_JabV12_Build` (9 pose, 8685 chiavi).
Checkpoint 2 è nella storia git di questo file. Non esistono file Step dell'owner per queste mosse: le pose sono i frame
chiave delle clip v3.1/v12 (Ganondorf: i 5 frame dell'owner a 0/7/10/27/37 più 6 intermedi).

- Checkpoint pose (una griglia per mossa, `Tools/AnimPipeline/sheets/`, non in git): `ganon_ftilt_v31_poses.png` (riga in
  alto = i 5 frame dell'owner), `jab_v12_poses.png`; build sopra sorgente: `ganon_ftilt_v31_poses_ab.png`.
- Ganondorf, difetti risolti con `edits` nella scheda (rotazioni FK risolte dallo Jacobiano del rig, `fk_solve.py`):
  gomito dietro fuori profilo da 31 a 13 frame (gomito ~20 cm dietro il petto invece di 33-35, chiavi 20-32);
  avambraccio sinistro nel fianco allo scatto: clearance al frame 10 da -4,0 a +4,1 cm (mano 28 cm di lato, era 13).
  Bloccanti 5/6 come la sorgente: legibility FAIL ereditato (controlli da pugno su un calcio), ma migliore della sorgente.
- Jab v12: bloccanti 6/6 PASS (come la sorgente). **Pugno dietro non risolto** dopo 2 tentativi mirati (togliere la
  torsione della mano 1-3: 70°/frame, peggio; mano + braccio: 36,5°, uguale). I 36° del frame 0->1 vengono dallo swing
  del braccio di guardia nel contatto di 1 frame, non dalla torsione: serve una decisione di posa (domanda 1).
- Motore: ultimo frame ora bakato giusto (la sezione tornava al frame 0 a t = durata; play rate 0,99999); `lock_lengths`
  (stretch spento sul bake, traslazioni locali fisse; tolto fino a 1,56 cm sul collo) -> bone_lengths PASS su entrambe;
  scheda: `edits`, `drop`, `spacing` per posa, `sheet_extra`; `check --src` (stessi controlli sulla sorgente); planted
  solo sui piedi della scheda; `probe <take>` (canali del rig); `poses` (griglia, `--over`, `--blind`).
- Da guardare: ginocchio dietro del jab 9 cm più avanti della sorgente in guardia (IK gamba del rig, frame 0 e 18-37);
  spaziatura Ganon 3-7 a 0,17 (sorgente 0,78); motion_check FAIL identico alla sorgente su entrambe.
- A/B cieco (Sonnet, A = sorgente, B = build): jab B di poco (pugno più pulito dal busto al contatto); Ganondorf B
  (recupero più basso e piantato, più vicino al riferimento), ma segnala al frame 10 vista frontale il braccio sinistro
  che si chiude a triangolo verso il busto: effetto dell'abduzione, da guardare in gioco.
- Non fatto: giudizio in gioco (montage + hitstop + camera), per budget. Editor chiuso col kill (asset salvati dal build).
  Scratch in `/Game/AnimPipeline/Rig`: `AS_/LS_Jac1_Build`, `Jac2_Build`, `GanonAxisProbe_Build` (cancellabili).

Domande per l'owner:
1. Jab: il braccio di guardia scatta indietro col busto nel frame di contatto e il pugno gira. Lo tengo fermo al mento
   (la guardia segue il petto, cambio di posa al frame 1) o accetti lo scatto stile Mario?
2. Ganondorf: braccio dietro ora vicino al corpo nel recupero e braccio sinistro aperto allo scatto (griglia `_ab`): ok?

Costo stimato: Opus ~85 chiamate, contesto finale ~130k, circa $9-11; subagente Sonnet (A/B cieco) ~75k token, < $0,5.

## Checkpoint 3 (2026-11-02): jab con guardia al mento e giudizio in gioco

Decisioni owner applicate: 60 fps, round-trip 1 cm, stretch OFF + ossa 0,1 cm; 4a pugno dietro fermo al mento; 4b braccia Ganon OK.
- **Jab rebuild** (`build cards/jab_v12.json`, 15 `edits` nella scheda, nessun frame a mano): chiavi 1/2/3/5/9 su
  `upperarm_r_fk_ctrl` (gomito al posto del frame 0 nel riferimento del petto), `lowerarm_r_fk_ctrl` (pugno sotto la testa)
  e `hand_r_fk_ctrl` (orientamento del frame 0 portato dal petto). Soluzioni dallo Jacobiano del rig (`fk_solve.py` per le
  posizioni, sonde +10° su X/Y/Z per la mano). Bloccanti **6/6 PASS** (clearance minima 0,27 cm al frame 6, stretta ma ok).
- Misure nel riferimento del petto (prima -> dopo): pugno dal mento 18-21 cm -> 0,6-3,6 cm; errore di orientamento ai frame
  1-3 54° -> 9-11°. Il salto 0->1 in spazio componente (50°) ora è solo il petto che gira di 52° al contatto: la guardia
  segue il busto, il pugno non gira più contro di esso. Griglia `sheets/jab_v12_poses.png` (build sopra sorgente).
- **Giudizio in gioco** (`ingame_judge.py <card> <hitstop>`, nuovo): crea `AM_<nome>` accanto alla build, cattura ogni frame
  dalla camera laterale Duel (el 12, 450 cm, FOV 55) e ripete il primo frame attivo per l'hitstop; mp4 60 fps + striscia in
  `sheets/ingame/`. Non è PIE con l'abilità vera: hitstop ipotizzati 4 (jab) e 8 (ftilt), da tarare in combat.
  - Jab: **verde**. Leggibile (braccio teso chiaro di lato, guardia chiusa visibile), forza dal busto + affondo + hitstop;
    smoothness ok sulla striscia, motion_check FAIL ereditato (stesso della sorgente, non bloccante).
  - Ganondorf ftilt: **verde con note**. Silhouette del calcio molto leggibile, peso sulla gamba d'appoggio, braccio sinistro
    aperto non si legge come "triangolo" a 450 cm. Nota: dal frame 16 al 24 la gamba scende in un blocco solo (spaziatura
    0,17 già segnalata) e il FAF a 37 chiude in una posa raccolta diversa dalla guardia del jab.
- Montage `AM_JabV12_Build`, `AM_GanonFTiltV31_Build` in `/Game/AnimPipeline/Rig` (Content non versionato).
- Editor: riaperto direttamente in `L_PilotCapture` (DefaultLevel risultava dirty all'avvio e non è stato toccato); l'indice
  di ripristino autosave è stato spostato in `Saved/Autosaves_restore_bak/` (nessun autosave cancellato).

Pushato su `feature/gas-npp-5.8` come fast-forward puro: `deeab05` (rebase pulito sopra `13cc565`).
Prossimo: (1) Ganondorf, gamba che scende in un blocco solo ai frame 16-24 (spaziatura), non corretta qui; (2) hitstop e
camera veri in PIE quando le abilità combat usano questi montage.
Costo stimato checkpoint 3: Opus ~95 chiamate, contesto ~110k, circa $8-10.

## Checkpoint 4 (2026-11-02): anteprime senza hitstop (whiff)
- `ingame_judge.py` ora produce sempre: `<mossa>_ingame.mp4` (hitstop), `_ingame_whiff.mp4`, `_ingame_compare.mp4` (hitstop a sinistra, whiff a destra); `--cached` rifà i video dai frame già catturati senza editor.
- Rigenerati jab v12 (hitstop 4) e Ganon ftilt v3.1 (hitstop 8) con l'editor aperto; compositi `<mossa>_hitstop_vs_whiff.png` in `sheets/ingame/` (non in git), copiati nei project files.
- Whiff: pose identiche all'hitstop (il congelamento ripete solo il frame attivo). Unica nota whiff: ftilt, recupero 19->25 cade in un blocco solo (già segnalato, spaziatura 0,17); nessun problema nuovo. Animazioni non toccate.
- Pushato su feature: `061bdbb`. Costo stimato: ~25 chiamate, contesto ~60k, circa $1,5-2 (Sonnet 5.5), niente subagenti.

## Checkpoint 5 (2026-11-02): Ganondorf ftilt, gamba che scende in un blocco (spaziatura recupero)
- Diagnosi (piede sinistro, cm/frame): il segmento 10-20 era `hold_recoil` con chiave piatta a 20 -> il piede si fermava
  a 20 (0,6) e poi cadeva 22 / 19 / 12,5 ai frame 21-23 (40 cm in 3 frame).
- Tentativo 1 (scheda: posa `active` 10 con `spacing: limb_led_recovery`, niente stop a 20): 5,1 -> 14,9 -> 23,3 -> 15,1. Picco ancora.
- Tentativo 2 (tenuto; scheda: `drop` a 20 su `foot_l_ik_ctrl` Location/Rotation e `leg_l_pv_ik_ctrl` Location):
  la gamba viaggia 10 -> 23 senza fermarsi: 7,9 -> 5,6 cm/frame quasi costante, poi 7,3 / 5,4 all'appoggio 24-25.
  Nessuna posa dell'owner cambiata (0/7/10/27/37), nessun frame a mano, il resto del corpo tiene la chiave a 20.
- Rovescio: niente più "blocco", ma la discesa è quasi lineare e il calcio teso non si tiene (a f14 piede 70 cm, prima 81).
  Bloccanti 5/6 come prima (legibility FAIL ereditato); motion_check uguale; planted 1,03 cm; clearance PASS.
- Prossimo passo proposto (motore, non scheda): oggi una chiave piatta il cui segmento seguente è `limb_led_recovery` ha
  peso di uscita 0 (`ue_rig.py`, out_w None) -> nessun ease-out da 10. Dare a quella chiave il peso di uscita del profilo
  del segmento entrante (1/3) darebbe: tenuta a 10-14, accelerazione, appoggio a 23-25. Serve l'ok dell'owner (cambia il
  motore per tutte le mosse). Alternativa solo-scheda: spostare la posa `hold_end` 20 -> 17 (stop più alto, caduta su 6 frame).
- Bug strumenti corretto: `legibility.py` riusa `sheets/raw/<take>_legbones.json` per percorso asset, quindi dopo un
  rebuild i check leggevano le ossa vecchie. `animpipe build` ora cancella quel file. (I check dei checkpoint precedenti
  dopo un rebuild potrebbero essere stati letti sul dump vecchio.)
- Anteprime in gioco (`ingame_judge.py`, hitstop 8 + whiff): `sheets/ingame/ganon_ftilt_v31_*.mp4` e composito
  `ganon_ftilt_v31_recovery_before_after.png` (prima whiff / dopo whiff / dopo hitstop, frame 10-27, vista laterale), nei project files.
- Costo stimato: Opus ~40 chiamate, contesto ~75k, circa $4-5; niente subagenti. Editor lasciato aperto su L_PilotCapture.

## Checkpoint 6 (2026-11-02): ease-out opt-in, Ganondorf tiene il calcio e poi cade
- Decisione coordinatore (regola 6): ease-out come opzione per posa nella scheda, `ease_out` (0-1), default spento.
  Motore (`ue_rig.py`): una chiave piatta seguita da un segmento senza pesi esce con peso `ease_out` x durata segmento.
- Jab v12 ricostruito col nuovo motore: ossa identiche al riferimento (differenza massima 0,0), check 6/6 PASS.
  Ricontrollo jab sulle ossa fresche (fix cache): 6/6 PASS, clearance minima 0,27 cm a f6, nessun risultato cambiato.
  `motion_curves` del jab fallisce nel worktree feature solo perché manca il ref Mario (non versionato): misura, non bloccante.
- Ganondorf: `ease_out: 0.333` sulla posa 10 (con `drop` a 20 e `limb_led_recovery` del checkpoint 5), 1 tentativo.
  Piede sinistro cm/frame: tenuta 10-14 (0,3 / 0,9 / 1,7 / 2,7), caduta che accelera 15-20 (picco 14,3, prima 22 dopo
  uno stop), rallenta all'appoggio 21-25 (14 / 10,8 / 7 / 7,3 / 5,4). Bloccanti 5/6 come prima (legibility ereditato;
  contatore "gomito dietro la schiena" 15 frame, prima 12-13), planted 1,03 cm, vs sorgente 33,28 cm come prima.
- Attenzione: il primo build del jab dopo i build Ganon è uscito rotto (mano a 103 cm); rifatto identico, era il bake e non
  il codice (riprodotto senza la modifica: ok). Se `vs source` supera ~40 cm, rifai il build una volta.
- Anteprime: `sheets/ingame/ganon_ftilt_v31_*.mp4` e composito `ganon_ftilt_v31_recovery_before_after.png`, nei project files.
- Costo stimato checkpoint 5+6: Opus ~65 chiamate, contesto ~95k, circa $6-8; niente subagenti. Editor lasciato aperto.

## Checkpoint 7 (2026-11-02): anticipazione Ganondorf più estrema (feedback owner)
- Owner: "gamba più verso il corpo nell'anticipazione, posa più estrema". Solo `edits` nella scheda, frame 7, 2 tentativi.
- Sonda: gli edit `Location` su `foot_l_ik_ctrl` / `body_ctrl` vanno 1:1 sugli assi componente (X sinistra, Y avanti, Z su).
- Tentativo 1 (piede [0,-15,+6], corpo [0,-3,-4]): il ginocchio finiva 14 cm dietro l'anca (IK senza guida). Scartato.
- Tentativo 2 (tenuto): piede [0,-19,-8], `leg_l_pv_ik_ctrl` [0,+40,+10] (ginocchio avanti e alto), corpo [0,-3,-4].
  Al frame 7: piede 19 cm più vicino all'anca (distanza anca-piede 20 cm, prima 43), ginocchio sopra l'anca, bacino 4 cm
  più basso; lo scatto 7 -> 9 ha più corsa. Bloccanti 5/6 come prima, clearance PASS, planted 1,02 cm.
- Non fatto: busto più inclinato indietro e braccia più caricate (servono sonde FK, `fk_solve.py`); da proporre se l'owner
  vuole ancora più estremo. Nota: il ginocchio si sposta di 8 cm verso il centro (x), da guardare in vista frontale.
- Anteprime: `sheets/ingame/ganon_ftilt_v31_*.mp4` e composito `ganon_ftilt_v31_anticipation_before_after.png`, nei project files.
- Costo stimato checkpoint 7: Opus ~20 chiamate, contesto ~110k (cumulato), circa $3-4.

## Checkpoint 8 (2026-11-02): niente scatto dopo un frame con hitbox (regola owner, tutte le mosse)
- Owner: durante l'hitstop il frame congelato è seguito da un salto grande delle braccia; lo scatto va NEL frame in cui
  esce la hitbox, non subito dopo. Regola generale, documentata nel README (regole owner + tabella check).
- Check `hitstop_snap` in `animpipe check`: **bloccante, default su ogni mossa** (scelta: non opt-in, come chiesto).
  Per ogni frame attivo f misura lo spostamento massimo per osso f -> f+1 (spazio componente, 65 ossa), limite 10 cm
  per frame anim (card `hitstop_max_cm` per cambiarlo, esenzione `hitstop_snap` possibile).
- Opzione scheda `hitstop_blend` [{frame, toward, amount, match}]: la chiave a `frame` va di `amount` verso quella a
  `toward` sui controlli indicati (vuoto = tutti), dopo gli `edits`; rotazioni per la via corta. Esplicita per mossa
  (scelta: il motore non la applica da solo, perché quale posa spostare è una decisione di posa).
- Ganondorf: 9 -> 10 da 63 cm (mani 49) a **9,8 cm**: posa 9 all'85% verso la 10, il salto ora è 8 -> 9 (entrata
  nella hitbox). Pose owner 0/7/10/27/37 intatte. Bloccanti 6/7: legibility FAIL ereditato come prima.
- Jab v12: 2 -> 3 da 29 cm a **7,4 cm** (1° tentativo 0,7 -> 13,7 cm, tenuto 0,85): il pugno resta teso al 3 e
  rientra 3-5 (3 -> 4 ora più veloce, fuori dalla finestra attiva). Bloccanti **7/7 PASS**.
- Anteprime: `ingame_judge.py ... --at=<frame attivo>` (nuovo: hitstop su un frame attivo qualsiasi); jab hitstop 4
  a f2, Ganon hitstop 8 a f9; mp4 hitstop/whiff/compare in `sheets/ingame/`. Compositi prima/dopo con sovrapposizione
  rosso/ciano `hitstop_sheet.py` -> `<mossa>_hitstop_snap.png` (prima jab: cattura del worktree aaqr80, ossa identiche).
- Da guardare: Ganon 9,8 cm è al limite (amount 0,9 darebbe margine); hitstop vero in PIE quando le abilità usano i montage.
- Costo stimato: Opus ~35 chiamate, contesto ~70k, circa $3-4; niente subagenti.

## Checkpoint 9 (2026-11-02): kit overlord e down air v1 (Ganondorf dair + stomp di M. Bison)
- Owner: mosse raggruppate per **kit** (proxy di un personaggio). Schede in `cards/<kit>/<kit>_<mossa>_vN.json` (`cards/README.md`).
  Kit **overlord** (nome provvisorio): stile "violenza elegante" alla M. Bison / Rugal (KOF): composto, potenza controllata,
  braccia conserte. Ganondorf ftilt spostato in `overlord/overlord_ftilt_v31.json` (`move` rinominato: i nuovi output avranno il
  prefisso overlord_; l'asset UE resta `AS_GanonFTiltV31_Build`). Jab nel kit `pilot`.
- Nuovo `python -m animpipe author <card>`: senza clip né pose Step, le pose chiave sono dati nella scheda (blocco `author`:
  bacino, colonna, testa, clavicole, mani/piedi IK con pole, in assi componente) -> ossa -> `ue_write_take.py` ->
  `AS_Pilot_OverlordDAir_V1` (sorgente del build). Pugni dal template Ganon V2.
- Dair v1 (`cards/overlord/overlord_dair_v1.json`): tempi UFD Ganondorf dair (startup 16, attivi 16-18, totale 44 -> anim
  attivi 14-16, FAF 42). Pose 0/6/12/14/16/26/34/42: raccolta in aria, braccia che salgono e si incrociano sopra la testa (12),
  al 14 gambe giù dritte e braccia conserte sul petto (lo scatto grande è SUL primo frame di hitbox), tenuta, ginocchia su al FAF.
  Build `AS_OverlordDAirV1_Build`; montage e video con hitstop 8 a f14 e whiff.
- Check: hitstop_snap PASS (0,6 cm/frame dopo gli attivi), ossa, contatto/FAF, pugni PASS; legibility FAIL (controlli da pugno
  su un calcio, come il ftilt). Clearance stampa PASS ma ha violazioni: avambracci incrociati che si toccano e avambraccio dx
  nella testa ai frame 9-13 (fino a -8 cm), braccio conserto nel petto a 34-36. 2 tentativi sulle pose, poi fermo (regola 5).
- A occhio (griglia laterale): fasi leggibili; le braccia conserte sporgono troppo in avanti (sembrano tese), lo stomp è
  pulito ma poco "pesante"; nessun giro elegante dopo l'hit (fase successiva, non fatta).
- Bug strumenti: `animpipe check` andava in errore senza piedi piantati (aereo): ora "n/a". L'editor va lanciato con
  `MSYS_NO_PATHCONV=1`, altrimenti Git Bash storpia `/Game/...` e si apre DefaultLevel (sporco). `ue_write_take` fallisce la
  rilettura subito dopo il salvataggio (dialog): il build campiona bene lo stesso.
- Output nei project files: `dair_v1_griglia_laterale.png`, `dair_v1_ingame_hitstop_vs_whiff.mp4`, `dair_v1_pose_lato_fronte.png`.
- v1b dopo le immagini dell'owner (Bison SF4 e SF2): braccia conserte strette e alte sul petto, mani sul braccio opposto, punte
  dei piedi tese allo stomp. Output `dair_v1b_*`. Fermato qui (owner darà pose Cascadeur): la compenetrazione 9-13 resta.
- Pose owner in ingresso: `.casc` in `Saved/AnimPipeline/overlord_dair_owner.casc` (mai sovrascritto), FBX a step accanto,
  import in `/Game/AnimPipeline/Pilot/` (`import_take.py`); nuova scheda `overlord_dair_v2.json` con `source_anim` = quella take,
  `source: {file, frame}` per posa e niente blocco `author`; poi `build`, `check`, `ingame_judge.py ... 8 --at=14`.
- Costo stimato: Opus ~75 chiamate, contesto ~110k, circa $7-9; niente subagenti (render e build erano 3 comandi).

## Checkpoint 10 (2026-11-02): down air v2 dalle pose Cascadeur dell'owner
- Pose owner lette dal file aperto (`autosave2-Down air.casc`, copia in `Saved/AnimPipeline/overlord_dair_owner.casc`; numeri in
  `Saved/AnimPipeline/dair_v2/owner_poses_cascadeur.json`). **15 = carica** (palla compatta, ginocchia al petto, avambracci
  incrociati davanti alla faccia), **23 = stomp** (corpo dritto verticale un po' indietro, gambe unite, braccia conserte alte).
  Nessun giro dopo l'hit nelle sue pose. Mani nel suo file = default: ignorate.
- Nuovo `casc_to_author.py <pose.json> <frame>...`: punti di controllo Cascadeur -> pose del blocco `author` (rotazioni bacino e
  busto da terne di riferimento, mani/piedi nei frame di author.py, poli = gomito/ginocchio spinti fuori). Riusabile per ogni
  posa dell'owner. `author.py`: chiave `fingers: [0..1,0,0]` = **mano villain** (aperta, dita dritte e unite), blend per frame.
  `checks.py`: `reach_axis` nella scheda (dair: `-z`), esenzione `fists` per frame.
- Scheda `cards/overlord/overlord_dair_v2.json`: tempi Ganondorf (owner conferma). Sua 15 -> anim 12 (+ tenuta 13 compressa,
  mano villain 8-13), sua 23 -> anim 14 (tutta l'estensione SUL primo frame di hitbox), punte dei piedi -45 ("troppo è giusto").
  Build `AS_OverlordDAirV2_Build`, sorgente `AS_Pilot_OverlordDAir_V2`.
- Check: 6/7 blocchi PASS (contact_faf, fists, hitstop_snap 3,3 cm, ossa, clearance stampa PASS); legibility FAIL ereditato
  (controlli da pugno su un calcio). Clearance dopo 2 tentativi: avambraccio dx nella testa -5 cm al 13; contatti delle braccia
  conserte (mano sul braccio opposto -4/-5, avambracci -5) restano. Per staccarli ho spinto le mani in avanti di 9-16 cm al 12:
  **nella vista laterale le braccia al 12 sembrano tese in avanti**, meno fedeli alla sua 15. Decisione chiesta all'owner.
- Output: `overlord_dair_v2_pose_owner_15_23.png`, `overlord_dair_v2_lato.png` (composito lato in gioco),
  `overlord_dair_v2_hitstop_vs_whiff.mp4`. Non fatto: compositi fronte/3-4 in gioco (budget); `animpipe poses` va in errore
  (ncols 0 con --take/--over), da sistemare.
- Lezioni: il server script Cascadeur va avviato dall'owner; la finestra minimizzata blocca i render (`ui.restore_window()`).
- Costo stimato: Opus ~75 chiamate, contesto ~100k, circa $7-9; niente subagenti (ogni build/render era un comando).
- **v3 (owner review di v2)**: v2 non aveva sinergia (braccia ferme sul busto mentre busto e gambe ruotavano) e la testa
  scattava dopo l'hit. Cause: braccia fisse nel frame del petto da 6 a 12; `part_delays.head = 1` (la testa arrivava alla
  posa dello stomp un frame dopo l'hit, quindi dopo l'hitstop). `hitstop_snap` ora conta la rotazione della testa (arco a
  20 cm): v2 = 27 cm al 14->15, v3 = 1,4 cm. Scheda `overlord_dair_v3.json`: braccia che viaggiano rispetto al busto in ogni
  tratto (basse 0, larghe e in salita 6, incrocio a petto 10, tua 15 al 12, spinta su al 13, giù sul petto al 14 con le
  gambe), busto che si chiude per gradi, mano villain che si apre da 4 a 12. Clearance: resta l'avambraccio dx nella testa
  -5,6 cm al 12-13 (posa dell'owner su un manichino con proporzioni diverse). Output `overlord_dair_v3_lato.png`,
  `overlord_dair_v3_hitstop_vs_whiff.mp4`. Costo totale del thread circa $9-11.

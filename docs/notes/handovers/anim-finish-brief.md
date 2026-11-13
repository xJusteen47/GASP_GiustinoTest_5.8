# Brief: finire le mosse dell'Overlord e pose di reazione dei manichini (ANIM-FINISH-1, 2026-11-14)
Richiesta owner 11-13 23:26Z: finire le animazioni "dell'altra volta" (down-air, jab, dropkick), usare il dropkick per provare stato a terra e
tech, e dare ai manichini pose di colpo e di lancio "anche solo posa ferma". Modello: Fable 5.2 (chiesto dall'owner), pipeline solita
(skill `animpipe-move` nel repo + `gstn-move-animation`). Leggi solo: questo brief, `anim-dair-10.md`, `anim-jab-5.md`, le schede che tocchi,
`docs/notes/anim-theory/` (3 documenti; il terzo, sagome da più direzioni, è su `origin/claude/project-thread-ujyw42`).

## Da dove si parte
- Branch `claude/project-thread-n0mxg5` (su origin): linea 5.9 `79454ebc` + jab v3 (`v1mo72`) + down-air v9 e schede dropkick (`se53y5`).
  Prima di questo merge niente dell'Overlord stava sulla linea 5.9. Pad "OVERLORD JAB" spostato a y 1050 in `build_test_hub_map.py`.
- Cartella e via libera: le dice il coordinatore nel messaggio di partenza. Editor dell'owner chiuso prima di cominciare.
- Editor: MCP integrato di Unreal 5.9 (127.0.0.1:8000), non UE_MCP_Bridge (regola owner 11-13). `animpipe` oggi parla col bridge (`ue.py`):
  PRIMO controllo, massimo 2 tentativi: far passare `ue.py` dal MCP integrato. Se non va, fermati e scrivilo (non installare nulla).
- Le take in `/Game/AnimPipeline/Rig` non sono in git: si rifanno con `animpipe build` dalla scheda. Le copie di gioco stanno in
  `Content/GASNPPTest/Combat/Overlord/` (in git) e le creano gli script `Tools/GAS/setup_overlord_*_kit.py`.

## Ordine di lavoro (un commit per voce; a 150k di contesto chiudi con handover e il resto va a un thread nuovo)
1. **Reazioni di chi viene colpito** (priorità dell'owner, 11-13 23:31Z: "una suitable animation per chi viene colpito, e va in tech").
   Schede in `cards/reaction/`, sul manichino (non sull'Overlord: i manichini sono `BP_GASNPP_TestCharacter`). Elenco, nomi e agganci:
   sezione "Stati e agganci". Ogni posa: griglia a 6 viste guardata prima dell'uso; deve leggersi DA DIETRO (telecamera di gioco) e
   DI LATO. Niente modifiche all'Anim BP.
2. **Dropkick** (`cards/overlord/dropkick/`: startup 0-15, loop 0-11, land 0-31, mai visto in gioco, nessun kit). Le schede hanno solo nomi
   e frame delle pose: scrivi le pose dal `_design`, `build`, `check`, poi kit `KIT_Test_OverlordDropkick` sul modello di
   `setup_overlord_dair_kit.py`. Chi usa la mossa atterra di schiena e resta a terra (come il j.HK di Big Band), poi tech o rialzata.
   Lo stato a terra di chi attacca lo costruisce il thread "Fix launches and add knockdown states": finché non c'è, la `land` è la
   sezione del montage all'atterraggio. Tech e rialzata dell'Overlord: due clip brevi nuove (`tech`, `getup`).
3. **Down-air v9**: pose chiuse. Resta lo scatto f33→f34 a vuoto all'inizio del blend-out. Prova il blend-out che parte dopo il FAF,
   ricattura solo `dair_whiff`, rimisura v69-v77 (nessun frame sopra 2400 px di lato). Massimo 2 tentativi; l'Anim BP non si tocca
   (owner 11-13): se la causa è lì, scrivilo e basta.
4. **Jab v3, giro 1 di 2** (elenco in `anim-jab-5.md`): jab 1 gambe lunghe sul colpo, tenuta che respira, clearance f3-f4; jab 2
   caricamento che sale fino a f6, mano all'altezza del petto, gamba dietro lunga. L'owner giudica DA DIETRO e di lato: il braccio del
   jab 1 deve uscire dalla sagoma anche da dietro (contatto in diagonale, se l'owner conferma). La capsula che scende di 12 cm col
   root motion è lato gioco: usa il passo in `pelvis.loc` oppure segnalalo, non correggerlo qui.

## Per ogni mossa, prima di chiudere (regola 10)
Cattura in Play vera (`Tools/GASNPPTest/pie_capture_move.py`): telecamera di gioco e di lato, a vuoto e sul colpo; mp4 + griglie
(`pie_capture_compare.py`, versione vecchia sopra e nuova sotto dove esiste) in `docs/notes/reviews/<variante>/`, committati. Il verdetto
lo dà una revisione cloud aperta dal coordinatore: niente arriva all'owner come pronto prima del PASS.

## Regole
Niente push su `feature`, niente force-push, niente mappe salvate, niente Anim BP, niente cablaggi Blueprint a mano, niente input
fissi nel codice. Massimo 2 tentativi per blocco. Output delle macchine su file, 5 righe in console. Passi meccanici (build, catture,
griglie) a subagenti Haiku/Sonnet. Alla fine: cartella di playtest `GASP_Giustino2_ue59` sull'ultimo lavoro e COMPILATA, editor chiuso,
handover di massimo 30 righe con costo in `docs/notes/handovers/anim-finish-1.md`.

## Stati e agganci
Tech dell'owner (11-13 23:31Z, non è quella di Smash): chi è lanciato tocca terra, fa un PICCOLO rimbalzo e resta a terra (salvo che il
knockback verso terra causi già un rimbalzo vero). Stick tenuto in una direzione = rotolata da quella parte. A = ci si alza sul posto.
Niente attacco da terra per ora. Tutto modellato come GA (le scrive il thread di gameplay). Rimbalzo e wall splat si provano con cloni
di una mossa con gli effetti: lato gioco, non animazione.

| Clip (`/Game/AnimPipeline/Rig/Reaction/`) | Stato nel codice | Forma |
|---|---|---|
| `AS_React_HitStand` | `EGSTN_ActionState::Hitstun` (reazione StaggerInPlace / PushBack) | posa ferma: busto e testa indietro, braccia larghe, un piede arretrato |
| `AS_React_Tumble` | `Tumble` (reazione Launch) | posa ferma in aria: schiena inarcata, il bacino guida, arti che restano indietro |
| `AS_React_Splat` | `Splat` | posa ferma: schiena piatta sulla parete, arti aperti e asimmetrici, testa indietro |
| `AS_React_GroundHit` | atterraggio dal Tumble (stato nuovo) | ~18 f: tocco f0, schiacciamento f2, rimbalzo bacino +12/15 cm f6, secondo tocco f10, assestamento f16 |
| `AS_React_LieUp` | a terra supino (stato nuovo) | posa ferma = ultimo frame di GroundHit |
| `AS_React_Roll` | GA rotolata (stick tenuto) | ~24 f da LieUp a idle eretto, sul posto: lo spostamento lo dà la GA, che gira il personaggio verso lo stick |
| `AS_React_GetUp` | GA rialzata (A) | ~26 f da LieUp a idle eretto, sul posto |

- **Aggancio: oggi non esiste.** Nel codice non c'è nessun campo o montage per le reazioni della vittima (solo `RecoilMontage` di chi
  attacca e il `Montage` della timeline). Rotolata e rialzata sono GA con timeline, quindi usano il campo `Montage` della loro `TL_`.
  Per Hitstun, Tumble, Splat, atterraggio e a terra serve un aggancio nuovo lato gioco (chiesto al thread di gameplay via coordinatore):
  finché non c'è, le clip si guardano nella vetrina Z6 dell'hub (`build_test_hub_map.py`, SkeletalMeshActor in loop) e con cattura.
- Capsula: a terra il corpo è sdraiato ma la capsula resta in piedi. È lato gioco: segnalalo, non compensarlo nelle pose.
- Scelte mie da confermare (owner): rotolata con una clip sola e personaggio girato verso lo stick (non 4 clip); a terra sempre supino.
- **Risposte dell'owner (11-13 23:34Z):**
  1. Dropkick: sì, chi lo usa atterra di schiena e resta a terra. "Sta in Antigravity di base": pose e clip di partenza sono nel
     worktree di Antigravity sul fisso (`C:\Users\jacop\.gemini\antigravity\worktrees\GASP_Giustino2_feature\animate_3d_moves_unreal`,
     sola lettura: non modificarlo). Guardale prima di scrivere pose nuove.
  2. Jab 1: sì, contatto in diagonale così il braccio si vede da dietro.
  3. Jab 2: sì a passo col sinistro e pugno dopo il tremore, ma "non farei il passo troppo lungo, vorrei che poi il personaggio in idle
     ritornasse indietro sufficientemente". Quindi passo di 15-20 cm (non 30) e nel recovery il corpo torna indietro: a fine mossa
     lo spostamento netto è piccolo (proposta mia: 5 cm o meno). Scrivi nell'handover i cm scelti.
- Richiesta nuova dell'owner, lato gioco (non animazione): capsula di collisione che si restringe durante alcuni attacchi e quando si è
  lanciati, come la ECB di Smash; serve per il dropkick. Il coordinatore la passa al thread di gameplay. Le pose non la compensano.

## Costo della preparazione (thread cloud, 11-13)
Fable ~150k token di contesto, 1 subagente Sonnet di ricerca (~60k token). Nessun editor usato, nessuna animazione costruita.

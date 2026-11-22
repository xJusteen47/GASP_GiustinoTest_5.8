# Revisione pipeline animazione (2026-11-02, dopo overlord_dair v2/v3 e overlord_ftilt)

Input letti: `docs/notes/handovers/anim-pipeline.md` (checkpoint 1-10 + v3), `Tools/AnimPipeline/animpipe/checks.py`,
`legibility.py`, `clearance.py`, `README.md`. Nessun thread riletto, nessun editor usato.

## 1. Esito in breve

I controlli bloccanti non vedevano tre dei quattro errori che l'owner ha trovato a occhio, e uno di loro (clearance)
**non bloccava mai niente**. Ho aggiunto in questo thread i controlli economici (sotto le 100 righe, solo `checks.py`);
restano tre lavori piccoli, uno per thread.

## 2. Cosa è scappato e perché

| Errore visto dall'owner | Perché i check non l'hanno preso | Stato |
|---|---|---|
| Dair v2: braccia ferme sul busto mentre busto e gambe ruotano | Nessun check misurava il moto di un arto **rispetto al genitore**; `motion_check` è un report, non blocca | **Nuovo check `co_motion`** (bloccante) |
| Dair v2: testa che scatta dopo il frame dell'hit | `hitstop_snap` misurava solo posizioni: la testa gira sul perno e il suo osso quasi non si muove. Il thread v3 ha aggiunto la rotazione della **sola testa** | Verificato ed **esteso** a tutte le ossa foglia (testa, punte dei piedi, punte delle dita) |
| Avambraccio dx nella testa (-5,6 cm, dair 12-13) | `clearance.py` non stampa un verdetto né esce con errore: `checks.py` leggeva l'ultima riga e dava **sempre PASS** (anche v1/v2 "stampa PASS ma ha violazioni") | **Corretto**: FAIL se gap < 0 cm, 0-2 cm solo riportato; contatti voluti con esenzione per coppia |
| Legibility FAIL su dair e ftilt | `legibility.py` è cablato sul pugno sinistro: pugno attaccante, braccio dietro che lo copre, gomito dietro, dita chiuse. Il calcio esiste solo come `strike_limb: leg_l`; niente gambe unite verso il basso, niente mani aperte volute | Da fare (thread B) |
| Pose Cascadeur che compenetrano su UE | Proporzioni diverse (avambraccio/testa); `casc_to_author.py` copia le posizioni IK delle mani senza riscalarle | Da fare (thread C) |

## 3. Check aggiunti oggi (`Tools/AnimPipeline/animpipe/checks.py`)

- **`hitstop_snap`**: oltre allo spostamento di ogni osso, conta la rotazione delle ossa foglia come arco di un punto
  a distanza fissa (`LEAF_LEVER`: testa 20 cm, punte dei piedi 8, punte delle dita 2,5). Le ossa non foglia sono già
  coperte dalla posizione dei figli. Prova sintetica: testa che gira di 30° al 12->13 = 13,3 cm, FAIL.
- **`co_motion`** (bloccante, default su ogni mossa): nello startup (0 -> primo frame attivo) la mano nel frame del petto,
  o il piede nel frame del bacino, non può restare ferma (< 1 cm/frame) per 3+ passi mentre il resto del corpo si muove
  (> 3 cm/frame: rotazione di bacino o petto a 30 cm, o un altro arto). Gambe piantate saltate. Arti tenuti fermi apposta
  nella scheda: `co_motion_skip` (jab: `arm_r`, guardia al mento, decisione 4). Prova sintetica tipo v2: `arm_l 6-12`, FAIL.
- **`clearance`**: verdetto dai dati di `clearance.json`, non dal testo. Contatti voluti (mano sul braccio opposto nelle
  braccia conserte): esenzione `{"check": "clearance", "frames": [...], "pairs": ["hand_l|lowerarm_r"]}`.
- Soglie di `co_motion` **non tarate su clip vere** (servono l'editor e i dump delle ossa, che non sono in git). Effetto
  atteso al prossimo `animpipe check`: dair v2 FAIL (co_motion, hitstop_snap, clearance), dair v3 FAIL solo clearance
  (avambraccio nella testa + contatti delle braccia conserte da esentare), ftilt/jab da verificare.

## 4. Costo del giro (dagli handover del 02/11)

Dieci checkpoint in un giorno, circa **$55-70**: 75-95 chiamate Opus per checkpoint, contesto 100-130k, 2 tentativi di posa
a mano (delta `edits` indovinati + sonde). Il costo più grande non è un render: è **il giro owner -> v successiva** per un
errore che un check poteva fermare (v2 -> v3 = un checkpoint intero, ~$9-11). Tagli proposti, in ordine di resa:
1. **Gate prima del render**: `ingame_judge` e i compositi solo se `animpipe check` è verde o ogni FAIL è esentato con
   motivo. Niente anteprime all'owner con FAIL non spiegati.
2. **Un comando `animpipe review <card>`**: build + check + ingame (hitstop e whiff) + un composito laterale, 5 righe di
   output. Oggi sono 4-6 comandi e altrettanti giri di lettura per versione.
3. **Un solo file all'owner per versione**: composito + mp4 compare (oggi 3-4 file, più copie con timestamp nei project files).
4. **Dump ossa separato da legibility**: oggi il dump che usano tutti i check nasce dentro `legibility.py measure`
   (ray-cast su 26 viste). Un `animpipe dump` leggero, e legibility solo quando serve il giudizio di lettura.
5. **Modelli**: build/check/render li lancia Sonnet; Opus solo per le decisioni di posa e un'occhiata alle immagini.
6. **Pose dell'owner già sul manichino** (thread C) per evitare i tentativi alla cieca sulla compenetrazione.

## 5. Piano: 3 thread piccoli, uno alla volta, 1 checkpoint ciascuno

**A. Tarare i check nuovi + `animpipe review`** (Sonnet, desktop con editor, ~$3-4).
`animpipe check` su dair v2, dair v3, jab v12, ftilt v3.1. Obiettivo: v2 FAIL su co_motion e hitstop_snap, le altre PASS
su questi due; al massimo 2 ritocchi di soglia. Esenzioni clearance nella scheda v3 per i contatti delle braccia conserte
(non per l'avambraccio nella testa). Aggiungere `animpipe review` e correggere `animpipe poses` (ncols 0 con --take/--over).

**B. Legibility guidata dalla scheda** (Opus medium, repo + desktop per la verifica, ~$5-6).
Blocco `strike` nella scheda: `limb` (arm_l/arm_r/leg_l/leg_r/legs), `hiders`, `pose_checks` attivi (gomito dietro, dita
chiuse solo per i pugni). Dair: gambe come parte attaccante, braccia come possibili coperture, mani villain escluse dalle
dita. Criterio: dair v3 e ftilt v3.1 smettono di fallire per controlli che non si applicano, jab v12 resta identico.

**C. Proporzioni Cascadeur -> UE all'import** (Opus medium, desktop, ~$4-5).
In `casc_to_author.py`: lunghezze dei segmenti (clavicola, braccio, avambraccio, mano, collo+testa) dei due rig; i target
IK delle mani riscalati sulla catena UE mantenendo la direzione dalla spalla; poi la clearance sulla posa authored prima
del build. Criterio: posa 15 dell'owner su UE senza avambraccio nella testa, con le direzioni delle braccia entro ~5°.

Ordine: A (sblocca il gate), poi C se l'owner sceglie di adattare la posa 15, poi B. Nessun thread richiede decisioni nuove.

## 6. Decisioni per l'owner

Nessuna nuova. Resta quella già chiesta: accettare la compenetrazione del dair v3 o adattare la posa 15 (il thread C la
risolve in modo riusabile per ogni posa futura).

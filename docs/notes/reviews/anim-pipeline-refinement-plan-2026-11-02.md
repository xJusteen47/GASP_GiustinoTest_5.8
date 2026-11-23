# Rifinitura pipeline animazione: modifiche manuali, pose approvate, meno errori grossolani (2026-11-02)

Thread di revisione (Fable, nessun editor). Letti: `cards/README.md`, coda di `handovers/anim-pipeline.md`,
`reviews/anim-pipeline-review-2026-11-02.md`, `animpipe/{__main__,card,ue_rig,author,checks}.py`, scheda dair v4.
Solo piano: nessuna riga di codice toccata. L'owner decide; le voci sono in ordine di resa.

## 1. Il problema di fondo (uno solo)

Oggi la **scheda** (`author`, `edits`, `hitstop_blend`) è la sola verità e `animpipe build` cancella e ricrea
`LS_<name>` e `AS_<name>` ogni volta (`ue_rig._new`). Quindi: (a) ogni ritocco a mano dell'owner in Sequencer o
nell'editor della sequenza sparisce al build successivo; (b) niente distingue una posa "accettata dall'owner" da una
provvisoria, così un giro di correzione sulle braccia ha riscritto anche le ginocchia buone (dair v4c -> v4d).
Risposta comune: **tutto ciò che l'owner ha toccato o approvato diventa anche dato della scheda, con uno
snapshot, e il build lo verifica prima di esportare**.

## 2. Strato additivo "Manuale" sul Control Rig (approvato dall'owner, in costruzione nel thread dair)

Meccanismo: una sezione additiva del rig che solo l'owner edita in Sequencer; il build la copia prima di ricreare la
sequenza e la riapplica sopra; i check girano sul risultato finale. Va bene come percorso sostenuto: l'owner lavora
nell'interfaccia che già usa, senza schema nuovo. Tre rischi da coprire, tutti nel punto 3:
- lo strato vive in un UAsset binario: non è leggibile in git né nel diff per giro, e un bump v4 -> v5 lo porta
  con sé solo se qualcuno lo copia (una perdita silenziosa sarebbe proprio l'errore da evitare);
- è additivo sopra chiavi che il build sposta (`edits`, `hitstop_blend`, `part_delays`): se un giro cambia la posa
  base al frame 5, il delta dell'owner resta e il risultato non è né la sua posa né quella nuova;
- non copre l'editor dell'Animation Sequence (chiavi sulle ossa, non sui controlli).

## 3. Rendere lo strato Manuale dato della scheda: `animpipe capture` (valore 1, con il punto 4)

Dopo ogni modifica dell'owner il thread lancia `animpipe capture <card>`: legge le chiavi dello strato Manuale e scrive
nella scheda `"manual": [{"control", "attr", "frame", "delta", "by": "owner", "date"}]` più i valori **finali** dei
controlli toccati (`cards/<kit>/manual/<move>.json`). Effetti:
- lo strato diventa leggibile e versionato: il bump v4 -> v5 copia il blocco, e il build può ricostruire lo strato
  dalla scheda se l'asset manca (`build --manual=card`), così la copia binaria non è l'unico salvataggio;
- ogni chiave toccata dall'owner diventa automaticamente un **lock** (punto 4) sul valore finale: se un giro cambia
  la posa base sotto al suo delta, la guardia 2 vede che il risultato si è spostato e ferma il build (è il secondo
  rischio del punto 2, chiuso senza toccare lo strato);
- modifiche fatte nell'editor della sequenza (ossa): `capture --from-anim` passa prima da `roundtrip` (esiste già:
  anim -> chiavi del rig), confronta con lo snapshot del build e scrive le differenze nello strato Manuale e nella scheda.
Tolleranza 0,5 cm / 1° per ignorare il rumore del bake. Modello: Opus medium (~100 righe in `ue_rig.py` sopra lo
script del thread dair); serve l'editor desktop. Criterio: owner sposta una mano al 5 -> capture -> si cancella lo
strato -> `build --manual=card` -> la mano è dove l'ha messa lui (≤0,5 cm).

## 4. Pose approvate: lock per frame e per parte, e diff per giro (valore 1)

Blocco di scheda `"approved": [{"frames": [13, 14], "parts": ["leg_l", "leg_r"], "ref": "v4c", "by": "owner",
"date": "2026-11-02", "why": "ginocchia a rana al 14 giuste"}]` più uno snapshot `cards/<kit>/approved/<move>.json`
(valori delle chiavi dei controlli di quelle parti a quei frame, scritto da `animpipe approve <card> --frames --parts`
dall'ultimo build). Parti = gruppi di controlli già noti al codice (`PART_CTRLS`, `CO_LIMBS`): arm_l/r, leg_l/r, trunk,
head, hand_l/r.
- **Guardia 1, senza editor** (`animpipe card`, lint puro Python): le pose `author` ai frame bloccati, per le parti
  bloccate, devono coincidere con quelle della scheda di riferimento (`ref`); differenze -> errore prima del build.
  Prende l'errore v4c -> v4d al costo di zero render.
- **Guardia 2, dopo il build**: `build` confronta le chiavi risultanti con lo snapshot; oltre 0,5 cm / 1° -> FAIL,
  niente export, 5 righe con frame e parte. Si supera solo con `--unlock leg_l@14` E rimuovendo la voce dalla scheda
  con un `why`: la rimozione finisce nel diff e nell'handover, quindi l'owner la vede.
- **`animpipe diff <card> [--vs=<card o approved>]`**: tabella per frame x parte dello spostamento massimo (cm/°) tra
  pose `author` (senza editor) e, se i dump esistono, tra ossa dei build; stampa 5 righe, file completo, una griglia
  `poses --over` solo dei frame cambiati. **Obbligatorio nell'handover di ogni giro**: "cambiato: braccia 3/5/12/13;
  intatto: gambe 13-14 (lock)". Oggi il "non toccare il resto" è solo una frase nel brief.
- Flusso di approvazione: l'owner scrive "le ginocchia al 14 vanno bene"; il thread esegue `approve`, commit. Un lock
  non scade con la versione: la scheda v5 nasce con gli `approved` di v4.
Modello: Sonnet per lint, diff e `approve` (dati scheda, nessun editor); Opus medium per la guardia 2 in `ue_rig.build`
(desktop). Criterio: ripetere la correzione v4c -> v4d con le gambe 13-14 bloccate: guardia 1 ferma il build.

## 5. Regole del README che oggi non sono check (valore 2, Sonnet, calibrazione sul desktop)

Da aggiungere in `checks.py` come blocchi dichiarati nella scheda (`rules` con soglie), con un composito per calibrare:
- **frame_0**: distanza di ogni parte dall'idle (aria/terra) al frame 0 ≥ X cm e ≤ Y % del tragitto verso la carica
  ("scatto visibile, non già la carica"); X, Y da tarare su dair v4 e jab v12.
- **knees_front**: nella carica (frame dichiarati) il polo del ginocchio resta davanti al bacino (y ≥ y bacino).
- **feet_side_by_side**: nello stomp le caviglie non si incrociano (separazione laterale ≥ soglia, nessuno scambio x).
- **villain_hand**: dove `fingers` = 1, `wrist_out` ≥ 25° misurato sulle ossa, non sulla scheda (prende un build che
  perde la piega); il pollice entra nel check solo dopo il punto 7.
- **timing_source**: `animpipe card` avverte se una scheda da v2 in su non ha `anim_active` dato dall'owner (regola:
  i tempi li dà lui); nel diff compare la riga "timing: owner / CSV".
Criterio: dair v4 (ultima scheda, ripristino di v4c) e jab v12 passano i nuovi blocchi senza esenzioni nuove.

## 6. Legibility per mosse non da pugno (valore 3, Opus medium, desktop)

Blocco `strike` nella scheda (`limb`, `hiders`, `pose_checks`), come nel piano B precedente, mai partito. Il dair
fallisce ancora "ereditato" a ogni giro: un FAIL permanente insegna a ignorare i FAIL. Criterio: dair v4 e ftilt v3.1
senza FAIL di legibility per controlli che non si applicano; jab v12 identico.

## 7. Proporzioni Cascadeur -> manichino UE e pollice (valore 3)

- Proporzioni (piano C, Opus medium, desktop): `casc_to_author.py` riscala i target IK delle mani sulle lunghezze della
  catena UE mantenendo la direzione dalla spalla; clearance sulla posa authored prima del build. Chiude l'avambraccio
  nella testa (-6 cm al 5) senza cambiare la lettura delle braccia di v4c (lock del punto 4 sulle braccia come prova).
- Pollice (Sonnet, desktop per una sola immagine di verifica): `fingers` oggi raddrizza indice-mignolo e lascia il
  pollice da pugno; aggiungere `thumb` nel blocco (default: disteso lungo l'indice per la mano villain), una posa salvata
  riusabile. Decisione per l'owner: pollice lungo l'indice o aperto.

## 8. Disciplina di giro (costa zero, da scrivere nel README delle schede)

- Un giro = una modifica dichiarata (parti x frame) + `animpipe diff` + `review`; il resto resta sotto lock.
- Massimo 2 tentativi su una stessa correzione, poi handover con ciò che serve (regola 5 del progetto).
- Prima di toccare una parte si legge il blocco `approved`; se la correzione chiesta tocca una parte bloccata si chiede
  all'owner (una domanda sola) invece di sbloccare.

## 9. Ordine, modelli, editor

| # | Lavoro | Modello | Editor desktop | Stima |
|---|---|---|---|---|
| 1 | Lock `approved` + snapshot + `approve` + lint scheda (guardia 1) + `diff` | Sonnet | no | ~$2 |
| 2 | Guardia 2 in `ue_rig.build` + `<move>_keys.json` + prova su dair v4 | Opus medium | sì | ~$3 |
| 3 | `animpipe capture` dello strato Manuale (+ `--from-anim`), blocco `manual`, `build --manual=card` | Opus medium | sì | ~$3 |
| 4 | Regole README come check + `timing_source`, calibrazione su dair v4 e jab v12 | Sonnet | sì (calibrazione) | ~$2 |
| 5 | Legibility `strike` | Opus medium | sì | ~$5 |
| 6 | Proporzioni Cascadeur -> UE | Opus medium | sì | ~$4 |
| 7 | Pollice villain | Sonnet | sì (1 immagine) | ~$1 |

1 e 4 partono subito senza editor (anche in parallelo al thread dair). 2 e 3 in un solo thread sul desktop appena
libero. Un checkpoint per thread, handover ≤30 righe, nessuna decisione nuova salvo il pollice (punto 7). Lo strato
Manuale del thread dair resta com'è: i punti 3 e 4 gli aggiungono memoria in git, lock e diff.

Costo di questo thread: Fable, ~10 chiamate, contesto ~45k, circa $1.

## 10. Follow-up (owner 22:43Z): perché la carica del dair si è allontanata dalla posa 15 di Cascadeur

Causa (git delle schede overlord_dair, handover): la posa dell'owner è entrata UNA volta (v2, 8df94ff) come numeri nel
blocco `author` e da lì è stata riscritta a mano senza riferimento: v2 mani +9-16 cm avanti per la clearance; v3 (936fe5b)
poli dei gomiti 8-12 cm più stretti, mani 4-10 cm più indietro e 4 cm più basse ("sinergia"); v4 ha ereditato v3; 194c7d8
le ha alzate di 8-10 cm, e3628b6 è tornata a v3. In ogni scheda `poses[].source` è `null` e il JSON della posa Cascadeur
vive in `Saved/` (non in git): nessun check poteva confrontare, e `casc_to_author` riduce la posa a target IK (mano, polo,
piede) che poi si editano come numeri qualsiasi. Lock e diff della PR #24 avrebbero fermato v3 solo se v2 fosse stata
approvata; mancano tre cose, da fare nel thread desktop (Opus medium, ~$3):
1. **Pose dell'owner in git**: `cards/<kit>/refs/<move>_owner_poses.json` (export di `pose.get` per frame) e `source:
   {file, frame}` compilato nelle pose della scheda che vengono da lui.
2. **Check `source_fidelity`** (bloccante): per ogni posa con `source`, `casc_to_author` rigenera i target e li confronta
   con i campi `author` della scheda (mano/piede/polo ≤ 3 cm, busto/testa ≤ 5°, dita escluse); ogni scostamento voluto va
   dichiarato come `edits` con `why`, mai riscrivendo i numeri. Così il diff mostra "scheda vs owner", non solo "vN vs vN-1".
3. **Posa dell'owner come chiave diretta** (modo migliore, opzionale): importare lo Step FBX/posa come ossa sul rig e usarla
   come chiave (`build` già campiona le pose da `source_anim`), lasciando ad `author` solo i frame non posati dall'owner.
Ordine owner per il dair: carica = Cascadeur 15, rilascio = Cascadeur 23, entrambe approvate e bloccate (dita escluse).

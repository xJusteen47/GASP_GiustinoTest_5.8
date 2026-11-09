# Proiettili, piattaforme mobili, entità generate dai nemici (valutazione, 2026-11-09)

Solo analisi, niente codice. Domanda dell'owner (02:36Z): "proiettili stile hadoken, piattaforme e oggetti che si muovono/ruotano,
altre entità (anche generate dai nemici): le hai già considerate?". Risposta breve: **sì per metà**. Proiettili e piattaforme-nemico
sono già progettati (M10, many-enemies-replication §4) e una parte delle piattaforme è già costruita; mancano le piattaforme di
livello, i proiettili dei minion e le entità "a tempo" (zone, trappole, mine). Qui sotto: cosa c'è, opzioni, raccomandazione, costi.

## 1. Cosa esiste oggi (verificato nel codice di `claude/merge-feature-2026-11-08j`)

| Cosa | Stato |
|---|---|
| **Proiettili, progetto** | `docs/architecture/combat-m10-held-objects.md` §9-§11: proiettile = **segmento balistico analitico** (`PositionAt(frame)` in forma chiusa), autorità server, "fantasma" predetto sul tiratore, si replicano solo **eventi** (spawn, rimbalzo, colpo). Opzione NP-per-proiettile scartata (costo per entità). Non costruito (M10 non partito, piano PR #27 non in feature). |
| **Proiettili, aggancio combat** | `EGSTN_HitboxSpace::Projectile` esiste in M4 come "dello spazio proiettili, più avanti". Il sottosistema combat dei minion lo rifiuta (`GSTN_MinionCombatSubsystem.cpp` L121). |
| **Proiettili nel fork** | `AbilitySystemSimulation/ProjectilesSimulator`: proiettili predetti dentro lo stato sync dell'ASC (spawn, rimbalzo, hit, rollback). **Nessun codice GSTN lo usa**; colpisce con la logica propria dell'attore, non con la nostra pipeline hitbox/hurtbox/inbox; i minion non hanno ASC. Utile come riferimento, non come base. |
| **Piattaforme-nemico** | `GSTNPlatform` (T3): collider cinematici posati **una volta per frame di simulazione** (pose point su `OnPreSimulationTick` di Mover), stati per frame (anche in risimulazione), `FGSTN_PoseHistory` (GSTNCore). `UGSTN_ScriptedPlatformComponent` = **posa funzione del tempo** (avanti/indietro su un asse + oscillazione di yaw): esatta su ogni macchina. `GSTNPlatformParts` (T6): forma che sparisce quando si rompe la parte. |
| **Salire su una base mobile** | Mover di serie (base-local, riconciliazione in spazio base). **T7** (lag comp delle piattaforme, ciclo di vita sotto il giocatore) **non fatto**: saltato insieme al movimento, che verrà rifatto come modalità separata. |
| **Oggetti di livello** | `GSTNLevelObjects`: solo il trampolino (fermo). Nessuna piattaforma di livello mobile, nessuna trappola. |
| **Minion** | Solo mosse in mischia (`MinRange`/`MaxRange`). Nessun attacco a distanza. Lo spawner (`UGSTN_MinionSpawner`, ondate) esiste già. |
| **Lag compensation** | Rewind per numero di frame, `FGSTN_LagCompTime`, `MaxRewindMs` 400; minion da storico; piattaforme non ancora nel rewind (era T7). |

## 2. Il principio comune: "deterministico prima"

Tutte e tre le domande hanno la stessa risposta di fondo. Un oggetto che **segue una regola** (traiettoria, percorso, timer) non va
trasmesso frame per frame: si trasmette **una volta** "parte al frame F con questi parametri" e ogni macchina calcola la posizione
da sola. Vantaggi, tutti importanti per noi:

- **Banda quasi zero**: un evento da ~20-30 B invece di uno stream (rilevante: il nostro limite è la banda, vedi netcode-nemici-confronto).
- **Lag compensation gratis**: il rewind è "calcola la posa al frame rewindato", nessuno storico.
- **Predizione esatta**: il client può disegnare l'oggetto **al proprio frame predetto**, quindi quello che vedi è quello che il
  server controlla contro di te (niente "mi ha colpito ma era lontano").
- **Toaster**: un calcolo in forma chiusa costa meno di un'entità replicata.

Solo ciò che **pensa** (IA) o **reagisce alla fisica in modo libero** ha bisogno di stato replicato: per quello abbiamo già il
negozio minion (10 Hz, 24 B) e lo storico pose.

## 3. Proiettili (hadoken, anche dei nemici)

| Opzione | Pro | Contro |
|---|---|---|
| A. Modello NP per proiettile | rollback esatto | spawn/despawn in predizione, ID concordati, ~0.04 ms per proiettile per frame risimulato: sfora il budget |
| A'. `ProjectilesSimulator` del fork | già scritto, predetto | dentro lo stato sync dell'ASC (byte a ogni frame per proiettile), colpi fuori dalla nostra pipeline, niente minion |
| B. Attore replicato dal server | semplice | il tiratore lo vede partire RTT/2 in ritardo: brutto per un hadoken |
| **C. Segmento analitico + server + fantasma (M10 §9)** | eventi soli, `PositionAt` per rewind e disegno, stesso codice per giocatori e minion | rimbalzi/impatti sono eventi del server (arrivano dopo RTT/2 agli altri) |

**Raccomandazione: C, estratto in un plugin suo (`GSTNProjectile`) prima di M10**, invece che dentro `GSTNHeld`. Così lo usano
hadoken dei giocatori, attacchi a distanza dei minion e (dopo) oggetti lanciati di M10. Dettagli che contano:

- **Tiratore**: fantasma predetto al frame di rilascio (nessun ritardo), poi adotta il segmento del server con blend di 6 frame.
- **Vittima e osservatori**: disegnano il proiettile al **loro frame predetto** (forward, possibile perché la traiettoria è nota),
  non al tempo d'interpolazione. Solo la nascita arriva in ritardo: il proiettile "compare già partito" di qualche decina di cm,
  coperto da un flash di spawn. È quello che serve per schivare/parare un hadoken in modo leggibile.
- **Colpi**: decisi dal server a ogni frame fisso (sweep del segmento contro hurtbox, parti e mondo, incluse le piattaforme al pose
  point), poi la stessa **hit inbox** dei colpi in mischia: parata, armatura HD2 (P18), penetrazione e rottura parti funzionano
  senza codice nuovo. Per la vittima: al massimo una correzione, come oggi per i colpi dei minion.
- **Proiettile vs proiettile** (scontro hadoken, riflesso, deflect di M10): passo di risoluzione sul server; il deflect crea un
  nuovo segmento (evento).
- **Minion**: mossa con chiave "proiettile" nel timeline; il server la spara, il cervello StateTree sceglie solo bersaglio/tempo.
  Proprietario = `FGSTN_EntityRef{Minion}`.
- **Casi non analitici** (a ricerca, boomerang): un nuovo segmento ogni 100-200 ms dal server (evento), oppure diventano un tipo di minion.
- **Tetto**: `MaxActiveProjectiles` per mondo (es. 64), visual in pool (Niagara/ISM). Costo di un proiettile in volo: una sweep per frame sul solo host.

## 4. Piattaforme e oggetti che si muovono o ruotano

| Tipo | Come | Note |
|---|---|---|
| **Di livello, a percorso** (ascensori, ruote, pendoli, barre rotanti, nastri) | **Deterministico** (approccio C, già usato da `UGSTN_ScriptedPlatformComponent`): posa = f(frame) | esatto su tutte le macchine, 0 byte, mai correzioni a chi ci sta sopra |
| **Attivate da un evento** (leva, pulsante, piattaforma che cade quando ci sali) | Evento server "parte al frame F" + percorso deterministico | stesso schema dei proiettili; chi le attiva vede RTT/2 di ritardo, coperto da un telegrafo (tremolio) |
| **Nemici-piattaforma** | Già fatto (T3/T6): base interpolata, giocatore in spazio base (approccio B); D (posa come input) solo se i test lo chiedono | errori piccoli a salita/discesa proporzionali a velocità × lag |
| **Spinte da fisica libera** (casse spinte, detriti) | Server-autoritativi, pochi, interpolati con storico pose; i detriti puramente estetici restano solo locali | niente fisica in rete nel gameplay: con NP non si sincronizza |

Da aggiungere per il livello (quando M12 ne avrà bisogno): un attore in `GSTNLevelObjects` che riusa il pose point di `GSTNPlatform`,
percorsi più ricchi (spline/waypoint con soste ed easing, rotazione continua 360°, pendolo, ping-pong/loop, avvio da evento),
**trappole** = hitbox attaccate alla posa deterministica (pressa, lama rotante): poiché client e server calcolano la stessa posa allo
stesso frame, il colpo dell'ambiente si può **predire** nella simulazione della vittima. Parti rompibili su oggetti di livello
(ponte che crolla): la timeline di stato delle forme (`SetShapeMaskAtFrame`, Breaking con grace) esiste già.

**Attenzione al movimento**: la parte "stare sopra, essere trasportati, scendere con la velocità giusta" vive nel movimento, che
l'owner farà rifare come modalità separata. Quindi: la piattaforma (posa, percorso, collisione, rewind) si costruisce ora quando
serve; il comportamento del giocatore sopra si valida con il nuovo movimento. Il rewind delle piattaforme (ex T7) è piccolo per
quelle deterministiche (si valuta la posa al frame) e usa `FGSTN_PoseHistory` per i nemici-piattaforma; regola già scritta: un
solo `FGSTN_LagCompTime` per tutto, e per chi sta sopra si salva la trasformazione composta, non ricalcolata.

## 5. Entità generate dai nemici

Proposta: **due famiglie, non un terzo negozio generico**.

| Famiglia | Esempi | Dove vive |
|---|---|---|
| **Pensano** (IA, decisioni) | minion evocati, torrette, droni | negozio minion esistente (`UGSTN_MinionSpawner::Spawn` da un evento timeline del nemico; seam D-1 c'è già) |
| **Seguono una regola** | proiettili, onde d'urto, zone di fuoco/veleno, mine, muri temporanei | `GSTNProjectile` generalizzato a "entità a tempo": segmento (anche velocità 0) + durata + forma dell'hitbox nel tempo |
| **Ci si sta sopra / bloccano** | muro di ghiaccio, piattaforma evocata | forma piattaforma (pose point) posseduta da una delle due famiglie sopra |

Una zona di fuoco è "un proiettile fermo con durata 5 s e hitbox che pulsa": stesso codice, stessi eventi, stesso tetto e pool.

## 6. Costi e posizione nel piano (stime, Opus/Sonnet secondo le regole)

| Lavoro | Costo | Dove |
|---|---|---|
| `GSTNProjectile`: store, segmenti, passo server, eventi, fantasma, disegno al frame predetto, `HitboxSpace::Projectile`, chiave timeline | $15-25 | prima di M10 (M10 G3/G7 si riducono di ~$10-15: costo netto ~$5-10) |
| Attacco a distanza dei minion (mossa + cervello + test harness) | $4-6 | dopo il plugin, prima delle ondate di M12 |
| Hadoken di esempio del giocatore | $2-4 + animazione owner | con le mosse esempio BP (dopo 5.9) |
| Scontro/riflesso proiettili | $4-6 | M10 (deflect) o M14 PvP |
| Entità a tempo (zone, mine, onde) sullo stesso plugin | $5-8 | M11 kit o M12, quando un nemico/kit le usa |
| Evocazione minion da un nemico (evento timeline → spawner) | $2-4 | M12 |
| Piattaforme di livello deterministiche + avvio da evento | $8-14 | M12 (livelli) |
| Trappole su piattaforme (pressa, lama) | $5-8 | M12; in PvP gli hazard sono "nessuno" per ora (M14 G2) |
| Rewind piattaforme e chi sta sopra (ex T7, ridotto) | $6-10 | con il nuovo movimento |
| **Totale** | **~$50-85**, di cui ~$10-15 già contati in M10 | distribuito su M10-M12, nulla subito |

## 7. Decidere ora / dopo

- **Ora (owner, architettura)**: approvare il principio "deterministico prima" e lo spostamento dei proiettili in un plugin
  **`GSTNProjectile`** comune a giocatori, minion e M10 (raccomandato). Nessuna spesa adesso.
- **Dopo**: proiettili a ricerca, fisica di gioco in rete (sconsigliata), approccio D per i nemici-piattaforma (solo se i test di
  salita/discesa falliscono), hazard in PvP, comportamento sopra le piattaforme (con il nuovo movimento).
- **Rischi**: nascita in ritardo dei proiettili altrui (mitigata dal flash e dal disegno al frame predetto); costo host con molti
  proiettili su toaster (tetto + misura nel bench esistente); il rewind piattaforme dipende dal rifacimento del movimento.

Costo di questa analisi: ~$3-4 (Opus, solo lettura e doc).

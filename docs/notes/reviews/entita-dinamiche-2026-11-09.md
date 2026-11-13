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

## 8. Movement modes condivise (note dell'owner, 2026-11-09)

- **Il concetto.** Fighter e minion condividono le stesse **modalità di movimento** (camminata, balistico/lanciato, hover, ecc.).
  La modalità è il "come si muove" (regole, integrazione, transizioni); il trasporto è il "come arriva agli altri" (replica, frequenza,
  formato sul filo). Sono due strati separati: la stessa modalità può girare dentro Mover + Network Prediction (fighter) o dentro lo
  store dei minion sull'host (minion) senza cambiare le sue regole.
- **Interfaccia.** Verrà definita quando il movimento dei fighter sarà rifatto come modalità propria. Fino ad allora niente interfaccia
  comune scritta "in anticipo": la si estrae dal primo caso reale.
- **Minion.** Restano simulati sull'host, replicati per tier (T0/T1/T2/T3, pack da 16, budget di rilevanza) come oggi. Condividere la
  modalità non li sposta in Network Prediction.
- **Proiettili.** Usano una modalità **"balistico"**: stessa matematica del lanciato, coerente con il principio "deterministico prima"
  del §2 (nascita con parametri, simulazione locale).
- **Formati sul filo attuali** (riferimento, non da cambiare per questo punto):
  - fighter: stato Mover quantizzato e delta contro la base confermata, `docs/notes/handovers/netcode-lever1.md`;
  - minion: item del pack con posizioni relative all'ancora del pack (leva 3), `docs/notes/handovers/m8-netcode-34.md`.

## 9. Oggetti fisici (note dell'owner, 2026-11-09)

Tre categorie, ognuna con la sua autorità:
- **(a) Rilevanti per il gameplay**: simulazione deterministica, "nasce una volta con i parametri, poi si simula in locale" su ogni
  macchina (come i proiettili del §3). In rete viaggiano solo la nascita e gli eventi.
- **(b) Cosmetici**: solo locali, **mai replicati** (detriti, schegge, oggetti che non cambiano il gioco).
- **(c) Pochi oggetti pesanti a fisica vera**: autorità solo dell'host, **5-10 al massimo** contemporaneamente.

Regole comuni:
- **Sleep** appena l'oggetto si ferma; un oggetto fermo non costa né CPU né banda.
- **Rottura a eventi**: la rottura è un evento (chi, cosa, quando), non uno stato fisico replicato di continuo.
- **Spinte come impulsi**: chi spinge applica un impulso, non una posizione.
- **I personaggi sono cinematici**: spingono gli oggetti, **mai il contrario** (un oggetto non sposta un fighter o un minion).
- Gli oggetti sono **ostacoli della navmesh**.
- **Soglia di risveglio** (sotto non si sveglia) e **tetto sugli oggetti svegli** (oltre il tetto i più vecchi/lontani tornano a dormire).

## 10. Confronto con Fortnite (note dell'owner, 2026-11-09)

- **Fortnite (cifre dell'owner, approssimate):** aggiornamento di rete ~30 Hz; per client ~20-60 KB/s in download e ~5-10 KB/s in upload.
- **Noi, misurato** (per client, host + 2-3 client, laptop/desktop):
  - G12b (`docs/notes/handovers/m8-minion.md`, T0 10 Hz): minion N=40 ~5 KB/s, N=80 7.5-8.1 KB/s, N=150 ~6.4 KB/s sopra una base
    giocatori di ~45 KB/s; upload host a N=80 con 3 client 23.7 KB/s.
  - Leva 1 (`netcode-lever1.md`): giocatori fermi N=0 **50.2 -> 21.7 KB/s** (-57 %).
  - Leva 2 Oodle (`m8-hover-oodle.md`): giocatori soli 35.3 -> 4.8 KB/s, N=80 totale 44.0 -> 9.7 KB/s (dizionario addestrato sullo
    stesso scenario: ottimistico).
  - Leva 3 (`m8-netcode-34.md`): -10.5 % dei bit degli item minion; con Oodle attivo il guadagno è nel rumore.
  - Leva 4: punteggio host, nessun effetto sui byte.
  In sintesi siamo sotto la fascia di download di Fortnite con 80 minion; l'upload del client resta il solo input (frequenza fissa).
- **Cosa fa costare Network Prediction** (trovato con `np.Debug.NetBytes`, NP = 89 % del filo prima della leva 1):
  - **float pieni**: `FSmoothWalkingState` mandava 5 double interi (128 B);
  - **niente delta compression**: ogni update rimandava tutto lo stato sync;
  - **reinvii ridondanti**: nome della modalità come stringa (16 B), modificatori e collection rimandati anche se uguali.
- **Cosa ha fatto la leva 1:** quantizzazione di `FSmoothWalkingState` (128 -> ~5 B); delta contro la base confermata per nome modalità,
  modificatori ed elementi della collection (1 bit se uguali); Mover SP 243 -> 52 B per update, AP 179 -> 26 B. Restano senza delta
  Combat e ASS sync (43 + 42-108 B/update) e l'input SP.

## 11. Frequenza di simulazione vs frequenza di invio per modalità (note dell'owner, 2026-11-09)

- **Fissi, sempre** (regola da picchiaduro): simulazione a **62.5 Hz** e invio dell'input client -> host a ogni tick. Non cambiano
  con la modalità di gioco, perché l'input e il timing dei colpi devono essere identici ovunque.
- **Variabile per modalità**: solo la frequenza con cui l'host manda lo stato ai client (host -> client).
  - **PvP e livelli classici**: frequenza piena.
  - **Orde PvE**: stato ai simulated proxy **ogni 2 tick**, tramite la cvar `gstn.Net.SimProxySendEveryNTicks` (default 1 = ogni tick).
- Stato: misura in corso sul desktop; i risultati andranno in `docs/notes/handovers/netcode-step3.md` quando arriva. Nessun aggancio alla
  modalità di gioco finché la misura non c'è.

Costo delle sezioni 8-11: ~$1 (solo doc, dalle note dell'owner e dagli handover citati).

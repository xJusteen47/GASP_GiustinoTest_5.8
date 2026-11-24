# 1v1 classificato in rete senza server dedicato: ricerca (2026-11-23)

Ambito: **solo ricerca**, nessun codice toccato. Nato come "server dedicato per il ranked" (decisione 3A); l'owner ha poi
precisato che **all'inizio non ci sarà un server dedicato** (costo) e che i picchiaduro di solito usano P2P con rollback.
Quindi il vincolo vero è: **1v1 tra due giocatori, uno dei due fa da host o i due sono pari**. Il server dedicato resta
come sezione breve (§7, "se mai in futuro").

Riuso dichiarato (regola 1): `docs/INDICE-PROGETTO.md` (branch `claude/indice-progetto`, glossario NP e §3 "chi chiama
chi"), `docs/notes/input-azioni-mappa.md` (branch `claude/project-thread-uvefj5`: tre canali di input, NP è
client-server), `docs/notes/networking-spiegato.md`, `docs/architecture/combat.md` §2, §5.1, §12.4, §12.7,
`docs/architecture/testing.md`, l'artefatto "Networking di GASP Giustino" (8-11, confronto in §8). Nessun codice nuovo.

## Riassunto in 10 righe

1. **Sì, abbiamo già un rollback "misto"**: il tuo personaggio è predetto e, se l'autorità dice altro, torna indietro e
   rigioca i frame (combattimento, abilità e Mover insieme). È verificato nel codice e nei test di rete (§1).
2. Ma è un rollback **client-server**: si torna allo *stato* mandato dall'autorità. L'avversario **non** viene predetto: lo
   vedi interpolato, nel passato. Un rollback P2P "alla GGPO" predice anche l'avversario e scambia solo input.
3. Network Prediction (NP) ha sempre un'autorità: senza server dedicato l'autorità è **l'host (listen server)**. Funziona già:
   è la configurazione principale del nostro harness di rete (testing.md riga 25).
4. Il costo del listen server: l'host ha latenza zero e vede tutto "vero"; l'ospite subisce i colpi come correzioni (rollback
   di circa RTT/16 ms frame) e vede l'host in ritardo. Per un ranked l'host è anche un punto di fiducia (può barare).
5. Connessione tra case (NAT): Epic Online Services offre P2P "regardless of firewalls, router configuration" e "without
   royalty or hosting fees" (fonte §9). Non serve un server nostro per mettere in contatto due giocatori.
6. Il pezzo che più accorcia il ritardo sull'avversario esiste già come prova: la **predizione in avanti** dello spike S-FP
   (combat.md §12.7). In un 1v1 vale per una sola entità, quindi i suoi costi (rollback per input remoto) sono sostenibili.
7. Un rollback P2P **vero e deterministico** (due pari senza autorità) con Mover + GAS + collisioni Chaos non è realistico a
   breve: servirebbe determinismo bit a bit tra due PC, cosa che nessuna fonte Epic promette. Sarebbe una simulazione nuova.
8. Le scelte già fatte (comando unico per frame `FGSTN_InputCmd`, timing in frame, stato di combattimento nel sync state)
   **aiutano** qualunque futuro rollback; i tre canali di input in parallelo invece vanno unificati prima.
9. Consiglio: **A = listen server + EOS P2P + predizione dell'avversario nel 1v1 + limite di ping nel ranked**, a piccoli passi.
10. Decisioni all'owner: 3 (§6). Cosa non è verificato: §9.

## 1. Cosa fa oggi la nostra rete (verificato nel codice, salvo dove scritto "dedotto")

| Fatto | Prova |
|---|---|
| Simulazione a passi fissi 62,5 Hz; altri giocatori **interpolati**; buffer di interpolazione **50 ms**; smoothing acceso; 6 input ripetuti per pacchetto | `Config/DefaultNetworkPrediction.ini` riga 4 (`FixedTickFrameRate=62.5`, `SimulatedProxyNetworkLOD=Interpolated`, `FixedTickInterpolationBufferedMS=50`, `FixedTickInputSendCount=6`) |
| Ordine del tick per ogni entità: combattimento (47) → abilità/ASC (48) → Mover (50); al rollback **tutti e tre** sono ripristinati allo stesso frame e rigiocati | `docs/architecture/combat.md` §2 fatto 8 e §5.1 (verificato in M0, dichiarato "verified in code") |
| Mover passa da NP | combat.md §2 fatto 2 (priorità Mover = 50 nel tick NP). Documentazione Epic: Mover supporta il rollback "con Network Prediction o con la fisica in rete di Chaos" (§9). Che il nostro personaggio usi il backend NP e non Chaos è **dedotto** da combat.md, non riletto negli asset |
| Lo stato di combattimento è **dentro** il rollback: hitstop, timeline della mossa (frame di inizio, pause), salto, schivata, contatto, richieste, stato dei colpi, input | `Plugins/GSTNCombatNP/Source/GSTNCombatNP/Public/Simulation/GSTN_CombatSimTypes.h` `FGSTN_CombatSyncCore` (righe 102-208: `Input` r.105, `HitstopStartFrame` r.108, `TimelineStartFrame` r.145, `Hit` r.208) |
| Buffer e storia degli input sono nel sync state (quindi rigiocati) | `Plugins/GSTNInputNP/Source/GSTNInputNP/Public/Simulation/GSTN_InputSimTypes.h` `FGSTN_InputSimState` r.155, `History` r.158, riconciliata r.221 |
| Comando unico per frame del combattimento: bottoni tenuti/premuti, camera, comando forzato, mira | stesso file, `FGSTN_InputCmd` r.23 |
| Ogni tempo del combattimento è un **numero di frame** (finestre, cancel, buffer): niente tempo assoluto | combat.md §2 fatto 4 |
| L'avversario interpolato **non simula mai**: riceve solo stati interpolati | combat.md §2 fatti 5 e 10 |
| Colpi: l'attaccante predice il proprio lato; la vittima è decisa dall'autorità; **ogni colpo subito costa un rollback di circa RTT/16 ms frame** | combat.md §12.7 |
| Lag compensation: l'autorità riavvolge i bersagli al tempo che l'attaccante vedeva, max `MaxRewindMs` | `GSTN_LagCompensation.cpp` r.15-105; `NetworkPredictionWorldManager.cpp` r.1012-1040 (fork) |
| Listen server già supportato e testato (host = autorità e giocatore) | `docs/architecture/testing.md` r.25, 41 |
| Nessun target Server, nessun Online Subsystem attivo | `Source/` contiene solo `GASP_Giustino2.Target.cs` e `...Editor.Target.cs`; `.uproject` senza plugin Online/EOS/Steam (letto con script) |

**Risposta alla domanda "abbiamo già un rollback misto?"**: sì. Il personaggio locale è predetto e riconciliato con
risimulazione completa di combattimento, abilità e movimento; buffer di input, frame della mossa, hitstop e finestre sono nel
sync state e tornano indietro con lui. È lo stesso meccanismo visivo dei picchiaduro con rollback (snap al frame giusto).
**La differenza** con un rollback P2P vero:

| | Il nostro (NP) | Rollback P2P (stile GGPO) |
|---|---|---|
| Chi decide | Un'autorità (server o host) | Nessuno: i due pari calcolano lo stesso risultato |
| Cosa viaggia | Input verso l'autorità, **stato** verso i client | Solo **input** tra i pari |
| L'avversario sul mio schermo | Interpolato, nel passato (oggi) | Predetto ("ripeto il suo ultimo input"), poi corretto |
| Cosa serve | Stesso risultato client/autorità *a parità di stato*; se sbaglia, lo stato del server ripara | **Determinismo bit a bit** tra due PC: un errore non si ripara mai, la partita diverge (desync) |
| Quando torno indietro | Quando lo stato dell'autorità differisce | Quando arriva un input remoto diverso da quello predetto |

## 2. Ritardo con cui si vede l'avversario

Misure esistenti (laptop, server dedicato + 2 client, 60 ms per direzione, 15 ms di variazione; `networking-spiegato.md` §4,
`combat.md` §12.7):
- dal tasto al server: ~180-220 ms (rete + coda input 4 frame = 64 ms + allineamento);
- dal server allo schermo dell'altro: ~70-130 ms (rete + buffer 50 ms + allineamento);
- con predizione in avanti dell'avversario (spike S-FP) l'osservatore vedeva un dash remoto 40-50 ms prima (73-82 ms invece di
  108-132), al prezzo di ~14 frame di rollback per ogni nuovo input remoto (~5 rollback/s per giocatore attivo) e di salti di
  2-3 m in un frame sulle azioni improvvise non predette (manca lo smoothing della correzione sul proxy).

**In listen server (dedotto, non misurato)**: per l'host il proprio personaggio e l'autorità coincidono (0 ms); l'ospite gli
appare dopo un viaggio + la coda di input. L'ospite vede sé stesso predetto e l'host interpolato (viaggio + buffer). Quindi
l'host vede la mossa dell'ospite prima di quanto l'ospite veda la mossa dell'host, e solo l'ospite subisce correzioni sui
colpi. È il "vantaggio dell'host" noto.

**Le tre famiglie usate dai picchiaduro** (fonte infil.net, §9):
- **delay-based**: si aspetta l'input remoto ritardando anche il proprio; deterministico, niente correzioni, ma la latenza si
  sente nelle mani e varia con la rete;
- **rollback**: si simula subito con l'input remoto predetto e si torna indietro quando arriva quello vero; richiede salvare e
  caricare lo stato ogni frame, velocemente, e un gioco deterministico; la fonte cita una pausa di 1-2 frame per chi è avanti e
  usa 3 frame di ritardo come esempio (non come valore tipico);
- **server autoritativo con predizione** (il nostro): giusto per chi è predetto, ma l'avversario arriva in ritardo o, se
  predetto in avanti, con correzioni.

**Cosa significa per mosse con hitbox e finestre di frame**: i nostri tempi sono in frame dentro lo stato (buona cosa: un
rollback non sposta le finestre). Il problema è *cosa vedi*: con l'avversario interpolato reagisci a una mossa che è già
iniziata da 4-8 frame (a 62,5 Hz, 70-130 ms ≈ 4-8 frame); parry e whiff punish con finestre di pochi frame diventano più
difficili per chi non è host. Con la predizione dell'avversario vedi l'inizio della mossa prima, ma le prime frame di
un'azione improvvisa "saltano" (come in ogni gioco con rollback).

## 3. Cosa permette il nostro stack senza server dedicato

- **(a) Listen server**: pronto oggi, testato. Serve solo come trovare l'host (§3c). Contro: vantaggio e fiducia dell'host.
- **(b) Rollback P2P deterministico vero**: richiederebbe che due PC diano risultati identici bit a bit su Mover (sweep di
  collisione nella scena Chaos), GAS fork, root motion dei montage, e serializzazione di tutto lo stato a ogni frame senza
  autorità che ripari. Nessuna fonte Epic dichiara Mover/NP deterministici tra macchine diverse; NP è costruito per
  correggere con lo stato dell'autorità. Strada realistica = **una simulazione di combattimento dedicata** (stile picchiaduro
  2D/3D, con matematica controllata) separata da Mover: lavoro da mesi, non da thread. La fonte infil.net riporta che
  aggiungere il rollback tardi a un picchiaduro (MKX) costò "approximately 8 man-years". Non consigliato ora.
- **(c) Alternative a basso costo**:
  1. *Host con predizione* (oggi) + **predizione in avanti del solo avversario** nel 1v1 (spike S-FP portato a produzione):
     è l'equivalente più vicino a GGPO dentro NP. Nel 1v1 è una sola entità, il limite "mai i minion" non si pone.
  2. *Ritardo d'input all'host* (delay-based leggero): ritardare di N frame il comando locale dell'host per pareggiare il
     vantaggio. Facile perché il comando è un'unica struttura per frame. Da misurare: NP non lo offre, va fatto nel nostro
     produttore di input (dedotto, non provato).
  3. *Relay/NAT gratuito*: EOS P2P (o Steam Datagram Relay se il gioco va su Steam, §9). Nessun server nostro.
  4. *Ranked con limite di ping*: matchmaking che non accoppia oltre una soglia (soglia da misurare in prova, non la invento).
  5. *Server economico solo per matchmaking/classifica*: EOS offre già lobby/sessioni (pagina multiplayer, §9); un nostro
     server servirebbe solo per regole anti-cheat del ranked, più avanti.
- **(d) Le scelte già fatte**: comando unico per frame (`FGSTN_InputCmd`), storia input nel sync state, timing in frame, stato
  di combattimento serializzato (`NetSerialize`/`ShouldReconcile` obbligatori, combat.md r.206) sono **esattamente** i
  prerequisiti di un rollback a input. Ostacoli: i **tre canali di input** paralleli (input-azioni-mappa.md punto 3: comando
  GSTN, input Mover dal Blueprint GASP, `FAbilitySimInputCmd` del fork) e le cvar debug lette in simulazione. Unificarli
  (piano P1-P5 di quella mappa) serve sia oggi sia per ogni rollback futuro.

## 4. Opzioni

| | Cosa | Pro | Contro | Costo stimato* |
|---|---|---|---|---|
| **A (consigliata)** | Listen server + EOS P2P + predizione in avanti del solo avversario nel 1v1 + limite di ping; ritardo d'input all'host come opzione regolabile | Riusa tutto; nessun costo di hosting; l'avversario si vede ~40-50 ms prima (misura S-FP) | L'host resta autorità (fiducia, vantaggio residuo); 4 patch del fork da fare | 25-45 $ in 5-6 thread |
| B | Solo listen server + EOS P2P, avversario interpolato | Il più economico; gira già | Ospite svantaggiato in modo visibile su parry/punish | 8-15 $ |
| C | Simulazione di combattimento deterministica nuova + rollback P2P a input | Il "vero" netcode da picchiaduro, simmetrico | Mesi di lavoro; Mover/GAS fuori dal combattimento 1v1 o riscritti | non stimabile con serietà ora (ordine di centinaia di $ e mesi) |

\* Stime a occhio dai costi dei thread di codice registrati in memoria progetto (5-12 $ per thread cloud di codice, desktop per
build e test); non verificate contro `handoffs/INDICE.md`.

## 5. Piano a passi piccoli per A (un thread = un checkpoint)

1. **Misura listen 1v1** (desktop, Haiku/Sonnet, ~3-5 $): harness in listen, 2 giocatori, rete 30/60/90 ms; misurare ritardo
   visto da host e ospite e rollback per colpo. Serve il PC.
2. **Unificazione input P1/P4/P2** (già pianificata in input-azioni-mappa.md, ~10-14 $): prerequisito.
3. **EOS P2P per il listen server** (cloud codice + desktop prova, ~5-8 $): plugin Online Subsystem EOS, login, sessione,
   connessione P2P. Serve un account sviluppatore Epic dell'owner (credenziali: le mette lui). Prova vera tra due reti diverse.
4. **Predizione dell'avversario nel 1v1** (Opus, ~10-15 $): le patch elencate in combat.md §12.7 (LOD per entità, input
   ripetuti sul proxy, smoothing della correzione, inbox colpi lato client, bug degli attributi base), solo per l'avversario.
5. **Ritardo d'input all'host** come cvar/opzione (Sonnet, ~3 $) e limite di ping nel matchmaking (~3 $).
6. **Prova a distanza vera** con un amico (owner, nessun costo agente) e video.

Rischi: le patch del fork per la predizione sono le parti più delicate (desync, salti visivi); EOS aggiunge login e
dipendenza da un servizio esterno; un host può manipolare la partita (ranked non affidabile senza controlli lato servizio).

## 6. Decisioni per l'owner

1. Strada per il 1v1 online: **A** listen + EOS + predizione avversario (consigliata) · B solo listen + EOS · C simulazione
   deterministica nuova. Resta fermo: niente finché non si sceglie; il passo 1 del piano si può fare comunque.
2. Piattaforma del P2P: **A** EOS (gratis, multipiattaforma; consigliata) · B Steam (solo se il gioco esce su Steam).
3. Vantaggio dell'host: **A** ritardo d'input all'host regolabile, spento di default e deciso in playtest (consigliata) · B
   nessuna compensazione.

## 7. Se mai in futuro: server dedicato (breve)

- Richiede un **motore compilato da sorgente** ("You must use a source build of Unreal Engine") e i file
  `<Progetto>Server.Target.cs` / `<Progetto>Client.Target.cs`, configurazioni "Development Server/Client", cook con Build
  Target "Server" (documentazione Epic, §9). Oggi usiamo il motore installato da launcher: è il primo costo.
- Il nostro codice è già pensato per l'autorità separata (harness "dedicated" come controllo secondario, testing.md r.129).
- Costi di hosting: **non raccolti** in questa ricerca (prezzi variabili, da cercare quando servirà).

## 8. Artefatto "Networking di GASP Giustino" (8-11): cosa è superato

Coerente con il codice di oggi su: 62,5 Hz, buffer 50 ms, coda input 4 frame, 6 input per pacchetto, lag compensation,
trade F→F+1. **Superato**: la nota "sul ramo feature il buffer è ancora 100 ms" (oggi in `main` è 50 ms, riga 4 dell'ini) e il
riferimento al ramo `claude/fp-g0c-laptop` come base (oggi la base è `main` a5b20950). Il README del fork NP dice ancora "100
ms" (`Plugins/GASNPP/README.md` r.61): da correggere. Non tratta il caso listen/P2P né il ranked.

## 9. Cosa NON è verificato e fonti

Non verificato: ritardi in listen server (dedotti, mai misurati in questa ricerca); che il pawn usi il backend NP di Mover e
non Chaos (da combat.md, non dagli asset); determinismo tra PC diversi (nessuna prova fatta); funzionamento di EOS P2P con il
nostro NP (nessuna prova); presenza di relay EOS (la pagina non li cita, parla di connettività attraverso firewall); fattibilità
del ritardo d'input all'host; nessuna build o PIE in questo thread.

Fonti:
- Epic, *Setting Up Dedicated Servers*: https://dev.epicgames.com/documentation/unreal-engine/setting-up-dedicated-servers-in-unreal-engine
- Epic, *Mover in Unreal Engine*: https://dev.epicgames.com/documentation/unreal-engine/mover-in-unreal-engine
- Epic, API Network Prediction (`FNetworkPredictionSettings`): https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/NetworkPrediction/FNetworkPredictionSettings
- Epic Online Services, multiplayer: https://onlineservices.epicgames.com/multiplayer
- Redpoint (plugin EOS di terzi), P2P vs IP: https://docs.redpoint.games/eos-online-subsystem/docs/networking_p2p_vs_ip ("If you have players hosting games (as listen servers), you must be using peer-to-peer connections")
- Valve, Steam Datagram Relay: https://partner.steamgames.com/doc/features/multiplayer/steamdatagramrelay (non letta in dettaglio)
- infil, *Netcode* (delay-based vs rollback): https://words.infil.net/w02-netcode-p5.html
- Interni: combat.md §2, §5, §12.7; networking-spiegato.md; testing.md; input-azioni-mappa.md; spike-sfp-report.md.

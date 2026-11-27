# Sezione MMO: piano per i limiti di scala (CPU del server, rete, test di carico)

Piano, nessun codice. Scritto il 25/11/2026 su richiesta dell'owner ("mettili in un piano a parte"). Parte **quando la sezione MMO
esisterà nel design**, non prima. È separato dal piano "Portare tutto il gameplay nel rollback"
([artifact](https://claude.ai/artifact/SfrTpktgxySeSCUc2gWkPp)), che prevede già l'interruttore di partita **Rollback completo /
Scalato**: questo file dice cosa serve perché la modalità Scalato regga molti giocatori e nemici su un server dedicato.

Etichette di affidabilità come in [many-enemies-replication.md](../many-enemies-replication.md): **[V]** verificato nel codice o nei
doc del repo; **[K]** conoscenza del motore non ricontrollata ora; **[E]** stima mia, da misurare. Le stime in $ partono dai costi dei
thread registrati (5-12 $ per thread cloud di codice, [server-dedicato-ricerca.md](../../notes/server-dedicato-ricerca.md) §4).

Documenti di partenza: [tag-verbi-simulazione-rollback.md](../tag-verbi-simulazione-rollback.md) (le cinque zone; arriva in `main` con la PR #55, copia nei project files `architettura/`),
[combat-m8-minions.md](../combat-m8-minions.md) (§7 fasce LOD, §8 pacchetti a lotti, §11 budget),
[combat.md](../combat.md) §12.7 (modello di rete, spike S-FP), [server-dedicato-ricerca.md](../../notes/server-dedicato-ricerca.md)
(§7 server dedicato), [game-design-direction.md](../game-design-direction.md) §6b (oggi listen server).

---

## 0. Risposta breve alla domanda dell'owner (25/11, 19:24)

**"Si può tornare a un modello ibrido con server dedicato se ci sono molti giocatori e nemici?"** Sì, ed è già la modalità
**Scalato** del piano rollback. Non è un ritorno al misto vietato da "niente misto" (decisione B del 25/11): tutto il gameplay resta
nella simulazione con rollback (zona A). Cambia solo **cosa ogni client predice e cosa interpola**: il proprio personaggio sempre
predetto con rollback, gli altri giocatori e i minion interpolati (o predetti solo i 1-2 che ti combattono da vicino, combat.md §12.7).

**"Oppure rollback completo lato server per i giocatori?"** Il server non torna mai indietro: è lui l'autorità e simula ogni frame
una volta sola [V] (tag-verbi §1, zona C nota; la lag compensation riavvolge solo le posizioni delle vittime per il colpo, zona B). Il
rollback avviene **sempre e solo sui client**. Quindi "rollback completo per i giocatori" c'è già in entrambe le modalità: ogni
giocatore è in rollback sul proprio client. Il limite della scala non è il rollback, sono tre costi diversi:

| Costo | Dove | Cresce con | Sezione |
|---|---|---|---|
| CPU della simulazione | server | giocatori × 3 modelli NP × 62,5 Hz, più il lotto dei minion | 2 |
| CPU del rollback | ogni client | entità predette × frame risimulati × rollback al secondo | 3 |
| Rete | server (upload) e client | giocatori e nemici rilevanti × frequenza × byte | 4 |

---

## 1. Cosa esiste già e si riusa

| Pezzo | Stato | Dove |
|---|---|---|
| Simulazione indipendente da chi è autorità (nessun `IsServer()` nel codice simulato) | [V] regola del progetto | CLAUDE.md regola 1, gas-npp.md |
| Server dedicato come configurazione dell'harness (secondaria) | [V] | testing.md, `Tools/GASNPPTest/gasnpp_test.py` |
| LOD di rete NP: `Interpolated` / `ForwardPredict`, oggi **globale** (`SimulatedProxyNetworkLOD`) | [V] | `Plugins/GASNPP/NetworkPrediction/Source/NetworkPrediction/Public/NetworkPredictionSettings.h` |
| Patch "LOD per entità" già elencata per la predizione in avanti | da fare | combat.md §12.7 |
| Minion a lotti: store sul server, fasce T0-T3 con passo di tick diverso, pacchetti `AGSTN_MinionPack` con FastArray, push model, dormienza, tasso per oggetto, rilevanza a budget (`MaxRelevantPerClient`, `HostUploadBudgetKBps`) | [V] doc, codice G0-G12 | combat-m8-minions.md §7, §8 |
| Bench dei minion (`gstn.Minion.Bench.*`) e scenari `SUITE_MINION_BENCH` (`minion_horde`, `minion_bytes`) | [V] | testing.md, `Tools/GASNPPTest/minion_scenarios.py` |
| Interruttore Rollback completo / Scalato e requisito "la presentazione legge solo lo stato, mai *sono predetto*" | piano | artifact del piano rollback, F0/F2/F9 |

Niente di questo piano tocca la simulazione: cambia **chi simula dove** (zone, istanze), **quanto spesso si replica** e **cosa si
predice**. Le regole d'oro della simulazione restano intatte.

---

## 2. CPU del server

### 2.1 Il costo

Ogni giocatore porta **3 modelli NP** a 62,5 Hz: combattimento `GSTN_Combat` (con l'input dentro), ability system del fork, Mover
[V] (tag-verbi §1.1). In tick fisso NP tutti i modelli avanzano **ogni** frame, insieme (è quello che rende possibile il rollback di
gruppo) [V] (`ENetworkPredictionTickingPolicy::Fixed` in `NetworkPredictionConfig.h`).

Stima di partenza [E]: dalla misura S-FP (0,03-0,04 ms per entità predetta per frame risimulato, sul client, combat.md §12.7) un
giocatore costa al server **0,04-0,1 ms per tick** con i tre modelli. Con il budget di 16 ms al frame e circa metà per la simulazione:

| Giocatori per processo server | Simulazione giocatori [E] | Con 60 minion svegli (M8 §11, ≤ 2 ms) | Lettura |
|---|---|---|---|
| 20 | 0,8-2 ms | 3-4 ms | comodo |
| 50 | 2-5 ms | 4-7 ms | ci sta, da misurare |
| 100 | 4-10 ms | 6-12 ms | al limite: serve una delle leve sotto |

A questo si aggiunge la replica con il sistema classico: il lavoro per connessione cresce con (connessioni × attori rilevanti), ed è
quello che Replication Graph e Iris riducono [K] (many-enemies §1.1). Sotto le 50 connessioni non serve; sopra va misurato (§5, S6).

### 2.2 Le leve, in ordine di costo e rischio

1. **Tetto di giocatori per istanza** (consigliata come base). Ogni zona di combattimento è un'istanza (un processo server) con un
   tetto misurato in S3, per esempio 24-50. Le zone sociali senza combattimento possono averne di più perché i giocatori lì costano
   meno (abilità ferme, Mover a passo). È il modello di Monster Hunter, Destiny, Lost Ark per le zone di combattimento [K].
2. **Costo ridotto fuori dal combattimento, senza cambiare il tick.** Un giocatore che cammina in una zona tranquilla fa poco lavoro
   in ogni modello (nessuna timeline attiva, nessuna query di colpo, abilità inattive). Si misura e si accorcia il percorso "a riposo"
   dei tre modelli. Nessuna patch al fork, resta deterministico.
3. **Tick ridotto per chi è lontano dai combattimenti** (quello che chiedeva la nota del coordinatore). Per i **minion esiste già**:
   le fasce T1/T2/T3 avanzano ogni 2, 4, 16 frame (M8 §7), sicuro perché timeline e cooldown sono timbri di frame. Per i
   **giocatori non esiste**: NP in tick fisso fa avanzare tutti insieme e il client manda un input per frame. Farlo vuol dire una
   patch al fork (un modello che salta frame e consuma gli input accumulati), con rischio sul rollback di gruppo. Si fa **solo** se S3
   mostra che 1 e 2 non bastano, come spike con uscita netta (S5b). Non si abbassa invece il tick globale del server (per esempio a
   30 Hz): le timeline sono in frame a 62,5 Hz e cambierebbe ogni mossa.
4. **Mondo diviso su più server**. Due forme:
   - **Istanze con passaggio** (consigliata): lasciare una zona = viaggio verso un altro processo, con una breve transizione. Unreal lo
     fa già (server travel, sessioni) [K]; il lavoro è il passaggio dello stato del giocatore (loadout, vite, progressi: zona C/E,
     effetti esterni una volta sola).
   - **Mondo continuo su più server** (un confine invisibile tra due server): Unreal ha un plugin sperimentale di replica multi-server
     [K, da verificare per 5.9], ma con NP, Mover e il fork è un progetto a sé, di mesi. Non in questo piano.

---

## 3. CPU del rollback sui client

Più entità predette = rollback più cari: ogni correzione risimula tutte le entità predette per ~RTT/16 ms frame. Dato S-FP [V]:
~14 frame per rollback, ~5 rollback al secondo per ogni giocatore remoto predetto, 0,03-0,04 ms per entità per frame.

In Scalato la regola è quella già decisa in combat.md §12.7: **predetti solo il proprio personaggio e al massimo 1-2 entità che ti
combattono da vicino**; tutti gli altri giocatori e tutti i minion interpolati. Così il costo del rollback sul client non cresce con
la folla. Serve la patch "LOD per entità" (oggi il LOD è globale e Mover lo legge in 3 punti [V]), che è già nel lavoro di predizione
in avanti: questo piano la riusa, non la rifà.

---

## 4. Rete

### 4.1 Il costo

Per client in download: giocatori rilevanti × frequenza × byte dello stato NP, più i minion (M8 §8.3). Sul server in upload: la somma
su tutti i client. I byte per aggiornamento di un giocatore (stato di sync di combattimento, Mover, ability system) **non sono
misurati**; li misura S3. Esempio di dimensionamento [E], 50 giocatori in un'istanza, ipotesi 100 B per aggiornamento completo:

| Chi vedo | Quanti | Come | Frequenza | KB/s per client |
|---|---|---|---|---|
| Giocatori vicini (stesso combattimento) | 8 | attore NP completo, interpolato | 30 Hz | 24 |
| Giocatori lontani ma visibili | 20 | voce compatta a lotti (§4.2) | 5 Hz | 2 |
| Giocatori fuori portata | 21 | niente | 0 | 0 |
| Minion svegli | 60 | pacchetti M8 | per fascia | 8,3 |
| **Totale** | | | | **~35** |

Server: ~35 KB/s × 50 client ≈ 1,7 MB/s ≈ 14 Mbit/s di upload [E]: nulla per un server in datacenter, impossibile per un host di casa.
Per questo la sezione MMO presuppone il server dedicato (il listen server resta per 1v1 e co-op piccolo).

### 4.2 Le leve

1. **Rilevanza a budget per i giocatori**, come per i pacchetti dei minion: per ogni connessione si ordinano i giocatori per priorità
   (in combattimento con me, vicino, in vista, compagno di squadra) e si concedono fino a un tetto `MaxRelevantPlayersPerClient` e a un
   budget in KB/s; isteresi di 1 s come M8 R5.
2. **Frequenza a fasce per i giocatori**: vicini 30 Hz, medi 10 Hz, lontani 2-5 Hz, come le fasce T0-T2 dei minion.
3. **Rappresentazione a lotti per i giocatori lontani** (estende i pacchetti di `GSTNMinion`): oltre una distanza, un giocatore non
   arriva più come attore NP ma come voce compatta in un pacchetto (posizione, yaw, stato d'azione, mossa, salute, ~20 B come una voce
   minion, M8 §8.2), mostrato come marionetta interpolata. Quando si avvicina torna attore NP completo, con la stessa isteresi.
   Richiede: un id stabile del giocatore nella voce (M8 §8.1 lo prevede già come opzione `GlobalId`), un passaggio pulito marionetta ↔
   attore senza scatti, e la presentazione che legge solo lo stato (requisito F0/F2/F9 del piano rollback).
4. **Iris o Replication Graph** solo sopra le ~50 connessioni e solo dopo lo spike R3 di M8 (Iris con NP e Mover del fork mai provato
   [V]: nessun `UseIrisReplication` in `Config/`).

---

## 5. Test di carico con 20-50 client finti

**Cosa serve.**
- **Build server dedicato**: motore compilato da sorgente e target `Server` / `Client` (server-dedicato-ricerca §7). È il primo costo
  e il primo passo (S1).
- **Client finti**: client senza grafica (`-nullrhi -nosound`) con un bot che produce **input virtuali** (`Input.Button.*`), mai tasti
  fisici (regola 5), dalla stessa via dell'harness (`combat_tap`). Tre comportamenti: fermo, gira per la zona, combatte (mosse a caso
  sui minion e tra giocatori).
- **Lanciatore**: script che avvia N processi client contro un server, con ritardo, jitter e perdita come l'harness, dentro
  `Tools/CI/Run-Timed.ps1`, e un riassunto con il percorso del log.
- **Macchine**: un client headless occupa memoria e CPU anche senza grafica; 50 su un PC solo probabilmente non stanno [E]. Si
  dividono tra fisso e portatile, oppure si noleggia una VM cloud a ore (decisione 3 sotto).

**Cosa si misura** (scenari: hub tranquillo, combattimento sparso, combattimento denso; 20 poi 35 poi 50 client):

| Metrica | Strumento | Soglia di partenza [E] |
|---|---|---|
| Frame del server, medio e p99 | `stat unit`, Unreal Insights | ≤ 12 / 16 ms |
| ms per modello NP (combattimento, ability system, Mover) e del lotto minion | Insights, `STAT_GSTN_Minion*` | da registrare, base di §2.1 |
| Replica sul server (ms) | Insights, `stat net` | ≤ 2 ms |
| KB/s per client e upload totale | Net Insights (`-NetTrace=1`), `minion_bytes` | ≤ 40 KB/s per client |
| Rollback al secondo e frame risimulati sul client | `np.PrintReconciles 1` | come oggi in 1v1 |
| Memoria del server per giocatore | `memreport` | da registrare |

Il risultato è una tabella "giocatori per istanza" vera, che sostituisce le stime di §2.1 e §4.1.

---

## 6. Quando partire

Non prima che la sezione MMO **esista nel design** (game-design-direction.md, game-modes.md): quanti giocatori per zona, quanto
combattimento denso, se c'è PvP aperto. Prerequisiti tecnici, in ordine:

1. Il lavoro "Portare tutto il gameplay nel rollback" chiuso almeno fino all'interruttore Rollback completo / Scalato.
2. La patch "LOD per entità" della predizione in avanti (combat.md §12.7).
3. M8 minion con R1-R5 misurati (i pacchetti sono la base di §4.2).

Fino ad allora l'unico obbligo è già nel piano rollback: **la presentazione legge solo lo stato**, così nulla va rifatto dopo.

---

## 7. Fasi e costi stimati

Un thread = un obiettivo provato sul fisso (regola 1). Stime [E] dai costi registrati; l'hosting non è stimato (prezzi da cercare
quando servirà, server-dedicato-ricerca §7).

| Fase | Cosa | Macchina, modello | Costo [E] | Uscita |
|---|---|---|---|---|
| S0 | Design della sezione MMO: numeri per zona, tipo di combattimento, PvP sì/no | owner + coordinatore | 1-2 $ | numeri scritti in game-modes.md |
| S1 | Build server dedicato: motore da sorgente, target Server/Client, avvio con l'harness | fisso, Sonnet per i passi meccanici | 5-10 $ + ore di compilazione | harness in dedicato verde con la build server |
| S2 | Bot a input virtuali e lanciatore di N client headless | cloud codice + fisso, Opus | 10-15 $ | 20 client collegati per 10 min senza errori |
| S3 | Misura base: 20 / 35 / 50 client nei tre scenari | fisso (+ portatile o VM), Sonnet + lettura Opus | 5-8 $ | tabella "giocatori per istanza" (§5) |
| S4 | Rete: rilevanza a budget e fasce per i giocatori, voce compatta a lotti per i lontani | cloud + fisso, Opus | 15-25 $ | ≤ 40 KB/s per client a 50 client, nessuno scatto marionetta ↔ attore |
| S5a | CPU: percorso "a riposo" più leggero nei tre modelli | cloud + fisso, Opus | 8-12 $ | -30 % ms per giocatore fermo [E] |
| S5b | Spike tick ridotto per i giocatori lontani (patch fork), **solo se S3 lo chiede** | Opus | 10-15 $ | rollback di gruppo integro (`forced_rollback`, `combat_spike`); altrimenti si ferma |
| S6 | Spike Iris / Replication Graph a 50 connessioni, **solo sopra ~50** | Opus | 8-12 $ | guadagno ≥ 25 % o si ferma (come R3) |
| S7 | Istanze multiple con passaggio tra zone (viaggio, stato del giocatore portato) | cloud + fisso, Opus | 20-40 $ | due istanze, passaggio senza perdere loadout e progressi |

**Totale**: circa **65-110 $** senza le fasi condizionali (S0-S5a, S7), **85-140 $** con S5b e S6. Un piano approvato copre fino a +20 %
(regole-processo §3).

---

## 8. Decisioni per l'owner (quando la sezione partirà, non ora)

1. **Forma del mondo MMO.** **A (consigliata)**: istanze con tetto di giocatori e passaggio tra zone (§2.2 leva 4); B: mondo continuo
   su più server (progetto di mesi, fuori da questo piano). Costo A: S7, 20-40 $.
2. **Tick ridotto per i giocatori lontani.** **A (consigliata)**: prima tetto per istanza e percorso a riposo (S5a), patch al fork solo
   se la misura lo chiede; B: patch subito. A non costa nulla in più finché non serve.
3. **Dove girano i test di carico.** **A (consigliata)**: fisso + portatile per 20-35 client, VM cloud a ore solo per i 50; B: tutto su
   VM cloud. A usa macchine che ci sono già; il costo della VM è da cercare.

Fino a S0 resta fermo tutto questo piano; nessun altro lavoro dipende da lui.

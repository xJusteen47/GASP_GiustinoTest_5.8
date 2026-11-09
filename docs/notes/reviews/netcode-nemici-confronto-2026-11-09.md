# Netcode dei nemici: confronto con Warframe e altri (2026-11-09)

Ricerca + documento, nessun codice. Fonti in fondo. Legenda: **[D]** = documentato da fonte pubblica (sviluppatore, talk, doc ufficiale);
**[I]** = dedotto (community, analisi di terzi, o ragionamento nostro). Dove un numero manca, nessuno lo ha pubblicato.

## In breve

1. **Siamo già nella stessa fascia dei giochi noti.** Warframe stimava ~20 byte per nemico a 30 aggiornamenti/s e ~64 KB/s per l'host con
   30 nemici e 3 client [D]. Noi mandiamo ~24 byte per minion a 10-15 Hz, con 40-150 minion, e restiamo a 5-8 KB/s per client per i minion.
2. **I minion non sono il nostro problema di banda: lo sono i giocatori.** Il traffico Network Prediction dei giocatori è 44-50 KB/s per
   client, 5 volte quello dei minion (G11b). Qualsiasi risparmio serio parte da lì, non dai nemici.
3. **Si può fare meglio a basso costo**: compressione Oodle sui pacchetti (la stessa che usa Warframe), posizioni relative al pack, e una
   misura di cosa pesa nei 44-50 KB/s dei giocatori. Totale stimato ~$6-9, guadagno atteso 30-50 % sul traffico totale.
4. **Distribuire i nemici tra i client (la tua idea)**: esiste (Destiny, Roblox, Unity "distributed authority"), ma oggi non ci serve:
   l'host simula 150 minion in 0,56 ms per frame. Il collo di bottiglia non è la CPU dell'host. Consiglio: **non ora**; sotto c'è cosa la
   renderebbe fattibile più avanti.

## 1. Come fanno gli altri giochi

| Gioco | Chi comanda (host) | Chi simula i nemici | Frequenze / banda | Note |
|---|---|---|---|---|
| **Warframe** | P2P: un giocatore fa da host; server dedicati solo opzionali/PvP [D] | L'host [I, la slide non lo dice ma è il modello host] | Esempio dello sviluppatore: 30 update/s, 30 NPC x 20 B, ~512 kbps (64 KB/s) per l'host con 3 client; congestion control tra ~10 e 80 KB/s; compressione Oodle 1,4-1,8:1 [D] | Priorità per oggetto e per client; oggetti non visibili saltati; precisione ridotta dove non serve; proprietà in classi critica/importante/normale/cosmetica [D]. PvE: "favorisce l'umano" contro gli NPC [D] |
| **Deep Rock Galactic** | Unreal 4, listen server (un giocatore ospita) [I] | L'host [I] | Non pubblicato | Replication standard di UE; le lamentele di desync dei client sono note [I] |
| **Helldivers 2** | P2P, un giocatore è host; backend centrale solo per la guerra galattica [I] | L'host [I] | Non pubblicato | Motore Stingray/Bitsquid; centinaia di nemici; host lento = tutti soffrono [I] |
| **Left 4 Dead 1/2** | Source: dedicato o listen [D] | Il server [D] | 30 tick/s [D]; snapshot delta rispetto all'ultimo confermato, interpolazione 100 ms, lag compensation [D] | Il "Director" tiene il numero di infetti attivi limitato (orde di decine) [D talk AI, numeri esatti I] |
| **Vermintide 2 / Darktide** | VT2: P2P host, poi server dedicati annunciati 2019 [D]; Darktide: server dedicati [D] | Host / server | Non pubblicato | Lo stesso motore Stingray; i giocatori lamentano tick basso su Darktide [I, solo community] |
| **Risk of Rain 2** | Unity UNET, un giocatore host [D wiki modding] | L'host; il movimento del proprio personaggio è autorità del client [D wiki modding] | Non pubblicato | Limite di ~40 mostri per squadra [I, community] |
| **Destiny 1/2** | Ibrido: "activity host" nel cloud per lo stato critico di missione; "physics host" (D1: la console di un giocatore, D2: server Bungie) [D] | Ogni console simula la "bolla", il physics host vince sui conflitti [D analisi GDC] | Danno campionato a 30 Hz, client PC fino a 40 Hz [I, analisi PC Gamer] | Ogni client è autorità sulla propria posizione e sui propri colpi (si accettano lag switch e trucchi in cambio di reattività) [D/I]. Migrazione host frequente e "invisibile" [D] |
| **Halo: Reach** (riferimento) | Un giocatore host, migrazione host [D] | Host | 20 % della banda di Halo 3; priorità per oggetto, stato reinviato finché converge, effetti cosmetici una volta sola senza garanzia [D] | Il client manda "ho sparato alla testa di X", l'host controlla la linea di tiro [D] |

Il quadro comune: **una sola autorità per i nemici** (host o server), stato quantizzato, priorità per distanza/importanza, interpolazione
lato client. Solo Destiny sposta autorità sui client, e solo per giocatori e colpi, non per i nemici.

## 2. Standard e strumenti noti

- **Snapshot + delta + interpolazione** (Quake 3, Source, Glenn Fiedler): il server manda stato, il client interpola ~100 ms indietro. È
  quello che facciamo già (interpolatore con dead reckoning limitato all'intervallo del mittente).
- **Quantizzazione** e **delta compression** rispetto all'ultimo stato confermato: la seconda è la nostra lacuna (vedi perf-debt
  "full per-minion net state, no delta").
- **Priorità / rilevanza / interest management** (Tribes, Halo, Warframe; in UE: NetCullDistance, priorità, **Replication Graph**,
  **Iris** con filtering e prioritizer): noi abbiamo già tier T0/T1/T2, budget di rilevanza per pack, dormancy.
- **Lockstep deterministico** (Age of Empires, "1500 archers on a 28.8"): manda solo gli input, regge migliaia di unità, ma richiede
  simulazione deterministica: UE/Chaos/Mover non lo sono. Non applicabile.
- **Compressione dei pacchetti**: Oodle Network (Warframe, e plugin incluso in UE) addestrata sul traffico reale.
- **UE Mass Replication** (sperimentale): replica entità Mass con LOD per client. Ha le stesse idee del nostro GSTNMinion, non ci
  darebbe nulla di nuovo.

## 3. I nostri vincoli contro quelli degli altri

| Voce | Noi (G11b, misurato) | Altri |
|---|---|---|
| Giocatori | 2-4 | 4 (Warframe, DRG, HD2, L4D co-op, VT2, RoR2), 6 (Destiny strike), 16 (Halo) |
| Nemici contemporanei | fino a 150 vivi (cap 160), max 48 a piena frequenza (T0) | ~30 per missione (Warframe [D]); decine (L4D, VT2); ~40 (RoR2 [I]); centinaia (HD2, DRG [I]) |
| Host | listen (giocatore) o dedicato | quasi tutti host-giocatore P2P; dedicati per Darktide, L4D (opz.) |
| Frequenza simulazione | 60 Hz | 30 tick (L4D), 30 Hz danno (Destiny) |
| Frequenza invio nemici | T0 10 Hz (nuovo default), T1/T2 meno | 30 update/s nell'esempio Warframe; 20-30 snapshot/s Source |
| Byte per nemico per update | ~24 B reali (20 B pianificati +20-25 %) | ~20 B (esempio Warframe) |
| Banda minion per client | 4,9 KB/s a N=40, 7,7 a N=80 (a 10 Hz) | Warframe ~15-21 KB/s per client (64 KB/s host / 3, include i giocatori) |
| Banda giocatori per client | 44-50 KB/s (Network Prediction) | non pubblicata |
| Upload host | 24-31 KB/s solo minion con 3 client (+ i giocatori) | Warframe ~64 KB/s totale (stima), test con host limitato a 30 KB/s |
| Latenza nei test | 100-250 ms | Source interp 100 ms; Warframe e Destiny P2P, latenza variabile |
| PC minimo | "tostapane" | Warframe e HD2 scalano su PC deboli; l'host debole peggiora tutti |

**Lettura:** per i nemici siamo più efficienti dell'esempio Warframe (meno Hz, tier per distanza). Il nostro totale per client
(~50-58 KB/s) però è dominato dai giocatori ed è sopra quello che un host su linea domestica regge comodamente con 3 client
(~150-175 KB/s in upload). È lì che conviene lavorare.

## 4. Possiamo fare di meglio a basso costo? Sì, con queste leve

| # | Leva | Guadagno stimato | Costo stimato |
|---|---|---|---|
| L1 | **T0RateHz 10 di default** (approvato) | minion -40 % (8,1 → 4,9 KB/s a N=40) | ~$0,5, già in coda |
| L2 | **Misurare cosa pesa nei 44-50 KB/s dei giocatori** (Network Insights / `stat net` per proprietà, sul fisso) e abbassare frequenza/precisione dei simulated proxy dove non si vede | probabilmente il più grande: 20-40 % del totale [I] | ~$1,5 misura (Haiku/Sonnet) + ~$2-3 fix |
| L3 | **Oodle Network** sul PacketHandler (plugin già in UE), dizionario addestrato su catture nostre | 30-45 % su tutto il traffico, giocatori compresi (Warframe: 1,4-1,8:1) [D per Warframe, I per noi] | ~$2-3 (config + cattura + training + misura) |
| L4 | **Posizione relativa all'origine del pack** (3 x 12-14 bit invece di 3 x 19) e tipo/ID globale solo all'aggiunta quando il pack lo permette | minion -15-25 % | ~$2-3 (codec + test Core) |
| L5 | **Host = il PC più forte** nella lobby (regola di lobby, come Halo che sceglie l'host di qualità) | evita il caso peggiore "host tostapane" | ~$1, solo regola + doc |

**Cosa NON fare adesso:** migrare a Iris o Replication Graph (tanto lavoro sul fork NPP e sul codec, poco guadagno: i minion sono già
pochi actor-pack); lockstep deterministico (UE non è deterministico); delta per-connessione fatto a mano (utile ma costoso, ~$8-12:
aspettare di vedere i numeri dopo L2-L4); alzare la frequenza dei minion "per sicurezza".

## 5. La tua idea: distribuire i nemici tra i client (solo PvE)

**Chi lo fa.** Destiny 1 metteva la "physics host" su una console di giocatore, con un arbitro nel cloud ("activity host") che teneva lo
stato critico (quanti nemici, porte, obiettivi) e correggeva dopo una migrazione: nemici in più morivano, quelli mancanti respawnavano [D].
Roblox assegna in automatico la proprietà dei pezzi fisici al client più vicino [D]. Unity Netcode 2 ha la modalità "distributed authority"
in cui ogni oggetto ha un client proprietario e la proprietà si ridistribuisce quando qualcuno entra o esce [D]. Per Warframe non c'è una
fonte pubblica che dica che i client possiedono nemici: le fonti descrivono un host P2P classico.

**Quanto costa in UE.** UE ha una sola autorità per oggetto (il server); NPP e GAS assumono lo stesso. Il nostro sistema minion però è
fuori dagli actor (store + pack + codec nostro), quindi sarebbe *meno* difficile del solito: un client simulerebbe i minion di un pack e
manderebbe gli item quantizzati all'host, che li ripubblica. I problemi veri: chi decide quando il giocatore A colpisce un minion del
client B (doppia latenza A → host → B); lag compensation e token di attacco che oggi vivono sull'host; passaggio di proprietà quando un
client esce; e la banda in upload dell'host **non scende** (UE non ha mesh P2P, l'host deve comunque inoltrare). Stima: $30-60 e un
milestone intero.

**Raccomandazione: non ora.** Guadagneremmo solo CPU dell'host, e oggi la CPU dell'host non è il limite (150 minion = 0,56 ms per frame
sul fisso). Diventa sensato solo se: (a) un benchmark su PC "tostapane" mostra l'host sopra budget con i minion; (b) il cervello dei minion
diventa costoso (oggi StateTree è ~10x una versione nativa, vedi perf-debt); (c) prepariamo il terreno a costo quasi zero: proprietario a
livello di **pack** (16 minion) e token d'attacco sempre sull'host. Prima di questo conviene una leva più semplice: host = PC più forte (L5)
o un server dedicato.

## Costo di questo thread

~$3 (ricerca web + lettura di m8-minion.md G11b e perf-debt.md, nessuna build).

---

## Appendice tecnica

**A. Nostro item minion** (`GSTN_MinionNetCodec.h`): Id 8 + Type 8(+1) + GlobalId 8 + Pos 3x19 + Yaw 8 + ActionState 4 + MoveId 6 +
MoveStartFrame 10 + FrameStamp 8 + HitCounter 3 + Health 5 + Speed 6 + Flags 3 (+ ext byte opzionale) ≈ 142 bit ≈ 18 B. Il reale
(~24 B) include ReplicationID/ReplicationKey del FastArray e l'header della proprietà per pack. L4 attacca i 57 bit di posizione (il pack
ha un'origine; 13 bit = ±40 m a 1 cm) e i 17 bit di tipo/GlobalId che oggi viaggiano a ogni update (vedi nota in `GSTN_MinionPack.h`).

**B. Config rete attuale:** nessuna voce in `Config/*.ini` per `MaxClientRate`, `MaxInternetClientRate`, `NetServerMaxTickRate` né
PacketHandler/Oodle: valgono i default di UE. Da controllare insieme a L2/L3 (verificare che il cap per client non tagli i giocatori a N alto).

**C. Mappa delle tecniche su UE:** priorità/rilevanza = `NetPriority`, `NetCullDistanceSquared`, Replication Graph, Iris prioritizer e
filtering; delta = FastArray (per item, non per campo), Iris delta compression (per campo, con baseline per connessione); compressione =
`OodleNetworkHandlerComponent`; LOD per client = Mass Replication (sperimentale) o i nostri tier T0/T1/T2.

**D. Fonti**
- Warframe, post sviluppatore DE su compressione e congestion control: https://forums.warframe.com/topic/391492-network-compression-and-congestion-control/
- "Networking Architecture of Warframe", M. Siniło (DE): https://www.slideshare.net/slideshow/networking-architecture-of-warframe/76337882
- Destiny, GDC 2015 "Shared World Shooter: Destiny's Networked Mission Architecture": https://gdcvault.com/play/1022246/ ; note di terzi: https://gist.github.com/nessus42/f12f094e4abe30c0d00c9b4c86c387ce
- Destiny 2, analisi PC Gamer: https://www.pcgamer.com/the-strange-science-of-destiny-2s-uniquely-complicated-netcode
- Halo: Reach, GDC 2011 "I Shot You First" (sintesi): https://wolfire-blog.appspot.com/2011/03/GDC-Session-Summary-Halo-networking
- Source: tickrate (L4D 30): https://developer.valvesoftware.com/wiki/Tickrate ; Source Multiplayer Networking: https://developer.valvesoftware.com/wiki/Source_Multiplayer_Networking
- Vermintide 2, dedicati annunciati (Fatshark, 2019): https://devtrackers.gg/vermintide2/p/dd120494-are-we-gonna-have-dedicated-servers-sooner-or-later
- Risk of Rain 2, rete UNET (wiki modding): https://risk-of-thunder.github.io/R2Wiki/Mod-Creation/C%23-Programming/Networking/UNet/
- Unity distributed authority: https://docs.unity3d.com/Packages/com.unity.netcode.gameobjects@2.6/manual/terms-concepts/distributed-authority.html
- Roblox network ownership: https://create.roblox.com/docs/physics/network-ownership
- UE Iris: https://dev.epicgames.com/documentation/en-us/unreal-engine/introduction-to-iris-in-unreal-engine ; prioritizzazione: https://dev.epicgames.com/documentation/en-us/unreal-engine/iris-prioritization-in-unreal-engine ; filtering: https://dev.epicgames.com/documentation/en-us/unreal-engine/iris-filtering-in-unreal-engine
- UE Replication Graph: https://dev.epicgames.com/documentation/en-us/unreal-engine/replication-graph-in-unreal-engine
- UE Mass (replication sperimentale): https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-mass-gameplay-in-unreal-engine
- G. Fiedler, Snapshot Interpolation / Snapshot Compression: https://gafferongames.com/post/snapshot_interpolation/ , https://gafferongames.com/post/snapshot_compression/
- Tribes networking model (Frohnmayer, Gift): https://www.gamedevs.org/uploads/tribes-networking-model.pdf
- "1500 Archers on a 28.8" (Age of Empires, lockstep): https://www.gamedeveloper.com/programming/1500-archers-on-a-28-8-network-programming-in-age-of-empires-and-beyond
- Overwatch, GDC 2017 "Gameplay Architecture and Netcode" (T. Ford): https://www.gdcvault.com/play/1024001/-Overwatch-Gameplay-Architecture-and
- L4D AI Director, M. Booth "The AI Systems of Left 4 Dead" (AIIDE 2009): https://steamcdn-a.akamaihd.net/apps/valve/2009/ai_systems_of_l4d_mike_booth.pdf

Gli ultimi sei link e quelli di Roblox/Replication Graph vengono da conoscenza nota, non riaperti in questo thread.

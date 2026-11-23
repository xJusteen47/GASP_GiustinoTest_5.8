# Controllo dei minion: StateTree, Mass, Mover (valutazione, 2026-11-09)

Domanda dell'owner (2026-11-09 02:31Z): avendo una soluzione custom per i minion, dovremo agganciarci agli AI graph o a Mover
(stile MoverMassIntegration)? Solo analisi, nessun codice. Fonti: `Plugins/GSTNMinion/README.md`, `docs/notes/handovers/m8-minion.md`,
`docs/notes/perf-debt.md`, `docs/architecture/combat-m8-minions.md` §6, `docs/architecture/many-enemies-replication.md`, API Epic di
MoverMassIntegration (link in fondo). Il sorgente del motore 5.8 non è nel repo: lo stato "sperimentale" dei plugin va riconfermato
sul `.uplugin` del motore installato.

**Nota:** il movimento dei **fighter** verrà rifatto come movement mode separato (decisione owner). Mover per i fighter è un tema a
parte e questa valutazione non lo tocca: qui si parla solo dei minion (store leggero).

## 1. Cosa abbiamo oggi

- **Dati**: `FGSTN_MinionStore` (GSTNMinionCore), struct-of-arrays fino a 160 slot, pacchi da 16, nessun attore per minion.
  Il batch (G3) gira per frame: spawn, LOD, drain colpi, timeline, movimento, publish.
- **Cervello**: è **già StateTree**, cioè l'AI graph standard di Unreal. Ogni definizione ha un asset `Brain` (schema
  `UGSTN_MinionStateTreeSchema`, task MoveToTarget / FaceTarget / RequestAttack, condizioni TargetInRange / IsLODAtLeast).
  Non c'è un `UStateTreeComponent` per attore: `UGSTN_MinionBrainSubsystem` esegue un `FStateTreeExecutionContext` per slot,
  con frequenza per tier (T0 ogni frame, T1 1/4, T2 1/16, Dormant mai). Il cervello scrive solo un **intent** (muovi, guarda, attacca),
  mai lo store. Esiste un cervello nativo di riserva (insegui e colpisci).
- **Costo cervello** (spike 6): StateTree 4.96 µs per tick per minion, nativo 0.49 µs (10×). A 40 minion in T0 = 0.2 ms: oggi passa,
  ma sul "tostapane" è la voce più grossa del cervello. Perf-debt: contesto ricostruito per slot, vicini cercati su tutto lo store,
  target scan su tutti i fighter.
- **Movimento**: interfaccia `IGSTN_MinionMovementBackend::StepBatch`. Lo spike 3 ha scelto il **backend B leggero** (capsula
  sweep + slide nello store). Il **backend A con Mover** (un `UMoverComponent` per minion, fuori da NP) è previsto come modulo
  opzionale `GSTNMinionMover` ma non è mai stato costruito. Aperto: il caso "hover" di 1 cm dopo un lancio.
- **Rete**: codec nostro, ~24 B per minion a 10 Hz (4.9-7.7 KB/s per client), già nella fascia di Warframe.
- **Promozione**: un minion può diventare fighter completo (GAS + NPP + Mover). Lì il movimento è quello dei fighter.

## 2. Cosa offre UE 5.8

- **StateTree**: stabile, è il sistema AI consigliato da Epic. Lo usiamo già.
- **Mass Entity / MassAI** (MassStateTree, processori, Mass LOD, MassNavigation): Mass core è maturo; 5.8 lo migliora (segnali nel core,
  creazione entità fuori dal game thread). MassStateTree usa uno schema e task propri (diversi dai nostri).
- **MassReplication**: sperimentale, "mai finito", nessun aggiornamento futuro, tre regressioni in 5.8.1; il sostituto
  (MassIrisReplication) è solo su UE6 main. Già respinto in `many-enemies-replication.md`.
- **MoverMassIntegration** (plugin sotto MoverIntegrations): classi `UMoverMassAgentTrait`, `UMoverInputTrait`,
  `UMassToMoverInputTranslator`, `UMoverInputToMassTranslator`, `FMoverWrapperFragment`. È un **ponte** tra un agente Mass e un
  **attore con `UMoverComponent`**: Mass decide la velocità, Mover muove l'attore, i traduttori copiano i dati avanti e indietro.
  Quindi resta **un componente Mover per minion**, cioè proprio il backend A che lo spike 3 ha scartato per costo. Mover è ancora
  sperimentale/beta in 5.8 e il ponte lo è di più.

## 3. Opzioni

| | Cosa | Adatto ai vincoli (tostapane, 40-160, codec 24 B, promozione, NPP fighter) | Costo stimato | Rischio | Cosa buttiamo |
|---|---|---|---|---|---|
| **A. Store custom + StateTree (oggi)** | Teniamo tutto, sistemiamo i debiti del cervello | Ottimo: niente attori, codec intatto, promozione intatta | $3-6 (contesto riusato, vicini da hash, cervello nativo per T1/T2 o per i grunt) | Basso | Niente |
| **B. Cervello su Mass (MassStateTree + processori), codec nostro** | Ogni minion diventa anche un'entità Mass; il cervello gira nei processori Mass | Medio: doppio stato (store + frammenti Mass) o riscrittura dello store; nessun guadagno di CPU misurabile rispetto al nostro batch | $25-45 | Medio-alto | Brain subsystem, schema e task StateTree, parte di LOD/scheduler |
| **C. Mover + Mass per il movimento (MoverMassIntegration)** | Richiede B, più un attore con Mover per minion | Scarso: un componente Mover per minion su 40-160 minion è il costo che lo spike 3 ha evitato; plugin sperimentale | $20-35 sopra B | Alto | Backend B (sweep + slide), parte dei pool |
| **D. Mass completo + MassReplication** | Store, rete e LOD tutti Mass | Pessimo: replica sperimentale e abbandonata, byte peggiori del nostro codec, nessuna promozione | $80-150 | Molto alto | Store, codec 24 B, pacchi, LOD, presentazione, gran parte di M8 |

Guadagni reali di B-D: strumenti Epic (debugger Mass, MassNavigation, SmartObjects già integrati), meno codice di collisione nostro (C).
Nessuna delle tre riduce byte di rete o CPU sul tostapane rispetto a oggi.

## 4. Raccomandazione

**Tenere l'opzione A.** L'aggancio agli AI graph c'è già: i designer scrivono il cervello in StateTree, lo stesso editor di Epic.
Mass e MoverMassIntegration oggi aggiungerebbero un secondo modello dati e un attore Mover per minion, cioè più costo, non meno.

**Prima del PvP** (solo se il bench sul tostapane lo chiede): i debiti del cervello in perf-debt (contesto riusato per albero,
vicini dalla hash del batch, posizioni fighter spinte una volta per frame) e il cervello nativo per i tier lontani, ~$3-6.
Il bug hover si chiude nel backend B, non cambiando backend.

**Dopo il PvP / con UE6**: rivalutare quando (a) MassIrisReplication arriva in una release stabile, (b) Mover esce dal beta e il
ponte Mass ha un percorso senza attore per agente, oppure (c) vogliamo MassNavigation/SmartObjects per i minion. Spesa da prevedere
solo allora.

**Cosa tiene la porta aperta** (già nel codice, da non rompere):
1. Il cervello scrive solo l'**intent**: un domani un processore Mass può scrivere lo stesso intent senza toccare movimento e rete.
2. **`IGSTN_MinionMovementBackend`**: un backend Mover o Mass si aggiunge come modulo opzionale (`GSTNMinionMover`), per tipo di minion.
3. **GSTNMinionCore puro** (niente Mover, NP, GAS): lo store può essere riempito da frammenti Mass senza cambiare il codec.
4. **Promozione**: i fighter restano GAS + NPP con il loro movimento (il nuovo movement mode), indipendente da questa scelta.

Fonti esterne: API MoverMassIntegration https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/MoverMassIntegration ;
Mass overview https://dev.epicgames.com/documentation/unreal-engine/overview-of-mass-gameplay-in-unreal-engine ;
MassReplication 5.8.1 (E14 in `many-enemies-replication.md`).

Costo di questa analisi: ~$2 (Opus, solo documenti).

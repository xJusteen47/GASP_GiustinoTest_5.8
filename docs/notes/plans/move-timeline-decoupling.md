# Mosse complesse: timeline separate dall'abilità (design, 2026-11-15)
Base: `claude/integ-cp3` 069287f7 (CP1-CP5). Solo design, nessun codice. Thread cloud, Opus high.

## 1. Cosa si può fare OGGI (letto nel codice)
**Il legame è 1 classe = 1 timeline.** I dati vengono dal CDO (`Timeline` inline o `TimelineAsset`, `GSTN_GA_BaseCombat.h:62-68`); lo stato di combattimento
salva solo *quale voce del registro* gira (`ActiveTimeline`, `GSTN_CombatSimTypes.h:133`), e l'abilità si riconosce per classe
(`IsStillTheRunningTimeline`, `GSTN_GA_BaseCombat.cpp:153-160`). Quando lo stato smette di nominarla, l'abilità **termina da sola** (`.cpp:682-690`).
| Domanda | Oggi | Come / limite |
|---|---|---|
| Il grafo di una mossa avvia un altro TL_? | **Indirettamente** | `Send Move Event` (`.h:174`) attiva un'altra classe GA con il suo TL, adottata **1 frame dopo** (`GSTN_CombatSimComponent.cpp:1223-1238`). La mossa che chiama **termina** (perde il suo grafo e le sue variabili). Nessun nodo "Play Timeline(TL_)". |
| Più timeline in fila | **Sì, solo da dati** | `FollowUpTimeline` (`GSTN_TimelineTypes.h:662-670`, `SimComponent.cpp:1213-1216`): a fine naturale parte la classe successiva. Una catena per classe, fissa. Le catene a input (jab1→jab2) usano cancel window + binding. |
| Scegliere la timeline a runtime | **No** | L'adozione scorre il registro per classe e vince la prima voce (`SimComponent.cpp:1229-1236`): `TimelineOverride` del binding (`GSTN_AbilityBinding.h:50-51`) vale solo per l'input, non dal grafo. |
| Nessuna timeline (logica GA pura) | **Sì** | Una BP figlia di `UGSTN_GA_Base` (non BaseCombat) si lega a un comando come `GA_PlatformJump`; ha `WaitCombatFrames` (`GSTN_GA_Base.h:92`), `bEndOnTriggerRelease` (`:45`), variabili Sync. |
| Stance con moveset proprio | **Sì in teoria, mai provato** | Stance = GA_Base che tiene il tag `Stance.X` (ActivationOwnedTags), mosse = binding con `Requirements` RequireTags `Stance.X` (`GSTN_AbilityBinding.h:36`). `Open Move Tag Window` NON basta: si chiude a fine mossa. Da verificare: che avviare una mossa della stance non cancelli la stance. |
| Aspettare un input dentro la mossa | **Solo polling** | Query pure (`WasCommandEnteredWithin`, `WasButtonPressedWithin`..., `GSTN_CombatSimComponent.h:396-420`) lette da `On Move Frame` o dopo `WaitCombatFrames`. Deterministiche (stato del frame), ma **non consumano** la pressione: un binding può partire sulla stessa (`.h:415-417`). |

**Gameplay task che aspettano input.** I task GAS classici (`AbilityTask_WaitInputPress`) usano le RPC di predizione: fuori dal modello NPP. Il fork ha task
rollback-aware (`UBasePredictionTask` con `StartTaskRollback` e dati sincronizzati, `AbilitySystemSimulation/Public/Tasks/BasePredictionTask.h:22-125`), incluso
`UWaitInputEventPredictionTask`: è deterministico perché nasce dall'input del frame simulato (`NpAbilitySystemComponent.cpp:281-351`), ma ascolta un
`UInputAction` grezzo (`WaitInputEventPredictionTask.h:31-38`): **ignora il nostro buffer**, i pattern dei comandi (motion, chord, doppio tap), la priorità e il consumo
→ input "rubati" o doppi. Nessun nostro codice usa i prediction task; il validatore CP2 segnala ogni async task in un evento di simulazione (`GSTN_SimGraphRules.cpp:222-225`).
**"Tag del comando + condizione" basta?** Quasi. Servono anche: **finestra** (frame da/a, o "fino a evento"), **buffer** (accetta pressioni fatte N frame prima
dell'apertura, come i binding), **consumo** (sì/no: chi vince fra questo attesa e un binding in cancel window → **priorità**), condizione opzionale = gli
`FGSTN_InputCondition` che il plugin input ha già (stick, held, tag). Così è un "binding temporaneo" della mossa, risolto dal tick di combattimento nello stesso
passo dei binding: stesso buffer, stesso determinismo, nessun input hardcoded.

## 2. Cosa farebbe Epic
- **GAS**: l'abilità è il contenitore di logica; gli asset si *riproducono* con task (`PlayMontageAndWait`: l'abilità sceglie quale montage a runtime, 0..N per
  attivazione). Il montage non è l'abilità. → Il nostro TL_ va trattato come un montage: asset riproducibile da qualunque abilità.
- **Lyra**: classe base con default comodi (activation policy, InputTag→abilità in un AbilitySet di dati). → `GA_BaseCombat` resta la scorciatoia "1 mossa = 1 TL".
- **Mover / NPP / Chaos**: stato solo in struct sincronizzate, effetti come richieste in coda (layered moves), niente callback fuori tick. → avviare un TL è una
  *richiesta* al tick di combattimento, non una scrittura diretta.
- **Animazione/editor**: dati in asset, tool che generano, validator. → coerente con factory CP5 e validatori.
Giudizio: il disaccoppiamento proposto è esattamente lo schema Epic (abilità = logica, timeline = asset riprodotto da task); Epic non avrebbe un buffer di
comandi in engine, ma lo esporrebbe come "InputTag" con un task di attesa: è la §1 "Wait Command".

## 2b. Sembrare parte dell'engine (obiettivo owner, tutto il plugin)
Già nativo: asset con `UAssetDefinition` sotto Add > Gameplay > Combat (`GSTN_AssetDefinition_CombatTimeline.cpp`), factory CP5, Data Validation, tag nativi,
input sopra Enhanced Input (i comandi riferiscono `UInputAction`, `docs/architecture/input.md:29`), GA normali ereditabili in BP.
**Scostamenti oggi** (ordine di quanto si notano):
1. Le mosse usano eventi di classe (On Move Key/Hit/Ended) invece del nodo-task con pin stile `PlayMontageAndWait`: è proprio il gap di §1.
2. Prefisso `GSTN` visibile nei picker e nei nodi (`UCLASS(Abstract, Blueprintable)` senza `DisplayName`, `GSTN_GA_BaseCombat.h:54`); categorie "Combat|Move Actions" invece di "Ability|Tasks|Combat".
3. Editor timeline in Slate proprio (`Private/Timeline/`), non con le righe/notify del Montage né il time slider di Sequencer: aspetto, righello e scorciatoie diversi.
4. Nessuna icona di classe/thumbnail (GAS ne ha uno style set, `GameplayAbilitiesEditorModule.cpp:129`); tempi solo in frame (l'engine mostra frame E secondi).
5. "Command" e "Input.Button" dove Lyra usa "InputTag": stesso concetto, nome diverso (va spiegato nel doc, non rinominato).
**Regole per i nodi nuovi**: `Play Combat Timeline And Wait` e `Wait Command` come task latenti con pin (nome "Verbo...AndWait", categoria Ability|Tasks), input e
uscite come i task GAS, tooltips stile engine; il validatore li riconosce come sicuri. **Passata "aspetto nativo"** separata (2-5): DisplayName senza prefisso,
icone, righello frame+secondi e scorciatoie del Montage editor (spazio, frecce, zoom) sull'editor esistente, senza riscriverlo su Sequencer (riscrittura = 60 $+, non vale).
**Effetto sulle decisioni**: nessuna opzione consigliata cambia; 1A diventa task con pin (+3-5 $, ~30-40 $). La passata nativa (~10-15 $) va con CP6 (pubblicazione), nessuna decisione ora.

## 2c. Utility GAS standard usate così come sono (62.5 Hz, rollback, buffer)
Regola del fork: si aspetta con i `*PredictionTask`, non con gli `UAbilityTask` di serie (`docs/architecture/gas-npp.md:9`). Problemi tipici: timer a orologio, RPC e prediction key
(fuori da NPP, mai annullati da un rollback), callback fuori dal tick, input letto due volte, montage che scavalca la timeline, root motion di CharacterMovement invece di Mover.
| Utility | Esito | Perché / cosa usare |
|---|---|---|
| Qualunque `UAbilityTask` di serie (WaitDelay, WaitInputPress/Release, PlayMontageAndWait, WaitConfirm, NetworkSyncPoint) | **No** | RPC/prediction key e timer del mondo: non rientrano nel rollback. Il validatore li segnala già. |
| Delay, Set Timer | **No** | Errore del validatore (`GSTN_SimGraphRules.cpp:248`). → **Wait Combat Frames** (esiste, `GSTN_GA_Base.h:92`, ferma in hitstop): farne un task con pin. |
| Wait (fork, secondi) | OK con riserva | Tempo di simulazione, ma in secondi (arrotondati a 16 ms) e conta l'hitstop: per il gameplay usare i frame. |
| WaitInputPress/Release, WaitInputEvent (fork) | **Serve equivalente** | Input grezzo: niente buffer/pattern/consumo, doppio input con i binding. → **Wait Command** / **Wait Command Release**; per tenere premuto c'è già `bEndOnTriggerRelease`. |
| WaitGameplayEvent (fork) | OK con riserva | Deterministico se l'evento nasce in simulazione (Send Move Event, key). Mai da AnimNotify o UI: sono presentazione. |
| WaitGameplayTagAdded/Removed (fork) | OK con riserva | Tag del tick di combattimento visti nello stesso frame; i cambi di modo di Mover arrivano un frame dopo (CLAUDE.md regola 5). |
| PlayMontageAndWait (fork) | OK solo senza hitbox | Per una mossa salta hitbox, hitstop, cancel window, FAF, e litiga con la timeline sullo stesso slot. → **Play Combat Timeline And Wait**. |
| ApplyRootMotion* | **No** | Root motion di CharacterMovement. → traccia Movement della timeline o Apply Move Effect (layered move di Mover). |
| WaitTargetData / targeting | **No** (riserva per la versione fork) | Conferma del client via RPC. → GetAimRotation (simulazione), hitbox, Fire Projectile. |
| WaitAttributeChange (fork) | OK | Attributi in simulazione. |
| Costo/Cooldown GE, CommitAbility | OK con riserva | Nelle mosse a timeline il commit lo fa il tick a fine finestra di startup (`GSTN_GA_BaseCombat.h:22-23`): un CommitAbility a mano paga due volte. Cooldown in secondi, l'hitstop non lo ferma (da verificare). |
| Add/Remove tag, ActivationOwned/Block/Cancel tags | OK | Bastano per la stance (decisione owner). Un tag loose a mano va tolto anche in simulazione; meglio ActivationOwnedTags o Open Move Tag Window. |
| Apply GE a sé / GameplayCue | OK | Al bersaglio solo via hit data (inbox), non dal grafo dell'attaccante. Cue = presentazione (dedup CP4). |
| Spawn Actor, Random, Get Actor Location | **No** | → Fire Projectile, Get Move Random, `GetMoverSimState`. |
**Equivalenti da aggiungere** (stesso nome-schema e pin dei task GAS): Play Combat Timeline And Wait, Wait Command (+Release), Wait Combat Frames come task.
Il validatore, per ogni nodo "No", dice quale equivalente usare: così chi viene da GAS trova la strada senza leggere la documentazione.

## 3. Proposta (chiude il gap, mosse standard invariate)
1. **Abilità ospite**: qualunque `UGSTN_GA_Base` può dichiarare `Timelines` (lista soft di TL_ sul CDO). Il registro le compila come voci (classe ospite, TL):
   resta funzione pura del loadout → deterministico, nessuna project setting.
2. **Nodo "Play Combat Timeline And Wait (TL_)"** con pin On Key / On Hit / On Ended(motivo) / On Interrupted: richiesta al tick di combattimento, avvio **nello stesso
   frame** (come il FollowUp, non via adozione); lo stato salva voce + slot dell'ospite. L'ospite **non termina** a fine timeline: decide lui (un'altra, nessuna, End Ability).
   Sequenze, scelta a runtime e "nessuna timeline" diventano normale grafo GAS.
3. **Nodo "Wait Command"** (comando, finestra, consumo, priorità, condizione opz.) → evento On Command; risolto con i binding.
4. `GA_BaseCombat` = ospite che al via fa Play della sua timeline e termina con essa: **zero cablaggio** per le mosse standard, factory CP5 invariata.
5. Implementazione: prima uno spike (½ giornata) sui `UBasePredictionTask` del fork con rollback forzato; se reggono, i nodi sono task; se no, request node
   non-latenti + eventi (stesso API visto dal designer). Validator: ammette solo i nostri task rollback-safe.
6. Stance: nessun sistema dedicato (owner): tag + binding RequireTags; basta una riga di guida. Esempio + test: una mossa a 3 fasi con scelta a input; review regola 10.

## 4. Decisioni per l'owner (risposta tipo "1A 2A 3A")
1. **Architettura timeline** — A) ospite + Play Combat Timeline, GA_BaseCombat come scorciatoia (**consigliata**, ~30-40 $ cloud Opus con nodo-task stile engine + build/test fisso ~5 $);
   B) solo nodo "Start Timeline Move (classe)" che avvia nello stesso frame un'altra GA-con-TL (~8-12 $, ma il grafo non sopravvive fra le fasi); C) nulla, si usano FollowUp/eventi.
   Fermo intanto: niente.
2. **Attesa input** — A) Wait Command integrato col buffer (comando+finestra+consumo+priorità+condizione) (**consigliata**, ~12-18 $); B) permettere il task del fork
   `WaitInputEvent` (input grezzo, niente buffer: rischio doppi input, 0 $); C) solo polling come oggi.
3. **Equivalenti delle utility GAS** — A) i 3 task (Play Combat Timeline And Wait, Wait Command, Wait Combat Frames) + validatore che suggerisce l'equivalente
   (**consigliata**, ~6-10 $ oltre 1A e 2A); B) solo avvisi del validatore (~2 $); C) rifare 1:1 tutti i task GAS (~40 $+, metà inutili: il fork ne ha già molti sicuri).
Stance: decisa (tag generici + binding, niente esempio dedicato). Totale consigliato 1A+2A+3A ≈ 50-70 $ + build/test sul fisso.

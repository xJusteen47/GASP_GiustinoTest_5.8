# Mappa input: azioni, tasti, contesti, e verifica della strategia "tutto ad azioni" per il 1v1

Cloud, sola lettura, 2026-11-23. Base: `main` a5b2095. Fonti: codice C++ e script Python del repo (gli asset `.uasset` sono
puntatori LFS: il loro contenuto **non** è leggibile dal cloud, quindi tutto ciò che sta dentro un Blueprint è dedotto dai
commenti degli script e dagli handover, ed è marcato *DEDOTTO*). Riuso dichiarato: `docs/INDICE-PROGETTO.md` §5.10 (Input) e §5.3-5.5,
`docs/architecture/input.md`, `gas-npp.md`, `combat.md` §12.7, gli handover `pad-menu-1.md` e `spin-drill.md`,
`Plugins/GASNPP/NetworkPrediction/readme.txt`.

## Riassunto (10 righe)

1. **L'ipotesi è giusta nello spirito ma sbaglia il livello.** Le azioni di Enhanced Input (`IA_*`) **non vanno in rete**: sono
   locali alla macchina del giocatore. Ciò che rende robusto il netcode non è "tasto → azione", ma **"tutto ciò che cambia la partita
   entra in UN comando per frame (`FGSTN_InputCmd`) e la simulazione legge solo quello"**.
2. Il progetto **lo fa già** per il combattimento: tasti → azioni → bit di bottone logici + stick quantizzato → comando NP, con buffer,
   rollback e ripetizione sicura in caso di pacchetti persi. Nessun codice C++ di gioco legge tasti fisici (zero `IsInputKeyDown`/`EKeys` in gioco).
3. Il problema vero è che **ci sono tre canali di input in rete in parallelo** (comando GSTN, input di Mover scritto dal Blueprint
   GASP, input del fork GAS `FAbilitySimInputCmd`) e qualche cosa di gioco legge ancora quello sbagliato (es. il **salto dal muro**
   legge il "salto" di GASP, che nel layout di combattimento non ha più tasti: *DEDOTTO*, da verificare in editor).
4. Letture fuori dal comando trovate: un **tasto letto direttamente nel Blueprint GASP** (L3/Z, volo di debug), un **salto di debug**
   iniettato nella simulazione del muro (`gstn.Wall.Jump`, solo non-Shipping), **interazione** via RPC al server (non predetta).
5. Camera, menu, opzioni, rimappatura e debug **devono restare locali**: legarli ad "azioni di gioco" in rete sarebbe sbagliato.
6. Per un 1v1 la robustezza dipende anche (di più) da **topologia** (server dedicato vs host) e da come si vede l'avversario
   (interpolato, ~100 ms di ritardo): nessuna scelta sull'input lo risolve.
7. Decisioni: 1 unificare i canali (consigliata A), 2 cosa fare del salto/muro e delle letture debug (A), 3 server per il 1v1 (A).
8. Piano: 5 checkpoint piccoli, ~10-14 $ totali, 3 su PC con editor.

## 1. Tabella degli input di gioco

Legenda percorso: **CMD** = entra in `FGSTN_InputCmd` (predetto, inviato al server, rollback). **MOV** = entra nell'input di Mover
(`FCharacterDefaultInputs`, predetto e inviato). **GAS** = entra in `FAbilitySimInputCmd` del fork (predetto e inviato). **RPC** = chiamata
al server, non predetta. **LOC** = solo locale, non tocca la simulazione.

Fonte tasti: `Tools/GASNPPTest/movement_setup.py:21-29` (tabella di riferimento), `Tools/GAS/setup_player_input_contexts.py:19-20,66-72`.
Il "bottone logico" (`Input.Button.*`) è il bit nel comando; i comandi e le abilità sono legati al bottone, mai al tasto
(`Plugins/GSTNInput/Config/DefaultGSTNInput.ini` righe ~14-35, tabelle dei kit).

| Input | Pad / tastiera | Azione (IA) | Contesto (IMC) | Bottone logico → comandi | Percorso | Note |
|---|---|---|---|---|---|---|
| Movimento | Stick sx / WASD | `IA_Move` | `IMC_GSTN_Locomotion` | `MoveAction` → `StickX/Y` | **CMD + MOV** | Letto due volte: dal comando GSTN (`GSTN_InputSource.cpp:66`, quantizzato, dead zone del giocatore) e dal Blueprint GASP per Mover (*DEDOTTO*). `GSTN_CombatSimComponent.cpp:923-1070` riscrive l'intento di Mover dal comando quando il profilo ha `AnalogWalk`. |
| Direzione camera nel comando | (da stick dx/mouse) | — | — | `CameraYaw`, opz. `AimYaw/Pitch` | **CMD** | `GSTN_InputSource.cpp:88-97`. Giusto: lo stick è relativo alla camera, quindi la sua yaw deve viaggiare col comando. |
| Leggero | X / LMB | `IA_GASNPP_Attack` | `IMC_GASNPP_Test` | `Attack` → `Combat.Attack`, `Link.Pummel` | **CMD** | |
| Salto | A / Y | `IA_GASNPP_Jump` | `IMC_GASNPP_Test` | `Jump` → salto del kit (`GSTN_GA_PlatformJump`), `Drill.Exit`, `Surface.LaunchBoost` | **CMD** | |
| Salto vecchio | Spazio | `IA_Jump` | `IMC_GAS_Abilities` (non aggiunto al giocatore) | `ActivationInput` di `GSTN_GA_Jump` (`Config/DefaultGame.ini:43`) | **GAS** | Solo personaggio di prova (audit, decisione 2). A tolto da qui (`movement_setup.py:34-35`). |
| Salto dal muro | (nessun tasto nel layout combattimento) | `IA_Jump` via Blueprint GASP | `IMC_Sandbox` (rimosso) | `bIsJumpJustPressed` di Mover | **MOV** | `GSTN_ControlledWallMovementMode.cpp:85`. *DEDOTTO*: con `IMC_Sandbox` sostituito e A tolta da `IA_Jump`, il salto dal muro non ha tasto nel layout di gioco. **Da verificare in editor.** |
| Special | Y / MMB | `IA_GASNPP_Special` | `IMC_GASNPP_Test` | `Special` → `Combat.Special` | **CMD** | |
| Forte | RT / K | `IA_GASNPP_Strong` | `IMC_GASNPP_Test` | `Strong` → `Combat.Strong`, `Held.Throw` | **CMD** | |
| Presa / raccogli / cambio kit | B / O | `IA_GASNPP_Interact` | `IMC_GASNPP_Test` | `Interact` → `Link.Grab/Release`, `Held.PickupDrop`, `Held.KitSwap`, `Mount.Interact`, `Command.Interact.PickUp` | **CMD**, poi scrittura server | Il cambio kit è deciso dal comando (deterministico), poi applicato dal server come nuova revisione del loadout (`GSTN_KitSwapSubsystem.h:16-24`). Corretto. |
| Lanci (N., F., Cr., Cr.F.) | stick + bottone durante la presa | (come sopra) | | slot di lancio del link (`GSTN_LinkSlots.h`) | **CMD** | |
| Mash per liberarsi | bottoni / stick | (come sopra) | | `GSTN_LinkMash.h` | **CMD** | |
| Schivata | RB / U | `IA_GASNPP_Dodge` | `IMC_GASNPP_Test` | `Dodge` → `Movement.Dodge` | **CMD** | |
| Scudo / parata | LB / I | `IA_GASNPP_Shield` | `IMC_GASNPP_Test` | `Shield` → `Defence.Shield` | **CMD** | DI/SDI e tech leggono stick e bottoni del comando. |
| Accovacciata | LT / C | `IA_GASNPP_Crouch` | `IMC_GASNPP_Test` | `Crouch` → `Combat.Crouch`, `Spin.Drill`, `Mount.Crouch` | **CMD** | Azione propria perché `IA_Crouch` di GASP fa scattare anche la scivolata del Blueprint (`setup_player_input_contexts.py:58`). |
| Scatto (run boost) | L3 | `IA_GASNPP_RunBoost` | `IMC_GASNPP_Test` | `BoostCommand` del profilo | **CMD** | Consuma il tasto perché il Blueprint GASP ha un evento **tasto diretto** su L3 (vedi §2). |
| Dash | L (nessun tasto pad) | `IA_GASNPP_Sprint` | `IMC_GASNPP_Test` | `Sprint` → `Movement.Dash` | **CMD** | Il layout dell'owner dice "L3 dash": oggi L3 = run boost e il dash non ha tasto sul pad. *Da chiarire.* |
| Mira di precisione (reticolo) | clic stick dx (auto) | `ReticleToggleAction` (creata in automatico) | contesto automatico prio 100 | `bPrecisionAim` | **CMD** (stato), LOC (reticolo) | `GSTN_InputConfig.h:118-121` (tasti di default `Gamepad_Right2D`, `Gamepad_RightThumbstick`), `GSTN_CombatSimComponent.cpp:914`. |
| Stick di mira | stick dx | `AimStickAction` (auto) | contesto automatico | `AimStickX/Y` | **CMD** | Stesso stick della camera: lo leggono sia camera (LOC) sia comando (CMD). |
| Comandi forzati (IA, bot) | — | — | — | `ForcedCommand` | **CMD** | `UGSTN_AIInputSource`, gira sul server. |
| Abilità di prova Dash/Buff | tasti di prova | `IA_GASNPP_Dash/Buff` | `IMC_GASNPP_Test` | `ActivationInput` | **GAS** | Solo harness. |
| Loadout (kit, mod, special) | menu | `ConfirmAction/BackAction/MoveAction` del menu | contesto del menu (prio alta) | — | **RPC** validata | `GSTN_LoadoutComponent.h:75,82,116`. Fuori dalla simulazione per scelta, entra come revisione a confine di frame. Corretto. |
| Sensibilità stick (input feel) | opzioni | — | — | soglie stick | **RPC** → revisione | `ServerSetInputFeel`: le soglie diventano dati del loadout, uguali su tutte le macchine. Corretto. |
| Interazione con oggetti del livello | B (tenuto) | (Blueprint, *DEDOTTO*) | | — | **RPC** non predetta | `GSTN_InteractorComponent.h:82`; `SetInteractHeld` non è chiamato da nessun C++, quindi da un Blueprint. Va bene per leve/porte, non per cose che decidono un duello. |
| Camera: look, ricentra, spalla, distanza | stick dx / mouse | `LookAction`, `RecenterAction`, `SwapShoulderAction`, `DistanceAction`, `AssistMoveAction` | `LookMappingContext` del profilo camera | — | **LOC** | `GSTN_CameraComponent.cpp:518-557`. Entra nella partita solo come `CameraYaw` nel comando. Giusto così. |
| Rimappatura tasti | opzioni | tutte | tutti | — | **LOC** | `GSTN_KeyBindingsSubsystem.h`: un tasto spostato non cambia nulla in rete (si ferma all'azione). |
| Menu loadout, pad di debug | stick, A, B | azioni del menu | contesto del menu | — | **LOC** → RPC | `GSTN_LoadoutMenuSubsystem.cpp:291,345-363`. |
| Input display di debug | P | — | — | — | **LOC** | `SGSTN_InputDisplay.cpp:605`, tasto letto direttamente in un widget Slate: ammesso (debug). |
| Editor timeline | W/E/R/K | — | — | — | solo editor | `GSTN_TimelineViewportClient.cpp`. |
| Console (`~`) e cvar | tastiera | — | — | — | **LOC**, alcune cambiano lo stato | Vedi §2. |

## 2. Letture dirette (fuori dal comando) ed eccezioni

| Dove | Cosa | Problema in rete | Giudizio |
|---|---|---|---|
| Blueprint `SandboxCharacter_Mover` (GASP) | Evento **tasto fisico** `Gamepad_LeftThumbstick` e `Z`: volo di debug (`setup_player_input_contexts.py:130-147`) | Non passa da nessun contesto, quindi rimappare o sostituire `IMC_Sandbox` non lo spegne; oggi è "zittito" facendo consumare L3 a `IA_GASNPP_RunBoost`. Z da tastiera resta attivo. | **Da chiudere** (cvar di debug o rimozione nel nostro figlio, non in GASP). |
| Blueprint GASP | Aggiunge `IMC_Sandbox` a ogni BeginPlay; lo togliamo a ogni frame (`ApplyMappingContexts`) | Funziona, ma è una gara: un frame con entrambi i contesti attivi è possibile. | Accettabile oggi, fragile. |
| Blueprint GASP | Scrive l'input di Mover (`FCharacterDefaultInputs`: intento di movimento, salto, rotazione) da `IA_Move`, `IA_Jump`... | Secondo canale in rete con gli stessi dati dello stick; il salto dal muro legge da qui. | **Principale debito**: vedi decisione 1. |
| `GSTN_WallMovementDebug.cpp:98-121` | `gstn.Wall.Jump`/`Detach` scrivono variabili globali lette **dentro la simulazione** (`ConsumeJumpRequest`, `GSTN_ControlledWallMovementMode.cpp:85-88`) | Solo sulla macchina che digita: client e server divergono → correzione. Compilato fuori dallo Shipping. | Ammesso come debug, ma va marcato e mai usato in prove di rete. |
| cvar e comandi console che cambiano lo stato (`gstn.Wall.Launch/Teleport/Enter`, `gstn.Held.*`, `gstn.Projectile.*`, `gstn.Debug.Kit`, `gstn.Respawn.*`, `gstn.Mission.*`, `gstn.Platform.SetMask`...) | Elenco completo nel report di scansione, righe citate in §1 | Molti girano solo sul server o passano dal percorso server (es. `gstn.Debug.Kit`); alcuni sono locali. | Debug: fuori dal 1v1. In un 1v1 classificato vanno **disattivati in Shipping** (già così per la maggior parte: verificare). |
| Camera, menu, rimappatura, opzioni, input display | Leggono azioni (o il tasto P) localmente | Nessuno: non toccano la simulazione, o la toccano solo tramite il comando (`CameraYaw`). | **Eccezioni giuste.** |
| Harness PIE (`GSTN_GASNPPTestComponent.cpp:439-485`, `GSTN_TestSlotUtils.cpp:51-60`) | Inietta **azioni** (`InjectInputForAction`) | Passa dallo stesso percorso del giocatore: le prove vedono ciò che vede la rete. | **Buon esempio**: è un argomento a favore delle azioni (vedi §3). |

Codice C++ di gioco che legge tasti fisici: **nessuno** (ricerca `IsInputKeyDown|WasInputKeyJust|EKeys::|BindKey|GetInputAnalog` su `Source/` e
`Plugins/GSTN*`, esclusi editor e test). Gli unici `FKey` sono i default delle azioni automatiche del reticolo (`GSTN_InputConfig.h:118-121`),
che creano un'azione, quindi non sono letture dirette.

## 3. Analisi indipendente della strategia

### Come è fatta la rete qui (fonti)
- **Network Prediction è client-server, non peer-to-peer.** Il client che controlla produce un `InputCmd` a ogni tick fisso e lo
  manda al server; il server lo mette in un piccolo buffer e simula; il client predice e, se lo stato del server differisce, torna
  indietro e risimula con i suoi comandi salvati (`Plugins/GASNPP/NetworkPrediction/readme.txt` righe 12-17, 62-74, 161-190; stesso
  testo nel plugin NetworkPrediction di Epic). L'input in rete è **solo** ciò che sta nell'`InputCmd`.
- **Mover** prende l'input dai produttori `IMoverInputProducerInterface::ProduceInput` e lo mette in `FMoverInputCmdContext`, che
  Network Prediction invia (documentazione Epic "Mover", sezione input/produce input; nel codice: `GSTN_CombatSimComponent.h:123,155`).
- **Il fork GAS** non usa le prediction key di GAS stock: legge le azioni delle mapping context elencate nelle impostazioni e ne
  manda lo stato in `FAbilitySimInputCmd` (`Plugins/GASNPP/AbilitySystemSimulation/README.md:26-28`, `docs/architecture/gas-npp.md`).
- **Enhanced Input** è un sistema locale: azioni, contesti, trigger e modificatori vivono sul `LocalPlayer`; non c'è replica
  (documentazione Epic "Enhanced Input"; nel codice: `UEnhancedInputLocalPlayerSubsystem`, per giocatore). La rimappatura
  ("Player Mappable Keys") è per giocatore e locale.
- **L'avversario** nel nostro setup è **interpolato** (`combat.md` §12.7: policy "tutto Interpolated", buffer 100 ms; la previsione
  in avanti costa ~5 rollback al secondo per giocatore remoto ed è rimandata).

### Cosa vuol dire "robusto" per un 1v1 qui
1. **Completezza**: tutto ciò che cambia l'esito sta nel comando o nello stato sincronizzato; niente si legge "di nascosto".
2. **Determinismo**: stesso comando + stesso stato → stesso risultato su client e server (regola 1 di CLAUDE.md).
3. **Tolleranza alle perdite**: se un comando arriva tardi, il server ripete l'ultimo senza ripetere le pressioni (`PrepareRepeatedInput`,
   presente in `GSTN_InputSimTypes.h:135`); i tap più brevi di un tick non si perdono (evento `Started` → `PendingTaps`, `GSTN_InputSource.cpp:56-60,128-145`).
4. **Equità**: chi ha latenza zero (host) e quanto in ritardo si vede l'avversario.
5. **Banda e costo dei rollback**.
6. **Anti-cheat**: il server simula da solo; il client può solo mandare input. Ogni input "valido" è accettato (macro e turbo non
   si fermano da nessun design di input), ma niente di più.

### Dove l'ipotesi è giusta
- Legare ogni input di gioco a un **bottone logico astratto** e non al tasto è la scelta giusta: rimappatura per giocatore,
  controller diversi, tastiera e pad uguali per la simulazione, prove automatiche che iniettano azioni come un giocatore. Il progetto
  lo fa già (`GSTN_KeyBindingsSubsystem.h`: "un tasto spostato non cambia nulla nella simulazione, nella predizione e in rete").
- Un tasto letto direttamente (come L3/Z nel Blueprint GASP) **è davvero un rischio**: sfugge a rimappatura, contesti e comando.

### Dove è incompleta o sbagliata
- **"Azione" non significa "in rete".** Un'azione Enhanced Input letta in un Blueprint e usata per cambiare lo stato è fragile quanto un
  tasto. La regola giusta è: *azione → bottone logico → comando per frame → simulazione*. L'azione è il primo anello, non la garanzia.
- **Non tutto deve diventare azione di gioco.** Camera, menu, opzioni, debug devono restare locali: mandarli in rete aggiunge banda e
  rollback senza dare niente. Entra solo il loro *effetto* sulla partita (la yaw della camera nel comando).
- **Più canali = meno robusto, non di più.** Oggi lo stesso stick va in rete due volte e il salto esiste in tre forme (comando GSTN, Mover
  di GASP, fork GAS). Ogni canale in più è un posto dove client e server possono leggere cose diverse.
- **Azioni "con valori" contro dati grezzi per frame**: per la rete conviene il secondo. Trigger e modificatori di Enhanced Input
  (hold, tap, dead zone, curve) girano **solo** sulla macchina locale e dipendono dal frame rate del render, non dal tick fisso: se una
  logica di gioco dipende da un trigger "Hold 0,3 s" nell'IMC, la durata vera la decide il client. Il progetto fa bene: legge solo
  premuto/tenuto e lo stick grezzo, e fa hold, charge, motion e buffer **dentro** la simulazione, in frame (`input.md` §5, §10).
- **Il 1v1 dipende soprattutto da topologia e avversario remoto**, non dall'input: con host in ascolto l'host gioca a latenza zero;
  con l'avversario interpolato lo si vede ~100 ms in ritardo e ogni colpo subito è una correzione (`combat.md` §12.7). I picchiaduro
  online di riferimento usano rollback simmetrico peer-to-peer; NP è asimmetrico (server autorità). Non è un errore, è un compromesso da
  conoscere.

### Altre cose controllate
- **Banda**: `FGSTN_InputCmd` è piccolo (bit dei bottoni + assi quantizzati); il doppione dello stick in Mover costa poco ma non serve.
  Il grosso del traffico dei giocatori è lo stato (44-50 KB/s per client, nota netcode del 2026-11-09), non l'input.
- **Replay e prove**: perché un replay funzioni bastano comandi + stato iniziale + revisioni del loadout; i canali extra (Mover del
  Blueprint, fork GAS) sono tre flussi da registrare e allineare. Un canale solo semplifica l'harness e la cattura PIE.
- **Validazione lato server**: il comando ha campi limitati (32 bit di bottoni, assi quantizzati); va aggiunto un controllo che un
  bottone non presente nel kit/config venga ignorato e che `CameraYaw`/`AimYaw` siano solo usati come direzione (già così).

## 4. Piano a passi piccoli (un thread per checkpoint)

Ambito 1v1 (comandi di combattimento e movimento). Nessun passo tocca i Blueprint GASP: si lavora nei nostri figli e nei dati
(regola 14). Il cloud non compila: i passi con codice si compilano e provano sul PC (regola 15h).

| # | Obiettivo | Dove | Modello | Stima | Rischio | Verifica in editor |
|---|---|---|---|---|---|---|
| P0 | **Verifiche in editor** (5 domande): il salto dal muro ha un tasto nel layout di gioco? Che cosa scrive il Blueprint GASP nell'input di Mover (salto, sprint, walk, strafe)? Z attiva ancora il volo di debug? Priorità vera di `IMC_GASNPP_Test` (5 o 10)? L3 = dash o run boost? | PC, editor, sola lettura | Sonnet medium | 1-2 $ | basso | è il passo stesso |
| P1 | **Salto dal muro dal comando**: il modo muro legge `Input.Button.Jump` dal comando GSTN (via un piccolo input Mover scritto dal nostro produttore, come `FGSTN_AirTurnInputs`) invece del salto di GASP | cloud codice + PC compila e prova harness | Opus medium | 2-3 $ | medio (movimento in rete) | harness 2 client, salto dal muro |
| P2 | **Un solo produttore per Mover**: il nostro `ProduceInput` scrive intento e salto dal comando anche senza `AnalogWalk`; il Blueprint GASP non contribuisce più (in un figlio, o con un flag) | cloud + PC | Opus medium | 3-4 $ | medio-alto (feel locomotion) | harness + video corsa/stop, confronto prima/dopo |
| P3 | **Fuori dalla partita i percorsi di debug**: volo L3/Z spento da cvar nel nostro figlio, `gstn.Wall.Jump/Detach` instradati nel comando o marcati "solo offline", check che le cvar che cambiano stato siano compilate fuori dallo Shipping | cloud + PC | Sonnet medium | 1-2 $ | basso | console in PIE con 2 client |
| P4 | **Ritiro del canale fork GAS per il gioco**: `GSTN_GA_Jump` e `IMC_GAS_Abilities` solo personaggio di prova (decisione audit 2), nessuna abilità di partita con `ActivationInput` | cloud + PC | Sonnet medium | 1-2 $ | basso | harness |
| P5 | **Guardia automatica**: un test/validatore che fallisce se codice di simulazione legge `UEnhancedPlayerInput`, `APlayerController`, `EKeys` o variabili globali; e un validatore dati che segnala un `ActivationInput` su un'abilità di kit | cloud | Sonnet medium | 1-2 $ | basso | nessuna (test headless) |

Totale 1v1: **~10-14 $**, di cui P0, P1, P2, P3 richiedono il PC. Ordine consigliato: P0 → P1 → P3 → P4 → P2 → P5 (P2 è il più
rischioso per il feel: dopo che il resto è stabile).

**Fuori dal 1v1 (stima a parte)**: interazione con oggetti del livello via comando invece di RPC (solo se un oggetto decide un
duello: ~3-4 $); menu, camera, opzioni, rimappatura restano locali (0 $); topologia 1v1 e avversario predetto in avanti (vedi
decisione 3: ricerca ~2-3 $, implementazione combat.md §12.7 molto più cara, 15-30 $, non ora).

## 5. Decisioni per l'owner (max 3)

1. **Canali di input in rete.** A (consigliata): un solo comando per frame (quello GSTN) e Mover lo legge dal nostro produttore;
   passi P1, P2, P4, ~6-9 $. B: lasciare i tre canali e documentarli, 0 $, ma il salto dal muro e i doppioni restano. C: solo P1 (salto
   dal muro), ~2-3 $. Resta fermo nel frattempo: niente.
2. **Percorsi di debug nella simulazione.** A (consigliata): spegnerli fuori dal test offline (P3, ~1-2 $). B: lasciarli, sapendo che
   usarli con 2 giocatori crea correzioni.
3. **1v1: chi fa da server.** A (consigliata): per il 1v1 classificato server dedicato (o host scelto dal matchmaking ma con avviso
   di vantaggio), e una breve ricerca sul buffer di interpolazione minimo (~2-3 $, cloud). B: host in ascolto e nient'altro (0 $,
   l'host gioca a latenza zero). C: aprire ora la previsione in avanti dell'avversario (15-30 $, sconsigliata prima di M12-M14).

Risposta tipo: "1A 2A 3A".

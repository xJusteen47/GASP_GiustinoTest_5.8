# Mappa delle cartelle (Content, Source, Plugins)

Stato: branch `claude/project-thread-98dml5` (stato di `_ue59`, con air momentum vbnr9g già dentro), 2026-11-18.
Solo lettura: questo file non sposta nulla. Gli spostamenti in arrivo sono in fondo.

## 1. Il quadro in tre righe
- **`Plugins/GSTN*/Source/`** = quasi tutto il nostro codice di gioco, movimento compreso. Ogni plugin ha il suo `README.md` (indice: `Plugins/README.md`).
- **`Source/GASP_Giustino2/`** = il modulo del progetto: muri (wall slide/climb), camera anchor, abilità e kit di test. Niente logica di corsa/salto.
- **`Content/`** = asset. `Blueprints/`, `Input/`, `Levels/` sono di GASP (Epic): non si toccano. I nostri asset sono in `Content/GASNPPTest/`.

## 2. Come è fatto un sistema di movimento (vale per tutti)
Ogni sistema è diviso in 3 strati, sempre negli stessi posti:

| Strato | Dove | Cosa ci va |
|---|---|---|
| **Logica pura** (matematica, nessun GAS/NP/Mover) | `Plugins/GSTNCombat/Source/GSTNCombatCore/` cartelle `Movement/`, `Jump/`, `Rules/`, `Timeline/` | Le formule: qui si scrive la logica di una mossa. Testabile senza editor. |
| **Simulazione** (rete, rollback, Mover) | `Plugins/GSTNCombatNP/Source/GSTNCombatNP/` cartelle `Simulation/`, `Movement/`, `Abilities/` | Chiama la logica pura ogni tick (62,5 Hz) e la applica al Mover (LayeredMove = "spinta" applicata al corpo). |
| **Presentazione** (solo grafica) | `GSTNCombatNP/.../Presentation/`, plugin `GSTNAnimation`, `GSTNPresentation` | Anim layer, VFX. La simulazione non la legge mai. |
| **Valori** (DA) | `Plugins/GSTNCombatGAS/.../Kits/GSTN_LocomotionProfile.h` → asset `DA_Locomotion_*` | Il kit punta al suo profilo con l'estensione `FGSTN_LocomotionKitExtension`. Kit senza profilo = valori di default. |

Percorsi abbreviati sotto: `Core/` = `Plugins/GSTNCombat/Source/GSTNCombatCore/{Public,Private}/`, `NP/` = `Plugins/GSTNCombatNP/Source/GSTNCombatNP/{Public,Private}/`.

## 3. Dove sta il codice del movimento

### Corsa e skid (corsa Mario: run, sprint1, sprint2)
| File | Cosa fa | Valori |
|---|---|---|
| `Core/Movement/GSTN_RunStageTypes.h` + `GSTN_RunStageLogic.cpp` | Stadi della corsa: quando si sale/scende di stadio, velocità per stadio | DA_Locomotion > Run Stages |
| `Core/Movement/GSTN_RunSkidTypes.h` + `GSTN_RunSkidLogic.cpp` | Quando parte lo skid (inversione di stick) e come frena | DA_Locomotion > Run Stages > Skid |
| `NP/Simulation/GSTN_CombatSimRunSkid.cpp` | Il tick di simulazione che avvia/ferma lo skid | (legge il profilo) |
| `NP/Movement/GSTN_LayeredMove_RunSkid.h/.cpp` | La frenata applicata al Mover durante lo skid | idem |
| `NP/Movement/GSTN_RunStageSyncState.h` | Lo stadio di corsa salvato nello stato sincronizzato (rollback) | — |
| `NP/Movement/GSTN_GroundProbe.h` | Sonda del pavimento condivisa da skid e momentum slide | — |
| `NP/Presentation/GSTN_RunPresentationComponent.h/.cpp` + `GSTN_RunLayerAnimInstances.h` | Cosa si vede: anim layer di skid e partenza corsa | asset anim nel kit |
| `Core/Movement/GSTN_AnalogWalkTypes.h` | Camminata analogica e partenza | DA_Locomotion > Analog Walk |

### Salto
| File | Cosa fa | Valori |
|---|---|---|
| `Core/Jump/GSTN_JumpLogic.h/.cpp` | Impulso e gravità del salto (modello HoldWindow "Mario 2D" di default, CutUntilApex opzionale) | proprietà del GA (sotto) |
| `Core/Jump/GSTN_JumpDesigner.h/.cpp` | Il salto dai numeri del designer (altezza, tempi) convertiti in velocità/gravità | DA_Locomotion > Jump Designer |
| `Core/Jump/GSTN_GravityPhases.h`, `GSTN_JumpTypes.h`, `Core/Movement/GSTN_GravityFrame.h` | Fasi di gravità (salita, taglio, caduta, fast fall) e "giù" locale | — |
| `NP/Abilities/GSTN_GA_PlatformJump.h/.cpp` | L'abilità del salto (tasto A): legge input, avvia la logica | categorie "Jump" e "Jump\|Kit Data" sul GA; default anche in ini e negli script kit |
| `NP/Simulation/GSTN_CombatSimJump.cpp` | Il salto dentro il tick di simulazione | — |
| `Source/.../GAS/GSTN_GA_Jump.h` | Vecchio salto GAS del progetto (usato dai muri), non quello di combattimento | — |

### Aria: momentum, drift (controllo aereo), rotazione
| File | Cosa fa | Valori |
|---|---|---|
| `Core/Movement/GSTN_AirMomentumTypes.h` + `GSTN_AirMomentumLogic.cpp` | Due canali: velocità del giocatore e spinte esterne (knockback, trampolino) che lo stick neutro non annulla; stili HighFriction (Astro, default) / Carry (Mario) / MoverLegacy | DA_Locomotion > Run Stages > Air > Momentum |
| `Core/Movement/GSTN_RunSkidTypes.h` | Controllo aereo della corsa Mario | DA_Locomotion > Run Stages > Air |
| `NP/Movement/GSTN_AirTurn.h/.cpp` | Velocità di rotazione in aria (sostituisce i 300°/s fissi del BP GASP `BP_MovementMode_Falling`) | DA_Locomotion > Analog Walk (air turn) |
| `Plugins/GSTNDefence/.../GSTN_DIRulesMath.h` | DI/SDI quando si è colpiti (non il drift normale) | regole Defence nel kit |

### Accovacciata
| File | Cosa fa | Valori |
|---|---|---|
| `Core/Rules/GSTN_GroundCrouch.h/.cpp` | Crouch a terra (LT) | `FGSTN_GroundCrouchKitExtension` nel KIT |
| `Core/Rules/GSTN_AirCrouch.h/.cpp` | Crouch in aria | `FGSTN_AirCrouchKitExtension` nel KIT (script `Tools/GAS/setup_air_crouch.py`) |

### Schivata (dodge) e dash
| File | Cosa fa | Valori |
|---|---|---|
| `NP/Abilities/GSTN_GA_Dash.h/.cpp` | Abilità `UGSTN_GA_Dash`, `UGSTN_GA_GroundDodge`, `UGSTN_GA_AirDodge` (RB) e l'impulso istantaneo | `FGSTN_DashMoveData` sul GA |
| `Core/Movement/GSTN_DashTypes.h` | Dati del dash/dodge (velocità, durata) | idem |
| `Core/Movement/GSTN_MoveTimelines.h` | Timeline di default di ground/air dodge (frame, intangibilità) | codice |
| `Core/Movement/GSTN_DodgeLanding.h/.cpp` | Lag di atterraggio del dodge e belly slide | codice/GA |

### Altre cose di movimento utili
- Atterraggio: `Core/Timeline/GSTN_Landing.cpp`; ledge check: `Core/Timeline/GSTN_Ledge.cpp` + `NP/Movement/GSTN_LayeredMove_LedgeGuard.h`.
- Momentum slide (attacco partito correndo): `Core/Movement/GSTN_MomentumSlide.h` + `NP/Movement/GSTN_LayeredMove_MomentumSlide.h`.
- Knockback, pushback, hitstop, lanci: `NP/Movement/GSTN_LayeredMove_{Knockback,PushBack,Freeze,Launch}.h`.
- Capsula per mossa (dropkick): `NP/Movement/GSTN_BodyCapsuleModifier.h`.
- Il "cuore" che chiama tutto ogni tick: `NP/Simulation/GSTN_CombatSimComponent.h/.cpp`.
- Muri: `Source/GASP_Giustino2/{Public,Private}/WallMovement/` (modes, transizioni, librerie).

### Camera
| File | Cosa fa | Valori |
|---|---|---|
| `Source/GASP_Giustino2/.../Camera/GSTN_CameraAnchor.h/.cpp` | Àncora della camera: niente sobbalzi su stomp e atterraggi | `Content/Blueprints/Data/DA_CameraAnchor_Default` |

## 4. Content: i nostri asset
| Cartella | Cosa c'è |
|---|---|
| `Content/GASNPPTest/Combat/` | `DA_Locomotion_Default` (usato da tutti i kit di test) e `DA_Locomotion_Playtest` (kit playtest, con jump designer); tutti i `KIT_Test_*` (Overlord, Playtest, Light/Medium/Heavy, varianti DAir/Dropkick), hurtbox `HBS_*`, mappa `L_Test_Combat` |
| `Content/GASNPPTest/Combat/Overlord`, `Reaction`, `Archetypes`, `Playtest`, `AirCrouch`, `Mods` | Mosse Overlord (AM/AS/GA/TL), reazioni, abilità degli archetipi, placeholder |
| `Content/GASNPPTest/Input/` | Input action del gioco (`IA_GASNPP_*`, `IMC_GASNPP_Test`, `IMC_GSTN_Locomotion`) |
| `Content/GASNPPTest/TestMaps/Hub/` | Materiali dell'hub (`L_Test_Hub` è in `Content/Levels/Test/`) |
| `Content/GAS/`, `Content/AnimPipeline/` | Zaini e input GAS; sorgente Live Link (copia voluta delle AS del dropkick) |
| `Content/Blueprints/`, `Input/`, `Levels/` | GASP di Epic (+ pochi nostri file, es. DA_CameraAnchor, wall layer, DepthMarker) |
| `Plugins/*/Content/` | Solo pochi plugin: `GSTNCombat/Rules`, `GSTNPresentation/VFX`, `Test/` di Defence, Link, Piggyback; `GSTNParts/Samples` |

## 5. Dove scriveresti tu una mossa nuova
1. Formula pura in `Core/Movement/` (o `Jump/`) con un `*Types.h` per i valori: compila senza editor e ha i test in `Core/Private/Tests/`.
2. Valori nuovi come campi di `FGSTN_*Tuning` dentro `GSTN_LocomotionProfile.h` → compaiono nel DA_Locomotion, modificabili da Blueprint/editor.
3. Applicazione nel tick: un file `NP/Simulation/GSTN_CombatSim<Nome>.cpp` e, se spinge il corpo, un `NP/Movement/GSTN_LayeredMove_<Nome>`.
4. Grafica in `NP/Presentation/` o anim layer di `GSTNAnimation`.

## 6. Cosa si sposterà (riorganizzazione fase 2, non ancora fatta)
Piano: `handoffs/riorganizzazione-cartelle-piano.md` + mappa CSV. Solo asset, il codice C++ non si sposta.
- `Content/GASNPPTest/...` → `/Game/GSTN/` (gioco), `/Game/GSTNTests/` (kit e fixture di test), `/Game/GSTNExamples/`, `/Game/GSTN/_Archive/` (DAir V7/V8, Jab V2; nulla cancellato).
- `DA_Locomotion_Default` → `/Game/GSTN/Movement/`; `KIT_Test_Overlord` → `/Game/GSTN/Characters/Overlord/` con `Moves/<mossa>/`; input → `/Game/GSTN/Input/`.
- GASP (`Blueprints`, `Input`, `Levels`) resta; escono solo i nostri 18 file.
- Si fa sul fisso con l'editor, quando nessun branch ha modifiche binarie non integrate; rinomine (`_Test_`, `GASNPP`→`GSTN`) in una fase 3 separata.

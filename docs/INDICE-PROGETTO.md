# Indice del progetto GASP_Giustino2

Pagina di benvenuto per chi arriva sul progetto (persona o agente): cosa esiste già, dove sta, se funziona.
Si legge in 5 minuti. Fotografia del 2026-11-23 su `main` (a5b20950); ricontrollata dal bibliotecario il 2026-11-27 su `main` ca2a8952 (PR #38-#59 entrate dal 24 al 27-11: sezioni 4, 5.6, 5.14, 5.16, 6 e Artefatti aggiornate; il resto della sezione 5 non è stato riletto riga per riga). Se un percorso qui non esiste più, l'indice è
vecchio: aggiornalo nello stesso commit che sposta il codice.

**Regola per ogni brief e ogni thread:** _prima di scrivere codice consulta `docs/INDICE-PROGETTO.md` e dichiara cosa
riusi._ Esempio vero: l'ergonomia di mira (AimPolicy, finestra di reverse, momentum) esisteva dal 16-11 e un thread
successivo stava per riscriverla.

Decisioni vive, versioni approvate, cose sospese: **[STATO-ATTUALE.md](STATO-ATTUALE.md)** (vince sugli altri documenti). Per agenti esterni: [BRIEFING-ANTIGRAVITY.md](BRIEFING-ANTIGRAVITY.md).

Pagine sorelle: [DOVE-METTO-MANO.md](DOVE-METTO-MANO.md) (15 casi "voglio cambiare X") ·
[notes/audit-codice.md](notes/audit-codice.md) (doppioni, buchi di documentazione, cose da snellire) ·
`handoffs/INDICE.md` nei project files (un rigo per thread, con costi: è un registro dei lavori, non questa mappa).

## Artefatti
Elenco con lo strumento Artifact il 2026-11-27: **23 artefatti** dell'owner (prima ne erano indicizzati 11). ARCHIVIO = superato, resta online solo come storia;
il segno ARCHIVIO è stato messo sugli artefatti il 23-11. Proposte di stato per i nuovi in `docs/notes/archivio-proposta.md` §E (non applicate).
| Titolo | Link | Aggiornato | Stato |
|---|---|---|---|
| Piano: tutto il gameplay nel rollback (Doc) | https://claude.ai/artifact/SfrTpktgxySeSCUc2gWkPp | 25-11 | NUOVO, vivo (decisione "niente misto" del 25-11) |
| Mosse di esempio GSTN (guida designer) | https://claude.ai/artifact/DyiBrsycoRfXNSMoZUBxun | 23-11 | VIVO (aggiornato il 23-11 con la Combat Move) |
| Architettura plugin combattimento | https://claude.ai/artifact/CLq2srEECQnd6bKDbBmm72 | 23-11 | VIVO; la parte rete va riletta dopo il 25-11 ("niente misto") |
| Networking di GASP Giustino | https://claude.ai/artifact/J6v4JZMLmDAMuemuXYxJhn | 23-11 | VIVO; da rileggere dopo il 25-11 |
| Silhouette multi-angolo e VFX | https://claude.ai/artifact/MySdFXExJmvrDviYWkatdb | 15-11 | VALIDO (VFX in pausa) |
| Guida mosse Blueprint GSTN | https://claude.ai/artifact/La2oqPzCkPdcqL2kERy9D6 | 23-11 | ARCHIVIO (fusa in Mosse di esempio) |
| Analisi regole di lavoro | https://claude.ai/artifact/1LEUDPK4VjC1bRgKj8qYqP | 23-11 | ARCHIVIO (regole sostituite il 24-11) |
| Cose da fare a mano | https://claude.ai/artifact/XjMiMf16f85qP5TvqPCaSk | 23-11 | ARCHIVIO |
| Cascadeur per animare le mosse (Doc) | https://claude.ai/artifact/E7KV71LLBD47zLF4HBAXzc | 30-10 | ARCHIVIO (vale `docs/architecture/anim-pipeline.md`) |
| Piano CI GSTN | https://claude.ai/artifact/BCMqSA2e4gGgoqKyMHSXFu | 15-11 | MAI INDICIZZATO, proposta ARCHIVIO (vale `Tools/CI/Run-Timed.ps1`) |
| Skill e riferimenti animazione | https://claude.ai/artifact/TXAt4tWjuWW5CfnjXH6o63 | 15-11 | MAI INDICIZZATO, proposta ARCHIVIO (vale la skill `gstn-anim-cycle`) |
| Guida editor hitbox GSTN | https://claude.ai/artifact/8wHRKYEfidKpd7YkjPVK68 | 13-11 | MAI INDICIZZATO, da verificare |
| Roadmap milestones GASP | https://claude.ai/artifact/71C1XSzSZJvc193LTshYgU | 23-10 | MAI INDICIZZATO, proposta ARCHIVIO |
| Ganondorf Jab v12, FTilt v3.1, FTilt v3, Forward Tilt v2, Forward Tilt | QXjKkSERuq92vyhoZ7FHJ1, 74Z5ofar8jg72Gu2zWqeKY, SU8aaTYWJpg2b8hdbazBCp, JunSeGT1dG2gGXG8a2LUDy, 2hGLPcS97LPXD9ghToLYGN | 31-10 / 02-11 | MAI INDICIZZATI: storia delle pose (jab v12 e ftilt v3.1 sono le versioni approvate) |
| Jab Legibility Review | https://claude.ai/artifact/JA3WXMcQjPKfUjQ2FXYA6x | 31-10 | MAI INDICIZZATO, storia |
| Moveset stile Smash – Manichino (Doc) | https://claude.ai/artifact/6VgJiADCtxkD7dtJSwDbyS | 26-10 | MAI INDICIZZATO, superato da `docs/notes/notazione-mosse.md` (da confermare) |
| Mappa classi GSTNCombat | https://claude.ai/artifact/7sbZxxkLphi7evsgRMTzDb | 19-10 | MAI INDICIZZATO, vecchio (vale questa pagina §3) |
| Guida Unreal: cristallo colorato, god rays e ombra sulla nebbia (Doc) | https://claude.ai/artifact/KvF8y4oiqJhRSaDYwQP5HX | 24-10 | MAI INDICIZZATO, ricerca di stile |
| Wall Movement Mode — logica e impostazioni (Doc) | https://claude.ai/artifact/3KmdvUsgpFajZYZQJaeM1T | 14-10 | MAI INDICIZZATO, da confrontare con `docs/architecture/wall-movement.md` |
Non più nell'elenco il 27-11 (cancellati o non raggiungibili): "Strumenti AI per animare le mosse" (LsLBLG476QD5dZ8pcvsFtd), "Pipeline agentica per animare le mosse" (V3EDNzjL6oQ56Y8Tos4nii), "Depth marker diegetico: opzioni" (SXR2GcJHbrVTnR7p9SvE3d). Erano già ARCHIVIO.

## Contenuti
1. [Il gioco in un minuto](#1-il-gioco-in-un-minuto)
2. [Glossario](#2-glossario)
3. [Come è fatto il codice: chi chiama chi](#3-come-è-fatto-il-codice-chi-chiama-chi)
4. [Legenda dello stato](#4-legenda-dello-stato)
5. [I sistemi, uno per uno](#5-i-sistemi-uno-per-uno)
6. [Non ancora in main](#6-non-ancora-in-main)
7. [Dove sono i documenti](#7-dove-sono-i-documenti)

## 1. Il gioco in un minuto
Picchiaduro/platform 3D in co-op (stile Smash dentro Astro Bot), in rete con rollback. Ogni personaggio usa un **kit**
(Overlord, BirdKnight, FlameFury, Voyager) che decide le sue mosse; la **skin** cambia solo l'aspetto. Il movimento di
base viene dal Game Animation Sample di Epic (**GASP**), che non si tocca: tutto il nostro sta nei plugin `GSTN*`.

## 2. Glossario
| Termine | Cosa vuol dire qui |
|---|---|
| **GASP** | Game Animation Sample di Epic: personaggio, animazioni e mappe di partenza. Cartelle `Content/Blueprints`, `Input`, `Levels`: non si modificano (regola 14, `docs/notes/gasp-divergenze.md`). |
| **Mover** | Sistema di movimento di Unreal che sostituisce il CharacterMovement. Copia modificata in `Plugins/EngineOverrides/Mover`. |
| **NP / Network Prediction** | Il motore della simulazione in rete: ogni macchina simula lo stesso tick (62,5 Hz) e torna indietro (**rollback**) quando arriva un dato diverso. Tutto ciò che è "simulazione" deve dare lo stesso risultato su ogni macchina. |
| **GAS** | Gameplay Ability System: abilità (`GA_`), effetti (`GE_`), tag. Qui è un **fork** che gira dentro NP (`Plugins/GASNPP`). |
| **Tag** (gameplay tag) | Etichetta gerarchica tipo `Command.Attack.Light`. Quasi tutti dichiarati in C++ (nativi). |
| **Comando** | Ciò che il giocatore vuole fare (es. `Command.Attack`), ricavato da tasti + stick dal plugin di input. |
| **Kit** | Asset `UGSTN_PrimaryKitDefinition`: arma, mosse (comando → abilità), statistiche, estensioni. |
| **Estensione del kit** | Blocco di dati opzionale attaccato al kit (crouch, locomotion, difesa, presa...). Base `FGSTN_KitExtension`. |
| **Timeline** (di una mossa) | I dati frame per frame di una mossa: finestre, hitbox, mira, montage. `FGSTN_CombatTimelineData`. |
| **Montage** | Clip d'animazione Unreal suonata da una mossa. La timeline la cita; la simulazione non legge mai la posa. |
| **Hitbox / Hurtbox / Pushbox** | Volume che colpisce / volume colpibile / volume che spinge i corpi fra loro. |
| **FAF** | First Actionable Frame: primo frame in cui puoi fare altro dopo una mossa. |
| **Hitstop** | Pausa di pochi frame quando un colpo va a segno. |
| **AimPolicy** | Come una mossa sceglie la direzione all'avvio (tieni la faccia, segui lo stick, ...). |
| **Reverse window** | Primi frame in cui uno scatto dello stick all'indietro gira tutta la mossa. |
| **Link** | Legame fra due entità: presa, lancio, cavalcata, oggetto tenuto, trivella. Plugin GSTNLink. |
| **Layered move** | "Spinta" che una mossa o un colpo applica al corpo tramite Mover (knockback, skid...). |
| **cvar** | Variabile di console `gstn.*`, cambiabile a runtime. Quelle con `Debug` nel nome compaiono nel menu GSTN Debug. |
| **GSTN Debug** | Finestra dell'editor (Tools > Debug > GSTN Debug Options) con un controllo per ogni cvar `gstn.*Debug*`. |
| **Harness** | Prova di rete automatica in PIE (server + 2 client): `Tools/GASNPPTest/gasnpp_test.py`. |
| **Core / GAS / NP (moduli)** | Ogni plugin è diviso così: **Core** = regole pure senza rete, **GAS** = abilità ed effetti, **NP** = parte simulata. |
| **_ue59** | La copia del progetto dell'owner per il playtest sul fisso; un thread alla volta (`_ue59/IN-USO.txt`). |
| **DA_Locomotion_Default** | Asset locale dell'owner con i valori di corsa e salto. Non si committa mai. |

## 3. Come è fatto il codice: chi chiama chi
```
Tasti/stick ─▶ GSTNInput (comandi, buffer) ─▶ GSTNInputNP (input nella simulazione)
                                                   │ comando → abilità (tabella del kit)
                                                   ▼
Kit (GSTNCombatGAS) ─▶ abilità GA_ (GSTNCombatNP/Abilities) ─▶ timeline della mossa (GSTNCombat/Timeline)
                                                   │ ogni tick (62,5 Hz)
                                                   ▼
             GSTN_CombatSimComponent (GSTNCombatNP/Simulation): mira, hitbox vs hurtbox, hitstop
                 │ spinte via Mover (layered move)          │ estensioni: Defence, Link, Parts, SpinDrill...
                 ▼                                          ▼
            Mover (movimento)                       plugin di combattimento opzionali
                 │ stato presentato (mai il contrario)
                 ▼
  Presentazione: GSTNAnimation (ABP, layer) · GSTNCombatNP/Presentation · GSTNPresentation (VFX, suoni) · GSTNCamera
```
Regole d'oro (dettagli in `CLAUDE.md`): la simulazione non legge mai la presentazione; le abilità muovono il corpo solo
con le spinte Mover; i plugin **Core** non dipendono da GAS/NP/Mover; il modulo del progetto `Source/GASP_Giustino2` usa
tutti i plugin, nessun plugin usa lui.

## 4. Legenda dello stato
- **OK** = compilato e testato su UE 5.9. Ultima prova completa: `Automation RunTests GSTN` **1614/0** il 2026-11-21
  (`docs/notes/handovers/anim-round-2-postmerge.md` riga 6) + harness rete 50/50 (`docs/architecture/testing.md`).
- **SCRITTO** = in main ma mai compilato. Il buco del merge del 22-11 (PR #37) è chiuso: l'integrazione C4 del 24-11 ha compilato
  i gruppi 0-6 (`docs/notes/handovers/c4-integrazione.md` riga 5) e il riordino 2b ha fatto build + **1937 test** + smoke
  (`docs/notes/handovers/content-reorg-2b.md` riga 48). Le PR del 25-27/11 sono state compilate ognuna sul suo branch;
  una build di `main` intero dopo il 27-11 non risulta da nessun handover (**da confermare**).
- **SPENTO** = plugin presente ma spento a runtime finché un asset o una cvar non lo accende.
- **BRANCH** = esiste solo su un branch (sezione 6).
- Test: numero di macro di test nel plugin (`grep -c IMPLEMENT_SIMPLE_AUTOMATION_TEST`), prefisso per lanciarli.

## 5. I sistemi, uno per uno
Percorsi brevi: `Core/` = `Plugins/GSTNCombat/Source/GSTNCombatCore/Public/`, `NP/` = `Plugins/GSTNCombatNP/Source/GSTNCombatNP/Public/`,
`GAS/` = `Plugins/GSTNCombatGAS/Source/GSTNCombatGAS/Public/`, `Prj/` = `Source/GASP_Giustino2/Public/`.

### 5.1 Mira, direzione dell'attacco e reverse · OK
In gioco: all'avvio di un attacco il personaggio si gira verso lo stick; uno scatto all'indietro nei primi frame gira la mossa.
- Dati: `Core/Timeline/GSTN_TimelineTypes.h` (`EGSTN_AimPolicy`, `FGSTN_ReverseWindow`, `FGSTN_SteerWindow`, `EGSTN_MomentumMode`), campi di `FGSTN_CombatTimelineData`.
- Logica pura: `Core/Timeline/GSTN_StartupSteer.h` (unico punto di decisione). Lettura nel tick: `NP/Simulation/GSTN_CombatSimComponent.cpp` (`StepStartupSteer`), applicata da `NP/Movement/GSTN_SetFacingEffect.h`.
- Mira di precisione: `NP/Presentation/GSTN_PrecisionAimComponent.h`, `GSTN_SteerIndicatorComponent.h`.
- Parametri: sulla timeline di ogni mossa (asset `UGSTN_CombatTimeline` o inline sull'abilità). Test `GSTNCombat.Core.*` (`Private/Tests/GSTN_CombatTests_AttackAim.cpp`).
- Doc: `docs/architecture/combat-m3-moves-kits.md` (AimPolicy), handover `attack-aiming.md` (1594/0) e `attacchi-stick-reverse-1.md`.

### 5.2 Movimento: corsa, skid, salto, aria, crouch, dodge · OK (S1 SCRITTO)
In gioco: corsa a stadi stile Mario, frenata (skid), salto regolabile, controllo in aria, accovacciata, schivata.
- Regole pure: `Core/Movement/` (RunStage, RunSkid, AnalogWalk, AirMomentum, Dash, MomentumSlide, DodgeLanding), `Core/Jump/` (JumpDesigner, GravityPhases), `Core/Rules/` (GroundCrouch, AirCrouch).
- Nel tick: `NP/Simulation/GSTN_CombatSim*.cpp`, spinte `NP/Movement/GSTN_LayeredMove_*.h`, `GSTN_AirTurn.h`. Abilità `NP/Abilities/GSTN_GA_PlatformJump.h`, `GSTN_GA_Dash.h`.
- Parametri: asset `UGSTN_LocomotionProfile` (`GAS/Kits/GSTN_LocomotionProfile.h`, i `DA_Locomotion_*`) collegato al kit. Mappa dettagliata file per file: `docs/notes/mappa-cartelle.md` sul branch `claude/project-thread-f4ovn9`.
- S1 "stadi di corsa leggibili" (`gstn.RunStage.Readable`): SCRITTO (`docs/notes/handovers/run-stages-s1.md`: "NON compilato").
- Muri (wall slide/climb): `Prj/WallMovement/` (modes, transizioni, librerie `*Utils`), test 117, doc `wall-movement.md`.
- Attenzione: `Prj/GAS/GSTN_GA_Jump.h` è il **vecchio** salto ("TO BE REPLACED", riga 43); quello vero è `GSTN_GA_PlatformJump`.

### 5.3 Locomotion e animazione (ABP) · OK
In gioco: come si muove a schermo il personaggio. Default dal 2026-11-25: capsula greybox (`gstn.Greybox 1`) sul grafo semplice (`gstn.Loco.Base 2`); base GASP stock = `gstn.Loco.Base 1` (quella che i run del harness fissano, `Run-Timed.ps1`), poi nostri layer.
Quattro case, ognuna con un compito:
| Casa | Cosa contiene |
|---|---|
| `Plugins/GSTNAnimation` | Genitore dell'ABP (`GSTN_CharacterAnimInstance.h`), layer sostituibili, set per skin (`Anim.Trait.*`), nodi stance/piedi. Doc `anim-layers.md`. cvar `gstn.Loco.Stance*`, `gstn.Loco.TorsoLean`. |
| `NP/Presentation/` | Layer di skid e partenza corsa (`GSTN_RunLayerAnimInstances.h`, `GSTN_RunPresentationComponent.h`), lean (`GSTN_LocomotionLean.h`). |
| `Prj/Animation/` | Interruttore ABP stock/nostro (`GSTN_LocoBaseSubsystem.h`, `gstn.Loco.Base`) e postura per kit (`GSTN_KitStance.h`). |
| `Prj/WallMovement/Animation/` | Layer del muro. Doc `wall-anim.md`. |
Pipeline delle clip (Cascadeur → UE → video): `Tools/AnimPipeline/` (`python -m animpipe <verbo> <mossa>`), skill `animpipe-move`, doc `docs/architecture/anim-pipeline.md`. Animazioni approvate e bloccate: `docs/notes/anim-approvate.md`.

### 5.4 Combattimento: colpi, hitbox, hurtbox, pushbox · OK
In gioco: chi colpisce chi, quanto danno, che reazione, che knockback.
- Hitbox: `Core/Hits/GSTN_HitShape.h`, `GSTN_HitData.h` dentro le finestre della timeline. Hurtbox: `Core/Hits/GSTN_HurtboxTypes.h` (asset `HBS_*`, generati da `Tools/GAS/make_hurtbox_set.py` o dall'editor). Pushbox: `Core/Timeline/GSTN_Pushbox.h`, `GSTN_BodyCapsule.h`.
- Calcolo: `Core/Hits/GSTN_HitRunner.h` (pipeline a passi) chiamata da `NP/World/GSTN_CombatWorldSubsystem.h`, `NP/Simulation/GSTN_CombatAttacker.h` e `GSTN_CombatVictim.h`.
- Regole generali: asset `UGSTN_CombatRules` (`Core/Rules/`, default `Plugins/GSTNCombat/Content/Rules/`), impostazioni progetto `UGSTN_CombatSettings`.
- Debug: `gstn.Combat.Debug.Hitboxes`, `.Hurtboxes`, `.HitLog`. Test 256 Core + 228 NP. Doc `combat.md`, `combat-m4-hits.md`.

### 5.5 Kit, mosse, montage e dati · OK
In gioco: cosa sa fare un personaggio. Un kit lega comandi ad abilità; ogni abilità suona una timeline con il suo montage.
- Kit: `GAS/Kits/GSTN_PrimaryKitDefinition.h`; arma `GSTN_WeaponDefinition.h`; super `GSTN_SuperDefinition.h`; legame comando→abilità `GSTN_AbilityBinding.h`.
- Mossa: abilità figlia di `NP/Abilities/GSTN_GA_BaseCombat.h` + timeline `Core/Timeline/GSTN_CombatTimeline.h` (montage e sezione dentro `FGSTN_CombatTimelineData`).
- Asset: i kit veri stanno in `Content/GSTN/Characters/<Personaggio>/Kits/` e le mosse in `Characters/<Personaggio>/Moves/<Mossa>/` (`KIT_Test_Overlord`...), i kit dei test automatici in `GSTN/_Test/Combat/`; l'Asset Manager li cerca sotto `/Game/GSTN` (`Config/DefaultGame.ini` righe 10-19). Cartella di un asset dal nome: `Tools/GAS/gstn_content.py`. Il nome "Test" inganna: è il contenuto del gioco (vedi audit A1).
- Creazione: script `Tools/GAS/setup_overlord_*_kit.py` (usano le classi di `Prj/GASNPPTest/GSTN_TestKitArchetypes.h`). Editor timeline: plugin GSTNCombatEditor (doc `combat-m5-editor.md`). Notazione mosse (F.L, N.S, Cr.F.Sp...): `docs/notes/notazione-mosse.md` (branch `claude/anim-inventario-2026-11-23`).
- Mod dei kit: GSTNKitMods (SPENTO di default, attivo nel progetto). Raccolta e scambio kit: GSTNPickups. Zaini: GSTNBackpack. Schermata del carico: GSTNLoadoutMenu.

### 5.6 Presa, lanci, cavalcata, oggetti tenuti · OK (la presa "al collo" è BRANCH)
In gioco: afferrare un nemico, colpirlo (pummel), lanciarlo; caricare un alleato in spalla; portare oggetti.
- GSTNLink (125 test, `GSTNLink.*`): tipi di link `GSTNLinkCore/Public/GSTN_LinkTypeDefinition.h`, regole presa `GSTN_GrabRules.h`, lanci `GSTNLinkNP/Public/Moves/GSTN_LinkThrowMoves.h` + `Sim/GSTN_LinkThrow.h`, mash `Sim/GSTN_LinkMash.h`, chi è preso resta fermo `Movement/GSTN_LinkedMovementMode.h`, lanci dal kit `Input/GSTN_LinkSlots.h` (`ApplyKitThrows`).
- Presa al collo e lanci N./F./Cr./Cr.F. (modalità Notation) sono in main (grab-link). Spin con il nemico in mano (`Moves/GSTN_LinkGrabSpinMoves.h`, `Data/GSTN_GrabSpinTrack.h`) e numeri di Ganondorf sul kit di playtest (`Tools/GAS/setup_grab_ganon_playtest.py`, fonti `docs/notes/grab-ganon-dati.md`): in main dal 27-11 (PR #53), compilato, test GSTNLink 133/133 (`docs/notes/handovers/grab-ganon.md`).
- Cavalcata: GSTNPiggyback. Oggetti tenuti e lanciati: GSTNHeld (+ GSTNProjectile). Trivella e spin: GSTNSpinDrill (interfaccia `GSTN Spin Turnable` per gli oggetti di livello girati dallo spin); vite a terra: GSTNSpinScrew. Doc `combat-m6-defence-links.md`, `combat-m7-piggyback.md`, `combat-m10-held-objects.md`.

### 5.7 Difesa e parti distruttibili · OK (SPENTI di default)
- GSTNDefence: scudo, parry, tech, DI/SDI (83 test). README e `combat-m6-defence-links.md`.
- GSTNParts (+ GSTNPartsMinion): parti del corpo che si rompono, tabella di penetrazione (165 test). `combat-parts.md`.

### 5.8 Nemici, minion, piattaforme, superfici · OK
- GSTNMinion: 40+ nemici simulati come dati (103 test, molte cvar `gstn.Minion.Bench.*`). `combat-m8-minions.md`.
- GSTNSurface: cosa offre una superficie (scivolo, scalata, lancio). GSTNPlatform: nemici come piattaforme. GSTNLevelObjects: trampolino. Doc `traversal-surfaces.md`.

### 5.9 Camera · OK (tre strati)
- `Prj/Camera/GSTN_CameraAnchor.h`: àncora che toglie i sobbalzi alla camera di GASP (cvar `gstn.Camera.Anchor*`, `gstn.Camera.P1a`). Doc `camera-anchor.md`.
- `Plugins/GSTNCamera`: camera nuova a modi (piattaforma, duello, condivisa), cvar `gstn.Camera.*`. Doc `camera.md`. Non ancora su un pawn vero (lo fa M12 G5, BRANCH).
- `Prj/GASNPPTest/GSTN_PlaytestCamera.h`: ritocchi della camera di playtest (nonostante la cartella "Test" è usata in gioco).

### 5.10 Input · OK
In gioco: tasti e stick diventano comandi, con buffer, priorità e blocchi.
- `Plugins/GSTNInput` (122 test, `GSTNInput.Core.*`), `Plugins/GSTNInputNP` (simulazione, tabella comando→abilità `GSTN_CommandBindingSet.h`). Impostazioni `UGSTN_InputSettings`; tag dei designer in `Config/Tags/GSTNCommands.ini`.
- Mapping dei tasti: `Content/GSTN/Input/`, contesti elencati in `Config/DefaultGame.ini` (`AbilitySystemMappingContexts`). Doc `input.md`. Schema tasti in memoria progetto (X leggero, A salto, Y special, B presa...).

### 5.11 GAS e tag · OK
- Abilità del progetto: derivare sempre da `Prj/GAS/GSTN_GameplayAbility.h`. Tag nativi del progetto: `Prj/GAS/GSTN_AbilityTags.h`; tag dei plugin: tabella in `Plugins/README.md`. Fork: `Plugins/GASNPP/` (`PATCHES.md`). Doc `gas-npp.md`, `modularity-and-tags.md`. Skill `add-gstn-ability`.
- Mappa unica in italiano di tag, verbi, cosa è dentro/fuori dalla simulazione e cosa torna indietro al rollback, con il file di ogni voce: `docs/architecture/tag-verbi-simulazione-rollback.md` (25/11).

### 5.12 Presentazione: VFX, suoni, vibrazione · OK (superfici/vibrazione SCRITTO)
- `Plugins/GSTNPresentation`: evento (`Presentation.Event.*`) → Niagara/suono/scossa camera, tabella `UGSTN_PresentationTable`. Hitstop e colpi visivi: `NP/Presentation/GSTN_HitPresentation*.h`.
- **Posizione presentata (punto unico, 2026-11-24, SCRITTO):** tutto ciò che è estetico e deve seguire il personaggio disegnato legge `UMoverComponent::GetPresentedTransform()` / `GetPresentedVelocity()` (radice e velocità interpolate dallo smoothing di Mover, senza scosse di hitstop o offset cosmetici della mesh; BlueprintPure). Lo usano il marker di profondità (`GSTNDepthMarkerMover`), le forme di debug (`gstn.Combat.Debug.FollowMesh`) e la capsula greybox. Mai dalla simulazione.
- Passi per superficie e vibrazione pad (`GSTN_SurfaceFeedbackComponent.h`, `gstn.Audio.*`, `gstn.Haptics.*`): SCRITTO (`docs/notes/handovers/surface-feedback-1.md`). Rendering: `gstn.Render.Profile 0-4` (`Private/Rendering/GSTN_RenderProfile.cpp`).

### 5.13 Cosmetica e skin · OK la base, il resto è ricerca
- Skin: `Core/Kits/GSTN_SkinDefinition.h`, campo `Skin` del loadout, applicata da `GAS/Cosmetics/GSTN_SkinComponent.h`; varianti d'animazione per `Anim.Trait.*` (GSTNAnimation). La schermata del carico non ha la riga skin.
- Decisioni e piani (non codice): `docs/notes/skin-cosmetici-modulari.md` (branch `claude/skin-cosmetici-ricerca`), `skin-proporzioni-analisi.md` (branch `claude/project-thread-4pj7kf`).

### 5.14 Modalità, missioni, respawn · OK (SPENTI di default)
- `Plugins/GSTNRespawn` (vite condivise, checkpoint) e `Plugins/GSTNMission` (obiettivi): SPENTI finché un asset GameRules non li accende (M12 parte 1).
- M12 G0-G8 ora in main: `Plugins/GSTNExperience` (Experiences/Game Features, test `GSTNExperience` 7/0), `Plugins/GSTNModeVerbs` (verbi di modalità, 3/0), `Plugins/GameFeatures/GSTNMode_Levels` (6/0), G5 prompt e G7-G8 livello di test. Esiti in `docs/notes/handovers/m12-g*.md`; G5 e G7-G8 senza riga ESITO (**da confermare**).
- Decisione dell'owner del 25-11 "niente misto": vite condivise, respawn e missioni co-op andranno dentro il rollback (`docs/architecture/tag-verbi-simulazione-rollback.md` §10). Il codice di oggi non lo fa ancora.

### 5.15 cvar e menu GSTN Debug · OK
- 171 cvar `gstn.*` (conteggio `grep TAutoConsoleVariable`). Finestra: `Plugins/GSTNCombatDebug/.../Private/Window/SGSTN_DebugOptionsPanel.cpp`; mostra **da sola** ogni cvar che si chiama `gstn.<Gruppo>.Debug...`; le altre (es. `gstn.Loco.Base`) vanno scritte in console.
- Overlay hitbox, BakeCheck, scelta del kit (`gstn.Debug.Kit`): stesso plugin, mai in Shipping. La riga kit della finestra nasconde i kit dei soli test (cartelle `_Test`/`Test`; `gstn.Debug.ShowTestKits 1` li mostra).

### 5.16 Strumenti: build, test, catture, MCP
| Cosa | Dove | Doc |
|---|---|---|
| Aggiornare il progetto a main + build (owner, doppio clic) | `Aggiorna e compila.bat` → `Tools/CI/Aggiorna-Progetto.ps1` | testa dello script |
| Build/test/cattura a tempo | `Tools/CI/Run-Timed.ps1 -Preset Build/Test/Capture`, esito con `Check-TestLog.ps1` | testa dello script |
| Guardia git (file dell'owner, segreti) | `Tools/CI/Test-GitGuard.ps1`, `Install-GitHooks.ps1` | CLAUDE.md |
| Prova di rete (50 scenari) | `Tools/GASNPPTest/gasnpp_test.py`, `m3_suite_driver.py` | `docs/architecture/testing.md` |
| Creazione asset (kit, mappe, VFX) | `Tools/GAS/setup_*.py` (60 script, `setup_gas_assets.py` è il principale) | `Tools/README.md` |
| Domande sull'engine | `Tools/UE59/ask_unreal_assistant.py` | docstring |
| Indipendenza dei plugin | `Tools/Canary/make_canary.py`, `Tools/Modules/check_modules.py` | `Plugins/README.md` |
| MCP dell'editor | `unreal-mcp` (`.mcp.json`), toolset `Plugins/GSTNAnimToolsets` | skill `unreal-mcp` |
| MCP animazione, cattura deterministica | `Tools/AnimPipeline/gstn_anim_mcp/`, `Tools/AnimPipeline/capdet/` (in main dal 24-11) | `docs/notes/handovers/c5-sessione.md` |
| Indice dei thread e costi (GENERATI, mai a mano) | `Tools/CI/Build-Indice.py` → `handoffs/INDICE.md`, `handoffs/costi.csv` (nel repo) | testa dello script |

### 5.17 Test · OK fino al 21-11
- ~1600 test automatici (`Automation RunTests GSTN`), quasi tutti nelle cartelle `Private/Tests` dei plugin, sempre dietro `WITH_DEV_AUTOMATION_TESTS`. Come lanciarli: `docs/architecture/testing.md` (attenzione: cita ancora UE 5.8 e "94 test").

## 6. Non ancora in main
Rifatto il 2026-11-27. Dal 23-11 sono **entrati in main** (PR #38-#59): M12 G0-G8, presa al collo (`grab-link`) e spin/numeri
Ganondorf (`grab-ganon`), MCP animazione e cattura deterministica, riordino Content fase 2b, versioni approvate pubblicate in
`Content/AnimPipeline/Published/`, capsula greybox, lanci orizzontali, kit dei personaggi, pannello locomotion, documento tag/verbi/rollback.
Branch remoti: 201, di cui 64 uniti e 137 no; 36 non uniti hanno commit dal 23-11. Metodo: `git ls-remote` + SHA presente nella
storia di `origin/main`; `git diff --stat origin/main...<branch> -- docs`.
| Cosa | Branch (`origin/claude/...`) | Stato |
|---|---|---|
| Schivata dive+roll | `project-thread-g997re` (27-11) | codice, handover `dodge-dive-roll-1125.md` |
| Carica degli aerial | `project-thread-uyhp3p` (27-11) | codice, handover `aerial-charge-1.md` (PR #54 secondo la coda del fisso) |
| Mosse di riferimento Overlord (Ganon/Ike) e cubi di test | `project-thread-sznhpw` (27-11), `project-thread-k3aapg` (26-11) | codice + `docs/notes/overlord-ref-moves.md` |
| Rollback: misura F0 del mondo | `project-thread-n7diie` (26-11) | strumento, handover `rollback-f1-f0-1.md` |
| Valutazione skill "Sakurai" | `project-thread-jnj2tt` (26-11) | solo handover |
| Oscillazione della corsa (capsula) e run start di prova | `project-thread-u1p2ha` (25-11) | fermo al passo 7, `docs/notes/run-start-drive-capsula.md` |
| Zoom camera PSO2 solo nel pannello | `project-thread-dome4c` (25-11) | codice, handover `camera-zoom-pso2.md` |
| Nausea in PIE | `project-thread-0gtkfd` (24-11) | solo handover |
| Piano pulizia repo C1-C5, brief skill unica, consulti review | `project-thread-yt6kwu`, `project-thread-mdss5a`, `project-thread-39uwfb`, `project-thread-aeh8di` | documenti, i file risultano già in main per altra via |
| Dropkick K7/K8 (review e piano r3, fermo per decisione dell'owner) | `project-thread-6blrlz`, `project-thread-qsgab4`, `project-thread-2u7rwj`, `project-thread-690lv0` | documenti e immagini |
| Copia di sicurezza materiali greybox di `_ue59` | `backup-ue59-2026-11-26` | solo asset, non unire senza guardare |
| Mappa input e piano P0-P5 | `project-thread-uvefj5` | il documento è già in main (`docs/notes/input-azioni-mappa.md`); il branch ha solo l'handover |
| Ricerca 1v1 senza server dedicato | `ricerca-server-dedicato` | documento già in main (`docs/notes/server-dedicato-ricerca.md`) |
| Ricerche skin, mesh, inventario animazioni, mappa cartelle | `skin-cosmetici-ricerca`, `project-thread-4pj7kf`, `project-thread-nfz4at`, `anim-inventario-2026-11-23`, `project-thread-f4ovn9` | documenti già in main tranne `docs/notes/mappa-cartelle.md` (solo `project-thread-f4ovn9`) |
| Riordino Content fase 1 | `content-riorganizzazione` | superato: la fase 2b è in main (PR #43) |
| Strumenti di moto per Sequencer, minion G9b, ferite parti | `project-thread-t93zat`, `desktop-g9b`, `parts-rest-laptop` | codice vecchio, da verificare se superato |
| Turni DeepSeek e harness | `deepseek-turn4`, `harness-deepseek-fix` | storia (Antigravity/DeepSeek non coordinano più) |
| Copie degli artefatti | `artifacts` | 3787 file di differenza: è una copia larga, non unire così |
`feature/gas-npp-5.8` esiste ancora sul remoto (e79e401a) e **il tag `archivio/feature-gas-npp-5.8` citato da `CLAUDE.md` non c'è** (`git ls-remote --tags`, 27-11): non partire da lì.

## 7. Dove sono i documenti
- `CLAUDE.md`: regole e tabella "Where things are". `Plugins/README.md`: indice dei plugin (in parte vecchio, vedi audit).
- `docs/architecture/`: progetto di ogni sistema. `docs/notes/`: note di lavoro, piani, `handovers/` (una per thread), `memoria-progetto.md` (storia), `regole-progetto.md` (regole in vigore).
- Guide pubblicate per l'owner: sezione [Artefatti](#artefatti) in cima; copie in `Artifacts/` (Designer, Combattimento, Progetto), copie nuove solo sul branch `claude/artifacts`.
- Rapporto settimanale sulla documentazione (orfani, contraddizioni, archivio): `docs/notes/archivio-proposta.md`, rifatto ogni venerdì dal bibliotecario.
- Documenti non citati da nessun indice: 130 (elenco in `archivio-proposta.md` §A); `docs/architecture/` andrebbe elencato qui intero.

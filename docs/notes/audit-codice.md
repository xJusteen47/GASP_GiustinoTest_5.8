# Audit del codice: doppioni, buchi di documentazione, cose da snellire

Stato: `main` a5b20950, 2026-11-23. Sola lettura: niente è stato modificato. Ambito: `Source/GASP_Giustino2` e
`Plugins/GSTN*` (esclusi `GASNPP`, `EngineOverrides`, `UE_MCP_Bridge`). Mappa dei sistemi: [../INDICE-PROGETTO.md](../INDICE-PROGETTO.md).

## In breve (10 righe)
1. Il codice è pulito: nessun tag duplicato, nessuna cvar morta, nessun test fuori dalla guardia, quasi zero TODO/`#if 0`.
2. Il problema vero è **trovare** le cose: `Plugins/README.md` dice che 6 plugin "non esistono ancora" (esistono) e non elenca altri 5.
3. La cartella `GASNPPTest` ("test") contiene classi del **gioco vero** (kit Overlord, camera di playtest, look satinato).
4. Due salti (`GSTN_GA_Jump` vecchio e `GSTN_GA_PlatformJump` vero): il vecchio è marcato "da sostituire" e serve solo al personaggio di prova.
5. Animazione/locomotion sta in 4 case e la camera in 3 strati: giusto, ma nessun documento dice "cosa va dove".
6. 30 campi regolabili sono dichiarati e **mai letti** (stesso difetto che aveva AimPolicy), es. `AimOriginSocket` del kit.
7. 948 dichiarazioni pubbliche su 5585 (17%) senza commento; il peggiore è GSTNCamera (35%).
8. 46 percorsi citati nei documenti non esistono più; `testing.md` parla ancora di UE 5.8 e "94 test".
9. Prima di tutto: **main non è stato compilato** dopo il merge del 22-11 (4 lavori cloud mai compilati).
10. Proposta: 5 thread piccoli, i primi 3 in cloud a basso costo (~6 $ in tutto), gli altri sul fisso con l'editor.

Decisioni per l'owner (risposta tipo "1A 2A 3A"):
- **1. Classi di gioco nella cartella "test"** · A (consigliata): spostarle durante la riorganizzazione dei contenuti sul fisso, ~+1 $ su quel thread · B: lasciarle e scriverlo solo nell'indice. Fermo intanto: niente.
- **2. Vecchio salto `GSTN_GA_Jump`** · A (consigliata): segnarlo "solo personaggio di prova" ora, cancellarlo dopo il controllo in editor · B: lasciarlo com'è.
- **3. 30 campi mai letti** · A (consigliata): uno per uno collegarlo o scrivere "riservato, non fa niente" nel tooltip, ~2 $ cloud · B: cancellarli (rischio: perdere valori salvati negli asset).

Legenda: costo in $ stimato su thread simili in `handoffs/INDICE.md`; rischio B/M/A = basso/medio/alto. "Editor" = si può
verificare solo con l'editor aperto (uso da Blueprint o asset). Ogni voce cita il comando che la prova (regola 11); gli
script citati sono stati eseguiti nel thread e non sono nel repo, i comandi `grep` sì si possono rilanciare.

## A. Codice DOPPIO (stessa responsabilità in due posti)

**A1. La cartella "test" contiene il gioco vero** · impatto ALTO
- Dove: `Source/GASP_Giustino2/{Public,Private}/GASNPPTest/` (13 header + 13 cpp, circa 6250 righe, modulo Runtime, senza guardia `WITH_DEV_AUTOMATION_TESTS`).
- Perché: `GSTN_TestKitArchetypes.h` (`UGSTN_GA_TestKitStrike`, `UGSTN_TestKitContentLibrary`) costruisce i kit Overlord veri (usato da `Tools/GAS/setup_overlord_{jab,dair,dropkick}_kit.py`); `GSTN_PlaytestCamera.h` e `GSTN_SatinLook.h` sono la camera e il look del playtest; `Content/GASNPPTest` è la cartella dove il gioco cerca kit, armi, skin e timeline (`Config/DefaultGame.ini` righe 10-16). Chi legge "Test" pensa che si possa ignorare o cancellare.
- Prova: `grep -rln TestKitStrike Tools Config`; `grep -n GASNPPTest Config/DefaultGame.ini`.
- Azione: spostare camera e look in `Camera/` e `Rendering/` (stesso modulo, il nome `/Script/...` non cambia); rinominare le classi dei kit con CoreRedirects insieme alla riorganizzazione dei contenuti. I veri strumenti di prova (`GSTN_TestProbeAbilities`, `GSTN_TestEffects`, `GSTN_GASNPPTestComponent`...) restano lì.
- Costo ~1 $ in più sul thread di riorganizzazione (fisso, editor). Rischio M (percorsi di classe negli asset). Editor: sì.

**A2. Due abilità di salto** · impatto MEDIO
- Dove: `Source/GASP_Giustino2/Public/GAS/GSTN_GA_Jump.h` (vecchio) e `Plugins/GSTNCombatNP/.../Abilities/GSTN_GA_PlatformJump.h` (quello del gioco). Anche il generatore di spinta è doppio (`UGSTN_JumpImpulseMoveCreator` contro `UGSTN_PlatformJumpMoveCreator`).
- Perché: il commento a riga 43 dice "TO BE REPLACED"; lo usano solo `Config/DefaultGame.ini:42` e `Tools/GAS/setup_gas_assets.py` per il personaggio di prova. Un thread che cerca "jump" può modificare quello sbagliato.
- Prova: `grep -rln "GSTN_GA_Jump\b" Source Plugins/GSTN* Config Tools`.
- Azione: decisione 2. Costo 0,5 $ (commento) + controllo in editor dei Blueprint. Rischio B.

**A3. Animazione e locomotion in 4 case** · impatto MEDIO
- Dove: `Plugins/GSTNAnimation` (genitore ABP, layer, skin), `Plugins/GSTNCombatNP/.../Presentation/` (layer di skid e partenza corsa, lean), `Source/.../Animation/` (interruttore ABP stock, postura per kit), `Source/.../WallMovement/Animation/` (layer del muro).
- Perché: ognuna ha un motivo (dipendenze), ma un thread che aggiunge un layer non sa dove metterlo.
- Prova: `find Source Plugins/GSTN* -ipath '*animation*' -name '*.h'`; `ls Plugins/GSTNCombatNP/Source/GSTNCombatNP/Public/Presentation`.
- Azione: tabella "cosa va dove" in `docs/architecture/anim-layers.md` (fatta in breve nell'indice 5.3). Spostare i layer di corsa in GSTNAnimation solo se un thread ci lavora già. Costo 0,5 $ doc. Rischio B.

**A4. Camera in 3 strati** · impatto MEDIO
- Dove: `Source/.../Camera/GSTN_CameraAnchor.h` (àncora sulla camera di GASP), `Plugins/GSTNCamera` (camera nuova a modi), `Source/.../GASNPPTest/GSTN_PlaytestCamera.h` (ritocchi, cerca `UGSTN_CameraComponent`).
- Perché: tre posti che toccano lo stesso punto di vista; `camera-anchor.md` e `camera.md` non dicono chi comanda il pivot.
- Azione: una frase in entrambi i documenti su chi possiede il pivot oggi e cosa cambia con M12 G5. Costo 0,3 $. Rischio B.

**A5. Piccola matematica ripetuta** · impatto BASSO
- Smorzamento/molla in 5 versioni (`GSTN_WallMovementUtils.h:112`, `GSTN_CameraAnchor.h:337`, `GSTN_CameraMath.h:167`, `GSTN_GroundContactSolver.h:33`, `GSTN_DepthMarkerToy.h:92`); proiezione su piano in 4 (`GSTN_GravityFrame`, `GSTN_HitMathUtils`, `GSTN_WallMovementUtils`, `GSTN_DepthMarkerMath`).
- Azione: NON unificare in blocco (codice di simulazione, regole di determinismo). Se un thread ne tocca una, usa quella di GSTNCore. Costo 0. Rischio M se toccate.

Verificato e pulito: nessun tag con lo stesso nome in due moduli (531 tag, `... | sort | uniq -d` vuoto); un solo sistema di mira (`GSTN_StartupSteer.h`); una sola storia delle pose condivisa (`GSTNCore/Net/GSTN_PoseHistory.h`).

## B. Codice POCO DOCUMENTATO

**B1. Gli indici dei plugin sono vecchi** · impatto ALTO (è il motivo per cui si rifanno le cose)
- `Plugins/README.md` riga ~151 "On the parallel branch claude/project-thread-v1plhd (not merged yet)": i 6 plugin elencati (Surface, Interact, Parts, CombatEditor, CombatDebug, Platform, + PlatformParts) sono tutti nel repo e accesi.
- Mancano dalle tabelle: GSTNCamera, GSTNDepthMarker, GSTNDepthMarkerMover, GSTNHeld, GSTNPiggyback. Righe superate: GSTNSpinDrill ("solo tag": ha abilità e 32 test), GSTNPartsMinion ("modulo vuoto": P16 fatta), GSTNBackpack (manca il modulo NP), README di GSTNDefence e GSTNLink ("NP non costruito": esiste).
- Senza README: `Plugins/GSTNBackpackPickups`, `Plugins/GSTNDepthMarkerMover`. `CLAUDE.md` "Where things are" non ha righe per Camera, Minion, Held, Piggyback, Projectile, SpinDrill, Respawn, Mission, Backpack, LoadoutMenu, DepthMarker, Interact, Platform, KitMods.
- Prova: `for d in Plugins/GSTN*; do [ -f $d/README.md ] || echo $d; done`; `grep -c GSTNHeld Plugins/README.md` (0).
- Azione: thread T1. Costo ~1,5 $ cloud. Rischio nessuno.

**B2. Percorsi morti nei documenti** · impatto MEDIO
- 46 percorsi citati in `docs/architecture/*.md` e nei README non esistono più (circa 20 sono falsi allarmi, es. file in `Saved/`). Esempi: `input.md` righe 80-84 (`GSTN_CombatCoreTypes.h`, `GSTN_CombatInputTags.h`), `modularity-and-tags.md:716,793` (`Tools/Lint/*.py` mai esistiti), `combat-m8-minions.md:180-181`, `combat-m10-held-objects.md:96,106`.
- `docs/architecture/testing.md` righe 7 e 10: "94 tests today" e percorso `UE_5.8` (oggi ~1600 test, UE 5.9). `Tools/README.md` copre solo GAS e GASNPPTest.
- Azione: thread T1. Rischio nessuno.

**B3. Dichiarazioni pubbliche senza commento** · impatto MEDIO (i tooltip sono ciò che legge un designer)
- 948 su 5585 (17%). Per plugin: GSTNCamera 35%, GSTNMission 33%, GSTNKitMods 32%, GSTNAnimation 28%, GSTNDepthMarker 20%, GSTNCombatNP 19%, GSTNCombat 18%; migliore GSTNSurface 1%.
- File peggiori (membri regolabili o esposti a Blueprint senza commento): `GSTNCamera/.../GSTN_CameraTypes.h` (53), `Core/Hits/GSTN_HitResolution.h` (29), `NP/Presentation/GSTN_RunPresentationComponent.h` (24), `Core/Timeline/GSTN_TimelineTypes.h` (24), `GSTN_DepthMarkerTypes.h` (20), `GSTN_MinionDefinition.h` (19), `NP/Presentation/GSTN_LocomotionLean.h` (18), `Core/Loadout/GSTN_LoadoutRules.h` (18).
- Metodo: commento nella riga non vuota precedente; conta in eccesso le proprietà raggruppate sotto un solo commento.
- Azione: thread T3 sui primi 5 file. Costo ~2-3 $ cloud + build. Rischio B (solo commenti).

**B4. 29 cvar non citate in nessun documento** · impatto BASSO
- Es. `gstn.Camera.{Assist,AutoFollow,DeadZoneDepthCm,DeadZoneWidthCm,Distance}`, `gstn.Minion.Net.*`, `gstn.Projectile.Ghost*Frames`, `gstn.Respawn.{Checkpoints,DelaySeconds,RegenSeconds}`.
- Azione: una tabella cvar in ogni README di plugin (thread T2). Costo ~0,5 $. Rischio nessuno.

## C. Codice DA SNELLIRE

**C1. Campi regolabili mai letti** · impatto ALTO (un designer li cambia e non succede niente)
- 30 campi `UPROPERTY` il cui nome compare una sola volta nel codice. I più importanti:
  `GAS/Kits/GSTN_PrimaryKitDefinition.h:108` `AimOriginSocket` (stesso caso del vecchio AimPolicy);
  `Plugins/GSTNPlatform/.../GSTN_PlatformTypes.h:68-83` (5 flag: `bCountsAsOccupant`, `MaxSpeedWhileOccupied`, `BraceTimeline`, `bApplyToWallSlide`, `bApplyToStand`);
  `Plugins/GSTNPiggyback/.../GSTN_MountTypes.h` (`RemountCooldownFrames`, `AnchorYawLimitDeg`, `PendingTimeoutFrames`, `bCarrierShieldCoversRider`);
  `Plugins/GSTNLink/.../GSTN_GrabRules.h` (`ResistanceAttribute`, `TapMaxFrames`, `InteractPriority`), `GSTN_LinkOutcomeTrack.h` (`ExitPlacement`): da guardare per la presa al collo;
  `GSTN_DefenceRules.h` `bReflectProjectiles`, `GSTN_WeaponDefinition.h` `VisualActor`, `GSTN_CombatStep.h` `bActionable`, `GSTN_BackpackDefinition.h` `bDropOnDeath`, `GSTN_SpinDrillTypes.h` `LatchDepthCm`.
- Prova: `grep -rn AimOriginSocket Source Plugins/GSTN* Config Tools` (1 sola riga, la dichiarazione).
- Azione: decisione 3; thread T2. Un Blueprint potrebbe leggerli: controllo in editor prima di cancellare. Rischio M se cancellati.

**C2. Codice di prova nel gioco spedito** · impatto MEDIO
- `GASNPPTest` (vedi A1) e `Plugins/GSTNMinion/.../Private/Debug/GSTN_MinionBench.cpp` (1530 righe nel modulo Runtime, esiste già `GSTNMinionTests`): finiscono in una build Shipping.
- Azione: con A1; il bench nel modulo di test quando si lavora su M8. Verifica con una build Shipping. Costo ~1 $. Rischio M.

**C3. File molto grandi** · impatto BASSO
- `GSTN_MinionCombatSubsystem.cpp` 2919 righe, `GSTN_CombatSimComponent.cpp` 2685 (già diviso in parte), `GSTN_GASNPPTestComponent.cpp` 1723, `GSTN_MinionBench.cpp` 1530 (`wc -l`).
- Azione: dividere solo quando un thread ci lavora già, con harness di rete. Rischio A (simulazione).

**C4. Codice vecchio segnalato** · impatto BASSO
- `Source/.../WallMovement/Modes/GSTN_SimpleWallMovementMode.h:41` impostazioni "Legacy" tenute solo per migrare asset vecchi: si tolgono dopo aver risalvato gli asset (editor).
- `Plugins/GSTNInputNP/.../Simulation/GSTN_InputSimulator.h:134` TODO sull'attivazione delle abilità che passa dal controllo di ruolo dell'engine (tocca il fork: lasciare).
- Strumenti: in `Tools/GAS` coppie e versioni (`setup_overlord_dair_kit.py`/`_v10`, `setup_playtest_fixes_1117`/`_1117b`, 6 `setup_presentation_vfx*`); in `Tools/AnimPipeline` script per singola versione (`jab_v1..v6_build`, `ue_save_v5..v11`) e backup JSON in `cards/`; `Tools/UE59/ue59_ask_assistant.py` superato da `ask_unreal_assistant.py`; `IN-USO.txt` nella radice (quello vero è `_ue59/IN-USO.txt`). Azione: cartella `_archivio` per strumenti superati (T2). Rischio B.

## Ordine dei lavori proposto (un thread per checkpoint)
| # | Thread | Macchina, modello | Costo | Contenuto |
|---|---|---|---|---|
| T0 | Compilare e testare main | fisso o portatile, Haiku | già in coda | prerequisito di tutto: build + `RunTests GSTN` + harness 50/50 dopo il merge del 22-11 |
| T1 | Indici e documenti allineati | cloud, Haiku medium | ~1,5 $ | B1, B2: `Plugins/README.md`, 2 README mancanti, righe di `CLAUDE.md`, `testing.md`, percorsi morti |
| T2 | Campi mai letti e cvar | cloud, Sonnet medium | ~2 $ | C1 (decisione 3), B4, A2 (commento), archivio strumenti superati |
| T3 | Tooltip dei 5 file peggiori | cloud, Sonnet medium | ~2-3 $ | B3; compila al giro successivo di integrazione |
| T4 | Cartella test e verifiche in editor | fisso, Opus medium, con la riorganizzazione contenuti | ~+1 $ | A1, C2, uso di `GA_Jump` e dei campi C1 nei Blueprint, impostazioni Legacy dei muri |
| T5 | Case dell'animazione, file grandi | quando un thread ci lavora già | 0 extra | A3, A4, A5, C3 |

## Cosa non ho potuto verificare (serve l'editor)
- Se un Blueprint o un asset usa `GSTN_GA_Jump`, gli effetti `GE_Test*`, o uno dei 30 campi "mai letti" (il C++ non lo vede).
- Se i kit in `Content/GASNPPTest` dipendono da classi della cartella test oltre a quelle citate.
- Che una build Shipping escluda davvero il codice di debug.
- Che main compili dopo il merge del 22-11.

# Gameplay tag, verbi, simulazione e rollback: la mappa unica

Stato al **25/11/2026**, `main` = `152d25e6` (PR #51 unita). Le novità non ancora in `main` sono segnate riga per riga con il loro
branch o PR (sezione 7). Scritto per l'owner che vuole ricontrollare tutto: ogni voce ha il file sorgente da aprire.

**Da dove viene.** Un documento unico non c'era. C'erano quattro pezzi, che restano validi e a cui questo rimanda:

| Documento | Cosa copre | Lingua, data |
|---|---|---|
| [verbs-vs-commands-guide.md](verbs-vs-commands-guide.md) | Comando vs abilità vs verbo di modalità, con colonna rollback | italiano, 22/11 (M12 G2) |
| [../notes/networking-spiegato.md](../notes/networking-spiegato.md) | Predetto / interpolato / presentazione predetta, per chi progetta | italiano, novembre |
| [modularity-and-tags.md](modularity-and-tags.md) §4 e "Goal 9" | Tassonomia dei tag, chi possiede quale radice, "ogni verbo è un'abilità" | inglese, proposta del 21/10 |
| [gas-npp.md](gas-npp.md) "Hard rules" | Le regole del codice di simulazione | inglese |

Questo file li riunisce e li allinea al codice di oggi. Se un dettaglio qui contraddice il codice, vale il codice: segnalalo e si corregge.

---

## 1. Le cinque zone

Ogni cosa del gioco sta in una di queste cinque zone. È la domanda da farsi prima di tutto: "in che zona sta?".

| Zona | Cosa vuol dire | Rollback? | Esempi |
|---|---|---|---|
| **A. Stato simulato** | Dati salvati frame per frame nello stato di Network Prediction (NP). Ogni macchina lo calcola uguale; quando il server corregge, si torna indietro e si ricalcola. | **Sì**, ripristinato | posizione e velocità (Mover), azione in corso e timeline, hitstun, salto, tag ed effetti GAS, frammenti (scudo, link, parti) |
| **B. Derivato dalla simulazione** | Non è salvato: si ricalcola ogni frame da dati della zona A. Dopo un rollback esce uguale da solo. | **Sì, gratis** (non costa byte) | i tag `Combat.State.*` proiettati, l'air crouch, il crouch, la vista di lag compensation, la "inbox" dei colpi |
| **C. Fuori simulazione, solo server** | Decisioni prese una volta sola dall'host, fuori dal tick fisso, su frame già confermati. I client le vedono in ritardo (replica). | **No, mai** (non si può contare due volte) | missioni, respawn e vite, effetto di un verbo di modalità, raccolta oggetti, cambio loadout, spawn dei proiettili, eventi autoritativi |
| **D. Presentazione locale** | Solo aspetto, su ogni macchina per conto suo. Legge lo stato "presentato" (smussato), mai il contrario. | **No**: si ricostruisce dallo stato; i doppioni da rollback si filtrano | animazione, capsula greybox, VFX, suoni, camera e zoom, marker di profondità, cosmetici, prompt |
| **E. Dati statici e configurazione** | Asset e impostazioni uguali su ogni macchina: kit, timeline, tabella comandi compilata, impostazioni di progetto. Non cambiano durante la partita (o cambiano con una revisione). | **Non serve** | `KIT_*`, `WPN_*`, timeline delle mosse, `UGSTN_CombatSettings`, CVar lette "alla compilazione del kit" |

Due regole d'oro che tengono in piedi tutto (dettagli in [gas-npp.md](gas-npp.md), CLAUDE.md regole 1-5):

1. **A e B non leggono mai C, D.** La simulazione non guarda animazione, camera, mesh o stato del server fuori banda. Nessun `IsServer()`
   o "sto risimulando?" nel codice simulato.
2. **C e D leggono A e B, mai il contrario.** La presentazione segue lo stato presentato (`UMoverComponent::GetPresentedTransform()`);
   il server pubblica gli eventi autoritativi una volta sola.

### 1.1 Ordine del tick fisso (62,5 Hz)

| Ordine | Modello NP | File | Note |
|---|---|---|---|
| 1 | Input standalone `GSTN_Input` (priorità `PreKinematicMovers - 4`) | `Plugins/GSTNInputNP/Source/GSTNInputNP/Private/Simulation/GSTN_InputSimComponent.cpp` | Solo pawn senza modello di combattimento |
| 2 | Combattimento `GSTN_Combat` (`- 3`) | `Plugins/GSTNCombatNP/Source/GSTNCombatNP/Private/Simulation/GSTN_CombatSimComponent.cpp` | Contiene anche l'input (stato `Input` dentro il core) |
| 3 | Ability system del fork (`- 2`) | `Plugins/GASNPP/AbilitySystemSimulation/` | Abilità, effetti, tag, attributi |
| 4 | Mover | `Plugins/EngineOverrides/Mover/` | Movimento, modalità, salto applicato |

Conseguenza (CLAUDE.md regola 5): tag ed effetti dati al frame N li vede Mover nel frame N; il cambio di modalità di Mover al frame N le
abilità lo vedono al frame N+1 (per questo esistono le "grace frames", es. `UGSTN_GA_WallSlide::EntryGraceFrames`).

---

## 2. Zona A: cosa c'è nello stato rollbackato

### 2.1 Modello di combattimento (`FGSTN_CombatSyncState`)

File: `Plugins/GSTNCombatNP/Source/GSTNCombatNP/Public/Simulation/GSTN_CombatSimTypes.h`. È fatto di un **core fisso** più una lista di
**frammenti** che le feature aggiungono solo se presenti sull'entità. Un combattente fermo costa 1 bit per parte.

| Campo del core | Cosa tiene | Rollback |
|---|---|---|
| `Input` (`FGSTN_InputSimState`, `GSTNInputNP/Public/Simulation/GSTN_InputSimTypes.h`) | Storia dei tasti, classificatore dei comandi, buffer, richiesta di abilità con il suo frame | Sì |
| `HitstopStartFrame`, `HitstopEndFrame`, `PausedFramesTotal` | Finestra di hitstop e orologio del combattimento | Sì |
| `Jump` (`FGSTN_JumpState`) | Fase del salto, salti in aria usati, coyote, tasto tenuto | Sì |
| `DodgeLanding` (`FGSTN_DodgeLandingState`) | Lag di atterraggio della schivata e blocco della ri-schivata | Sì |
| `ActionState`, `ActiveTimeline`, `ActiveSource`, `TimelineStartFrame`, `TimelinePausedFrames` | Cosa sta facendo e quale timeline sta suonando, a che frame | Sì |
| `RecoilStartFrame`, `HardContact`, `StrikeTrack` | Rimbalzo dell'arma su superfici dure (ricochet) | Sì |
| `bAirborneOk`, `bLedgeStop`, `bTumbleActionable` | Mossa da terra partita in aria, stop sul bordo, tumble azionabile | Sì |
| `ActiveWeaponIndex` | Arma attiva (primaria o aggiuntive) | Sì |
| `LoadoutRevision` / `TableRevision` | Revisione del loadout chiesta dal server / adottata dalla simulazione | Sì (ma `LoadoutRevision` lo **scrive solo il server**, fuori dal tick: vedi zona C) |
| `Requests` | Richieste del frame (comandi forzati, adozioni) | Sì |
| `Hit` (`FGSTN_HitSyncState`) | Lato attaccante: bersagli colpiti; lato vittima: finestre di hitstun, lancio in attesa | Sì |
| `ChargeFrames` | Frame di carica di un aerial | Sì. **Solo PR #54, non in `main`** |

| Frammento | Plugin, file | Cosa tiene |
|---|---|---|
| `FGSTN_DefenceFragment` | `Plugins/GSTNDefence/Source/GSTNDefenceNP/Public/Sim/GSTN_DefenceFragment.h` | Scudo (metro, rottura, stun), parry, tech, knockdown |
| `FGSTN_LinkFragment` | `Plugins/GSTNLink/Source/GSTNLinkNP/Public/Sim/GSTN_LinkFragment.h` | Prese, lanci, latch, ruolo e partner, timer di fuga, modalità parcheggiata |
| `FGSTN_PartsFragment` | `Plugins/GSTNParts/Source/GSTNPartsNP/Public/State/GSTN_PartsFragment.h` | Parti rotte, danno per parte, ferite (`WoundedMask`) |
| `FGSTN_DrillFragment` | `Plugins/GSTNSpinDrill/Source/GSTNSpinDrillNP/Public/Sim/GSTN_DrillFragment.h` | Spin e trapano (latch, scavo, lancio) |
| `FGSTN_BackpackFragment` | `Plugins/GSTNBackpack/Source/GSTNBackpackCore/Public/GSTN_BackpackSim.h` | Stato dello zaino (cupola, jetpack) |
| `FGSTN_HitBounceFragment` | `Plugins/GSTNCombatNP/Source/GSTNCombatNP/Public/Simulation/GSTN_HitBounceSim.h` | Rimbalzo dopo un colpo (armato, lancio messo in coda a Mover, blocco, quanti rimbalzi di fila) |

Formato e regole dei frammenti: `Plugins/GSTNCombat/Source/GSTNCombatCore/Public/Sync/GSTN_SyncFragment.h`.

### 2.2 Ability system del fork (GAS dentro NP)

| Cosa | Dove | Rollback |
|---|---|---|
| Attivazione e fine delle abilità, tag concessi (`ActivationOwnedTags`), effetti e loro tag, attributi (salute, stat del kit, metri) | `UNpAbilitySystemComponent`, `Plugins/GASNPP/AbilitySystemSimulation/` | Sì, è il punto del fork ([gas-npp.md](gas-npp.md)) |
| Variabili delle abilità che devono sopravvivere al rollback: `FSyncInteger`, `FSyncBool`, `FSyncByte`, `FSyncGameplayTag` | nelle abilità sotto (sezione 2.4) | Sì. Un membro normale **non** viene ripristinato |
| Stato montaggio (pausa) | patch del fork, `Plugins/GASNPP/PATCHES.md` | Conta nel confronto solo mentre un montage suona |

### 2.3 Mover

| Cosa | File | Rollback |
|---|---|---|
| Posizione, rotazione, velocità, modalità di movimento, base | Mover (`Plugins/EngineOverrides/Mover/`) | Sì |
| Stadio di corsa `FGSTN_RunStageSyncState` | `Plugins/GSTNCombatNP/Source/GSTNCombatNP/Public/Movement/GSTN_RunStageSyncState.h` | Sì |
| Memoria del muro `FGSTN_WallMovementSyncState` | `Source/GASP_Giustino2/Public/WallMovement/GSTN_WallMovementTypes.h` | Sì |
| Input aggiunti a Mover: `FGSTN_AirTurnInputs`, `FGSTN_WallInputs` | `GSTNCombatNP/Public/Movement/GSTN_AirTurn.h`, `GSTN_WallMovementTypes.h` | Sono input del frame (viaggiano con l'input, si ripetono uguali in risimulazione) |
| Specchio dello stato di Mover per le abilità `UGSTN_MoverSimStateTracker` | `Plugins/GSTNInputNP/Source/GSTNInputNP/Public/Mover/GSTN_MoverSimStateTracker.h` | Segue il rollback di Mover; è ciò che le abilità devono leggere (`GetMoverSimState`), mai il componente Mover |

### 2.4 Abilità con variabili sincronizzate

| Abilità | File | Variabili rollbackate (esempi) |
|---|---|---|
| `UGSTN_GA_Base` (base di tutte) | `Plugins/GSTNCombatNP/Source/GSTNCombatNP/Public/Abilities/GSTN_GA_Base.h` | `CommandIndex`, `PressFrame`, `TriggerButton`, `ActivationSource`, `WaitStartClock`, `WaitFrames` |
| `UGSTN_GA_BaseCombat` (base delle mosse con timeline) | `.../Abilities/GSTN_GA_BaseCombat.h` | `StartFrame`, `StartClock`, `PlayTaskIndex`, `bStarted`, `bMontageReleased`, `ReportedHitGroups`, `RecoilStartClock`, `TagWindow*`, `bAwaitingAdoption` |
| `UGSTN_GA_Dash`, `UGSTN_GA_GroundDodge`, `UGSTN_GA_AirDodge` | `GSTNCombatNP/Public/Abilities/GSTN_GA_Dash.h` | `bLeftGround`, `LandedFrame`, `LandedLagFrames` |
| `UGSTN_GA_PlatformJump`, `UGSTN_GA_PlatformFastFall` | `GSTNCombatNP/Public/Abilities/GSTN_GA_PlatformJump.h` | `SquatPending` |
| Abilità di difesa (rialzarsi, rotolata) | `Plugins/GSTNDefence/Source/GSTNDefenceNP/Public/Abilities/GSTN_DefenceAbilities.h` | `LastTravelFrame` (scudo e parry stanno nel `FGSTN_DefenceFragment`) |
| `UGSTN_GA_ModeVerb` | `Plugins/GSTNModeVerbs/Source/GSTNModeVerbsNP/Public/GSTN_GA_ModeVerb.h` | `HoldStartClock`, `VerbState` |
| `UGSTN_GA_FireProjectile` | `Plugins/GSTNProjectile/Source/GSTNProjectileNP/Public/GSTN_GA_FireProjectile.h` | `TargetEntityId` |
| Abilità degli oggetti tenuti (lancio) | `Plugins/GSTNHeld/Source/GSTNHeldRuntime/Public/GSTN_HeldAbilities.h` | `PressClock`, `ReleaseAim`, `WindupClock`, `WindupFrames` |
| `UGSTN_GA_WallSlide` (progetto) | `Source/GASP_Giustino2/Public/GAS/GSTN_GA_WallSlide.h` | `FramesSinceActivation` |

---

## 3. Zona B: cosa si ricalcola da solo

| Cosa | File | Perché è sicuro al rollback |
|---|---|---|
| Tag `Combat.State.*` (Busy, InTimeline, Hitstun, Hitstop, Falling, CancelWindow, Crouch, AirCrouch, Tumble, Dodge.EndLag, Shielding, Spinning...) | proiettati da `FGSTN_CombatVictim::ProjectStateTags`, `Plugins/GSTNCombatNP/Source/GSTNCombatNP/Private/Simulation/GSTN_CombatTick_Victim.cpp` | Il tick di combattimento li riscrive ogni frame dallo stato A. **Mai concessi** da un'abilità o un effetto |
| Air crouch | `Plugins/GSTNCombat/Source/GSTNCombatCore/Public/Rules/GSTN_AirCrouch.h` | In aria + tasto tenuto nel frame: entrambi ripristinati dal rollback. Zero byte di stato. Oggi cambia solo il tag (quindi l'animazione) |
| Crouch a terra | `GSTNCombatCore/Public/Rules/GSTN_GroundCrouch.h` | Come sopra; la capsula si accorcia tramite il modificatore di capsula |
| Colpi in viaggio (`FGSTN_HitInboxEntry`) | `GSTNCombatNP/Public/Simulation/GSTN_HitInbox.h` | Scritto dall'attaccante al frame F, letto dalla vittima a F+1. Non è nello stato: la risimulazione dell'attaccante lo riscrive |
| Vista di lag compensation | `GSTNCombatNP/Public/Simulation/GSTN_LagCompensation.h` | Copia temporanea per l'attaccante, solo sul server, mai scritta nello stato condiviso |
| Piattaforme mobili | `Plugins/GSTNPlatform/Source/GSTNPlatformCore/Public/GSTN_PlatformPoseContext.h` | La posa è funzione del frame di simulazione: stesso frame, stessa posa |
| Superfici (scivola, arrampica, lancia) | `Plugins/GSTNSurface/` | Leggono configurazione statica o stato replicato |

---

## 4. I gameplay tag

### 4.1 Come nasce un tag e in che zona vive

| Tipo di tag | Chi lo mette | Zona | Rollback |
|---|---|---|---|
| **Comando** `Command.*` | Il classificatore dell'input, dentro la simulazione | A (stato input) | Sì |
| **Tasto** `Input.Button.*`, contesti `Input.Context.*`, blocchi `Input.Lock.*` | Dati di input e regole del buffer | A / E | Sì (il blocco è un tag sull'ASC) |
| **Stato proiettato** `Combat.State.*` | Il tick di combattimento ogni frame | B | Sì, gratis |
| **Tag concesso da abilità/effetto** `Ability.*`, `State.Movement.*`, `Combat.Lock.*`, `Combat.Immunity.*` | GAS del fork | A | Sì |
| **Identità/dati** `Kit.*`, `Weapon.*`, `Super.*`, `Loadout.*`, `Skin.*`, `Combat.Link.Type/Role.*`, `Combat.HitType.*`, `Combat.Relationship.*`, `Combat.SetByCaller.*`, `Surface.*` | Asset e tabelle | E | Non serve |
| **Evento autoritativo** `Combat.Event.*` (quelli pubblicati sullo stream), `Message.*` | Il server, una volta sola, fuori dal tick | C | **No** |
| **Proiettato fuori sim** `Interact.Target.*` (bersaglio del tasto interagisci) | Sonda locale del giocatore | D | No; il server rivalida quando premi |
| **Presentazione** `Presentation.*`, `Anim.*`, `Camera.*`, `Cosmetic.*`, `GameplayCue.*`, `Foley.*` | Lato locale | D | No (dedupe per evento e frame) |

> Attenzione: `Combat.Event.*` è usato in due modi. Alcuni sono **eventi di gameplay dentro la simulazione** (es. `Combat.Event.TimelineStarted`
> inviato all'abilità nel tick), altri sono **pubblicati sullo stream autoritativo** (`Combat.Event.KO`, `LinkFormed`, `Part.Broken`...). La
> colonna "Zona" della tabella 4.2 dice quale. Lo stream autoritativo è `UGSTN_SimEventSubsystem`
> (`Plugins/GSTNInput/Source/GSTNInputCore/Public/Events/GSTN_SimEventSubsystem.h`): sui client è vuoto.

### 4.2 Chi dichiara quali tag (nativi, C++)

Tutti dichiarati con `UE_DEFINE_GAMEPLAY_TAG*`. Conteggi fuori dai test. Per l'elenco completo apri il file indicato.

| Plugin | Radici (numero di tag) | File | Zona |
|---|---|---|---|
| Progetto `GASP_Giustino2` | `Ability.Movement.{Jump,WallSlide}`, `State.Movement.*` (3), `Anim.Set.*` (1) | `Source/GASP_Giustino2/Private/GAS/GSTN_AbilityTags.cpp` | A |
| GSTNInput | `Command` (radice), `Input.Button`, `Input.Context.*` (Grounded, Airborne, OnWall), `Input.Lock.Command`, `Input.BufferEvent.*`, `Input.Event.CommandExecuted` | `Plugins/GSTNInput/Source/GSTNInputCore/Private/Input/GSTN_InputTags.cpp` | A (evento: C) |
| GSTNCombat (Core) | `Command.{Attack,Strong,Special,Combo}.*`, `Command.Super.Slot1..8`, `Command.Movement.*`, `Command.Defence.Dodge`, `Command.Interact.*` | `Plugins/GSTNCombat/Source/GSTNCombatCore/Private/Input/GSTN_CombatCommandTags.cpp`, `GSTN_CompiledBindingTable.cpp` | A |
| GSTNCombat (Core) | `Input.Button.{Attack,Strong,Special,Crouch,Shield,Jump,...}` (9), `Input.Lock.Command.*` (5) | `.../Input/GSTN_CombatButtonTags.cpp`, `GSTN_CombatLockTags.cpp` | A / E |
| GSTNCombat (Core) | `Combat.State.*` proiettati (Busy, InTimeline, SuperInput, CancelWindow, OverrideWindow, Falling, Hitstun, Hitstop...), `Combat.State.Crouch`, `Combat.State.AirCrouch`, `Combat.State.Dodge.EndLag`; radici `Weapon`, `Kit`, `Super`, `Loadout`, `Skin` | `.../Private/GSTN_CombatCoreTags.cpp`, `Rules/GSTN_GroundCrouch.cpp`, `Rules/GSTN_AirCrouch.cpp`, `Movement/GSTN_DodgeLanding.cpp` | B (stati), E (radici) |
| GSTNCombat (Core) | `Combat.HitType.*`, `Combat.Relationship.*`, `Combat.Hurtbox`, `Combat.Source.*` | `.../Private/Hits/GSTN_CombatHitTags.cpp` | E |
| GSTNCombatGAS | `Combat.Ability.*`, `Combat.Source.{Command,Forced,Adopted}`, `Combat.Lock.Movement`, `Combat.Weapon.Role.*`, `Combat.Event.*` (14), `Combat.SetByCaller.*`, `Combat.Hurtbox.*`, `Combat.KOReason.*`, radici `Combat.Stat`, `Combat.Meter` | `Plugins/GSTNCombatGAS/Source/GSTNCombatGAS/Private/GSTN_CombatTags.cpp` | A (lock), E, eventi A o C |
| GSTNCombatNP | `Combat.Event.*` (6), `Combat.Mode.{Duel,PvE,TDM}`, `Combat.SetByCaller.*` (15), `Movement.State.Skid(.JumpWindow)`, `Anim.Set.*` (3) | `Plugins/GSTNCombatNP/Source/GSTNCombatNP/Private/GSTN_CombatNPTags.cpp` | A (`Movement.State.Skid*` sono tag di Mover, copiati nel contesto di input), E |
| GSTNDefence | `Combat.State.{Shielding,ShieldStun,ShieldBroken,Parried,ParryWindow,ParryRecovery,Teching,Tech.Lockout,Knockdown}`, `Combat.Immunity.*`, `Combat.Event.{Parried,Teched,ShieldBroken,...}`, `Command.Defence.Shield` | `Plugins/GSTNDefence/Source/GSTNDefenceCore/Private/GSTN_DefenceTags.cpp` | B, A, eventi C |
| GSTNLink | `Combat.Link.{Linked,Pending}`, `Combat.Link.Type.*`, `Combat.Link.Role.*`, `Combat.Link.BreakReason.*` (16), `Command.Link.*` (12: Grab, Pummel, Throw.*, Release, Latch.Attack), `Combat.Lock.Link`, `Interact.Target.Grab` | `Plugins/GSTNLink/Source/GSTNLinkCore/Private/GSTN_LinkTags.cpp` | A, E, `Combat.Event.LinkFormed/Broken` = C |
| GSTNSpinDrill | `Ability.Movement.{Spin,SpinCancel}`, `Ability.Combat.Drill.*`, `Combat.State.{Spinning.*,AirSpinSpent,SpinLowGravity,DiveGravity,Drilling,DrillLatched}`, `Command.Movement.{Spin.*,Drill}`, `Combat.Event.Spin/Drill.*` | `Plugins/GSTNSpinDrill/Source/GSTNSpinDrillCore/Private/GSTN_SpinDrillTags.cpp` | A, B |
| GSTNHeld | `Combat.Held.*` (11), `Command.Held.{Pickup,Drop,Throw,ThrowEnemy}`, `Command.Kit.Swap`, `Interact.Target.{Pickup,Equipment}` | `Plugins/GSTNHeld/Source/GSTNHeldCore/Private/GSTN_HeldTags.cpp` | comando A, esito C |
| GSTNPiggyback | `Combat.Link.*` (cavalcare), `Combat.PairMove.*`, `Combat.State.{Riding,Carrying,MountPending}`, `Combat.Event.Link.Mounted/...` | `Plugins/GSTNPiggyback/Source/GSTNPiggybackCore/Private/GSTN_PiggybackTags.cpp` | A, B, eventi C |
| GSTNParts | `Combat.Part.*`, `Combat.Event.Part.*` (6), `Message.Combat.*` | `Plugins/GSTNParts/Source/GSTNPartsCore/Private/GSTN_PartsTags.cpp` | A (frammento), eventi C |
| GSTNBackpack | `Backpack.*`, `Backpack.State.*` (5), `Command.Backpack.{Use,Dome}` | `Plugins/GSTNBackpack/Source/GSTNBackpackCore/Private/GSTN_BackpackTypes.cpp` | A |
| GSTNMinion | `Minion.State.*` (12), `Minion.LOD.*`, `Combat.Event.Minion*` | `Plugins/GSTNMinion/Source/GSTNMinionCore/Private/GSTN_MinionTags.cpp` | Server (vedi 6) |
| GSTNModeVerbs | `Ability.Mode.{Interact,CallDrop,CallExtraction,CallReinforcement}`, `Command.Mode.Interact`, `Message.Mode.*` (3), `Interact.Target.Objective` | `Plugins/GSTNModeVerbs/Source/GSTNModeVerbsCore/Private/GSTN_ModeVerbTags.cpp` | A (tenere premuto), C (effetto) |
| GSTNMission / GSTNRespawn / GSTNMode_Levels | `Mission.*`, `Message.Mission.*` (7), `Message.Respawn.*` (6), `Message.Levels.*` (5) | `Plugins/GSTNMission/.../GSTN_MissionTypes.cpp`, `Plugins/GSTNRespawn/.../GSTN_RespawnLogic.cpp`, `Plugins/GameFeatures/GSTNMode_Levels/.../GSTN_LevelsTags.cpp` | C |
| GSTNInteract | `Interact`, `Interact.Target`, `Message.Interact.FocusChanged` | `Plugins/GSTNInteract/Source/GSTNInteractCore/Private/GSTN_InteractTags.cpp` | D |
| GSTNSurface | `Surface.Type`, `Surface.Behaviour.*` (6), `Surface.Event.Launched` | `Plugins/GSTNSurface/Source/GSTNSurfaceCore/Private/GSTN_SurfaceTags.cpp` | E (evento: D) |
| GSTNPlatform | `Platform.State.*`, `Platform.Event.*` | `Plugins/GSTNPlatform/Source/GSTNPlatformCore/Private/GSTN_PlatformTags.cpp` | B |
| GSTNKitMods | `Mod`, `Mod.Slot` | `Plugins/GSTNKitMods/Source/GSTNKitModsCore/Private/GSTN_KitModBaking.cpp` | E |
| GSTNPresentation | `Presentation.Event.*` (34), `Presentation.Footwear.*` | `Plugins/GSTNPresentation/Source/GSTNPresentationCore/Private/GSTN_PresentationTags.cpp` | D |
| GSTNAnimation | `Anim`, `Anim.Set`, `Anim.Trait` | `Plugins/GSTNAnimation/Source/GSTNAnimationCore/Private/GSTN_AnimTags.cpp` | D |
| GSTNCamera | `Camera.Mode.*` (6), `Camera.State.*` (4), `Camera.Request.*`, `Camera.Section` | `Plugins/GSTNCamera/Source/GSTNCameraCore/Private/GSTN_CameraTags.cpp` | D |
| GSTNCosmetics | `Cosmetic.Slot.*` (11), `Cosmetic.Spline.*`, `Cosmetic.Style`, `Cosmetic.Theme` | `Plugins/GSTNCosmetics/Source/GSTNCosmeticsCore/Private/GSTN_CosmeticTags.cpp` | D |
| Mover (override) | `Mover.*` (13) | `Plugins/EngineOverrides/Mover/Source/Mover/Private/MoverTypes.cpp` | A (tag dello stato di Mover) |

### 4.3 Tag in file .ini (dei designer)

| File | Cosa contiene |
|---|---|
| `Config/Tags/GSTNCommands.ini` | Tasti e comandi dei designer: `Input.Button.Dash/Buff/RunBoost`, `Command.Dash/Buff`, `Command.Movement.RunBoost` (input.md §3.1) |
| `Config/Tags/GSTNCombatTest.ini` | 33 tag delle mappe e dei kit di test |
| `Config/Tags/GSTNExamples.ini` | Esempi: `Kit.Example.*`, `Weapon.Example.*`, `Cosmetic.*`, `GameplayCue.Hit.*` |
| `Config/DefaultGameplayTags.ini` | Tag del sample GASP: `Foley.Event.*` (19), `MotionMatching.*`, `SmartObject.*` |

Regole: i tag del codice sono nativi; i leaf dei designer (kit, armi, skin, comandi nuovi) in ini; il codice non nomina mai un leaf dei
designer (modularity-and-tags.md §4).

---

## 5. I "verbi"

La parola "verbo" nel progetto ha **tre significati**. Solo i primi due riguardano il gioco.

| Significato | Cos'è | Dove è definito |
|---|---|---|
| **Verbo del giocatore** | Ogni azione di un giocatore o nemico è un'**abilità** data dai dati (kit, arma, mod, zaino), legata a un comando. Aggiungere o togliere un verbo = cambiare dati | [modularity-and-tags.md](modularity-and-tags.md) "Goal 9", [combat-loadout-mods.md](combat-loadout-mods.md) §2 |
| **Verbo di modalità** | Azione verso la partita (terminale, chiamata rifornimenti, estrazione): metà predetta, metà solo server | [verbs-vs-commands-guide.md](verbs-vs-commands-guide.md), plugin `GSTNModeVerbs` |
| Verbo dell'animpipe | Comando dello strumento animazioni (`python -m animpipe <verbo>`). Non c'entra col gioco | [anim-pipeline.md](anim-pipeline.md) |

Catena di un verbo del giocatore: **tasto** (`Input.Button.*`) → **comando** (`Command.*`, riconosciuto nella simulazione) → **riga della
tabella compilata** del kit (comando + contesto → abilità + timeline) → **abilità** (classe C++ o Blueprint figlio) → **timeline** (dati
frame per frame) → Mover / colpi / stato. Tabella compilata: `Plugins/GSTNCombat/Source/GSTNCombatCore/Public/Loadout/GSTN_CompiledBindingTable.h`.

### 5.1 Verbi del giocatore oggi

Tutte le righe sono **zona A, rollback sì**, salvo dove la colonna Note dice altro. "Classe" è la classe C++; le mosse vere sono di solito
Blueprint figli (`GA_*`) con la loro timeline.

| Verbo | Comando | Classe dell'abilità | File | Note |
|---|---|---|---|---|
| Attacchi (jab, tilt, smash, aerial, special, combo, super) | `Command.{Attack,Strong,Special,Combo}.*`, `Command.Super.SlotN` | `UGSTN_GA_BaseCombat` (BP figli per mossa) | `Plugins/GSTNCombatNP/Source/GSTNCombatNP/Public/Abilities/GSTN_GA_BaseCombat.h` | Kit Overlord: nuove mosse di riferimento solo su branch (sez. 7) |
| Salto, fast fall | `Command.Movement.Jump`, `Command.Movement.FastFall` | `UGSTN_GA_PlatformJump`, `UGSTN_GA_PlatformFastFall` | `.../Abilities/GSTN_GA_PlatformJump.h` | Chi può saltare lo decide il tick (`FGSTN_JumpState`), l'abilità applica l'impulso |
| Salto (vecchio) | input diretto | `UGSTN_GA_Jump` | `Source/GASP_Giustino2/Public/GAS/GSTN_GA_Jump.h` | Bring-up del personaggio Mover di base |
| Scatto (dash) | `Command.Movement.Dash` | `UGSTN_GA_Dash` | `.../Abilities/GSTN_GA_Dash.h` | |
| Schivata a terra / in aria | `Command.Defence.Dodge` (+ `.CrouchJump` sul branch) | `UGSTN_GA_GroundDodge`, `UGSTN_GA_AirDodge` | `.../Abilities/GSTN_GA_Dash.h` | Lag di atterraggio in `FGSTN_DodgeLandingState`. Tuffo + rotolata solo su branch (sez. 7) |
| Abbassarsi (a terra, in aria) | tasto Crouch tenuto | nessuna abilità: regola | `GSTNCombatCore/Public/Rules/GSTN_GroundCrouch.h`, `GSTN_AirCrouch.h` | Zona B (tag proiettato) |
| Scudo | `Command.Defence.Shield` | `UGSTN_GA_Shield` | `Plugins/GSTNDefence/Source/GSTNDefenceNP/Public/Abilities/GSTN_GA_Shield.h` | |
| Tech, rialzarsi, rotolata da terra, rimbalzo a terra | dalle reazioni | `UGSTN_GA_Tech`, `UGSTN_GA_GetupNeutral`, `UGSTN_GA_GetupRoll`, `UGSTN_GA_GroundBounce` | `.../GSTNDefenceNP/Public/Abilities/GSTN_DefenceAbilities.h` | Attivate dal tick (fonte `Forced`) |
| Presa, pummel, rilascio | `Command.Link.{Grab,Pummel,Release}` | timeline con tracce di link (`UGSTN_GA_BaseCombat`); harness: `UGSTN_GA_LinkTest*` | `Plugins/GSTNLink/Source/GSTNLinkNP/Public/Harness/GSTN_LinkTestAbilities.h` | Stato nel `FGSTN_LinkFragment` |
| Lanci (avanti, dietro, su, giù, neutro, crouch) | `Command.Link.Throw.*` | `UGSTN_GA_LinkThrow*` | `Plugins/GSTNLink/Source/GSTNLinkNP/Public/Moves/GSTN_LinkThrowMoves.h` | |
| Spin con la presa | `Command.Link.Spin.{Clockwise,CounterClockwise}` | `UGSTN_GA_LinkGrabSpin*` | `GSTNLinkNP/Public/Moves/GSTN_LinkGrabSpinMoves.h` | **Solo PR #53** |
| Spin, cancel, spin in aria | `Command.Movement.Spin.*` | `UGSTN_GA_Spin*`, `UGSTN_GA_SpinCancel*`, `UGSTN_GA_AirSpin*` | `Plugins/GSTNSpinDrill/Source/GSTNSpinDrillGAS/Public/Abilities/GSTN_GA_Spin.h` | |
| Trapano (tuffo, scavo) | `Command.Movement.Drill` | `UGSTN_GA_DrillDive`, `UGSTN_GA_DrillBore` | `.../GSTNSpinDrillGAS/Public/Abilities/` | |
| Cavalcare, scendere, far cadere, mosse di coppia | comandi dai dati del kit | `UGSTN_GA_Mount`, `UGSTN_GA_Dismount`, `UGSTN_GA_Drop`, `UGSTN_GA_PairMove` | `Plugins/GSTNPiggyback/Source/GSTNPiggybackRuntime/Public/` | Il partner vede la mossa in "presentazione predetta" (networking-spiegato §2) |
| Raccogliere, posare, lanciare un oggetto | `Command.Held.*` | `UGSTN_GA_HeldPickup`, `UGSTN_GA_HeldDrop`, `UGSTN_GA_HeldThrow` | `Plugins/GSTNHeld/Source/GSTNHeldRuntime/Public/GSTN_HeldAbilities.h` | **Ibrido**: l'abilità è predetta, ma chi prende l'oggetto lo decide `UGSTN_HeldSubsystem` sul server (zona C) |
| Cambiare kit da terra | `Command.Kit.Swap` | `UGSTN_GA_KitSwap` | `GSTNHeldRuntime/Public/GSTN_KitInteract.h` | Il cambio di loadout è zona C (vedi 6) |
| Sparare un proiettile | dati | `UGSTN_GA_FireProjectile`, `UGSTN_GA_AimedHitscanShot` | `Plugins/GSTNProjectile/Source/GSTNProjectileNP/Public/GSTN_GA_FireProjectile.h` | La carica è predetta; lo **spawn** lo fa solo l'host, una volta (zona C) |
| Zaino (cupola, uso) | `Command.Backpack.*` | `UGSTN_GA_BackpackDome` | `Plugins/GSTNBackpack/Source/GSTNBackpackNP/Public/GSTN_GA_BackpackDome.h` | |
| Scivolare sul muro | nessun tasto | `UGSTN_GA_WallSlide` | `Source/GASP_Giustino2/Public/GAS/GSTN_GA_WallSlide.h` | Il movimento resta nelle modalità di Mover |

Le varianti `...Server` (`UGSTN_GA_HeldPickupServer`, `UGSTN_GA_FireProjectileServer`...) sono per harness, IA e comandi dev: si attivano
solo sull'host, quindi non predette.

### 5.2 Verbi di modalità

| Verbo | Comando | Parte predetta (A) | Parte solo server (C) | File |
|---|---|---|---|---|
| Attivare un terminale | `Command.Mode.Interact` → `Ability.Mode.Interact` | Tenere premuto, animazione, interruzione da colpo (`HoldStartClock`, `VerbState`) | `UGSTN_VerbSubsystem` valida e pubblica `Message.Mode.Verb` una volta | `Plugins/GSTNModeVerbs/Source/GSTNModeVerbsNP/Public/GSTN_GA_ModeVerb.h`, `GSTN_VerbSubsystem.h` |
| Chiamare rifornimenti, estrazione, rinforzi | `Ability.Mode.{CallDrop,CallExtraction,CallReinforcement}` | uguale | uguale | idem |

---

## 6. Zone C e D: cosa è fuori dalla simulazione

| Sistema | File | Zona | Perché fuori | Cosa vede il client |
|---|---|---|---|---|
| Stream di eventi autoritativi | `Plugins/GSTNInput/Source/GSTNInputCore/Public/Events/GSTN_SimEventSubsystem.h`, `GSTN_SimEvent.h` (`FGSTN_SimEventOutbox`) | C | Missioni, statistiche, kill feed non devono contare due volte un evento risimulato | Nulla: lo stream è vuoto sui client; il feedback arriva da stato replicato o presentazione |
| Missioni e obiettivi | `Plugins/GSTNMission/Source/GSTNMissionRuntime/Public/GSTN_MissionSubsystem.h` | C | Progresso solo da eventi autoritativi e poll del server | Stato replicato |
| Respawn, vite, checkpoint | `Plugins/GSTNRespawn/Source/GSTNRespawnRuntime/Public/GSTN_RespawnSubsystem.h` | C | KO letti dallo stream; respawn = pawn nuovo | `AGSTN_RespawnStateInfo` replicato |
| Effetto dei verbi di modalità | `GSTNModeVerbsNP/Public/GSTN_VerbSubsystem.h` | C | Vedi 5.2 | Esito replicato |
| Chi prende un oggetto, lancio oggetti | `Plugins/GSTNHeld/Source/GSTNHeldRuntime/Public/GSTN_HeldSubsystem.h` | C | Due richieste nello stesso frame: vince la più vecchia, poi id più basso | La presa un giro di rete dopo |
| Equipaggiamento a terra, cambio kit | `Plugins/GSTNPickups/` | C | Regola di gara come sopra | Loadout replicato |
| Loadout (cambio kit, arma, mod) | `UGSTN_CombatSimComponent::ServerWriteLoadoutRevision`, `Plugins/GSTNCombatGAS/Source/GSTNCombatGAS/Public/Loadout/GSTN_LoadoutComponent.h` | C → A | Il server scrive la revisione nello stato; la simulazione cambia tabella nello stesso frame su tutte le macchine quando ha la tabella | Il cambio avviene a un frame preciso, poi è stato A |
| Spawn dei proiettili e loro colpi | `Plugins/GSTNProjectile/Source/GSTNProjectileNP/Public/GSTN_ProjectileSubsystem.h` | C | L'host non risimula, quindi spawna una volta; traiettoria in forma chiusa uguale ovunque | Un "fantasma" locale, una volta per frame di partenza |
| Minion (nemici di massa) | `Plugins/GSTNMinion/` | C | Il server li simula tutti insieme, a lotti | Marionette interpolate |
| Lag compensation | `GSTNCombatNP/Public/Simulation/GSTN_LagCompensation.h` | B, solo server | Riavvolge le vittime per l'attaccante | Il colpo che hai visto conta |
| Esperienza / caricamento modalità | `Plugins/GSTNExperience/` | C | Blocca la simulazione dei pawn finché non è tutto caricato | |
| Focus di interazione | `Plugins/GSTNInteract/Source/GSTNInteractRuntime/Public/GSTN_InteractorComponent.h` | D | Solo il giocatore locale cerca il bersaglio; il server rivalida | Prompt locale |
| Eventi di presentazione (VFX, suoni, shake) | `Plugins/GSTNPresentation/Source/GSTNPresentationCore/Public/GSTN_PresentationLogic.h` (`FGSTN_PresentationDedupe`) | D | Un evento per (evento, frame): il rollback non lo ripete | |
| Animazione, layer, anim set | `Plugins/GSTNAnimation/`, ABP | D | La simulazione non legge mai la posa | |
| Capsula greybox (squash, tuffo, rotolata, air crouch) | `Plugins/GSTNCombatNP/Source/GSTNCombatNP/Public/Presentation/GSTN_GreyboxAnimInstance.h` | D | Legge stato presentato e stato combattimento in sola lettura | |
| Camera (anchor, zoom, auto follow) | `Plugins/GSTNCamera/Source/GSTNCameraRuntime/Public/GSTN_CameraComponent.h` | D | Locale per giocatore; uno scatto da rollback della radice non arriva alla vista | |
| Marker di profondità | `Plugins/GSTNDepthMarker/Source/GSTNDepthMarkerRuntime/Public/GSTN_DepthMarkerComponent.h` | D | Segue `GetPresentedTransform()` | |
| Cosmetici | `Plugins/GSTNCosmetics/` | D | Solo gli ID viaggiano in rete | |

---

## 7. Novità recenti, con stato di unione

Nessuna riga sotto è compilata o provata sul fisso, salvo dove scritto. "Branch senza PR" = il lavoro esiste solo nel branch.

| Novità | Stato | Tag nuovi | Zona e rollback | File chiave |
|---|---|---|---|---|
| **Grab spin e numeri della presa di Ganondorf** | **PR #53 aperta** (`claude/project-thread-h46ltf`), build sul fisso in pausa | `Command.Link.Spin.{Clockwise,CounterClockwise}`, `Combat.HitType.GrabSpin`, `Combat.HitType.GrabSpin.Wall` | A: la rotazione la scrive il tick del link (`StepGrabSpin`) nello stato core e nel `FGSTN_LinkFragment`; rollback sì. La CVar `gstn.Grab.Spin` è letta alla **compilazione del kit** (E) | `Plugins/GSTNLink/Source/GSTNLinkNP/Public/Moves/GSTN_LinkGrabSpinMoves.h`, `GSTNLinkCore/Public/Data/GSTN_GrabSpinTrack.h`, `GSTNLinkNP/Private/Sim/GSTN_LinkTickExtension.cpp` |
| **Carica degli aerial (Charge Hold)** | **PR #54 aperta** (`claude/project-thread-uyhp3p`), non compilata | nessuno | A: nuovo campo di sync `ChargeFrames` (1 bit se 0); la pausa usa `TimelinePausedFrames`; danno scalato nel tick dell'attaccante; rollback sì. Il loop del montage è presentazione (D) | `Plugins/GSTNCombat/Source/GSTNCombatCore/Public/Timeline/GSTN_ChargeHold.h`, `GSTN_CombatSimTypes.h` |
| **Schivata tuffo + rotolata (Mario 64 / Odyssey)** | **Branch senza PR** (`claude/project-thread-g997re`), non compilata | `Command.Defence.Dodge.CrouchJump` | A: lag in `FGSTN_DodgeLandingState` (pancia fino all'arresto, calcolata una volta all'atterraggio, + 10 f di rotolata); rollback sì. Scelta input `UGSTN_CombatSettings::DodgeInput` = E (impostazione di progetto, applicata alla compilazione della tabella). Tuffo e rotolata della capsula = D | `GSTNCombatCore/Public/Movement/GSTN_DodgeLanding.h`, `GSTN_CompiledBindingTable.cpp`, `GSTN_GreyboxAnimInstance.h` |
| **Air crouch** (regola e tag) | **In `main`** (handover `air-crouch-1`) | `Combat.State.AirCrouch` | B (proiettato, zero byte) | `GSTNCombatCore/Public/Rules/GSTN_AirCrouch.h` |
| **Air crouch sulla capsula** (schiacciamento 0,78) | **Branch senza PR** (`claude/project-thread-g997re`) | nessuno | D | `GSTN_GreyboxAnimInstance.h` |
| **Mosse di riferimento Ganon/Ike sul kit Overlord** | **Branch senza PR** (`claude/project-thread-sznhpw`) | nessuno | E (dati: timeline e binding scritti da script); le mosse poi girano in A come ogni timeline | `Tools/GAS/setup_overlord_ref_moves.py`, `Tools/GAS/overlord_ref_moves_data.py`, `docs/notes/overlord-ref-moves.md` |
| **Zoom camera (PSO2, 580 cm) e auto follow spento** | **Branch senza PR** (`claude/project-thread-dome4c`), non compilato | nessuno | D (CVar `gstn.Camera.Zoom`, `gstn.Camera.AutoFollow`, `gstn.Camera.Distance`, solo nel pannello debug) | `Plugins/GSTNCamera/Source/GSTNCameraRuntime/Public/GSTN_CameraComponent.h`, `GSTN_CameraProfile.h` |
| Capsula greybox di default, kit di test nascosti | **PR #52 aperta** | nessuno | D (e CVar debug `gstn.Debug.ShowTestKits`) | `Source/GASP_Giustino2/Public/Animation/GSTN_GreyboxSubsystem.h` |
| Oscillazione della capsula in sprint | **PR #45 aperta** | nessuno | D | `GSTN_GreyboxAnimInstance.h` |
| Spin per tutti i kit, vite del pavimento, interfaccia "spin turnable" | **In `main`** (PR #51) | (tag spin già esistenti) | A per lo spin; la vite è un oggetto tenuto (esito C) | `Plugins/GSTNSpinDrill/`, `Plugins/GSTNSpinScrew/` |
| Presentazione legge lo stato smussato di Mover | **In `main`** (PR #49) | nessuno | D (`GetPresentedTransform()`) | `UMoverComponent::GetPresentedTransform` |

---

## 8. Cose da ricontrollare (attenzioni)

1. **CVar che toccano la simulazione.** Una CVar letta dentro il tick sarebbe diversa tra client e server. Le CVar di gameplay del link
   (`gstn.Grab.*`) sono lette alla compilazione del kit, quindi valgono come dati: ma vanno impostate **uguali sull'host e sui client**,
   altrimenti le tabelle differiscono. In multiplayer vero vanno spostate nei dati del kit o nelle impostazioni replicate.
2. **Opzioni per giocatore** (es. metodo di schivata). Oggi `DodgeInput` è un'impostazione di progetto proprio perché la tabella si
   compila uguale su ogni macchina; un'opzione per giocatore dovrà viaggiare con il loadout (handover `dodge-dive-roll-1125.md`).
3. **`Combat.Event.*` ha due usi** (sezione 4.1): prima di ascoltarne uno per una missione o una statistica, controlla che sia pubblicato
   sullo stream autoritativo e non solo inviato all'abilità nel tick.
4. **Variabili delle abilità.** Ogni dato che deve durare tra frame in un'abilità è `FSync*`; un membro normale non torna indietro
   (gas-npp.md regola 4 e 8).
5. **Mai leggere il componente Mover dalle abilità** (`GetMovementModeName`, `IsOnGround`): usa `GetMoverSimState`.
6. **Il server decide, il client prevede.** Ciò che è in zona C arriva sempre con un giro di rete di ritardo (~120 ms a 60 ms per
   direzione, misure in networking-spiegato.md).

## 9. Come tenere aggiornato questo file

Chi aggiunge un tag radice, un frammento, un campo di sync, un'abilità nuova o sposta qualcosa fuori dalla simulazione aggiorna la
riga qui nello stesso commit (come per `docs/INDICE-PROGETTO.md`). Per rifare i conteggi dei tag:
`grep -rhoE 'UE_DEFINE_GAMEPLAY_TAG[A-Z_]*\([^,]+, *"[^"]+"' Plugins Source --include=*.cpp`.

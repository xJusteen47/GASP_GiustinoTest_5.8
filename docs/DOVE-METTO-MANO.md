# Dove metto mano se voglio cambiare...

Quindici casi comuni, dal più semplice. Per ognuno: dove si cambia, con cosa, e cosa NON fare.
Termini spiegati nel glossario di [INDICE-PROGETTO.md](INDICE-PROGETTO.md#2-glossario). Percorsi brevi come nell'indice:
`Core/` = `Plugins/GSTNCombat/Source/GSTNCombatCore/Public/`, `NP/` = `Plugins/GSTNCombatNP/Source/GSTNCombatNP/Public/`,
`GAS/` = `Plugins/GSTNCombatGAS/Source/GSTNCombatGAS/Public/`, `Prj/` = `Source/GASP_Giustino2/Public/`.

Regola generale: se è un numero, si cambia in un **asset** o in una **cvar**, non nel codice. Se serve codice, prima si
cerca nell'indice se esiste già.

### 1. Velocità di corsa, stadi della corsa, frenata
Asset `DA_Locomotion_*` (classe `UGSTN_LocomotionProfile`, `GAS/Kits/GSTN_LocomotionProfile.h`), sezioni Run Stages e Skid.
Il kit punta al suo profilo con l'estensione "Locomotion". Non committare mai `DA_Locomotion_Default` dell'owner: si fa
una copia o si usa lo script `Tools/GAS/setup_locomotion_profile.py`.

### 2. Altezza e tempi del salto
Stesso asset, sezione Jump Designer (altezza e tempi in numeri da designer, `Core/Jump/GSTN_JumpDesigner.h`), oppure le
proprietà "Jump" sull'abilità `GSTN_GA_PlatformJump`. Il vecchio `Prj/GAS/GSTN_GA_Jump.h` non c'entra.

### 3. Controllo in aria (drift, quanto lo stick frena le spinte)
`DA_Locomotion_*` > Run Stages > Air > Momentum (`Core/Movement/GSTN_AirMomentumTypes.h`). Rotazione in aria: sezione
Analog Walk (`NP/Movement/GSTN_AirTurn.h`).

### 4. Frame data di una mossa (startup, attivi, FAF, cancel)
Nell'editor timeline (GSTNCombatEditor, doc `docs/architecture/combat-m5-editor.md`) sull'asset `UGSTN_CombatTimeline`
della mossa o sulla timeline inline della sua abilità `GA_*`. I validatori segnalano subito i dati incoerenti.

### 5. Come una mossa sceglie la direzione (segue lo stick, reverse)
Stessa timeline: campi `AimPolicy`, `Reverse` (finestra di reverse), `MomentumSlide`. Logica: `Core/Timeline/GSTN_StartupSteer.h`.
Non scrivere un secondo sistema di mira: c'è già (indice 5.1).

### 6. Dimensione e posizione di una hitbox
Nella timeline della mossa, traccia delle hitbox (forma `Core/Hits/GSTN_HitShape.h`). Se la mano non arriva al bersaglio
si sposta la hitbox o il manichino, non si accorcia l'arco dell'animazione (regola 13e). Vedere a schermo: cvar
`gstn.Combat.Debug.Hitboxes 1`.

### 7. Dove si può colpire un personaggio (hurtbox)
Asset `HBS_*` (`UGSTN_HurtboxSetDefinition`, `Core/Hits/GSTN_HurtboxTypes.h`), modificabile con l'editor di hurtbox o
generato con `Tools/GAS/make_hurtbox_set.py`. Il set appartiene al KIT, non alla skin.

### 8. Danno, knockback, hitstop di un colpo
Dati del colpo `Core/Hits/GSTN_HitData.h` nella finestra della timeline; regole globali nell'asset `UGSTN_CombatRules`
(default in `Plugins/GSTNCombat/Content/Rules/`). Effetto visivo dell'hitstop: `NP/Presentation/GSTN_HitPresentation*.h`.

### 9. Aggiungere una mossa a un kit
Si crea l'abilità (figlia di `GSTN_GA_BaseCombat`) con la sua timeline e montage, poi la si lega a un comando nel kit
(`KitBindings`, `GAS/Kits/GSTN_AbilityBinding.h`). Per i kit Overlord ci sono gli script `Tools/GAS/setup_overlord_*_kit.py`.
Prima della build: brief con 2-3 tratti d'identità (regola 13a); dopo: video in gioco e review (regola 10).

### 10. Cambiare l'animazione di una mossa
Pipeline `Tools/AnimPipeline` (`python -m animpipe status <mossa>`), skill `animpipe-move` / `gstn-move-animation`. Mai
sovrascrivere una versione in `docs/notes/anim-approvate.md`: si lavora su una variante.

### 11. Un ritocco "un po' di più / un po' di meno"
Si espone una cvar o un additivo regolabile (regola 15c). Per farla comparire da sola nel menu GSTN Debug il nome deve
essere `gstn.<Gruppo>.Debug<Qualcosa>` (pannello `Plugins/GSTNCombatDebug/.../Window/SGSTN_DebugOptionsPanel.cpp`).

### 12. Tasti e comandi
Azioni e mapping: `Content/GASNPPTest/Input/`; nuovi comandi o pulsanti da designer: `Config/Tags/GSTNCommands.ini`;
legame comando→abilità nel kit. Un nuovo contesto di mapping va aggiunto a `Config/DefaultGame.ini`
(`AbilitySystemMappingContexts`). Doc `docs/architecture/input.md`.

### 13. Effetti visivi, suoni, vibrazione per un evento
Tabella `UGSTN_PresentationTable` (Plugins/GSTNPresentation): una riga per evento `Presentation.Event.*`. Passi e
vibrazione per superficie: `GSTN_SurfaceFeedbackComponent` (ancora da compilare). Prima di un VFX si chiede all'Unreal
Assistant (`Tools/UE59/ask_unreal_assistant.py`).

### 14. La camera
Camera di GASP con àncora: `Prj/Camera/GSTN_CameraAnchor.h` e cvar `gstn.Camera.Anchor*`, `gstn.Camera.P1a`.
Camera nuova a modi: `Plugins/GSTNCamera` (non ancora sul pawn vero). Decidere prima quale delle due si tocca.

### 15. Presa e lanci
Plugin GSTNLink: tipo di link (asset `GSTN_LinkTypeDefinition`), regole di presa (`GSTN_GrabRules.h`), lanci dal kit
(`Input/GSTN_LinkSlots.h`, `Moves/GSTN_LinkThrowMoves.h`). Il lavoro "presa al collo" è sul branch `claude/grab-link`:
guardare lì prima di ricominciare.

## Cosa non toccare mai
- `Content/Blueprints`, `Content/Input`, `Content/Levels` (GASP di Epic) e `Plugins/EngineOverrides`, `Plugins/GASNPP` senza un piano.
- I file locali dell'owner (`DA_Locomotion_Default`, `CHT_*_PoseMatch`): la guardia git li blocca.
- Un Blueprint condiviso senza avvisare l'owner (sono binari, non si uniscono).

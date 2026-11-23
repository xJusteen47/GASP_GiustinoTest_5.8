# Riorganizzazione di Content/ (fase 1: mappa e proposta, SOLO LETTURA)

Stato: `origin/main` a5b2095 (PR #37, 2026-11-22). Nessun asset spostato. Fonte dei conteggi: `git ls-files Content 'Plugins/*/Content'`, il commit `f1a280d5` (import di GASP, branch `claude/project-thread-a0j7ip`) e `git grep "/Game/..."` (comandi in fondo). Aggiorna il piano del 2026-11-16 (`handoffs/riorganizzazione-cartelle-piano.md`, mai eseguito).

## Riassunto (10 righe)
1. In `Content/` versionato ci sono 641 file: **261 di GASP** (Epic), **380 nostri**; più 91 nostri nei plugin.
2. **GASP** = `Blueprints/`, `Input/`, `Levels/`: è il sample di Epic. Ci sono finiti dentro **55 file nostri** (wall layer, run start/skid, RunStop Ganon, DepthMarker, 3 mappe). GASP non si sposta: si tirano fuori solo i nostri.
3. **"GSTN Test" non esiste come cartella** nel repo né in nessun branch: è quasi certamente `GASNPPTest/` (239 file). Nome storico: era il banco di prova della rete (GAS-NPP). Oggi dentro c'è di tutto: kit e mosse vere dell'Overlord, DA_Locomotion_Default, input, mappa di combattimento, fixture dei test, esempi.
4. **GSTN** (`Content/GSTN/`, 79 file) è la cartella nuova "giusta": animazioni nostre (run start, reazioni, crouch, mani) e locomotion stock. È pulita ma ha due posti per la locomotion (`GSTN/Animation/Locomotion` e `GSTN/Locomotion/StockBase`).
5. Differenza in una riga: GASP = di Epic, non si tocca; GASNPPTest = nostro ma "cassetto" storico misto; GSTN = nostro, ordinato.
6. Asset di dati sparsi: circa 75 file (tabella §2), soprattutto DA dentro cartelle GASP e kit/armi/regole buttati in `GASNPPTest/Combat` (79 file in una cartella sola).
7. Target: **tutto il nostro sotto `Content/GSTN/`**, con `_Test`, `_Examples`, `_Archive` separati dal gioco (§3). GASP resta com'è.
8. Rischio principale: circa 340 file di codice/ini/script citano i percorsi `/Game/...` (i redirector non li sistemano) e ~38 branch recenti hanno binari diversi da main negli stessi percorsi.
9. Lo spostamento vero è un thread desktop con editor (~12 $, ~3 h), con backup, un commit solo e ritorno indietro con `git revert` (§4).
10. Decisioni per te in fondo: 3, con la consigliata già scelta.

## 1. Mappa cartella per cartella

| Cartella | File | Di chi | Cosa c'è |
|---|---|---|---|
| `Blueprints/` (radice, AI, AnimModifiers, AnimNotifies, Cameras, ControlRigs, MovementModes, RetargetedCharacters, SmartObjects) | 152 | GASP | personaggio sandbox, camere, notify. 3 file GASP **modificati da noi** (restano): `SandboxCharacter_Mover`, `SandboxCharacter_Mover_ABP`, `SandboxCharacter_CMC` |
| `Blueprints/Data/` | 48 | 46 GASP, **2 nostri** | `DA_CameraAnchor_Default`, `DA_GroundContact_Default` |
| `Blueprints/Animation/` (radice) | 13 | **nostri** tutti e 13 | `ABP_GSTN_{RunStart,Skid,Wall,Wall_Heavy}Layer`, `ALI_GSTN_*`, `DA_AnimSet_*`, `DA_AnimSetCollection_SandboxCharacter` |
| `Blueprints/Animation/Ported57/` | 14 | **nostri** | skid e wall slide portati dalla 5.7 (`SkidOwner/` = clip tue) |
| `Blueprints/Animation/RunStop/` | 12 | **nostri** | `AS_RunStop_Ganon_{L,R}_V1..V6` |
| `Blueprints/DepthMarker/` | 10 | **nostri** | materiali del depth marker |
| `Blueprints/Equipment/` | 1 | **nostro** | `SM_Implant_Placeholder` |
| `Blueprints/NewBlueprint` | 1 | da verificare | Actor vuoto dell'import iniziale |
| `Input/` (+ MobileHUD) | 28 | GASP | `IA_Crouch` e `IMC_Sandbox` modificati da noi (restano) |
| `Levels/` (radice, LevelPrototyping) | 56 | GASP | mappe e oggetti demo |
| `Levels/DefaultLevel_Harness`, `L_WallSlideAB`, `Levels/Test/L_Test_Hub` | 3 | **nostri** | L_Test_Hub è la mappa di avvio (DefaultEngine.ini) |
| `GAS/` | 3 | nostri | `Backpacks/DA_Backpack_{JetPack,ShieldDome}`, `Input/IMC_GAS_Abilities` |
| `GASNPPTest/` (radice) | 3 | nostri | `BP_GASNPP_TestCharacter`, `GM_GASNPPTest`, `BP_Test_BouncePad` (personaggio e game mode del playtest) |
| `GASNPPTest/Combat/` (radice) | 79 | nostri, misti | 34 kit `KIT_Test_*`, 18 armi `WPN_Test_*`, hurtbox `HBS_*`, `SUP_*`, `MOD_*`, `TL_Test_*`, regole, PM fisici, 5 BP di esempio, `L_Test_Combat`, `DA_Locomotion_Default` |
| `GASNPPTest/Combat/Overlord/` | 68 | nostri, **gioco** | DAir V7..V10R4, Dropkick + TechBounce R2/R3, Jab1/2 V2/V3: AM, AS, GA, TL |
| `GASNPPTest/Combat/Archetypes/` | 21 | nostri, fixture | `GA_{Light,Medium,Heavy}_*` |
| `GASNPPTest/Combat/{Reaction,AirCrouch,Playtest,Mods}` | 18 | nostri | reazioni (gioco), air crouch (gioco), GA playtest |
| `GASNPPTest/Examples/` (+Cosmetics, DoubleShot) | 24 | nostri, esempi per designer | |
| `GASNPPTest/Input/` | 17 | nostri, **gioco** | `IA_GASNPP_*`, `IMC_GASNPP_Test`, `IMC_GSTN_Locomotion`, `IC/SS/BS` |
| `GASNPPTest/Stance/`, `TestMaps/Hub/` | 9 | nostri | stance di prova, materiali della mappa hub |
| `GSTN/Animation/` | 71 | nostri | RunStart (38, molte varianti), Reaction (14), Mixamo (8), Crouch, Dash, Hands, Locomotion (ABP + 3 chooser) |
| `GSTN/Locomotion/StockBase/` | 7 | nostri | `ABP_GSTN_StockMover`, 2 chooser, land/sprint stop |
| `GSTN/Materials/` | 1 | nostro | `MI_GSTN_Mannequin_Satin` |
| `AnimPipeline/Rig/Overlord/` | 4 | nostri | sorgente Live Link del dropkick (doppione voluto) |
| Plugin `GSTNPresentation/VFX` | 58 | nostri | VFX shipping |
| Plugin `GSTNDefence/Test`, `GSTNLink/Test`, `GSTNPiggyback/Test` | 26 | nostri, fixture | |
| Plugin `GSTNCombat/Rules`, `GSTNParts/Samples` | 7 | nostri | regole standard, esempi parti |

**Non visibile dal repo**: tutto il `Content/` non versionato (MetaHumans, audio, animazioni GASP, ~5 GB per `.gitignore`), e gli asset che i thread hanno creato sul fisso senza committarli (es. sotto `/Game/AnimPipeline`, che gli script citano 399 volte ma ha solo 4 file in git). Il thread desktop deve fare l'inventario reale dall'editor prima di spostare.

## 2. Asset di dati sparsi e dove vanno

| Oggi | Problema | Dove va |
|---|---|---|
| `Blueprints/Data/DA_CameraAnchor_Default`, `DA_GroundContact_Default` | nostri in cartella GASP | `GSTN/Data/Camera/`, `GSTN/Data/Movement/` |
| `Blueprints/Animation/DA_AnimSet_*` (5), `DA_AnimSetCollection_SandboxCharacter` | nostri in GASP | `GSTN/Animation/Layers/Data/` |
| `GASNPPTest/Combat/DA_Locomotion_Default` | dato di gioco in cartella "test"; **file locale tuo** | `GSTN/Data/Locomotion/` (solo con decisione 3) |
| `GASNPPTest/Combat/Reaction/DA_ReactionAnims` + `GSTN/Animation/Reaction/DA_ReactionAnims_{HitAdd,HitArms}` | stesso tipo in due posti | `GSTN/Combat/Reactions/Data/` |
| `GAS/Backpacks/DA_Backpack_*`, `GASNPPTest/Combat/BPK_Test_JetPack` | zaini in due posti | `GSTN/Equipment/Backpacks/` (gioco), fixture in `_Test` |
| `KIT_Test_Overlord*` (9), `WPN_Test_Overlord*` (9) | kit dell'Overlord mischiati alle fixture | `GSTN/Characters/Overlord/Kit/` (+`Variants/`) |
| `GASNPPTest/Combat/HBS_*` (6) | hurtbox | `GSTN/Combat/Hurtboxes/` (`HBS_FromPhysics`), le altre in `_Test` |
| `GSTN_CombatRules_TestParts`, `PM_Test_*`, `PS_Test_Pawn`, `SIS_Test_Code`, `TL_Test_*` | fixture | `GSTN/_Test/Combat/` |
| `GASNPPTest/Stance/DA_Stance_TestRelaxed` | fixture | `GSTN/_Test/Stances/` |
| Chooser `GSTN/Animation/Locomotion/CHT_*` (3) e `GSTN/Locomotion/StockBase/CHT_*` (2) | locomotion in due cartelle | tutto in `GSTN/Animation/Locomotion/` (`StockBase/` sotto). `CHT_*_PoseMatch` è file locale tuo: solo con te presente |
| `GASNPPTest/Input/*` + `GAS/Input/IMC_GAS_Abilities` | input in due radici | `GSTN/Input/` (`Actions/`, `Contexts/`, `Commands/`) |

## 3. Struttura proposta

```
Content/
  Blueprints/ Input/ Levels/ ...   GASP: NON si tocca (solo si tolgono i nostri 55 file)
  GSTN/                            TUTTO il nostro
    Core/            personaggio giocabile, game mode, player controller
    Characters/<Kit>/  Kit/ (KIT_, WPN_, SUP_), Moves/<Mossa>/ (AM_, AS_, GA_, TL_ insieme), Variants/, Animation/
    Animation/       Locomotion/ (ABP, chooser, StockBase/), Layers/ (ABP_GSTN_*Layer, ALI_, Data/), RunStart/, RunStop/, Shared/ (mani, crouch, Mixamo/)
    Combat/          Reactions/ (AM_/AS_ + Data/), Hurtboxes/, Rules/
    Data/            DA globali: Locomotion/, Camera/, Movement/
    Input/           Actions/, Contexts/, Commands/
    Equipment/       Backpacks/, Placeholders/
    Cosmetics/       Skins/<Skin>/, Styles/, Attachments/   (vuota, per il lavoro skin)
    UI/  Materials/  DepthMarker/
    Maps/            L_Test_Hub (+Hub/Materials), mappe di gioco
    _Test/           fixture dei test automatici: Combat/{Kits,Weapons,Archetypes,...}, Maps/ (L_Test_Combat, L_WallSlideAB, DefaultLevel_Harness)
    _Examples/       esempi per designer (Cosmetics, DoubleShot, kit di esempio)
    _Archive/        versioni superate che niente usa (non si cancella nulla)
  AnimPipeline/                    resta per ora (sorgente Live Link, 154 script la citano)
Plugins/<GSTN*>/Content/<Sistema>/ shipping del plugin, Test/ fixture, Examples/ esempi
```
Con le Game Features (D5 = A) i contenuti di una feature andranno nel suo plugin; `GSTN/` resta il "nucleo" del gioco. `GSTNCosmetics` avrà `Content/` suo per le classi base, le skin vere in `GSTN/Cosmetics`.

### Regole per i designer (10 righe)
1. Mai salvare niente dentro `Blueprints/`, `Input/`, `Levels/`: sono di Epic.
2. Tutto il nuovo va sotto `Content/GSTN/`.
3. Una mossa = una cartella `Characters/<Kit>/Moves/<Mossa>/` con AM_, AS_, GA_, TL_ insieme.
4. Le versioni vecchie di una mossa vanno in `Variants/`; quando nessuno le usa, in `_Archive/`.
5. Un DataAsset globale (valori per tutti i kit) va in `GSTN/Data/<Sistema>/`; uno di un solo kit nella cartella del kit.
6. Prefissi UE sempre: DA_, DT_, CHT_, KIT_, WPN_, AM_, AS_, GA_, TL_, MI_, M_, SM_, IA_, IMC_.
7. Niente "Test" nel nome di asset di gioco; ciò che serve solo ai test automatici va in `_Test/`.
8. Esempi da copiare stanno in `_Examples/`: copia, non modificare l'esempio.
9. Spostare un asset solo dall'editor (mai da Esplora file), poi "Fix Up Redirectors" sulla cartella.
10. Prima di spostare chiedi al coordinatore: codice e script citano i percorsi.

## 4. Rischi e piano per il thread desktop

**Rischi**
- **Percorsi scritti nel testo**: 343 file fuori da docs/ (soprattutto Tools/AnimPipeline, Tools/GAS, Tools/GASNPPTest; poi Source, plugin, Config) citano `/Game/...`. I redirector valgono solo fra asset: questi vanno riscritti con uno script dal CSV.
- **Config**: `DefaultEngine.ini` (`EditorStartupMap`/`GameDefaultMap` = `/Game/Levels/Test/L_Test_Hub`), `DefaultGame.ini` (`PrimaryAssetTypesToScan` per kit/armi/super = `/Game/GASNPPTest`: se non si aggiorna, **i kit spariscono**; `AbilitySystemMappingContexts`; input di attivazione `IA_GASNPP_*`), `DefaultInput.ini`, `DefaultGSTNDepthMarker.ini`, `Config/Tags/*`, cvar `gstn.Debug.KitAtStart=KIT_Test_Playtest` (nome, non percorso: regge).
- **Redirector**: restano finché non si fa Fix Up; se un branch salva un asset che punta al vecchio percorso dopo il fix, si rompe. Per questo un solo commit e gli altri branch rieseguono lo stesso script.
- **Branch aperti**: dal 19-11 ~38 branch `claude/*` hanno binari diversi da main in `GASNPPTest/Combat` (fino a 72), `GSTN/Animation`, `GSTN/Locomotion`, `Blueprints/Animation`, `SandboxCharacter_Mover*`. Molti sono copie vecchie: il thread li classifica (vivo / già in main / morto) e il coordinatore decide prima di partire.
- **File locali tuoi**: `DA_Locomotion_Default` e `CHT_*_PoseMatch` sono in git ma tu li modifichi in locale; la guardia (`Tools/CI/protected-paths.json`) ne blocca il commit **per nome**, quindi dopo uno spostamento resta valida, ma la tua copia locale col vecchio percorso diventerebbe un asset orfano. Vedi decisione 3.
- **Versioni approvate** (`anim-approvate.md`, tag `approved/*`): il tag punta a un commit, regge; le righe del file vanno aggiornate col nuovo percorso.
- `_ue59` è un'altra copia del progetto: va riallineata dopo (rule 7: solo l'ultimo thread della sessione).

**Piano (fisso, editor, Opus medium, ~12 $, ~3 h di desktop)**
1. Prerequisiti: coordinatore conferma i branch vivi mergiati o fermi; prende `_ue59/IN-USO.txt`; tu salvi o confermi i tuoi file locali (decisione 3). Branch `claude/content-reorg-apply` da main.
2. Backup: zip di `Content/` + `Config/` in `Saved/Backups/Reorg-<data>/` e tag git `pre-reorg-<data>`.
3. Inventario dall'editor (Asset Registry) dei file non versionati sotto le cartelle nostre; si aggiungono al CSV.
4. CSV `Tools/Reorg/content-map.csv` (vecchio percorso, nuovo, motivo) generato dalle tabelle §1-§2; review cloud del CSV (Haiku, 0,3 $) prima di applicarlo.
5. `reorg_apply.py` in editor: `EditorAssetLibrary.rename_asset` per riga, poi Fix Up Redirectors su `/Game` e plugin, poi cartelle vuote via. Idempotente.
6. `reorg_rewrite.py`: riscrive i percorsi nei file di testo (C++, ini, Tools); i documenti storici non si toccano.
7. Verifica: build (`Run-Timed.ps1 -Preset Build`), test, apertura editor senza "missing asset", kit nell'Asset Manager, `L_Test_Hub` parte, PIE harness 1 giro. Se fallisce: max 2 fix (rule 5), poi ritorno indietro.
8. Un solo commit, PR verso main, handover.

**Tornare indietro**: prima del merge si butta il branch; dopo il merge `git revert` del commit (gli asset tornano ai vecchi percorsi, il testo pure) e, sul disco, si ripristina lo zip del punto 2 se l'editor ha lasciato cartelle vuote o redirector.

## 5. Cosa NON toccare
- Tutto GASP (`Blueprints/`, `Input/`, `Levels/` salvo i 55 file nostri), compresi i 5 file GASP che abbiamo modificato.
- `DA_Locomotion_Default` e `CHT_*_PoseMatch` senza la decisione 3.
- Le versioni approvate: si spostano ma non si rinominano né si modificano.
- `AnimPipeline/` (sorgente Live Link) e i plugin `Plugins/GASNPP`, `Plugins/EngineOverrides`.
- Rinomine degli asset (`KIT_Test_Overlord` → `KIT_Overlord`, `GASNPP` → `GSTN`): fase separata (decisione 2).
- Nessuna cancellazione: le cose morte vanno in `_Archive/`.

## Decisioni per l'owner (applico la consigliata)
1. **Dove vanno test ed esempi.** **A (consigliata)** dentro `GSTN/` come `_Test/`, `_Examples/`, `_Archive/`: una sola cartella "nostra", il trattino basso le mette in cima e le separa dal gioco; costo 0. B radici separate `GSTNTests/`, `GSTNExamples/` (piano del 16-11): stessa cosa, ma tornano tre cartelle con nomi simili. Resta fermo: niente.
2. **Rinomine (via "Test" e "GASNPP" dai nomi di gioco).** **A (consigliata)** dopo, in una fase 3 a parte (~3 $): lo spostamento è già abbastanza grosso e i nomi sono citati da cvar e script. B insieme (+~3 $, più rischio di un solo giro).
3. **I tuoi file locali (`DA_Locomotion_Default`, `CHT_*_PoseMatch`).** **A (consigliata)** prima dello spostamento ti fermi un attimo sul fisso: o "ok salva" (committiamo i tuoi valori una volta) o li copio in un backup e li sposto con te presente; 5 minuti tuoi. B li lasciamo nei vecchi percorsi e li spostiamo più avanti: la cartella `GASNPPTest/Combat` resta in vita solo per loro. Resta fermo con A: il thread desktop finché non rispondi.

Risposta tipo: "1A 2A 3A".

## Comandi usati per i numeri
- conteggi: `git ls-files Content | awk -F/ ...` e `git ls-files 'Plugins/*/Content/*'`
- GASP vs nostro: `git show --name-only f1a280d5 -- Content` (267 file, import 2026-10-17) confrontato con `git ls-files Content/{Blueprints,Input,Levels}` (316) → 55 nostri
- GASP modificati da noi: `git log --diff-filter=M f1a280d5..origin/claude/project-thread-a0j7ip` sui file dell'import
- riferimenti: `git grep -l -E "/Game/(GASNPPTest|GSTN|GAS|AnimPipeline|...)" -- ':!docs'`
- branch: `git diff --name-only $(git merge-base origin/main B) B -- Content` filtrato con `git diff --quiet origin/main B`

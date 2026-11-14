# Plugin di combattimento GSTN: architettura (per designer e per la futura versione pubblica)

Stato: 2026-11-14, ramo `claude/project-thread-0tmywc`. Scritto da lettura del codice e degli handover (react-hook-1/2, launch-impacts), **non eseguito in editor**.
Questo documento è la mappa breve; la specifica completa è [docs/architecture/combat.md](../../docs/architecture/combat.md) (1187 righe), l'indice dei plugin è [Plugins/README.md](../../Plugins/README.md), e ogni plugin ha il suo README.

## 1. Moduli (livelli)

Regola: un plugin **Core** puro (niente GAS, niente Network Prediction, niente Mover), adattatori sopra. Le dipendenze vanno solo verso il basso.

| Plugin | Livello | Cosa contiene |
|---|---|---|
| `GSTNInput` / `GSTNInputNP` | Core / NP | Comandi, buffer, priorità, lock; la simulazione input dentro il modello NP; tracker dello stato Mover. |
| `GSTNCombat` | Core | Tag di combattimento, `UGSTN_CombatRules`, timeline (`FGSTN_CombatTimelineData`), hitbox/hurtbox, modelli di knockback, tabella impatti di superficie. Pura e deterministica. |
| `GSTNCombatGAS` | GAS | Tag nativi, attribute set (vita, stats, meter), GE di danno, kit/armi/super (data asset), loadout, **reazioni visive** (`UGSTN_ReactionAnimSet` + componente). Funziona anche con il GAS standard. |
| `GSTNCombatNP` | Adattatore fork | Modello NP del combattimento (`UGSTN_CombatSimComponent`), abilità base (`UGSTN_GA_Base`, `UGSTN_GA_BaseCombat`), risoluzione colpi, mosse a strati (`FGSTN_LayeredMove_Knockback`, Freeze). **Richiede il fork GAS+NPP.** |
| `GSTNDefence` (Core/GAS/NP) | Estensione | Scudo, parry, tech, **atterraggio dal tumble, rimbalzo, knockdown, rialzata/rotolata**, DI/SDI. |
| `GSTNCombatEditor`, `GSTNCombatDebug` | Editor | Editor timeline, bake socket, validatori; debug. |
| Altri (Link, Parts, Projectile, Pickups, KitMods, Backpack, Minion...) | Estensioni opzionali | Si agganciano alla pipeline dei colpi senza modificarla. |

## 2. Classi principali e data asset

- **`UGSTN_CombatRules`** (data asset, uno per modalità): catena di passi di risoluzione colpo (`UGSTN_HitResolutionStep`, istanziati), modello di knockback, tabella impatti, team/friendly fire. Esempio: `GSTN_CombatRules_Standard`.
- **`UGSTN_KnockbackModel`** (sostituibile): Weight Driven (con `PercentGrowth`), Percent Scaling, Capped, Fixed. Decide forza di lancio e hitstun.
- **`UGSTN_SurfaceImpactTable`**: cosa succede quando un lanciato tocca una superficie (`Splat`, `Bounce`, `BreakSurface`); l'attacco può forzare (`FGSTN_ImpactProfile.Force`).
- **Kit** (`UGSTN_PrimaryKitDefinition`), **arma** (`UGSTN_WeaponDefinition`), **super**: tabelle comando -> abilità. Le mosse sono sottoclassi Blueprint di `UGSTN_GA_BaseCombat` con la timeline compilata nel kit.
- **`UGSTN_CombatTimeline`** / timeline inline: tracce (Hitbox, HurtboxModifier, CancelWindow, Movement, Tags, Event, Charge). Gli hitbox usano `BakedSocket`: il socket è campionato in editor, a runtime nessuna animazione è valutata (deterministico, anche su server dedicato).
- **`FGSTN_DefenceKitExtension`** nel kit: `FGSTN_GetupSet` (Neutral, Roll, RollBack), `FGSTN_TechSet`, scudo.
- **`UGSTN_ReactionAnimSet`** (data asset visivo): vedi 4.

## 3. Flusso GAS + simulazione (lancio, knockback, knockdown)

Tick fisso 62.5 Hz. Per ogni entità, ogni frame F: **modello combat (47) -> ASC (48) -> Mover (50)**; in rollback i tre vengono ripristinati e rieseguiti in quest'ordine.

1. **Colpo.** La hitbox dell'attaccante (query su snapshot di fine F-1) deposita una voce nella **inbox** della vittima, con timestamp. La vittima la consuma al frame successivo (reazione sempre a F+1).
2. **Risoluzione.** Catena di `UGSTN_HitResolutionStep`: team -> intangibile -> parry -> scudo -> armatura -> clank -> *apply* (GE danno con `UGSTN_DamageExecution`, evento `Combat.Event.Hit.Received`) -> hitstop.
3. **Scelta della reazione** (`EGSTN_HitReaction`): `StaggerInPlace`, `PushBack` (scivolata a terra) o `Launch`. Il modello di knockback dà la forza; sotto `LaunchThreshold` un colpo a terra è un PushBack. Un PushBack senza terreno diventa Launch.
4. **Hitstop.** Congela timeline, clock, montage (pausa) e Mover (`FGSTN_LayeredMove_Freeze` via GE).
5. **Lancio.** Dopo l'hitstop, `FGSTN_LayeredMove_Knockback` (accodato con un GE con `UApplyMoveGameplayEffectComponent`, **mai** scrivendo lo stato del personaggio): velocità iniziale, decadimento, gravità scalata, forza Falling. Dopo DI/SDI e resistenza. Stesso movimento sonda i contatti: scrive la faccia toccata (muro/pavimento/soffitto) che il combat tick legge al frame dopo.
6. **Impatto.** Contatto con muro: la tabella impatti decide `Splat` (vittima inchiodata N frame) o `Bounce`. Contatto con il pavimento: `FGSTN_TumbleLanding` (GSTNDefenceNP) chiude il tumble.
7. **Knockdown (Defence F8).** Atterraggio dal tumble -> mini rimbalzo (`UGSTN_GA_GroundBounce`, avviata Forced) -> stato a terra `Combat.State.Knockdown` (`UGSTN_GE_DownedLock`). Da terra: stick tenuto = `UGSTN_GA_GetupRoll` (o `RollBack` se lo stick è verso la testa), salto (A) = `UGSTN_GA_GetupNeutral` (comando `Command.Movement.Jump` che richiede `Knockdown`); tech opzionale (`UGSTN_GA_Tech`). Auto-knockdown (dropkick): traccia Tags `Combat.Move.KnockdownOnLanding`.
8. **Stati come tag.** Il combat tick proietta `Combat.State.Hitstun`, `.Tumble`, `.Splat`, `.Knockdown`, `.Teching`, `Combat.Lock.Reaction` ecc. come conteggi assoluti sull'ASC (mai concessi da altri; niente trigger su questi tag). Le abilità li leggono solo in `ActivationBlockedTags`.

Regola d'oro: il combat tick **scrive solo il proprio stato di sync**; ogni cosa per ASC/Mover è una richiesta con timestamp (`RequestFrame == F`), eseguita una sola volta anche in re-simulazione. Tutti i timer sono frame, non secondi.

## 4. Animazione: solo visiva, separata dalla simulazione

- **Mosse del giocatore:** il montage della timeline è seguito dalla timeline (pausa in hitstop); la verità è il frame della timeline.
- **Reazioni della vittima** (dal 2026-11-14): `UGSTN_ReactionAnimComponent` (su `SandboxCharacter_Mover`) legge **i tag dell'ASC ogni frame**, solo su macchine che disegnano (mai server dedicato), e riproduce sullo slot `DefaultSlot` l'animazione scelta da `UGSTN_ReactionAnimSet`. Non scrive mai stato di simulazione; senza clip semplicemente nessuna posa di reazione.
- Una **regola** (`FGSTN_ReactionAnimRule`): `RequiredTags` (tutti) + `BlockedTags` (nessuno), `Priority` (vince la più alta: Splat > Hitstun), `Animation`, `ThenAnimation` opzionale (concatenata con cross-fade, es. GroundHit -> LieUp), `EndBehaviour` (HoldLastPose / Loop / BlendOut), `PlayRate`, `BlendInFrames` (regola del proprietario: 0-1, il primo frame deve cambiare chiaramente), `ChainBlendFrames`, blend out. Cede il passo ai montage delle timeline.
- Contenuto: `Tools/GAS/setup_reaction_anims.py` (headless) scrive `DA_ReactionAnims` e il componente. Limiti noti: crouch dopo la rialzata (causa nell'ABP, non in git), capsule/ECB che si riduce non fatto.

## 5. Come aggiungere (passi per il designer)

**Una mossa** (oggi: duplicare un esempio)
1. Duplica `GA_Example_Light` (o Strong/Special) in `Content/GASNPPTest/Examples` (kit e armi devono stare sotto `/Game/GASNPPTest`).
2. Nella timeline: lunghezza, traccia Hitbox (forma, `Space = BakedSocket`, socket, anteprima mesh + montage, **Bake Sockets** Ctrl+B), danno, angolo, knockback base/crescita, `Impact.Force` se serve (Bounce/Splat).
3. Aggiungi la mossa al kit/arma (`WPN_Example`, `KIT_Example`): slot comando (X leggera, RT forte, Y speciale) e contesto (Neutral/Directional/aria).
4. Tag di identità in `Config/Tags/*.ini` (`Kit.*`, `Weapon.*`).
5. Provala su `L_Test_Hub` (pad kit). Guida con esempi: artifact "Mosse di esempio GSTN".

**Una reazione visiva** (solo dati)
1. Apri `DA_ReactionAnims`, aggiungi una regola: tag richiesti (es. `Combat.State.Tumble`), clip, priorità, fine (Hold/Loop/BlendOut).
2. Per concatenare (impatto -> a terra) compila `ThenAnimation`. Imposta `BlendInFrames` 0-1.
3. Nessun codice, nessun Blueprint da cablare.

**Cambiare come si vola** (dati): `GSTN_CombatRules_Standard` > Knockback Model (`PercentGrowth`, soglie), `GSTN_SurfaceImpacts_Standard` (velocità minima, restituzione, frame di splat), regole Defence (rimbalzo, rotolata `TravelDistanceCm`/`TravelKeys`), `bTurnBackAlongKnockback`.

## 6. Cosa è ancora solo C++ o specifico del progetto (per renderlo pubblico e solo-BP)

Oggi un designer *può* fare con soli dati: mosse da duplicati, kit, reazioni visive, regole. Non può ancora, o dipende dal progetto:
1. **Fork GAS+NPP obbligatorio** per `GSTNCombatNP` (e NP di Defence/Link/Input): il plugin non è usabile con motore standard. Serve un percorso `GSTNCombatStockGAS` (previsto, non fatto) o pubblicare il fork con `PATCHES.md`.
2. **Creazione di mosse solo da script Python/duplicati**: non c'è una procedura guidata o un asset "Move" creabile da Content Browser (factory); scrivere la timeline in BP è possibile ma l'editor timeline/hitbox ha UX ancora in coda.
3. **Nuovi passi di risoluzione colpo, tipi di link, fragment di sync**: classi C++ (i passi sono Blueprintable in teoria, ma i fragment di stato NP, le tick extension e i `LayeredMove` sono solo C++). Il codice per frame caldi deve restare C++ per scelta.
4. **Contenuto e ABP del progetto**: lo slot `DefaultSlot`, `SandboxCharacter_Mover`, ABP (non in git), skeleton GASP, hurtbox che dipendono dal motion matching. Il componente reazioni va montato dallo script sul BP condiviso. Un ABP o skeleton diverso richiede lavoro.
5. **Input e Mover del progetto**: i comandi standard (`Command.*`), il tracker Mover e le modalità muro (`GASP_Giustino2`) sono nel modulo di progetto, non in un plugin; Defence legge la rotolata dalla faccia/stick del progetto.
6. **Tag e config**: tag di progetto in `Config/Tags/*.ini` e `DefaultGame.ini` (Asset Manager, contesti di input) vanno forniti come setup; manca un "installer" o Game Feature.
7. **Contenuto di esempio**: `Plugins/**/Content/` è ignorato da git; gli esempi stanno in `Content/GASNPPTest` e nei dati creati da script. Per un plugin pubblico servono esempi dentro il plugin.
8. **Ancora da progettare**: ECB/capsula che si riduce, reazioni direzionali (da dietro/di lato), clip GroundHit dall'altezza del tumble, fix crouch ABP, UI loadout e tag in italiano/inglese, README unico di installazione, test automatici che non dipendano dalla mappa hub.
9. **Dedicated server**: il modello di rete è listen server; reazioni visive e replica sono testate solo così.

Priorità suggerita per renderlo pubblico: (a) decisione sul fork (1), (b) factory/asset "Move" e wizard (2), (c) esempi e tag dentro il plugin (6, 7), (d) pulizia dipendenze dal progetto (4, 5).

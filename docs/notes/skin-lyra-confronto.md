# Lyra e le nostre skin: cosa prendere, cosa no (ricerca, 2026-11-23)

Cloud, Opus medium, sola lettura. Lyra **non è** nel PC (cartella Unreal Projects: solo GASP e worktree), quindi il confronto usa
il codice Lyra noto (UE 5.x) e le note pubbliche [x157 Character Parts](https://x157.github.io/UE5/LyraStarterGame/CharacterParts).
Dove scrivo **[da verificare]** va controllato su un Lyra scaricato (gratis dal launcher, ~solo lettura) prima di copiare codice.
Confrontato con `skin-cosmetici-modulari.md` (branch `claude/skin-cosmetici-ricerca`) e `skin-proporzioni-analisi.md` (`claude/project-thread-4pj7kf`).

## Come funziona in Lyra (in breve)
- **`ULyraControllerComponent_CharacterParts`** (sul Controller): la lista delle parti scelte dal giocatore; sopravvive alla morte
  e al respawn e le rimette sul nuovo pawn.
- **`ULyraPawnComponent_CharacterParts`** (sul pawn): lista replicata (FastArray) di voci `{classe Actor, socket, modo collisione}`
  con un handle. Il server la scrive; **ogni macchina** crea la parte in locale nei callback di replica (un `ChildActorComponent`
  attaccato al socket, non replicato). Collisione di default: nessuna.
- **Tag cosmetici**: dopo ogni cambio raccoglie i tag delle parti (es. corpo femminile/maschile) e con
  `FLyraAnimBodyStyleSelectionSet` sceglie **la mesh del corpo invisibile** che guida l'animazione (`SKM_Manny_Invis` / Quinn):
  il corpo visibile segue quello invisibile, quindi le proporzioni dei due devono combaciare.
- **`FLyraAnimLayerSelectionSet`**: gli stessi tag scelgono il layer di animazione dell'arma (varianti per corporatura).
- **Cheat di sviluppo** (`ULyraCosmeticDeveloperSettings`): forza parti in PIE senza toccare i dati.
- Niente armadietto, niente cache, niente costruzione asincrona, niente placeholder: le parti sono asset fatti a mano, caricati e attaccati.

## Cosa abbiamo già (e Lyra non aggiunge)
| Idea Lyra | Da noi oggi |
|---|---|
| Scelta salvata fuori dal pawn | Loadout sul PlayerState (`UGSTN_LoadoutComponent`), replicato come ID |
| Parti create in locale, mai sul server dedicato | `UGSTN_SkinComponent`: solo presentazione, nessun server dedicato, caricamento soft e **asincrono** con scambio "tutto insieme" (meglio di Lyra) |
| Parti su socket | `FGSTN_EquipmentVisual` + provider (zaini, mod), con fallback di socket e "segui la mesh visibile" |
| Layer di animazione scelti da tag | Già fatto "alla Lyra" in `GSTNAnimation` (anim-layers.md: `SkinTags` + `Anim.Trait.*`) |
| Corpo invisibile che guida quello visibile | GASP "VisualOverride" (attore figlio che copia la posa del manichino nascosto) |

## Cosa vale la pena prendere
1. **Lista di parti con handle** (aggiungi/togli una parte e ottieni un handle): utile per parti **temporanee** (forma della skin
   reattiva, power-up visivo), che oggi il loadout non esprime. Da mettere nel `UGSTN_SkinComponent`, replicando solo ID.
2. **Tag raccolti dalle parti**: un pezzo del tema o un'armatura può portare un tag (`Anim.Trait.Heavy`) che sceglie la variante
   visiva della mossa; si aggancia a `SetCosmeticTags` che esiste già.
3. **Collisione esplicita per parte**, default nessuna: è già la nostra regola, ma Lyra la rende un campo controllato. Va nel validatore.
4. **Forzare skin/tema da debug**: come i cheat Lyra, ma nel menu GSTN Debug (regola 15c), per le catture di prova delle proporzioni.

## Cosa non va bene per noi
- **Cambiare la mesh invisibile per corpo** (il trucco Manny/Quinn): da noi lo scheletro guida hitbox cotte, socket di presa e
  bake, ed è il manichino base (BirdKnight). Va tenuto **fisso**; le proporzioni restano nella posa di riferimento della skin
  (opzione A dell'analisi), con i limiti di tolleranza. Lyra non ha limiti di proporzione: non ne ha bisogno, ha due corpi.
- **`ChildActorComponent` per parte**: un attore per pezzo è pesante; con l'assemblatore vogliamo una mesh fusa per oggetto.
- **Costruzione dell'oggetto decorato, cache armadietto, coda asincrona dei giocatori remoti, effetto amorfo**: Lyra non ha niente
  di simile. È il 70-80% del costo della prova, e resta tutto nostro.
- **Riferimenti "hard"** alle classi delle parti: noi teniamo tutto soft e caricato a richiesta.

## Licenza
Lyra è distribuito con la EULA di Unreal Engine: codice e contenuti si possono usare e modificare in un progetto **fatto con
Unreal**, anche commerciale, con le royalty normali della EULA; non si possono usare fuori da Unreal né rivendere come asset a sé
([EULA](https://www.unrealengine.com/en-US/eula), [forum Epic](https://forums.unrealengine.com/t/can-i-use-lyra-game-assets-for-my-game/737723)).
Per noi: copiare è lecito. Se copiamo un file, si tiene l'intestazione "Copyright Epic Games" e si rinomina in `GSTN_`; meglio
riscrivere seguendo l'idea, perché le nostre classi sono già diverse. **[da verificare]** il testo EULA della versione in uso, non è una consulenza legale.

## Cosa cambia nella stima della prova (16-26 $)
Quasi nulla: **16-25 $**. Lyra non tocca l'assemblatore né la cache; risparmia forse 1 $ in F1 perché la lista di parti e la
raccolta di tag si scrivono sul modello Lyra dentro il `UGSTN_SkinComponent` esistente invece di un componente nuovo. Il debug
"forza skin/tema" costa ~0,5-1 $ e si paga da solo nelle catture. La migrazione proporzioni (16-24 $) non cambia con 1A.

## Decisioni per l'owner: **1A 2A 3A CONFERMATE dall'owner il 2026-11-23** (corpo invisibile fisso, parti con handle in F3, idea riscritta non copia)
**1. Corpo invisibile che guida l'animazione**
- **A (consigliata)**: uno solo, fisso (manichino base); proporzioni nella skin entro i limiti. Costo 0, hitbox e prese sicure.
- B: come Lyra, un corpo invisibile per corporatura (3 corpi). Piedi e mani più precisi su skin lontane dalla base, ma ogni bake e
  presa va rifatto per corpo: +4-6 $ e più revisioni d'animazione. Da tenere come uscita se la cattura dei corpi di prova va male.

**2. Parti temporanee con handle (stile Lyra)**
- **A (consigliata)**: si aggiungono in F3, quando arrivano skin reattive e stili. Nessun costo ora.
- B: subito in F1 (+1-2 $), anche se nessuna feature le usa ancora (contro la regola "niente lavoro speculativo").

**3. Come usiamo il codice Lyra**
- **A (consigliata)**: solo l'idea, riscritta nelle nostre classi `GSTN_` con un commento "pattern da Lyra"; prima di F1 un thread
  Haiku legge i 4 file Lyra veri (~0,5 $) per confermare i punti [da verificare].
- B: copiare i file Lyra e adattarli (lecito, ma ci porta `ChildActorComponent` e riferimenti hard da togliere).

COSTO: ~1,5 $ | 0 min desktop | 0 token review

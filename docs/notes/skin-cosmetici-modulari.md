# Skin cosmetiche modulari: temi 3D applicabili a ogni arma e oggetto (ricerca)

Data 2026-11-23. Solo ricerca e design, **nessuna implementazione**. Thread cloud, branch `claude/skin-cosmetici-ricerca`.
Fonti: conoscenza dei giochi citati e di Unreal fino alle versioni 5.6/5.7. Dove scrivo **[da verificare 5.9]** è dedotto,
non controllato su 5.9: va chiesto all'Unreal Assistant (`Tools/UE59/ask_unreal_assistant.py`) prima di scrivere codice.

## Riassunto (10 righe)

1. Le camo di Fortnite scalano perché sono **solo materiale**; le skin 3D (Apex, Valorant, Destiny ornaments) non scalano perché il costo è **temi × armi**: un artista per ogni coppia.
2. Il trucco per avere il 3D che scala è far diventare il costo **temi + armi**: ogni arma descrive *dove* si può decorare (slot, spline, maschere), ogni tema descrive *come* decora (pezzi, moduli lungo spline, materiale). Un assemblatore unisce le due cose da solo.
3. L'esempio più vicino nei giochi veri è Warframe: palette colore a canali uguale per tutto e accessori (syandana, armature) su punti di attacco standard di ogni Warframe. Noi lo estendiamo alle armi.
4. Proposta: plugin opzionale `GSTNCosmetics` (spento di default), dati in Data Asset, esposto a BP, editor tool di validazione e di "bake".
5. Ogni arma base riceve, **dentro Unreal, senza Blender**: socket con nome di slot, 1-4 spline decorative, 4 canali di maschera vertex color, UV1 per trim sheet, pivot e scala standard. 30-60 minuti per arma.
6. Ogni tema = 1 materiale a strati + 1 trim sheet + 8-15 pezzi modulari + 2-4 moduli da spline + profilo reattivo. Funziona su ogni arma conforme, con fallback a gradini (materiale sempre, pezzi solo dove c'è lo slot).
7. Stili alla Fortnite = canali di opzioni (colore, parti, emissivo, reattività) dentro il tema o la skin del kit.
8. Skin reattive = componente solo cosmetico che legge tag GAS, attributi e GameplayCue e guida parametri di materiale (Custom Primitive Data) e Niagara. Mai dentro la simulazione NP.
9. La cosmetica non tocca hitbox, hurtbox, animazioni: niente collisione e un **involucro di silhouette** per slot, così un orpello non "mente" sulla portata dell'arma.
10. Prima prova (non iniziata): 1 tema, 2 armi molto diverse, poi un 2° tema e una 3a arma senza lavoro dedicato per dimostrare la scala. Stima 15-24 $ in tutto.

## Decisioni per l'owner (max 3)

**1. Approccio di base**
- **A (consigliata)**: grammatica di slot + spline + materiale a strati, assemblata in automatico. Costo arte ~temi + armi. Prova F0-F2 ~15-24 $.
- B: solo materiale e maschere alla Fortnite, più charm su socket. Economico (~5 $), ma niente 3D vero sull'arma.
- C: skin 3D fatte a mano per coppia arma/tema. Nessun codice, ma è esattamente ciò che non vuoi (non scala).

**2. Quando si assembla la mesh decorata**
- **A (consigliata)**: in gioco con componenti per il prototipo, poi "bake" in editor/cook in una mesh unica per coppia arma/tema quando i temi sono più di 2. Costo runtime finale uguale a una skin fatta a mano.
- B: solo bake da subito. Più veloce a runtime, iterazione più lenta sul prototipo.
- C: solo runtime. Semplice, ma 5-20 componenti in più per arma: costo in draw call con molti personaggi.

**3. Nomi dei temi**
- **A (consigliata)**: Orokin, Dax, Sentient restano solo riferimenti interni; i temi del gioco hanno nomi nostri (sono marchi Digital Extremes / Warframe).
- B: usarli come nomi provvisori anche negli asset, rinominando prima di qualsiasi pubblicazione.

Fermo finché non decidi: nulla. È ricerca; F1 parte solo quando lo dici (vedi coda: dopo la skill di animazione).

---

## 1. Come fanno i giochi veri e dove si rompe la scala

| Gioco | Cosa è universale | Cosa è ad hoc | Perché scala o no |
|---|---|---|---|
| Fortnite (wraps/camo) | Materiale applicato a ogni arma e veicolo tramite UV e maschere preparate sulla mesh base | Niente 3D | Scala perfettamente: ogni arma ha regole UV/maschera comuni. Il limite è che è piatto |
| Fortnite (outfit, stili, reattive) | Sistema di "styles": canali che cambiano materiale, parti mesh, particelle; skin reattive guidate da eventi di gioco | Ogni outfit è modellato a mano | Il sistema di stili è generico, il contenuto no |
| Warframe | **Palette colore a 4+ canali** (primario, secondario, terziario, accenti, emissivo, energia) uguale per tutto; **accessori su punti di attacco standard** (syandana, armature spalle/petto/gambe) con offset regolabile | Skin delle armi (Tennogen) fatte per singola arma | La parte "accessori su slot standard" è la prova che il 3D modulare funziona su corpi diversi |
| Destiny 2 | Shader: materiale universale con canali di tinta per ogni oggetto | Ornaments: geometria per singola arma/armatura | Shader scala, ornament no (venduti per pezzo) |
| Apex, Valorant, CoD | Charm/ciondoli su un socket standard per ogni arma; camo di maestria (CoD) solo materiale | Skin leggendarie e "reactive" per singola arma (Apex reactive cambia con le uccisioni) | Il charm scala (un socket), la skin no |
| Halo Infinite | Coatings: materiale universale per armatura/arma | Modelli arma e kit armatura | Come Destiny |
| Lyra (sample Epic) | `ULyraPawnComponent_CharacterParts`: parti = Actor attaccati a socket, con tag cosmetici che scelgono anche il layer di animazione | Le parti sono asset singoli | Ottima base tecnica per "parti su socket" replicate; non risolve la scala tema × arma |

**Il limite di scala**: appena la geometria del tema deve "abbracciare" la forma specifica di un'arma (una guardia, un dorso
della lama) qualcuno la modella su quell'arma. Nessuno dei giochi sopra ha un sistema che *adatta* pezzi di tema a forme
diverse, perché il loro obiettivo commerciale è vendere la coppia. Il nostro obiettivo è il contrario: coerenza di tutto
l'equipaggiamento con un solo tema. È fattibile se l'arma base dichiara **dove e quanto spazio** c'è, e il tema porta
**pezzi che si adattano** (scalano, si ripetono lungo una linea, si deformano lungo una spline) invece di pezzi su misura.

## 2. Opzioni tecniche in UE per la geometria per tema

Costo artistico "per arma" = quello che serve su ogni nuova arma; "per tema" = una volta per tema.

| Tecnica | Cosa fa | Pro | Contro | Arte per arma | Arte per tema | Runtime |
|---|---|---|---|---|---|---|
| **Socket semantici** (slot) sulla mesh base | Pezzo del tema attaccato a un punto con nome e tag (pomolo, guardia, base lama, punta, fodero) | Semplicissimo, fatto nell'editor Static Mesh, nessun DCC | Il pezzo non segue la forma: va bene per tappi, gemme, anelli, corna | 5-10 socket, 10-15 min | 1 pezzo per classe di slot | 1 componente per pezzo (o bake) |
| **Spline + mesh lungo spline** (`USplineMeshComponent`) | Un modulo (filigrana, cresta, bordo) ripetuto e deformato lungo una curva che segue un bordo dell'arma | È la parte "3D che segue la forma": stesso modulo su spade, martelli, scudi | Le curve vanno tracciate; deformazione brutta su curve strette | 1-4 spline, 15-30 min | 2-4 moduli piastrellabili | 1 componente per segmento: va fatto bake |
| **Kit modulare per tema con regole di adattamento** | Stesso concetto dei socket, ma ogni pezzo ha regole (scala uniforme/lungo asse, min/max, specchiatura, ripetizione) | Copre il 70-80% dei casi senza toccare l'arma | Le regole vanno scritte bene una volta | Incluso negli slot | 8-15 pezzi | Come socket |
| **PCG** (editor) | Sparge pezzi sulla superficie (borchie, cristalli, spine) campionando la mesh e una maschera | Ottimo per temi "organici" (Sentient-like: escrescenze) | Va usato **in editor** per il bake; a runtime è costoso e non deterministico tra macchine se non fissato il seed | Solo la maschera | 1 grafo PCG per tema | Zero se fatto bake |
| **Geometry Script** (Dynamic Mesh, editor) | Unisce, deforma lungo spline, fa booleane, saldature e il bake finale in una Static Mesh | È il motore dell'"assemblatore"; tutto in Unreal | Solo editor per uso serio; API cambia tra versioni **[da verificare 5.9]** | 0 | 0 | 0 (produce asset) |
| **Nanite displacement / tessellation** | Rilievi (incisioni, scaglie) da texture di displacement | Rilievo vero senza modellare | Sperimentale nelle ultime versioni note **[da verificare 5.9]**; con il nostro rendering (deferred, no Lumen, TSR) va provato; non cambia la silhouette in modo leggibile | 0 (serve UV pulito) | 1-2 mappe | Medio |
| **Material layers + maschere vertex color** | Livello "camo": tinta, metallo, emissivo, trim, sporco per canali | Base universale e sempre disponibile (fallback) | Non è 3D | Pittura vertex color 10-15 min (Mesh Paint in UE) | 1 material layer + palette | Basso |
| **Decal/trim sheet su UV1** | Bordini, rune, incisioni seguono gli spigoli | Fa sembrare "su misura" un tema generico | UV1 da preparare | Incluso | 1 trim sheet | Basso |
| **Morph target / mesh deformer** | Deforma la mesh base (lama più seghettata, guardia più larga) | Cambia la forma dell'arma stessa | Ogni morph va fatto per arma: **non scala**; da evitare per i temi, ok per gli "stili" di un singolo kit | Alto | — | Basso |
| **Chaos Cloth / AnimDynamics / ossa fisiche** | Nastri, nappe, catene, mantelli d'arma | Molto "vivo" | Solo cosmetico lato client; il pezzo dev'essere skeletal | 0 (usa slot) | Per pezzo | Medio |

Conclusione tecnica: **socket + spline + material layer** sono le tre gambe; PCG e Geometry Script servono in editor per
l'assemblaggio e il bake; displacement è un bonus da verificare; i morph restano per gli stili di un kit, non per i temi.

## 3. Convenzioni sul modello base di ogni arma/oggetto

Tutto si fa **dentro Unreal** (Static Mesh Editor per i socket, Modeling Mode per UV e pivot, Mesh Paint per i vertex color,
uno strumento editor nostro per le spline). Nessun passo richiede Blender. Gli strumenti di Unreal citati esistono nelle
versioni recenti; la loro forma esatta in 5.9 è **[da verificare 5.9]**.

**Regole**
1. **Pivot e assi**: pivot sul punto d'impugnatura principale; X = direzione della lama/canna, Z = dorso. Scala 1 unità = 1 cm. Così un pezzo "punta" sa sempre dove guardare.
2. **Slot (socket)**: nome `SLOT_<Classe>_<n>` (es. `SLOT_Pommel_0`, `SLOT_Guard_L_0`, `SLOT_BladeBase_0`, `SLOT_Tip_0`, `SLOT_Charm_0`). La classe mappa su un tag nativo `Cosmetic.Slot.*` gerarchico (`Cosmetic.Slot.Grip.Pommel`), così il fallback sale al padre.
3. **Involucro dello slot**: ogni slot dichiara una scatola (dimensioni massime in cm) e una direzione libera. Il pezzo del tema viene scalato per starci dentro: è la regola che rende un pezzo valido su armi diverse.
4. **Spline decorative**: 1-4 curve per arma, classi `Spine` (dorso), `Edge` (filo, solo se il tema lo vuole), `Band` (giro attorno al manico), `Rim` (bordo di scudo). Salvate come dati nell'asset host (punti in spazio mesh), tracciate con lo strumento editor sopra la mesh.
5. **Vertex color**: R = metallo/struttura primaria, G = secondaria, B = dettagli/accenti, A = zone emissive o "energia". Stesso significato su ogni oggetto: è la palette alla Warframe.
6. **UV**: UV0 per le texture dell'arma, **UV1 per il trim sheet** (strisce allineate agli spigoli lunghi), densità texel comune.
7. **Zone vietate**: area di presa della mano e parte che deve leggere la portata (filo della lama, punta) marcate come "no ornament" (slot assenti + maschera) per leggibilità del gameplay.
8. **Silhouette**: la decorazione non può allungare l'oggetto oltre +5% in direzione X né allargarlo oltre +15% (valori di partenza, da tarare con l'owner): l'hitbox resta la verità.
9. **Livello di supporto** calcolato dal validatore: T0 materiale, T1 + slot, T2 + spline, T3 + PCG/rilievo. Un oggetto vecchio o di terzi è sempre almeno T0.

**Checklist designer (nuova arma)**
1. Importa la mesh, metti pivot all'impugnatura e asse X sulla lama (Modeling Mode).
2. Aggiungi i socket `SLOT_*` che hanno senso (minimo `Pommel`, `BladeBase` o equivalente, `Charm`).
3. Per ogni socket imposta l'involucro nell'asset host.
4. Traccia 1-2 spline (dorso, fascia del manico) con lo strumento.
5. Dipingi i 4 canali vertex color (Mesh Paint).
6. Controlla UV1 per il trim.
7. Lancia il validatore: deve dire T2 o più e "0 violazioni di silhouette".
8. Guarda l'anteprima con 2 temi già esistenti. Se un pezzo è brutto, si corregge lo slot dell'arma, mai il pezzo del tema.

## 4. Skin reattive e stili

**Ingressi** (tutti letti lato presentazione, interpolati): tag GAS presenti (`State.Combo.3`, kit swap, super pronta),
attributi (vita, carica della super, contatore combo), GameplayCue (colpo dato/subito, parata, KO), eventi di partita.
**Uscite**: parametri scalari/colore per oggetto via **Custom Primitive Data** o Material Instance Dynamic, parametri utente
Niagara, attivazione di stili (es. la seconda forma dopo 10 colpi), suoni.
**Material Parameter Collection** è globale per tutto il mondo: va bene per cose di mondo (tempo, ora), **non** per lo stato di
un singolo personaggio; per quello Custom Primitive Data.

Modello: `FGSTN_ReactiveRule` = ingresso + curva + smussamento + uscita. Un profilo reattivo è una lista di regole nel tema o
nella skin. Il componente `UGSTN_CosmeticReactiveComponent` è solo client, non è nello stato NP, e non scrive mai su GAS:
regola 1 di CLAUDE.md rispettata (il gameplay non dipende mai dalla cosmetica).

**Stili alla Fortnite**: un `FGSTN_StyleChannel` (tag del canale + opzioni) con opzioni di tipo colore/palette, parti
(attiva/disattiva pezzi del tema o della skin), emissivo, reattività. La scelta del giocatore è una piccola mappa
canale → indice, replicata come dato di loadout.

## 5. Modello dati

- `UGSTN_CosmeticThemeData` (Primary Data Asset): tag tema (`Cosmetic.Theme.<Nome>`), material layer e palette, trim sheet,
  pezzi per classe di slot (con regole di adattamento: scala uniforme o lungo asse, min/max, specchiatura, ripetizione),
  moduli spline per classe di spline, grafo PCG opzionale, profilo reattivo, canali di stile, classi di slot richieste per
  dichiararsi "completo".
- `UGSTN_CosmeticHostData` (uno per arma/oggetto/pezzo d'equipaggiamento del kit): mesh base, slot letti dai socket con
  involucri, spline, livello di supporto, zone vietate, override facoltativi per tema (un pezzo alternativo solo dove un tema
  rende male: l'eccezione, non la regola).
- `FGSTN_CosmeticLoadout`: skin del personaggio, stile del kit, tema globale, override di tema per singolo oggetto, scelte di
  stile. Replicato come ID (Primary Asset Id + tag), mai come mesh.

**Fallback quando manca uno slot**: classe esatta → tag padre → nessun pezzo; il materiale si applica sempre. Se mancano
le spline, il tema usa solo slot e materiale. Il validatore segnala "tema X su arma Y: copertura 60%" così si sa dove
aggiungere uno slot.

**Rapporto con kit e skin del personaggio**: le skin del corpo sono usabili con qualsiasi kit (decisione del 2026-11-23) e
condividono lo scheletro unico. Questo aiuta molto: gli slot del corpo (spalle, cintura, polsi, schiena) si definiscono
**una volta sullo scheletro** come socket di osso, quindi un tema può aggiungere orpelli anche all'armatura di ogni skin,
con un offset per skin dovuto alle proporzioni (limiti in `skin-proporzioni-analisi.md` sul branch
`claude/project-thread-4pj7kf`). Le armi e gli oggetti del kit usano gli host data. Un tema applicato "a tutto il loadout"
dà la coerenza estetica che vuoi senza comprare o modellare coppie.

## 6. Integrazione con la pipeline esistente

- **Animazioni**: non cambiano. Le armi restano attaccate agli stessi socket di mano; i pezzi del tema sono figli della mesh
  dell'arma. Pezzi con fisica (nastri) sono solo cosmetici.
- **Hitbox e hurtbox**: appartengono al kit e sono cotte offline dal montage di riferimento; la cosmetica ha collisione
  `NoCollision` e non entra in nessuna query di simulazione. Il limite di silhouette (sezione 3, regola 8) esiste perché il
  giocatore deve vedere la portata vera.
- **Rete**: solo il loadout viaggia (ID). Assemblaggio e reattività sono locali. Niente nello stato NP.
- **Contenuti**: asset sotto `Content/GSTN/Cosmetics/`, GASP intatto. Plugin `GSTNCosmetics` con modulo runtime (dati,
  assemblatore, reattivo) e modulo editor (validatore come regola di `FGSTN_ValidationExtensions`, strumento spline, bake con
  Geometry Script). Opzionale e spento di default come gli altri plugin.
- **Prestazioni**: in prova componenti separati; dopo, bake in una Static Mesh per coppia (il bake è automatico, non lavoro
  d'artista), con LOD e Nanite se il profilo di rendering lo consente.

## 7. Rischi

1. **Qualità estetica dell'adattamento automatico**: è il rischio principale. Pezzi generici possono sembrare "appiccicati".
   Mitigazione: involucri, trim sheet che segue gli spigoli, override per tema solo dove serve, e la prova con 2 armi molto
   diverse prima di investire.
2. **Produzione dei pezzi dei temi senza Blender**: i pezzi sono piccoli e semplici (tappi, gemme, filigrane piastrellabili),
   più facili da generare con strumenti generativi (es. Meshy) o da Modeling Mode che un'arma intera. Restano da pulire.
3. **Strumenti 5.9** (Geometry Script, PCG, Nanite displacement): API o stato sperimentale da verificare prima di F1.
4. **Esplosione dei bake**: temi × armi asset generati; si limita facendo bake solo delle coppie usate o al cook.
5. **Leggibilità**: temi molto carichi possono confondere portata e direzione del colpo; il limite di silhouette va provato in
   gioco con la regola 10 (video guardato).
6. **IP**: Orokin, Dax, Sentient sono nomi Warframe (decisione 3).

## 8. Percorso a fasi minimo (NON iniziato)

| Fase | Cosa | Prova di successo | Stima |
|---|---|---|---|
| F0 | Verifica strumenti 5.9 con l'Unreal Assistant (Geometry Script spline deform e bake, PCG su mesh, displacement) | Risposta scritta, 1 pagina | ~1 $ |
| F1 | Plugin minimo: dati tema/host, assemblatore runtime (socket + spline + material layer), validatore. **1 tema** (oro e avorio a filigrana, nome nostro) su **2 armi** diverse (spada dell'Overlord + un oggetto largo, es. scudo o martello) | Video in gioco dietro e di lato, review cloud | codice cloud 6-9 $, build/test desktop 3-5 $, pezzi del tema 2-4 $ |
| F2 | **Prova della scala**: 2° tema sulle stesse 2 armi senza toccarle, e una 3a arma con i 2 temi senza toccare i temi | Zero lavoro "per coppia"; copertura >= T2 | 3-5 $ |
| F3 | Reattività e stili, bake al cook, slot del corpo sullo scheletro | Una skin reattiva in gioco | da stimare dopo F2 |

Totale F0-F2: circa 15-24 $. Se F2 richiede ritocchi per coppia oltre un override su dieci, l'approccio A va rivisto prima
di F3 (fermarsi e decidere con l'owner).

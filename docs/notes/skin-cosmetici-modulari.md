# Skin cosmetiche modulari: temi 3D applicabili a ogni arma e oggetto (ricerca)

Data 2026-11-23. Solo ricerca e design, **nessuna implementazione**. Thread cloud, branch `claude/skin-cosmetici-ricerca`.
Fonti: conoscenza dei giochi citati e di Unreal fino alle versioni 5.6/5.7. Dove scrivo **[da verificare 5.9]** è dedotto,
non controllato su 5.9: va chiesto all'Unreal Assistant (`Tools/UE59/ask_unreal_assistant.py`) prima di scrivere codice.

## Riassunto (10 righe)

1. Le camo di Fortnite scalano perché sono **solo materiale**; le skin 3D (Apex, Valorant, Destiny ornaments) non scalano perché il costo è **temi × armi**: un artista per ogni coppia.
2. Il trucco per avere il 3D che scala è far diventare il costo **temi + armi**: ogni arma descrive *dove* si può decorare (slot, spline, maschere), ogni tema descrive *come* decora (pezzi, moduli lungo spline, materiale). Un assemblatore unisce le due cose da solo, **a runtime**, quando il giocatore equipaggia: il tema si vende una volta come "stile di design" e va su qualsiasi oggetto idoneo, come le camo di Fortnite.
3. L'esempio più vicino nei giochi veri è Warframe: palette colore a canali uguale per tutto e accessori (syandana, armature) su punti di attacco standard di ogni Warframe. Noi lo estendiamo alle armi.
4. Proposta: plugin opzionale `GSTNCosmetics` (spento di default), dati in Data Asset, esposto a BP, validatore in editor. Su disco solo i pezzi del tema (una volta) e i dati dell'arma (pochi KB): nessuna mesh per coppia arma/tema.
5. Ogni arma base riceve, **dentro Unreal, senza Blender**: socket con nome di slot, 1-4 spline decorative, 4 canali di maschera vertex color, UV1 per trim sheet, pivot e scala standard. 30-60 minuti per arma.
6. Ogni tema = 1 materiale a strati + 1 trim sheet + 8-15 pezzi modulari + 2-4 moduli da spline + profilo reattivo. Funziona su ogni arma conforme, con fallback a gradini (materiale sempre, pezzi solo dove c'è lo slot).
7. Stili alla Fortnite = canali di opzioni (colore, parti, emissivo, reattività) dentro il tema o la skin del kit.
8. Skin reattive = componente solo cosmetico che legge tag GAS, attributi e GameplayCue e guida parametri di materiale (Custom Primitive Data) e Niagara. Mai dentro la simulazione NP.
9. La cosmetica non tocca hitbox, hurtbox, animazioni: niente collisione e un **involucro di silhouette** per slot, così un orpello non "mente" sulla portata dell'arma.
10. Prima prova (non iniziata): 1 tema, 2 armi molto diverse, poi un 2° tema e una 3a arma senza lavoro dedicato per dimostrare la scala. Stima 16-26 $ in tutto.

## Decisioni per l'owner (max 3)

**1. Approccio di base**
- **A (consigliata)**: grammatica di slot + spline + materiale a strati, assemblata in automatico. Costo arte ~temi + armi. Prova F0-F2 ~16-26 $.
- B: solo materiale e maschere alla Fortnite, più charm su socket. Economico (~5 $), ma niente 3D vero sull'arma.
- C: skin 3D fatte a mano per coppia arma/tema. Nessun codice, ma è esattamente ciò che non vuoi (non scala).

**Decise dall'owner il 2026-11-23**: 1A confermata. Assemblaggio **a runtime**: il tema si applica a qualsiasi oggetto,
quindi niente bake per coppia arma/tema; il bake resta solo come cache opzionale (sezione 2b). Orokin, Dax, Sentient erano
solo esempi di cosa si intende, non nomi da usare.

**2. Quando si costruisce l'oggetto decorato (nuova decisione, idea dell'owner sull'armadietto)**
- **A (consigliata)**: un solo assemblatore che fonde pezzi e spline in una mesh per oggetto, lanciato in due momenti fuori dalla partita: **all'armadietto** quando il giocatore salva il loadout (salvato in una cache locale su disco) e **nel caricamento della partita** per i loadout degli altri giocatori, che arrivano solo come ID. In partita zero costo di costruzione. Vedi 2c.
- B: solo armadietto. Non basta in multiplayer: i loadout degli altri non passano dal tuo armadietto, quindi servirebbe comunque la costruzione al caricamento o il download di mesh (pesante).
- C: solo runtime puro all'equipaggiamento, anche in partita. Più semplice, ma qualche ms nel momento sbagliato e nessuna cache su disco.

Fermo finché non decidi: nulla, la decisione 2 serve solo a F1. È ricerca; F1 parte solo quando lo dici (vedi coda: dopo la skill di animazione).

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
| **Socket semantici** (slot) sulla mesh base | Pezzo del tema attaccato a un punto con nome e tag (pomolo, guardia, base lama, punta, fodero) | Semplicissimo, fatto nell'editor Static Mesh, nessun DCC | Il pezzo non segue la forma: va bene per tappi, gemme, anelli, corna | 5-10 socket, 10-15 min | 1 pezzo per classe di slot | 1 componente per pezzo (o fusione, 2b) |
| **Spline + mesh lungo spline** (`USplineMeshComponent`) | Un modulo (filigrana, cresta, bordo) ripetuto e deformato lungo una curva che segue un bordo dell'arma | È la parte "3D che segue la forma": stesso modulo su spade, martelli, scudi | Le curve vanno tracciate; deformazione brutta su curve strette | 1-4 spline, 15-30 min | 2-4 moduli piastrellabili | 1 componente per segmento: va fusa (2b) |
| **Kit modulare per tema con regole di adattamento** | Stesso concetto dei socket, ma ogni pezzo ha regole (scala uniforme/lungo asse, min/max, specchiatura, ripetizione) | Copre il 70-80% dei casi senza toccare l'arma | Le regole vanno scritte bene una volta | Incluso negli slot | 8-15 pezzi | Come socket |
| **PCG** (editor) | Sparge pezzi sulla superficie (borchie, cristalli, spine) campionando la mesh e una maschera | Ottimo per temi "organici" (Sentient-like: escrescenze) | Va usato **in editor** per creare i pezzi del tema; a runtime è costoso e non deterministico tra macchine se non fissato il seed | Solo la maschera | 1 grafo PCG per tema | Zero (lavora in editor) |
| **Geometry Script** (Dynamic Mesh) | Unisce, deforma lungo spline, fa booleane e saldature | È il motore dell'"assemblatore"; tutto in Unreal | Uso a runtime e API da verificare **[da verificare 5.9]** | 0 | 0 | Pochi ms all'equipaggiamento |
| **Nanite displacement / tessellation** | Rilievi (incisioni, scaglie) da texture di displacement | Rilievo vero senza modellare | Sperimentale nelle ultime versioni note **[da verificare 5.9]**; con il nostro rendering (deferred, no Lumen, TSR) va provato; non cambia la silhouette in modo leggibile | 0 (serve UV pulito) | 1-2 mappe | Medio |
| **Material layers + maschere vertex color** | Livello "camo": tinta, metallo, emissivo, trim, sporco per canali | Base universale e sempre disponibile (fallback) | Non è 3D | Pittura vertex color 10-15 min (Mesh Paint in UE) | 1 material layer + palette | Basso |
| **Decal/trim sheet su UV1** | Bordini, rune, incisioni seguono gli spigoli | Fa sembrare "su misura" un tema generico | UV1 da preparare | Incluso | 1 trim sheet | Basso |
| **Morph target / mesh deformer** | Deforma la mesh base (lama più seghettata, guardia più larga) | Cambia la forma dell'arma stessa | Ogni morph va fatto per arma: **non scala**; da evitare per i temi, ok per gli "stili" di un singolo kit | Alto | — | Basso |
| **Chaos Cloth / AnimDynamics / ossa fisiche** | Nastri, nappe, catene, mantelli d'arma | Molto "vivo" | Solo cosmetico lato client; il pezzo dev'essere skeletal | 0 (usa slot) | Per pezzo | Medio |

Conclusione tecnica: **socket + spline + material layer** sono le tre gambe e funzionano tutte a runtime; Geometry Script
serve all'assemblatore per fondere i pezzi (il suo modulo base dovrebbe essere usabile fuori dall'editor **[da verificare 5.9]**);
PCG resta in editor per **creare** i pezzi di un tema, non per applicarlo; displacement è un bonus da verificare; i morph
restano per gli stili di un kit, non per i temi.

## 2b. Assemblaggio a runtime: costo per oggetto e come tenerlo basso

Modello di vendita (owner, 2026-11-23): il tema è uno **stile di design venduto una volta**, applicabile dal giocatore a
qualsiasi oggetto idoneo, come le camo di Fortnite ma con i pezzi 3D. Le combinazioni arma × tema × stile non si possono
preparare in anticipo: l'oggetto decorato nasce a runtime.

**Spazio su disco** (domanda dell'owner): il bake per coppia sarebbe davvero un problema (temi × armi × stili mesh, ognuna
centinaia di KB o più). A runtime su disco c'è solo: i pezzi del tema una volta (8-15 mesh piccole, 2-4 moduli spline,
1 materiale e texture, stimati 5-20 MB per tema con texture 2K) e i dati di ogni arma (slot, spline: pochi KB). Il totale
cresce come temi + armi.

**Stima del costo per oggetto equipaggiato** (ordini di grandezza, non misurati; rendering deferred senza Lumen):

| Modo | Componenti | Draw call (base + profondità + ombra) | Memoria | CPU |
|---|---|---|---|---|
| Ingenuo: 1 componente per pezzo e per segmento di spline | ~10 pezzi + ~20 segmenti = ~30 | ~90 | pezzi condivisi, quasi nulla | trasformazione e visibilità di 30 componenti ogni frame |
| Instancing (ISM) per pezzi uguali, spline separate | ~5-25 | ~15-75 | come sopra | medio |
| **Fusione in una mesh per oggetto (consigliata)** | 1-2 | 3-6 | 100-500 KB per combinazione usata, solo in RAM | 1-5 ms una volta all'equipaggiamento, in asincrono |
| Lontano o con molti personaggi: solo materiale | 1 | 3 | 0 | trascurabile |

Esempio: 8 personaggi a schermo con 2 oggetti decorati ciascuno = ~1.400 draw call nel modo ingenuo (troppe insieme al
resto della scena), ~100 con la fusione.

**Cosa serve per tenerlo basso**
1. **Un solo materiale per tema** sui pezzi (trim sheet o atlas condiviso): la mesh fusa ha una sezione di materiale in più rispetto all'arma base, non una per pezzo.
2. **Fusione all'equipaggiamento**: l'assemblatore deforma i moduli lungo le spline e attacca i pezzi agli slot in una Dynamic Mesh, poi la rende come componente unico, o crea una Static Mesh temporanea per avere LOD ed eventualmente Nanite **[da verificare 5.9: costruzione di Static Mesh a runtime nei build non editor]**.
3. **Cache in memoria** per chiave (arma, tema, stile): personaggi con la stessa combinazione condividono la mesh, liberata quando nessuno la usa. Opzionale: cache su disco solo delle combinazioni che il giocatore usa davvero (l'unico uso sensato del bake).
4. **Limiti per oggetto** fissati nel validatore: max 12 pezzi, max 4 spline, max ~8 segmenti per spline, max triangoli aggiunti per tema (es. 5-15 k).
5. **Livelli di dettaglio**: oltre una distanza o con molti personaggi a schermo si mostra solo il materiale del tema, che dà già l'aspetto. Pezzi con fisica (nastri, catene) solo da vicino.
6. **Nemici e minion**: di default solo materiale; la fusione completa per i giocatori.
7. **Mai a metà combattimento**: la fusione parte al cambio di loadout o al caricamento, in asincrono; nel frattempo si vede l'arma base con il materiale del tema.

## 2c. Bake all'armadietto contro runtime puro (idea dell'owner, 2026-11-23)

Idea: costruire l'oggetto decorato quando il giocatore lo sceglie nell'armadietto, così le modalità di gioco vere non pagano
niente. È giusta, con una precisazione: **in multiplayer ogni macchina vede anche i loadout degli altri**, che non sono
passati dal suo armadietto. Gli altri giocatori mandano solo ID (arma, tema, stile); scaricare mesh già fatte da altri
sarebbe pesante e un rischio (contenuti arbitrari). Quindi lo stesso assemblatore serve in due momenti.

| Momento | Per chi | Dove finisce | Costo per il giocatore |
|---|---|---|---|
| Salvataggio del loadout all'armadietto | Il proprio equipaggiamento | Cache locale su disco (`Saved/`), più memoria | 1-5 ms per oggetto, invisibile nel menu |
| Schermata di caricamento della partita | Tutti i giocatori presenti (ID ricevuti) | Memoria; su disco solo le proprie | Pochi oggetti × pochi ms, nascosti nel caricamento |
| In partita (giocatore che entra tardi, cambio di loadout se mai permesso) | Il nuovo arrivato | Memoria | In asincrono; nel frattempo arma base con materiale del tema |
| Lontano o con molti personaggi | Tutti | Nessuna mesh nuova | Solo materiale del tema |

**Chiave e invalidazione della cache**: hash di (versione dati dell'arma, versione del tema, scelte di stile, versione
dell'assemblatore). Se cambia uno dei quattro, la mesh in cache si ricostruisce alla prossima occasione fuori partita. Una
patch che cambia un tema invalida solo le combinazioni con quel tema.

**Spazio per giocatore**: 100-500 KB per combinazione salvata (stima). Con 20 combinazioni salvate tra kit e preset sono
2-10 MB; si tiene un tetto (es. 50 MB) e si cancellano le meno usate. Niente cresce con il numero di temi o armi del gioco,
solo con quello che il giocatore usa davvero.

**Multiplayer**: viaggia solo il loadout come ID (già previsto, sezione 5). Ogni client costruisce in locale le mesh che
vede; l'assemblatore dev'essere deterministico (stesso input, stessa mesh) così tutti vedono la stessa cosa. È cosmetica:
se un client non ha ancora finito, mostra il fallback senza effetti sul gioco. Il server dedicato non costruisce niente.

**Raccomandazione**: decisione 2 opzione A (armadietto + caricamento, stesso assemblatore). Il runtime puro resta solo
come rete di sicurezza per chi entra a partita in corso. Lo si aggiunge senza costo di progetto in più rispetto al runtime
puro: il codice è lo stesso, cambia solo quando viene chiamato e la cache su disco (stima +1-2 $ in F1).

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
  assemblatore, reattivo) e modulo editor (validatore come regola di `FGSTN_ValidationExtensions`, strumento spline). Opzionale e spento di default come gli altri plugin.
- **Prestazioni**: fusione in una mesh per oggetto all'armadietto e al caricamento, cache, fallback a solo materiale (sezioni 2b, 2c).

## 7. Rischi

1. **Qualità estetica dell'adattamento automatico**: è il rischio principale. Pezzi generici possono sembrare "appiccicati".
   Mitigazione: involucri, trim sheet che segue gli spigoli, override per tema solo dove serve, e la prova con 2 armi molto
   diverse prima di investire.
2. **Produzione dei pezzi dei temi senza Blender**: i pezzi sono piccoli e semplici (tappi, gemme, filigrane piastrellabili),
   più facili da generare con strumenti generativi (es. Meshy) o da Modeling Mode che un'arma intera. Restano da pulire.
3. **Strumenti 5.9** (Geometry Script, PCG, Nanite displacement): API o stato sperimentale da verificare prima di F1.
4. **Costo della fusione a runtime**: picchi di CPU o memoria se molti personaggi cambiano loadout insieme; si limita con cache
   condivisa, asincrono e fallback a solo materiale (sezione 2b).
5. **Leggibilità**: temi molto carichi possono confondere portata e direzione del colpo; il limite di silhouette va provato in
   gioco con la regola 10 (video guardato).

## 8. Percorso a fasi minimo (NON iniziato)

| Fase | Cosa | Prova di successo | Stima |
|---|---|---|---|
| F0 | Verifica strumenti 5.9 con l'Unreal Assistant (Geometry Script a runtime e deform su spline, Static Mesh a runtime, PCG su mesh, displacement) | Risposta scritta, 1 pagina | ~1 $ |
| F1 | Plugin minimo: dati tema/host, assemblatore con fusione, cache su disco e chiamata all'armadietto/caricamento (socket + spline + material layer), validatore, misura di draw call e ms. **1 tema** (es. oro e avorio a filigrana) su **2 armi** diverse (spada dell'Overlord + un oggetto largo, es. scudo o martello) | Video in gioco dietro e di lato, review cloud | codice cloud 6-9 $, build/test desktop 3-5 $, pezzi del tema 2-4 $ |
| F2 | **Prova della scala**: 2° tema sulle stesse 2 armi senza toccarle, e una 3a arma con i 2 temi senza toccare i temi | Zero lavoro "per coppia"; copertura >= T2 | 3-5 $ |
| F3 | Reattività e stili, livelli di dettaglio, slot del corpo sullo scheletro | Una skin reattiva in gioco | da stimare dopo F2 |

Totale F0-F2: circa 16-26 $. Se F2 richiede ritocchi per coppia oltre un override su dieci, l'approccio A va rivisto prima
di F3 (fermarsi e decidere con l'owner).

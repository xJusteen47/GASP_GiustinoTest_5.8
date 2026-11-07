# Principi di animazione corporea (body mechanics)

Documento di riferimento obbligatorio per chiunque (Claude, Antigravity, altri agenti)
generi, corregga o revisioni un'animazione di mossa in questo progetto.
Il documento per picchiaduro e action (`animazione-picchiaduro-action.md`) dà per letto questo.

Non è l'elenco dei 12 principi. È la parte di mestiere che un corso di body mechanics
(Animation Mentor, iAnimate, AnimSchool) insegna nei primi 6-9 mesi, scritta in forma
verificabile: numeri di frame, rapporti di spacing, controlli da fare fotogramma per fotogramma.
**Solo teoria generale.** Qui non ci sono correzioni di mosse passate né regole ricavate da
una singola animazione. I numeri sono **intervalli indicativi** con il loro perché: servono a
riconoscere un problema e a ragionarci, non sono soglie da rispettare. Gli esempi sono
marcati come tali e illustrano un principio.

Convenzioni:
- Frame a **60 fps**, numerati da 0 come i frame di animazione dell'owner.
- Tra parentesi l'equivalente a 24 fps (1 frame a 24 fps = 2,5 frame a 60 fps).
- "Chiave" = posa scelta dall'owner o dall'animatore. "Breakdown" = posa che decide come
  si passa tra due chiavi. "Inbetween" = tutto il resto, che di solito fa la curva.

---

## 1. Unità di misura e ritmo

| Durata | 60 fps | 24 fps | Cosa ci sta |
|---|---|---|---|
| 1/30 s | 2 f | ~1 f | uno snap, un impatto, uno smear |
| 1/10 s | 6 f | 2-3 f | ritardo di overlap tra due segmenti lontani |
| 1/6 s | 10 f | 4 f | anticipazione veloce, settle breve |
| 1/4 s | 15 f | 6 f | battito di ciglia completo, girata di testa rapida |
| 1/2 s | 30 f | 12 f | un passo di camminata, un salto sul posto |
| 1 s | 60 f | 24 f | girata di corpo con peso, alzarsi da seduto (veloce) |

"On ones / twos / threes": a 24 fps "on twos" = ogni posa tenuta 2 frame (12 pose/s).
L'equivalente a 60 fps è tenere ogni posa **5 frame** (12 pose/s) o **4 frame** (15 pose/s).
Vedi il doc picchiaduro per quando usarlo.

Regola di ritmo (Williams): un'azione si legge per **contrasto** tra parti lente e parti
veloci. Se ogni parte di una mossa dura più o meno lo stesso, la mossa è "piatta" anche se
le pose sono belle. Cerca sempre almeno un rapporto **1:3** tra la fase più veloce e la più
lenta (es. colpo in 3 f, ritorno in 10-15 f).

---

## 2. Timing e spacing

**Timing** = quanti frame dura un movimento. **Spacing** = quanto si sposta l'oggetto da un
frame al successivo. Due animazioni con lo stesso timing e spacing diverso sembrano
completamente diverse. L'errore tipico degli agenti è sistemare il timing e lasciare lo
spacing come esce dall'interpolazione.

### 2.1 Charts di spacing (Williams, Survival Kit, cap. "Timing and Spacing")

Fra due chiavi A e B a 10 frame di distanza, la distanza percorsa per frame può essere:

```
lineare     A |  |  |  |  |  |  |  |  |  | B    distanze uguali: "morto", meccanico
slow-in     A |   |   |   |  |  | | ||| B       frena verso B: si posa, ha peso
slow-out    A ||| | |  |   |   |   |    | B     parte lento da A: accelera, ha massa
slow-in-out A || |  |   |   |   |  | || B       ease standard: comodo ma generico
```

Rapporti utili (misurabili sulla distanza percorsa dalla parte guida, per esempio il polso):
- **Ease standard**: primo e ultimo intervallo ≈ **1/3-1/4** dell'intervallo centrale.
- **Slow-in marcato (si appoggia)**: ultimi 3 intervalli dimezzano ciascuno il precedente
  (1, 1/2, 1/4, 1/8 del centrale): l'oggetto "atterra" sulla posa.
- **Accelerazione di colpo** (punch, kick): spacing crescente quasi geometrico, ultimo
  intervallo prima del contatto il **più lungo** di tutti, nessun rallentamento prima
  dell'impatto. Un pugno che rallenta prima di colpire non colpisce.
- **Lineare**: accettabile solo per oggetti meccanici, o per 1-2 frame dentro una
  traiettoria veloce (smear). Se una parte del corpo ha 4+ intervalli uguali di fila,
  sembra telecomandata.

### 2.2 "Lineare = morto" e "spline = galleggiante"

Gli interpolatori di default (Bezier/auto tangent) danno slow-in-out su **ogni** chiave.
Risultato: ogni posa viene raggiunta con delicatezza, quindi niente sembra avere peso né
velocità. Questo è il "floaty" tipico delle animazioni generate.

Come si corregge (workflow professionale):
1. **Blocking stepped**: solo chiavi e breakdown, interpolazione a gradino. Si giudica il
   timing guardando le pose che cambiano di colpo. Se in stepped la mossa non funziona,
   le curve non la salveranno.
2. **Spline**: si passa a curve; si riparano subito i punti in cui la spline ha aggiunto
   moto non voluto (overshoot di tangenti, deriva tra due chiavi uguali, piedi che scivolano).
3. **Polish**: tangenti spezzate a mano. Sulla chiave di impatto la tangente in **entrata è
   ripida** (velocità piena) e in uscita è **spezzata** (cambio netto). Due chiavi uguali di
   seguito devono avere tangenti **piatte** o lo spline produce un rimbalzo.

### 2.3 Durate tipiche (azione realistica, corpo adulto)

| Azione | 60 fps | 24 fps | Nota |
|---|---|---|---|
| Battito di ciglia | 6-9 f | 3-4 f | chiusura 2 f, apertura più lenta |
| Girata di testa | 10-20 f | 4-8 f | gli occhi guidano 2-4 f prima |
| Passo di camminata (contatto→contatto) | 30 f | 12 f | camminata "normale" |
| Passo di corsa | 16-20 f | 6-8 f | fase di volo presente |
| Salto sul posto, stacco | 6-10 f di spinta | 2-4 f | dopo 10-20 f di carica |
| Atterraggio, assorbimento | 6-12 f | 2-5 f | il punto più basso 3-5 f dopo il contatto |
| Pugno di un non-combattente | 12-18 f | 5-7 f | il combattente è 2-3 volte più rapido |

Per personaggi giocattolo stilizzati (riferimento del progetto: Astro Bot, DK Bananza)
le durate si accorciano del **20-40%** e i contrasti si allargano: pose più tenute, passaggi
più rapidi.

---

## 3. Chiavi, breakdown e i due metodi

- **Pose-to-pose**: si decidono prima le chiavi (storytelling), poi i breakdown, poi gli
  inbetween. È il metodo per mosse di gioco: le chiavi coincidono con i frame che contano
  per il gameplay (startup, contatto, FAF). Le chiavi qui sono **dell'owner**.
- **Straight-ahead**: si anima frame dopo frame dall'inizio alla fine. Serve per moto
  caotico o fluido (fuoco, capelli, cadute rotolando). Nel 3D si usa a **strati**: prima il
  corpo pose-to-pose, poi le parti libere (coda, capelli, stoffa) straight-ahead sopra.

**Il breakdown decide il carattere.** Fra la posa A (braccio giù) e B (braccio su) il
breakdown può:
- seguire un **arco** (il polso passa più in fuori della linea retta),
- **favorire** una chiave (a metà tempo è già al 70-80% verso B: movimento deciso;
  al 20-30%: movimento esitante che scatta alla fine),
- introdurre il **trascinamento** (la spalla è già a B, il polso è ancora vicino ad A).

Un inbetween "a metà esatta" in tempo e posizione tra due chiavi è quasi sempre sbagliato.
Regola pratica: **nessun breakdown al 50%** né in tempo né in posa, salvo moto meccanico.

---

## 4. Peso ed equilibrio

### 4.1 Centro di massa (COM) e base d'appoggio

- Il COM di un corpo umano in piedi sta poco sotto l'ombelico, davanti all'osso sacro.
  Su un rig UE è vicino a `pelvis`, leggermente sopra e avanti.
- **In equilibrio statico** la proiezione verticale del COM cade dentro il poligono dei
  piedi a terra. Se esce, il personaggio **deve** essere in movimento (sta cadendo, sta
  facendo un passo, sta spingendo) entro pochi frame. Una posa tenuta col COM fuori dalla
  base, senza motivo, legge come "sbagliata" anche a chi non sa dire perché.
- **Su un piede**: il bacino si sposta sopra il piede d'appoggio **prima** che l'altro piede
  si stacchi (2-6 f prima a 60 fps). Se il piede si alza e il peso non si è spostato, il
  personaggio è appeso a un filo.
- **In aria**: il COM segue una **parabola** esatta. Gli arti possono muoversi come vogliono,
  il COM no. Ruotare il bacino in aria non sposta il COM: lo fa solo l'interazione con
  qualcosa (un colpo, una superficie).

### 4.2 Contrapposto e catena cinetica

- **Contrapposto**: se le anche si inclinano verso un lato, le spalle si inclinano verso
  l'altro. Se il bacino ruota a destra, il torace compensa a sinistra. Due linee (anche,
  spalle) **parallele** = posa rigida, da manichino.
- **Catena cinetica** (ordine in cui una forza attraversa il corpo): piedi → anche → torace
  → spalla → gomito → polso → mano. In un pugno o un calcio potente ogni segmento
  **inizia** dopo il precedente (1-2 f a 60 fps per anello) e **arriva al picco di velocità**
  dopo il precedente. Il polso arriva per ultimo e più veloce. Se tutti i segmenti partono
  insieme, la mossa sembra fatta da un braccio staccato dal corpo.

### 4.3 Camminata: contatto, discesa, passaggio, salita (Williams)

Per un passo da 30 f (12 f a 24 fps):

| Posa | Frame | Altezza bacino | Cosa succede |
|---|---|---|---|
| Contatto | 0 | media | tallone a terra, gambe divaricate, braccia opposte al massimo |
| Discesa (down) | 4-6 | **minima** | il peso cade sulla gamba avanti, ginocchio piegato |
| Passaggio | 15 | media | gamba libera passa accanto a quella di appoggio |
| Salita (up) | 20-23 | **massima** | spinta della gamba dietro, punta sta per staccarsi |
| Contatto | 30 | media | lato opposto |

Il bacino oscilla **anche** in laterale (sopra il piede d'appoggio) e in rotazione.
Sulla stessa base si leggono corsa, salto, atterraggio: il punto basso arriva **dopo** il
contatto, non sul contatto.

### 4.4 Come si "vede" il peso

Il peso non si anima con le pose, si anima con le **decelerazioni**.
- Oggetto pesante: parte lento (slow-out lungo), si ferma in fretta e si assesta poco
  (inerzia assorbita dal corpo che lo regge).
- Oggetto leggero: parte subito, si ferma con un piccolo overshoot.
- Atterraggio: il corpo continua a scendere **dopo** il contatto dei piedi per 3-8 f
  (assorbimento), proporzionale alla velocità di caduta. Atterrare e fermarsi nello stesso
  frame = peso zero.
- Spinta: prima di spingere su qualcosa il corpo si **carica** sul piede dietro.

---

## 5. Anticipazione

Un'anticipazione è un movimento **in direzione opposta** all'azione che lo segue, che
carica energia e avvisa lo spettatore.

Dimensione (regola di proporzionalità, Thomas & Johnston, *The Illusion of Life*):
- Durata dell'anticipazione ≈ **1-3 volte** la durata dell'azione principale per azioni
  "teatrali"; **0,3-1 volta** per azioni rapide e funzionali.
- Ampiezza: più è grande l'azione, più è ampia la carica. Un salto alto parte da un
  accovacciamento profondo; un colpetto parte da una carica di pochi gradi.

Valori di partenza a 60 fps:

| Azione | Anticipazione | Azione principale |
|---|---|---|
| Colpo leggero (jab) | 2-4 f, 5-15° all'indietro | 2-3 f |
| Colpo forte | 8-16 f, carica visibile del busto | 3-5 f |
| Salto | 6-12 f di accovacciamento | 6-10 f di stacco |
| Scatto / dash | 3-6 f, peso dietro | 4-6 f |

Tipi di anticipazione:
- **Ampia**: tutto il corpo va indietro. Leggibile, lenta.
- **Interna**: si muove solo una parte (spalla, occhi, bacino) mentre il resto è fermo.
  È quella per mosse veloci: lo spettatore la percepisce senza vederla.
- **Inversa d'arco**: il polso scende sotto la traiettoria finale prima di salire.

Errori: anticipazione assente (azione "teletrasportata"); anticipazione **dello stesso
asse e ampiezza** su tutto il corpo (manichino che si piega); anticipazione che si ferma in
una posa tenuta **senza moving hold** (vedi §8); anticipazione più lunga dell'azione in una
mossa che deve essere veloce.

---

## 6. Overshoot e settle

Quando una parte arriva su una posa, la supera un poco e torna. Questo dice che ha massa.

Ordini di grandezza (il perché: un arto con muscoli frena da sé, quindi assorbe l'energia in fretta):
- Ampiezza dell'**overshoot** di un arto muscolare: **5-15%** del tragitto appena fatto.
  Oltre il 20% sembra una molla o un arto rotto.
- Numero di oscillazioni visibili: **una** (oltre e indietro). Al massimo una seconda,
  molto più piccola della prima. Una terza oscillazione su un arto vivo è rara: di solito indica una molla, non un muscolo.
- Durata del settle: **4-12 f** a 60 fps per un arto; il busto si assesta più lento della
  mano ma con ampiezza minore.
- Un corpo con muscoli è **quasi criticamente smorzato**: si assesta, non vibra. Le parti
  che oscillano più a lungo sono solo quelle passive (capelli, stoffa, antenne, code), e
  anche loro con decadimento rapido (§7.3).

Forma del settle sulla curva (rotazione o posizione nel tempo):
```
valore
  B+δ |      ●
      |    ●   ●
  B   |  ●       ● ● ● ●     <- si posa e RESTA (moving hold minimo)
      | ●
  A   |●
      +--------------------- frame
        0 2 4 6 8 10 12
```
Se la curva dopo il punto B disegna 3+ creste, è un errore.

---

## 7. Overlap, drag, follow-through, secondary motion

Sono quattro nomi per la stessa idea: **le parti di un corpo non partono e non si fermano
insieme**. Ma hanno regole diverse.

### 7.1 Definizioni operative

- **Drag (trascinamento)**: all'inizio del movimento la parte figlia resta indietro rispetto
  alla parte genitore. Spalla parte, gomito parte 1-2 f dopo, polso 1-2 f dopo il gomito.
- **Overlapping action**: parti diverse del corpo hanno ritmi diversi e si sovrappongono:
  la testa si ferma dopo il busto, il braccio libero ha un suo tempo.
- **Follow-through**: dopo che la parte guida si ferma, le parti figlie continuano nella
  stessa direzione e poi tornano (overshoot sulle figlie).
- **Secondary motion**: un movimento in più che **supporta** l'azione principale senza
  rubarle l'attenzione: il braccio libero che bilancia, la testa che reagisce, i capelli.

### 7.2 Ritardi a catena ("successive breaking of joints", Williams)

Le articolazioni si "rompono" in sequenza: ogni segmento è in ritardo sul padre.
Valori a 60 fps per corpo umano:

| Catena | Ritardo per anello | Totale tipico |
|---|---|---|
| Busto → spalla → gomito → polso → dita | 1-2 f | 4-8 f |
| Bacino → coscia → ginocchio → caviglia | 1-2 f | 3-6 f |
| Bacino → spine_01..03 → collo → testa | 1 f | 3-5 f |
| Testa → capelli/orecchie/accessori | 2-4 f | 2-4 f |

A 24 fps lo stesso ritardo è di **0,5-1 frame** per anello: per questo nelle animazioni a
24 fps l'overlap si fa spesso con **rotazioni**, non con offset di chiave.

### 7.3 La regola chiave: il moto secondario è ritardato, decade, finisce

Principio: **il moto secondario è una conseguenza smorzata di un moto primario, non un
moto autonomo**. Le caratteristiche che seguono ne derivano; i numeri sono indicativi.

1. **Parte in ritardo**: 2-4 f dopo il cambio di direzione della parte genitore.
2. **È proporzionale** al cambio di velocità che lo ha causato: un arresto brusco produce
   un overshoot visibile, un rallentamento dolce quasi nulla.
3. **Decade in fretta**: ogni oscillazione successiva ha ampiezza **≤ 30-40%** della
   precedente.
4. **Ha un termine**: si ferma dopo un tempo limitato, che dipende da quanto la parte è
   leggera e libera (più muscolo = assestamento più rapido). Ordini di grandezza:
   - Arti con muscoli (gambe, braccia): **≤ 8-12 f** a 60 fps, **1 oscillazione**.
   - Parti morbide attaccate (pancia, guance, guanti grossi): ≤ 12-18 f, 1-2 oscillazioni.
   - Parti libere leggere (capelli, nastri, antenne, coda): ≤ 20-30 f, 2-3 oscillazioni
     con decadimento netto.
5. **Non si ricarica da sola**: se il genitore è fermo, la figlia non può ripartire.
   Ogni nuova oscillazione deve avere una **causa** visibile (un nuovo colpo, un atterraggio,
   un cambio di direzione).
6. **Non è "rumore"**: un'oscillazione costante sovrapposta a tutta la mossa non è secondary,
   è jitter.

*Esempio (illustrativo)*: un arto che ha appena colpito ha ceduto energia al bersaglio,
quindi dopo il contatto ha **meno** energia, non di più. Se dopo l'impatto oscilla più volte
senza una nuova causa, il moto non ha origine visibile e legge come "arto di gomma che
nessuno controlla". Cosa fare invece dipende dalla mossa e dalle chiavi: il principio è solo
che ogni oscillazione deve avere una causa e smorzarsi.

Distinguere **intenzionale** da **passivo**:
- Arto **attivo** (lo muove il personaggio): dopo l'azione va verso una posa con slow-in
  e un piccolo overshoot. Non oscilla.
- Arto **passivo** (rilassato, trascinato): può fare 1-2 overshoot smorzati.
- Il braccio che colpisce è attivo. Il braccio che non colpisce è passivo **solo** se il
  personaggio lo lascia andare; in una posa di guardia è attivo.

### 7.4 "Overcooked overlap" (overlap stracotto)

Il difetto opposto alla rigidità, tipico degli agenti che hanno appena scoperto il follow-
through: tutto ritarda, tutto ondeggia, il personaggio sembra fatto di spaghetti.
Sintomi misurabili:
- ritardo per anello **> 3 f** su arti attivi;
- le dita/mani arrivano **> 6 f** dopo la spalla in un colpo veloce;
- più di 1 overshoot su un arto attivo;
- la testa ondeggia dopo ogni movimento del busto;
- nel complesso, la posa chiave non è mai visibile "pulita" per almeno 2-3 f.

La posa chiave deve essere leggibile: l'overlap la **circonda**, non la cancella.

---

## 8. Moving hold

Una posa "tenuta" in 3D non deve essere ferma al 100%: un corpo vivo totalmente immobile
sembra un congelamento di rendering.
- Una **moving hold** è una posa che deriva lentamente verso una versione leggermente più
  rilassata o più intensa di sé stessa: **2-5%** dell'ampiezza dell'azione precedente, con
  slow-in.
- Durata minima perché una hold si legga: **6 f** a 60 fps (≈ 2-3 f a 24 fps). Una hold
  "teatrale" 12-30 f.
- Durante la hold proseguono gli assestamenti secondari (§7) che però **finiscono**.
- Eccezioni: hitstop (il gioco congela tutto, è voluto) e stile limitato a pose tenute
  (doc picchiaduro). In quei casi l'immobilità è un effetto di stile, non un errore.

---

## 9. Archi

Quasi tutto in un corpo organico si muove su archi, perché le articolazioni ruotano.
- Mani, piedi, testa, punta del naso: traiettorie curve. Una linea retta sulla traiettoria
  del polso = movimento robotico.
- Si verificano con il **motion trail** (UE Sequencer / Control Rig, Cascadeur trajectories)
  o sovrapponendo i frame (onion skin): la sequenza delle posizioni deve formare una curva
  liscia, **senza cuspidi** né zig-zag.
- Gli archi cambiano direzione solo con un motivo (impatto, contatto, nuova intenzione).
  Se l'arco del polso ha un gomito tra due frame senza motivo, c'è un breakdown sbagliato.
- Eccezioni: colpi a stocco (jab dritto) dove la **mano** va dritta ma **gomito e spalla**
  fanno archi; arresti contro una superficie.

Test pratico: segui con l'occhio un solo punto (per esempio la punta di un piede) per tutta la
mossa. Deve disegnare una forma che potresti tracciare con un solo gesto della penna.

---

## 10. Squash e stretch su un personaggio giocattolo

Su un rig senza stretch delle ossa lo squash/stretch si ottiene con la **posa**:
- **Squash** = compressione: colonna piegata, testa incassata, ginocchia piegate, braccia
  vicine al corpo. Si usa in atterraggio, carica, impatto ricevuto.
- **Stretch** = allungamento: colonna estesa, arti allungati al massimo, punte dei piedi
  tese. Si usa in stacco, spinta, colpo in estensione.

Limiti per lo stile "toy-like" (Astro Bot, DK Bananza):
- Volume costante: quello che si allarga deve accorciarsi.
- Il personaggio giocattolo è **duro** (plastica): lo squash vero è scarso. Si comunica la
  compressione con la posa per **2-4 f**, non con deformazioni elastiche lunghe.
- Se in futuro si usa scala su ossa: max **±10-15%** sul busto, solo 1-3 f, mai su arti
  durante un colpo (cambia la portata percepita rispetto all'hitbox).
- Squash e stretch **non** oscillano: dopo lo squash di atterraggio si torna alla forma
  normale in 4-8 f con slow-in, una volta.

---

## 11. Silhouette e linea d'azione

- **Silhouette**: riempi il personaggio di nero sul fondo bianco. La posa chiave deve
  ancora dire cosa sta facendo (chi colpisce, dove, con cosa). Arti sovrapposti al busto
  spariscono: allontanali dal busto o ruota il busto.
- **Linea d'azione**: una curva unica (C o S) che attraversa il corpo dalla testa al piede
  d'appoggio o all'arto che colpisce. Nelle chiavi forti è una C tesa; nelle pose di
  riposo è una S morbida. Una linea dritta verticale = posa neutra senza energia.
- La linea d'azione **cambia verso** fra anticipazione e azione (C rovesciata): è il modo
  più rapido per dare energia a un colpo.
- **Twinning** (gemellaggio): i due lati del corpo fanno la stessa cosa nello stesso
  momento (braccia uguali, piedi allineati, spalle parallele alle anche). Si rompe con
  asimmetrie di posa **e di tempo** (1-3 f di sfasamento).
- **Tangenze**: due contorni che si toccano esattamente (mano sul bordo della testa)
  confondono la lettura. O si sovrappongono chiaramente o si separano.

---

## 12. Errori comuni e come trovarli frame per frame

| Errore | Come si vede | Misura frame per frame | Correzione |
|---|---|---|---|
| **Floaty** (galleggia) | niente pesa, tutto scivola nelle pose | ogni chiave ha slow-in e slow-out; atterraggi senza discesa dopo il contatto; COM in aria non parabolico | spezzare tangenti sui contatti, aggiungere assorbimento 3-8 f, accorciare gli ease |
| **Spacing uniforme** | meccanico, "a velocità costante" | 4+ intervalli di distanza uguale sulla parte guida | favorire una chiave, chart di spacing (§2.1) |
| **Twinning** | rigido, simmetrico | lati sx/dx con stesse rotazioni ±5° nello stesso frame | sfasare 1-3 f, cambiare ampiezza di un lato |
| **Overlap stracotto** | gommoso, spaghetti | ritardi > 3 f/anello, > 1 overshoot su arti attivi, oscillazioni dopo l'azione | ridurre ritardi a 1-2 f, uccidere oscillazioni oltre la prima |
| **Secondary infinito** | "molleggia" dopo l'azione | creste ripetute sulla curva della parte dopo il fermo del padre | applicare §7.3: decadimento ≤ 30-40%, fine entro 8-12 f per arti |
| **Hit senza impatto** | il colpo "attraversa" | velocità della parte che colpisce non massima nel frame prima del contatto; nessun cambio netto di spacing sul contatto | accelerare fino al contatto, posa d'impatto tenuta, reazione del corpo (doc picchiaduro) |
| **Popping** | scatto non voluto | un osso cambia di > 30° in 1 f senza motivo (spesso flip di quaternion o IK/FK) | controllare chiavi doppie, gimbal, switch IK |
| **Piedi che scivolano** | pattina | piede "a terra" che si sposta > 0,5-1 cm tra frame | bloccare il piede (IK planted) |
| **Peso fuori base** | sta in piedi "per magia" | proiezione COM fuori dai piedi in una posa tenuta | spostare bacino sopra l'appoggio |
| **Arco spezzato** | robotico | traiettoria del polso/piede con cuspidi o tratti dritti | breakdown sull'arco |
| **Posa chiave invisibile** | non si capisce cosa è successo | la posa di impatto dura < 2 f senza hitstop, o è coperta dall'overlap | tenere la posa, ridurre overlap attorno |
| **Anticipazione nella direzione sbagliata** | colpo debole | il polso nella carica non va nel verso opposto al colpo | invertire la carica |
| **Teste mobili** | instabile | testa che ruota/scuote dopo ogni movimento del busto | testa stabilizzata (gli occhi cercano il bersaglio), ritardo 1 f non oscillazione |

---

## 13. Come derivare il giudizio su una mossa

Le regole per una mossa **non si conservano**: si ricavano ogni volta dai principi, partendo
dall'intenzione della mossa. Metodo in quattro passi:

1. **Intenzione**: cosa deve comunicare la mossa (peso, velocità, chi la fa, dove va
   l'energia)? Le chiavi dell'animatore la fissano.
2. **Principio → previsione**: per ogni principio pertinente, cosa ci si aspetta di vedere
   *in questa mossa*? Esempio di ragionamento: "colpo pesante → la parte che colpisce deve
   avere spacing crescente fino al contatto, la carica deve essere proporzionata, il recovery
   deve mostrare l'inerzia".
3. **Misura sui frame**: cosa si osserva davvero (spacing della parte guida, ordine di
   partenza dei segmenti, traiettorie, oscillazioni e loro causa, posizione del peso).
4. **Confronto e spiegazione**: dove osservazione e previsione divergono, si scrive il frame,
   il principio e il perché. Non si scrive "il braccio è troppo a destra" ma "a f12 la
   silhouette non mostra l'arto che colpisce (principio §11)": la correzione concreta la
   decide chi anima.

Domande guida (la risposta va ricavata per la singola mossa, non da valori fissi):

**Timing e spacing**
- In stepped, solo con chiavi e breakdown, la mossa si legge già?
- C'è contrasto tra fasi lente e veloci, o tutto dura più o meno uguale?
- La parte che colpisce è al massimo della velocità appena prima del contatto?
- Ci sono tratti a velocità costante su una parte organica? Sono voluti?
- I breakdown favoriscono una chiave, o sono a metà esatta?

**Peso**
- Nelle pose tenute il peso sta sopra l'appoggio? Se no, c'è un motivo (sta cadendo, spinge)?
- In aria il baricentro segue una traiettoria credibile?
- Dopo un contatto con il suolo il corpo assorbe, o si ferma di colpo?
- La forza attraversa il corpo in ordine (dal centro verso le estremità)?

**Anticipazione, overshoot, settle**
- C'è una preparazione in verso opposto, proporzionata all'azione e al tempo disponibile?
- Gli overshoot sono proporzionali alla frenata che li causa?
- Il settle dura in modo coerente con quanto la parte è pesante, muscolare o libera?

**Overlap e secondary**
- Le parti partono e si fermano in sequenza, o tutte insieme?
- Ogni oscillazione ha una causa visibile poco prima?
- Ogni oscillazione è più piccola della precedente?
- Qualcosa si muove a genitore fermo, senza causa? Il secondario finisce con la mossa?
- La posa chiave resta leggibile, o l'overlap la copre?

**Forma**
- La silhouette delle chiavi dice cosa succede?
- C'è una linea d'azione chiara, che cambia tra carica e azione?
- Ci sono simmetrie non volute (twinning) o tangenze?
- Le traiettorie di mani, piedi e testa sono archi lisci? I cambi di direzione hanno un motivo?
- I piedi a terra restano fermi? Ci sono scatti senza motivo?

---

## Fonti

- Richard Williams, *The Animator's Survival Kit* (2001, ed. ampliata 2009): timing e
  spacing charts, breakdown, successive breaking of joints, camminate (contact/down/passing/up),
  peso, anticipazione, overlap.
- Frank Thomas, Ollie Johnston, *The Illusion of Life: Disney Animation* (1981): i 12
  principi originali, proporzionalità dell'anticipazione, follow-through e overlapping action,
  moving hold, secondary action.
- Preston Blair, *Cartoon Animation* (1994, Walter Foster): linea d'azione, squash/stretch con
  volume costante, cicli di camminata e corsa cartoon, pose di peso.
- Eadweard Muybridge, *The Human Figure in Motion*: riferimento fotografico per
  passi, corse, salti, lanci.
- Kenny Roy, *How to Cheat in Maya* (serie): workflow blocking stepped → spline → polish,
  pulizia delle curve.
- Corsi body mechanics: Animation Mentor (Class 2 "Body Mechanics"), iAnimate (Workshop 1-2),
  AnimSchool (Body Mechanics): pesi, contrappesi, COM, salti, spinte e tiri, revisione
  frame per frame con il motion trail.
- Biomeccanica di base: catena cinetica prossimale-distale (letteratura sportiva su lancio
  e calcio), COM e poligono d'appoggio.

I numeri di questo documento sono **valori di partenza** derivati dalle fonti e dalla
pratica comune dell'animazione 3D per giochi. Le chiavi e i tempi scelti dall'animatore (nel progetto, l'owner) vincono sempre.

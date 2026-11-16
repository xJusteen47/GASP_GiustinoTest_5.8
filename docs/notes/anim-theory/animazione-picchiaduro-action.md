# Animazione per picchiaduro e action game

Seconda parte del riferimento. **Prerequisito: `principi-animazione-corporea.md`.**
Qui non si ripetono timing, spacing, peso, overlap: si spiega come cambiano quando
l'animazione deve servire al gameplay, a 60 fps, con hitbox, hitstop e una telecamera
lontana.

Convenzioni come nel doc 1: frame a **60 fps** numerati da 0 (frame di animazione
dell'owner), equivalenti a 24 fps tra parentesi quando servono.

Idea centrale: in un picchiaduro **l'animazione è un contratto con il giocatore**. Ogni
frame deve dire in che fase è la mossa. La bellezza viene dopo la leggibilità, e la
leggibilità viene dal contrasto tra pochi frame di movimento estremo e pose tenute chiare.

**Solo teoria generale.** Qui non ci sono correzioni di mosse passate né regole ricavate da
una singola animazione. I numeri sono **intervalli indicativi** con il loro perché: servono a
riconoscere un problema e a ragionarci, non sono soglie da rispettare. Gli esempi sono
marcati come tali e illustrano un principio.

---

## 1. Le fasi della mossa e i principi

| Fase di gameplay | Cosa è | Principio di animazione | Obiettivo visivo |
|---|---|---|---|
| **Startup** | frame prima che l'hitbox sia attiva | anticipazione | si capisce cosa sta arrivando |
| **Active** | frame con hitbox attiva | azione principale, contatto | la parte che colpisce è estesa e al massimo della velocità |
| **Hitstop** (se colpisce) | gioco congelato per entrambi | posa d'impatto tenuta | il colpo "pesa" |
| **Recovery** | dopo l'hitbox, prima di poter agire | follow-through, overshoot, settle | il corpo smaltisce l'energia, il giocatore è punibile |
| **FAF** (first actionable frame) | primo frame in cui si può fare altro | ritorno in guardia | la posa deve essere pronta a fondersi con qualunque mossa |

Regole di mappatura:
- **L'anticipazione deve stare dentro lo startup**, e lo startup è corto. Per un jab
  (startup 2-4 f) non c'è spazio per una carica ampia: si usa l'anticipazione **interna**
  (spalla e bacino, 2-3 f) o una guardia già "carica" da cui il colpo parte.
- **Il primo frame attivo** coincide con la posa di massima estensione o con il frame che
  la precede di 1 f. Il polso/piede deve avere la velocità massima **sul frame prima**
  del primo attivo (doc 1, §2.1).
- **Il recovery è follow-through**, non un ritorno lineare. Deve mostrare quanto il colpo è
  "costato": mosse forti hanno recovery lunga e visibile, perché il giocatore deve capire
  che è punibile.
- **Al FAF la posa deve essere vicina alla guardia** (idle/stance), con velocità quasi nulla
  su tutti gli arti. Qualunque oscillazione ancora in corso al FAF verrà tagliata dal blend
  della mossa successiva e sembrerà un glitch.

Durate di partenza (riferimento: frame data pubblicati della serie Smash Ultimate, Street
Fighter 6, Guilty Gear Strive; valori indicativi):

| Tipo di mossa | Startup | Active | Recovery fino al FAF |
|---|---|---|---|
| Jab / colpo leggero | 2-5 f | 2-3 f | 8-15 f |
| Tilt / colpo medio | 5-9 f | 3-5 f | 15-25 f |
| Colpo forte / smash | 10-20 f (+ carica) | 3-6 f | 25-45 f |
| Aereo (in volo) | 4-12 f | 3-10 f | 20-40 f in aria; atterrando vedi §7 |
| Speciale/proiettile | 8-20 f | variabile | 20-40 f |

---

## 2. La posa d'impatto e l'hitstop

### 2.1 Hitstop (hitlag)

Quando il colpo arriva, il gioco congela attaccante e vittima per qualche frame. Serve a
dare peso e a far leggere il contatto. Nel progetto l'hitstop è gestito dal gioco
(codice di combattimento), **non** va animato dentro la clip.

Ordini di grandezza:
- Colpi leggeri: **3-6 f**; medi: **8-12 f**; forti: **14-20 f**; finali speciali: fino a 30 f.
- Smash Ultimate (formula ricostruita dalla community): circa `danno × 0,65 + 6`, con
  moltiplicatori per alcune mosse, tetto intorno a 30 f.
- Durante l'hitstop la vittima di solito **vibra** (shake) lungo la direzione del colpo;
  l'attaccante resta immobile o vibra meno.

### 2.2 La posa d'impatto

Il frame su cui scatta l'hitstop è quello che il giocatore vede più a lungo in tutta la
mossa. Quindi:
- Deve essere la **posa più leggibile** della mossa: arto che colpisce esteso e libero
  dal busto, linea d'azione tesa dal piede d'appoggio (o dal bacino in aria) fino al punto
  d'impatto, silhouette chiara dalla telecamera di gioco.
- **Non** deve essere a metà di un overshoot o di un overlap: se le dita, i capelli o la
  gamba libera sono in una posa "di passaggio", durante l'hitstop si vede una posa casuale
  congelata per 10 f.
- Regola pratica: i 2-3 frame attorno al primo attivo devono essere **pose chiave curate**,
  non inbetween generati.

### 2.3 Dopo l'hitstop

Quando il tempo riparte, l'animazione riprende dal frame congelato. Errori tipici:
- La parte secondaria era a metà oscillazione: alla ripartenza **completa l'oscillazione
  e ne aggiunge altre**. Sembra che il colpo abbia caricato una molla
  (esempio in §8.4).
- Il follow-through riparte lento: dopo un congelamento lo spettatore si aspetta
  **un'esplosione** di movimento (la "liberazione" dell'energia). I primi 2-4 f dopo
  l'hitstop devono avere spacing **ampio**, poi decelerare.

---

## 3. Smear, multipli e scie

Tra la carica e l'impatto un colpo veloce dura 2-4 f. A quella velocità l'occhio vede
uno sfocato, non pose. Strumenti per rendere quella velocità:

- **Smear frame**: un frame in cui la parte che si muove è **deformata** lungo la traiettoria
  (allungata, a forma di goccia). In 2D è disegnato; in 3D si ottiene con: ossa con scala
  temporanea, mesh di smear dedicata, o shader. Dura **1-2 f** e **mai** sul frame di impatto.
- **Multipli (multiple limbs)**: 2-4 copie dell'arto lungo l'arco nello stesso frame.
  Funzionano bene sullo stile giocattolo; 1 f, al massimo 2.
- **Scie / trail VFX**: la soluzione più comune nei giochi 3D (Smash usa archi colorati sui
  colpi). Non richiede di deformare il rig; va sincronizzata con i frame attivi.
- Con un rig **senza stretch delle ossa** la via pratica è: scia VFX sincronizzata
  + eventualmente una posa di "stretch" con arto alla massima estensione possibile nel
  frame prima del contatto.

Regola: smear, multipli e scie **sostituiscono** i frame intermedi, non si aggiungono.
Un frame di smear al posto di due inbetween puliti.

---

## 4. Leggibilità alla distanza di gioco

Il giocatore vede il personaggio piccolo (spesso **100-200 px** di altezza, telecamera di
lato o a 3/4) e per pochi frame. Conseguenze:

- **Esagerare le pose del 20-40%** rispetto al realistico: gomiti più fuori, ginocchia più
  alte, busto più ruotato. Una posa realistica alla distanza di gioco sembra timida.
- **Test miniatura**: rimpicciolire il frame d'impatto a ~120 px (o guardarlo nella
  telecamera Follow reale del gioco). Deve essere chiaro chi colpisce, con cosa e in che
  direzione.
- **La parte che colpisce deve essere visibile** da tutte le telecamere che il gioco usa
  (anche le viste posteriori). Se il braccio è coperto
  dal busto, la mossa sembra non avere hitbox.
- **Direzione chiara**: la posa d'impatto deve "puntare" nella direzione del knockback.
  Un colpo che manda in alto deve avere la linea d'azione che sale.
- **Poche pose, ben distinte**: guardia → carica → impatto → recovery → guardia. Ognuna
  deve avere una silhouette diversa dalle altre.

---

## 5. Timing stilizzato: pose tenute e "12 fps su 60"

Molti giochi stilizzati riproducono l'animazione con **meno pose al secondo** del frame
rate del gioco, tenendo ogni posa per più frame. È lo stile di Guilty Gear Xrd/Strive
(animazione 3D "limitata" ispirata all'anime: pose a gradino, deformazioni a mano per
frame), di Arcane e Spider-Verse (molte parti "on twos").

Valori a 60 fps:
- Pose tenute **4-5 f** ciascuna = 12-15 pose/s, look anime/2D.
- Pose tenute **2-3 f** = 20-30 pose/s, ancora più fluide ma con "scatto".
- Mescolare: **startup e impatto tenuti**, smear di 1 f tra loro, recovery più fluido.

Quando usare le pose tenute:
- Carica di un colpo forte: la posa di carica tenuta 6-12 f dice "sta per arrivare".
- Impatto: già tenuto dall'hitstop; non serve tenerlo anche nella clip.
- Pose di vittoria, provocazioni, intro.

Quando **non** usarle:
- Locomozione e transizioni che il giocatore controlla continuamente: lo stepping sembra
  input lag.
- Se le pose tenute non sono abbastanza diverse tra loro: con 12 pose/s ogni posa deve
  essere un disegno a sé, altrimenti sembra solo un'animazione che va a scatti.

Le hitbox e la simulazione restano a 60 fps: lo stepping riguarda solo l'aspetto.
Per il progetto è una **scelta di stile da decidere** (owner), non da applicare di default.

---

## 6. Reazioni: hitstun, knockback, tumble

La vittima vende il colpo. Un colpo forte con una reazione debole sembra debole.

- **Direzione**: la prima posa di reazione spinge la parte colpita **nella direzione del
  colpo**, e la testa resta indietro di 1-2 f (drag). Se il colpo arriva al ventre, il
  bacino va indietro e testa e piedi restano avanti (forma a C).
- **Intensità a livelli**: 2-3 animazioni di hurt (leggero, medio, forte) scelte in base
  al danno o al knockback, più una per colpi dall'alto e dal basso.
- **Durata**: l'animazione di hitstun deve durare **almeno** quanto l'hitstun di gioco,
  e la posa finale deve indicare che la vittima torna a poter agire (vicina alla guardia
  o pronta per il controllo aereo).
- **Knockback forte / tumble**: il corpo gira su un asse coerente col lancio, gli arti
  seguono passivi (qui il secondary è legittimo, ma resta smorzato), la rotazione rallenta
  se il lancio rallenta.
- **Hitstop + shake**: durante l'hitstop la vittima tiene la **prima** posa di reazione
  (non la posa neutra) e vibra lungo la direzione del colpo; la vibrazione è del gioco,
  non della clip.

---

## 7. Mosse aeree e atterraggio

- **Il COM non lo comanda la clip**: in aria la traiettoria è del movimento del personaggio
  (fisica di gioco). La clip ruota e deforma il corpo **attorno** al COM. Un aereo che
  sposta molto il bacino nella clip fa sembrare che il personaggio "nuoti" nell'aria.
- **Aerei verso il basso (dair)**: la parte che colpisce (piedi o gambe) va verso il basso
  con slow-out iniziale (raccoglie le gambe, 3-8 f) e poi uno scatto (2-3 f) fino
  all'estensione; il busto si inarca all'opposto (contrapposto aereo).
- **Recovery in aria**: deve tornare verso una posa aerea neutra (gambe raccolte), non
  verso la guardia a terra. Se il personaggio tocca terra durante la mossa, vince il
  landing.
- **Landing lag**: frame di atterraggio non annullabili dopo un aereo.
  Valori indicativi (serie Smash): 4-8 f mosse leggere, 10-20 f mosse forti.
  L'animazione di atterraggio deve:
  1. fare lo squash di assorbimento con il punto più basso 2-4 f dopo il contatto;
  2. durare **esattamente** il landing lag (o meno, e poi tenere la posa);
  3. finire con velocità quasi nulla, pronta alla guardia.
- **Autocancel**: se un aereo può atterrare "pulito" in alcuni frame, la posa di quei
  frame deve essere compatibile con un atterraggio normale (gambe sotto il corpo).

---

## 8. Moto secondario nei picchiaduro: poco, corto, deciso

### 8.1 Il principio

I giochi d'azione di qualità usano **meno** moto secondario dei film, non di più: ogni
frame di oscillazione dopo l'azione confonde la lettura della fase della mossa. La regola
pratica è che **ogni moto secondario finisce prima del FAF** e non ricomincia senza una
causa nuova.

### 8.2 Come lo gestiscono i giochi di riferimento

- **Super Smash Bros. Ultimate**: personaggi 3D con mantelli, capelli e code spesso
  simulati, ma fortemente smorzati e che seguono il corpo; gli arti dei personaggi tornano
  in posa in modo deciso, con un piccolo overshoot. La leggibilità viene da pose esagerate,
  scie colorate e hitstop forte.
- **Guilty Gear Xrd / Strive** (Arc System Works; GDC 2015, J. Motomura, "Guilty Gear Xrd's
  Art Style: The X Factor Between 2D and 3D"): animazione 3D a pose tenute senza
  interpolazione, ogni frame ritoccato a mano, anche deformando il modello per la
  telecamera. Capelli e vestiti sono **animati a mano** per pochi frame attorno all'azione
  e si assestano in fretta; niente fisica che continua a muoversi.
- **Rivals of Aether** (pixel art): poche pose per mossa, ogni posa tenuta più frame;
  il secondary sono 1-3 sprite di overlap su code/capelli dopo l'impatto, poi fermo.
- **Stile Arcane / Spider-Verse** (riferimento cinematografico stilizzato): animazione
  per lo più on twos, pose molto disegnate, overlap scritto a mano e breve; il movimento
  "pittorico" viene dalle forme e dagli effetti, non da oscillazioni lunghe.

Conclusione comune: overlap e follow-through ci sono, ma sono **brevi, una volta sola, e
finiscono in posa**.

### 8.3 Come ragionare sul moto secondario in una mossa

Ordini di grandezza, con il perché (non soglie):
- **Arto che colpisce**: è la guida, non ha ritardo; dopo l'azione è controllato dal muscolo,
  quindi al più un piccolo overshoot e poi va in posa.
- **Arto libero e testa**: seguono con un ritardo breve e si assestano presto, perché il
  personaggio li controlla per restare in equilibrio e guardare il bersaglio.
- **Parti passive** (accessori, capelli, antenne): ritardo maggiore e qualche oscillazione,
  ma con decadimento netto; nei giochi tendono a finire entro la fine della mossa perché
  il blend verso la mossa successiva taglierebbe un moto ancora in corso.

Quando il secondario è troppo, di solito la correzione migliore è **togliere** oscillazioni,
non smorzarle un po' di più: il problema è che esistono senza causa, non quanto sono ampie.

### 8.4 Esempio illustrativo: secondario senza causa dopo un colpo aereo

*Esempio, non regola*: i frame sono inventati per mostrare il principio di §8.1 e del doc 1
§7.3. Mossa ipotetica: aereo verso il basso, startup f0-f11, attivo f12-f17, contatto f14,
FAF f36.

Da evitare:
- f14 contatto, hitstop congela su una posa in cui le ginocchia erano a metà flessione.
- Alla ripartenza le gambe oscillano: estese f17, piegate f21, estese f25, piegate f29,
  ancora un rimbalzo a f33-f35. Cinque inversioni senza nessuna causa nuova.

Una possibile versione coerente con il principio (non l'unica):
- f11-f13 gambe in accelerazione verso il basso, spacing crescente, estensione piena a f13.
- f14 posa d'impatto pulita (gambe tese, punte giù, busto inarcato), tenuta dall'hitstop.
- f15-f18 piccolo rimbalzo delle gambe verso l'alto (energia restituita dal bersaglio),
  una volta e piccolo.
- f18-f30 ritrazione **intenzionale** in posa aerea raccolta, slow-out e poi slow-in.
- f30-f36 moving hold in posa aerea; nessuna oscillazione al FAF.

---

## 9. Note per il progetto

- **60 fps** di animazione, frame numerati da 0: tutti i numeri qui sono già in quella scala.
- **Hitstop e shake li fa il codice di combattimento**: la clip non contiene frame duplicati
  per simulare l'hitstop.
- **Le chiavi sono dell'owner**: questi principi servono a ragionare su breakdown, spacing,
  overlap e settle **tra** le sue chiavi, e a spiegare perché una variante non funziona.
  Se un principio qui sembra contraddire una chiave, vince la chiave e si segnala il dubbio.
- **Regola 10 del progetto**: nessuna variante va all'owner senza che qualcuno abbia
  guardato il video in-game. Il §10 sotto è il metodo per quella revisione.

---

## 10. Revisione frame per frame del video in-game

Preparazione: video in-game con la telecamera reale, versione "colpisce" e versione "a
vuoto"; estrarre i frame attorno al **contatto** e attorno al **FAF**. Si applica il metodo
del doc 1 §13 (intenzione → previsione dai principi → misura → spiegazione), con queste
domande in più. Le risposte vanno ricavate per la mossa in esame; nessuna voce contiene
valori fissi.

**Startup**
- Dalla prima posa si capisce quale mossa sta partendo?
- C'è un'anticipazione (anche solo interna) compatibile con lo startup disponibile?
- La parte che colpisce accelera fino al contatto? Il centro del corpo parte prima dell'arto?

**Impatto**
- Il frame d'impatto (quello tenuto dall'hitstop) è una posa chiave pulita, o una posa di
  passaggio congelata?
- Alla dimensione e con la telecamera di gioco si vede chi colpisce, con cosa e verso dove?
- La linea d'azione è coerente con la direzione del knockback?
- La reazione della vittima va nella direzione del colpo?

**Dopo l'hitstop**
- La ripartenza libera l'energia (movimento ampio subito, poi decelera)?
- Per ogni oscillazione dopo l'impatto: qual è la causa? Se non c'è, è un errore.
- La versione "a vuoto" è coerente con quella "colpisce" (stessa mossa, energia non ceduta)?

**Recovery e FAF**
- Il recovery mostra un costo proporzionato alla forza della mossa?
- Al FAF il corpo è fermo e pronto per qualunque azione, o qualcosa è ancora in moto?
- Il passaggio alla mossa successiva o alla guardia è pulito?
- Negli aerei: l'atterraggio assorbe e dura quanto il landing lag?

Uso: le domande guidano lo sguardo, non danno un punteggio automatico. Per ogni problema si
scrive il frame, il principio coinvolto e il ragionamento; le domande di **Impatto**, **Dopo
l'hitstop** e **FAF** sono quelle che pesano di più sulla lettura della mossa.

---

## Fonti

- Tutte le fonti del doc 1 (Williams, Thomas & Johnston, Blair, corsi di body mechanics).
- Junya C. Motomura (Arc System Works), *GuiltyGearXrd's Art Style: The X Factor Between
  2D and 3D*, GDC 2015: animazione limitata in 3D, pose tenute, deformazioni per frame.
- Frame data pubblicati dalla community (ultimateframedata.com per Smash Ultimate, Dustloop
  wiki per Guilty Gear Strive, SuperCombo wiki per Street Fighter): startup, attivi,
  recovery, landing lag, hitlag.
- Masahiro Sakurai, canale YouTube *Masahiro Sakurai on Creating Games* (2022-2024):
  episodi su hitstop, esagerazione delle pose e leggibilità delle mosse.
- Talk e articoli di animatori di giochi d'azione (GDC Animation Bootcamp; Jonathan Cooper,
  *Game Anim: Video Game Animation Explained*, 2019, CRC Press): animazione al servizio del
  gameplay, leggibilità, frame di anticipazione contro reattività, blend e cancel.

I valori di frame sono indicativi e servono come punto di partenza; la frame data reale
delle mosse del progetto è quella dei dati di combattimento e delle chiavi dell'owner.

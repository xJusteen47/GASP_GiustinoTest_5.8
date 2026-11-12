# Overlord: jab 1, jab 2 e down-air visti da dietro e di lato (2026-11-12)

Applicazione di `docs/notes/anim-theory/silhouette-multidirezionali.md` (qui "doc 3") alle
tre mosse. L'owner giudica **soprattutto da dietro (camera di gioco) e di lato**: una mossa
passa solo se regge in tutte e due.

**Sono suggerimenti per il prossimo giro di pose, non decisioni.** Ricavati dagli handover
(`anim-jab-5.md`, `overlord_jab_v3_pose_plan.md`, `dair-v7-rereview.md`) e dalla geometria:
**non ho guardato le catture**. Il design delle mosse resta dell'owner; le sue chiavi vincono.
Personaggio pesante, stile giocattolo, 60 fps, pose molto spinte e leggibili da lontano.

## Jab 1 (schiaffo di rovescio, 29 f, attivi 6-8)

Causa del difetto visto in v3 (braccio che entra nello schermo e sparisce): il piano v3
mette il petto di lato a 80-90° e il braccio teso in orizzontale verso il bersaglio. Di lato
è una linea lunga; da dietro spalle e braccio sono sull'asse della camera, 0% visibile, e la
spalla vicina copre tutto (doc 3 §1, §2.1, §2.4).

1. **Petto a tre quarti (circa 55-65°), non a 80-90°.** La spalla destra esce dal contorno
   visto da dietro.
2. **Contatto a circa 30° in fuori e 20° in su**, non dritto avanti in orizzontale: da
   dietro si vede circa il 58% del braccio, di lato l'88% (prima 0% e 100%). L'hitbox resta
   dov'è nei dati: va solo controllato che la mano la attraversi sui frame 6-8.
3. **Arco inclinato**: il rovescio parte basso dal fianco opposto e finisce alto e in fuori
   (o al contrario). Uno schiaffo orizzontale ad altezza spalla è di taglio per tutte e due
   le viste (doc 3 §1).
4. **Carica leggibile da dietro**: gomito destro alto, sopra la linea della spalla, e spalle
   contro-ruotate. Con la mano nascosta davanti al petto, da dietro la carica non esiste.
5. **Follow-through tenuto fuori dal contorno**: mano oltre la spalla destra, palmo o dorso
   di faccia in diagonale (doc 3 §2.9). Risponde anche alla "tenuta statica" di v3: la posa
   tenuta è quella larga, con moving hold.
6. Braccio sinistro piegato sul busto: da dietro è invisibile. Va bene se non deve
   raccontare nulla; altrimenti il gomito deve sporgere dal fianco (scelta dell'owner).
7. Gambe "accovacciate" (v3): da dietro due gambe piegate uguali sono twinning. Appoggio
   sfalsato in avanti e di lato, peso su una gamba (doc 3 §2.8).

## Jab 2 (schiaffo ad artiglio con passo, 53 f, attivi 9-11)

1. **La posa tenuta "guarda la mano" con l'artiglio davanti al viso da dietro è coperta da
   testa e busto.** È la firma della mossa e la camera di gioco non la vede. Proposta: mano
   tenuta di lato e in alto (circa 40° in fuori, sopra la spalla, gomito in fuori), testa
   girata verso la mano. Da dietro si vedono il profilo della testa e l'artiglio accanto; di
   lato la mano resta davanti e sopra il viso.
2. **Mano al contatto più alta** (v3: troppo bassa): sopra la linea delle spalle esce dal
   contorno da dietro.
3. **Carica f2-f6 (v3: ferma)**: mano che sale in alto e indietro in fuori. Da dietro è la
   fase che si vede meglio, è il telegrafo (doc 3 §3.2).
4. **Tremito della mano**: un tremito avanti-indietro da dietro non si vede, uno laterale di
   lato non si vede. Asse verticale o diagonale, oppure rotazione del polso con le dita
   aperte (cambia la sagoma in ogni vista).
5. **Passo di 30 cm**: da dietro un passo dritto è quasi invisibile. Segnali: bacino che
   scende e risale, ginocchio che si alza, busto che si inclina, appoggio più largo
   all'arrivo, eventualmente passo leggermente diagonale (doc 3 §3.4). Il difetto di codice
   della capsula che scende di 12 cm è un'altra cosa e va corretto prima di giudicare.
6. Dita aperte: palmo di faccia in diagonale, mai di taglio verso una delle due camere.

## Down-air (pestone, attivi 15-17, colpo in 2 frame)

Il pestone è verticale: è la direzione che entrambe le viste vedono (doc 3 §2.2). I rischi
sono altri.

1. **Simmetria**: v7 aveva torsione zero e arti specchiati. Da dietro è una colonna. La
   chiave torta dell'owner (colonna [27,35,0], bacino ruotato, poli asimmetrici), già decisa
   per v8, risolve tutte e due le viste. Non appiattirla.
2. **Gamba che pesta fuori dalla linea centrale** di qualche centimetro e **gamba libera
   raccolta con il ginocchio in fuori**: da dietro due gambe in fila sono una sola.
3. **Braccia**: incrociate o raccolte sul petto da dietro spariscono. Devono uscire dal
   contorno (gomiti in fuori, oppure un braccio in alto e uno in contro-posa), asimmetriche.
4. **Colpo in 2 frame**: la corsa verticale del piede si vede da ogni vista; la scia sotto
   il piede è aperta in entrambe. Controllare che il frame congelato dall'hitstop non sia un
   frame di passaggio (doc 2 §2.2).
5. **Peso**: bacino che scende e busto che si chiude all'atterraggio si leggono da ogni
   vista; sono il segnale "pesante" più sicuro.

## Cosa serve alla catena di cattura

La revisione su due viste chiede, per la stessa mossa, **due riprese in PIE**: camera di
gioco dietro (pitch e distanza reali) e camera di lato, a segno e a vuoto. Se oggi la cattura
in gioco ha solo la camera di gioco, va aggiunta la laterale prima del prossimo giro.

## Domande per l'owner (non bloccanti)

1. Jab 1 con contatto in diagonale (30° in fuori, 20° in su) al posto del braccio dritto
   avanti: va bene come direzione, o il braccio dritto è irrinunciabile? In quel caso serve
   un aiuto di camera o di VFX (doc 3 §4-5).
2. Jab 2: la mano "guardata" di lato e in alto invece che davanti al viso.
3. Se per una mossa le due viste non si conciliano, quale vince: dietro o lato?

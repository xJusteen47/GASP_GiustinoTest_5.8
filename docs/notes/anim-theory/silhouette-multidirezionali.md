# Silhouette leggibili da più viste (dietro e di lato)

Terza parte del riferimento. **Prerequisiti: `principi-animazione-corporea.md` (doc 1) e
`animazione-picchiaduro-action.md` (doc 2).** Qui c'è solo quello che cambia quando la stessa
mossa deve leggersi da **due viste**: la camera di gioco **dietro** il personaggio e la vista
**di lato**. L'owner giudica le mosse soprattutto da queste due (2026-11-12).

**Solo teoria generale**, come gli altri due: niente correzioni di mosse passate, numeri solo
come intervalli indicativi. Le note sulle mosse dell'Overlord stanno a parte, in
`docs/notes/reviews/overlord-silhouette-viste-2026-11-12.md`.

Dove una regola viene da una fonte è citata (§9). Dove è **ragionamento geometrico nostro** è
marcata **[geom]**: va verificata sull'occhio, in gioco.

---

## 0. Cosa c'era già (non ripetuto qui)

- Doc 1 §11: silhouette nera su bianco, linea d'azione, twinning, tangenze. Vale per **una**
  vista alla volta.
- Doc 2 §2.2 e §4: la posa d'impatto è la più leggibile; test miniatura a ~120 px; "la parte
  che colpisce deve essere visibile da tutte le telecamere che il gioco usa (anche le viste
  posteriori)". Dice **che** deve leggersi, non **come** ottenerlo.
- Doc 2 §3: smear, multipli, scie.
- Strumenti: la griglia di `animpipe review --visual` ha già le pose chiave dalla camera di
  gioco e da dietro con silhouette colorate per parte; `Tools/AnimPipeline/legibility.py`
  misura quanto dell'arto si vede; la rubrica ha le voci `silhouette` e `parte_che_colpisce`.
- Pilota del jab (2026-10-31): spostando la linea del pugno 12 cm in fuori e la testa fuori
  linea, il braccio visibile da dietro è passato dal 10% al 43%. È il caso pratico da cui
  parte questo documento.

Mancava: perché una posa sparisce da una vista, come costruirla perché regga in due, e quali
aiuti esistono fuori dall'animazione (VFX, camera, regole di gioco).

---

## 1. Cosa vede ogni vista [geom]

Tre direzioni rispetto al personaggio: **A** avanti (verso il bersaglio), **L** laterale,
**V** verticale. Una camera non vede il movimento lungo il proprio asse.

| Vista | Vede bene | Perde |
|---|---|---|
| Dietro, camera orizzontale | L, V | **A** (e il busto copre quello che sta davanti) |
| Lato | A, V | **L** (e il braccio vicino copre quello lontano) |
| Dall'alto (top-down) | A, L | V |

Conseguenze:

- **V è l'unica direzione che entrambe le viste vedono.** A si legge solo di lato, L solo da
  dietro.
- **Camera alzata**: se la camera dietro guarda in giù di un angolo θ, uno spostamento in
  avanti di d si vede sullo schermo come uno spostamento verso l'alto di circa d·sin θ.
  A 15° si vede il 26%, a 25° il 42%, a 35° il 57%. Il pitch reale della camera di gioco
  decide quanto "avanti" sopravvive: va misurato sulla camera vera, non stimato.
- **Lunghezza visibile di un arto** che punta nella direzione (a, l, v) di lunghezza 1:
  da dietro √(l²+v²), di lato √(a²+v²).

| Direzione dell'arto | Visibile da dietro | Visibile di lato |
|---|---|---|
| Dritto avanti | 0% | 100% |
| 30° in fuori | 50% | 87% |
| 30° in fuori e 20° in su | 58% | 88% |
| 45° in fuori e 30° in su (o in giù) | 79% | 79% |
| Tutto laterale | 100% | 0% |
| Verticale | 100% | 100% |

- **La lunghezza non basta: conta l'occlusione.** Da dietro il busto e la testa sono davanti
  a tutto. Un arto che punta avanti dentro la larghezza delle spalle è invisibile anche se è
  inclinato. Si vede solo la parte che **esce dal contorno** di busto e testa: più larga delle
  spalle, sopra la testa, o fuori dal fianco.
- **Un arco visto di taglio è una linea.** Un arco orizzontale (schiaffo ad altezza spalla)
  è di taglio per **ogni** camera orizzontale: di lato sembra un avanti-indietro, da dietro
  un destra-sinistra. Inclinare il piano dell'arco lo apre in tutte e due le viste.

---

## 2. Regole di posa

Valgono sulle pose che il giocatore vede a lungo: carica tenuta, frame di contatto (lo
congela l'hitstop, doc 2 §2.2), follow-through tenuto.

1. **Mai sull'asse di una camera.** L'arto che colpisce non punta dritto avanti né tutto di
   lato. Indicativo: almeno metà della sua lunghezza visibile in **ciascuna** delle due viste
   (tabella §1). In pratica: 25-45° in fuori più una componente verticale. [geom]
2. **Il verticale è gratis.** Colpi dall'alto in basso, dal basso in alto, in diagonale si
   leggono da tutte e due le viste. Un colpo dritto e orizzontale è il caso peggiore. [geom]
3. **Estremità fuori dal contorno, in tutte e due le viste.** Mano, piede o punta dell'arma
   escono dal contorno di busto e testa. "Dobbiamo vedere il pugno prima che parta, senza
   eccezioni" (Lozada).
4. **Busto a tre quarti, non di profilo pieno.** Ruotare il busto (doc 1 §11) fa uscire la
   spalla che colpisce dal contorno. Ma con il petto a 90° il corpo visto da dietro diventa
   una lama stretta e il braccio si allinea all'asse della camera. Tra 30° e 60° circa tutte
   e due le viste vedono una faccia del busto e l'arto esce di lato. [geom]
5. **Gomiti e ginocchia in fuori o in alto.** Un'articolazione piegata che sporge mostra
   l'arto anche quando la mano è davanti al corpo. Doc 2 §4 lo chiede già come esagerazione:
   qui serve a rompere il contorno nella vista che altrimenti lo perde.
6. **Tutto il corpo racconta la mossa.** "Il giocatore legge lo stato del personaggio
   soprattutto dal contorno" (Ahn). Torsione spalle contro bacino (cambia la larghezza del
   busto visto da dietro), inclinazione, abbassamento del bacino, testa: si vedono da ogni
   vista. Prova: copri l'arto che colpisce; fase e direzione si capiscono ancora?
7. **La contro-posa come secondo indicatore.** L'arto libero va dalla parte opposta e in
   fuori. Da dietro spesso è l'unico arto che si vede bene: usalo, senza twinning.
8. **Niente arti in fila lungo l'asse della camera.** Due gambe o due braccia una davanti
   all'altra si fondono in una. Sfalsale in L (per la vista da dietro) e in A (per il lato). [geom]
9. **Presentare il lato largo.** Un oggetto piatto (mano aperta, lama, scudo) mostra la
   faccia larga alla camera che conta. Lozada tiene "il lato dell'arco quasi sempre verso chi
   guarda". Con due viste: faccia larga in diagonale, mai di taglio per una delle due.

Quando le due viste chiedono cose opposte e non c'è una posa che regga in entrambe, **non si
fa la media**: una posa tiepida fallisce in tutte e due. Si chiede all'owner quale vista vince
per quella mossa, oppure si usa un aiuto dei §4-5.

---

## 3. Regole di movimento

1. **Carica e follow-through portano la lettura.** Il contatto passa in 1-3 f; l'occhio
   ricostruisce il colpo dalla posa prima, dalla posa dopo e dalla scia: lo smear "aiuta il
   cervello a riempire quello che è appena successo" e la carica "deve essere assolutamente
   chiara" (New Frame Plus, analisi di Link in Smash). Quindi carica e follow-through passano
   le regole del §2 in tutte e due le viste, non solo il contatto.
2. **Da dietro la carica è la fase che si vede meglio**: la mano viene verso la camera o
   esce di lato. È il telegrafo naturale per la camera di gioco. Una carica nascosta davanti
   al petto spreca l'unica fase che quella vista regala. [geom]
3. **Archi larghi che attraversano il corpo.** L'estremità parte fuori da un bordo del
   contorno e finisce fuori dall'altro (o sopra/sotto). Piano dell'arco inclinato, mai di
   taglio (§1). Doc 1 §9 resta valido: l'arco va controllato in **ogni** vista.
4. **Il movimento in profondità si traduce in segnali visibili.** Un passo o un affondo in
   avanti da dietro è quasi invisibile. Lo rendono leggibile: bacino che scende e risale,
   ginocchio che si alza, busto che si inclina, appoggio che si allarga, passo leggermente
   diagonale. [geom]
5. **Silhouette diverse fra le fasi, in ogni vista.** Doc 2 §4 chiede pose distinte: va
   verificato separatamente da dietro e di lato. Due pose diverse di lato possono essere la
   stessa sagoma da dietro.

---

## 4. Aiuti che funzionano da ogni vista

- **Scia 3D lungo l'arco** (doc 2 §3). È un nastro nello spazio: si legge da ogni vista
  tranne quando l'arco è di taglio. In Smash "il percorso della lama è del tutto chiaro
  grazie a quello smear luminoso" (New Frame Plus).
- **Scala uniforme della parte che colpisce** (pugno, piede, arma) sui frame attivi. Guilty
  Gear Xrd scala arti, mani e piedi "per esagerare la prospettiva", ma lo fa per **una**
  camera fissa (il gioco "si gioca comunque su un piano 2D"). Con due viste funziona solo
  l'ingrandimento uniforme, non la deformazione verso una camera. Dipende dal rig.
- **Contrasto di valore e colore** fra estremità e busto (guanti, polsini, arma chiara su
  corpo scuro). Risolve l'arto **dentro** il contorno (braccio davanti al busto visto di
  lato). Non risolve l'occlusione da dietro. In Team Fortress 2 i dettagli interni "riprendono
  le forme della silhouette" e i personaggi vanno riconosciuti "a varie distanze e punti di
  vista".
- **Il bersaglio racconta metà del colpo.** Da dietro l'attaccante dà la schiena ma il
  bersaglio è di fronte alla camera: reazione, direzione del knockback e scintilla d'impatto
  nel punto di contatto sono pienamente visibili. Vanno giudicati insieme alla posa.
- **Ombra a terra e marcatori di profondità**: in 3D "è spesso difficile cogliere con
  precisione la distanza in profondità" (Ninomiya, Kirby e la terra perduta).

---

## 5. Soluzioni di camera e di regole di gioco

Non sono scelte dell'animatore: sono **decisioni dell'owner**. Stanno qui perché a volte il
problema non si risolve con la posa.

- **Altezza e pitch della camera**: più è alta, più l'avanti si vede (§1). È la leva più
  forte per i colpi dritti.
- **Scostamento laterale (camera sulla spalla)**: toglie il busto dalla linea fra camera e
  colpo. Aiuta i colpi della mano dal lato scoperto, peggiora l'altro e complica la mira.
  Batman: Arkham Asylum scosta la camera in esplorazione ma in combattimento "si allontana e
  si centra" (Buehler).
- **Camera di combattimento**: più distanza durante la lotta (prototipo Daedalic); in God of
  War (2018) la camera segue gli scarti laterali e si riorienta verso gli attacchi (Sheth).
- **Tenere il bersaglio in vista**: in God of War i nemici colpiti restano rivolti alla
  camera, le reazioni li spostano poco, c'è un limite all'altezza del juggle (Sheth).
- **Colpo generoso in profondità**: "se sullo schermo sembra che un attacco debba colpire,
  facciamo in modo che colpisca" (Kamiyama, Kirby). Compensa il fatto che da dietro la
  distanza in avanti non si giudica.
- **Animazioni specchiate nella vista laterale**: da Smash 4 i personaggi specchiano pose e
  mosse quando si girano, così mostrano sempre il davanti alla camera ("Kirby sta bene da
  ogni angolo", Sakurai). Vale solo con camera laterale fissa e cambia la mano che colpisce.
- **Corpo che copre l'azione**: dithering del personaggio vicino alla camera (The Witcher 3)
  o sagoma mostrata attraverso gli ostacoli (Super Mario Sunshine) (Buehler). Ultima risorsa.

---

## 6. Come lo risolvono i generi

| Genere | Camera | Soluzione tipica |
|---|---|---|
| Platform fighter (Smash) | lato, fissa | pose pensate per quella vista, animazioni specchiate, scie, pose estreme di carica e follow-through |
| Picchiaduro 2.5D (Guilty Gear Xrd) | lato, fissa | ogni frame posato "nel modo migliore" per quella camera, scala degli arti; non trasferibile a camera libera |
| Action in terza persona (God of War, Arkham, Kirby) | dietro | regole di camera e di bersaglio, colpo generoso, VFX; l'animazione deve "funzionare da ogni angolo" e "barare è fuori discussione" (Cooper) |
| Dall'alto / co-op | alta | vede A e L, perde V: pose larghe a terra, altezza comunicata con ombra e scala [geom, nessuna fonte primaria letta] |

Il progetto sta fra la prima e la terza riga: due viste da soddisfare insieme. Per questo le
regole del §2 chiedono diagonali e verticale invece di una posa ottimizzata per una camera.

---

## 7. Checklist per chi anima

Domande guida, da fare sulle chiavi prima di costruire gli inbetween.

1. Carica, contatto, follow-through: in ognuna, l'estremità che colpisce esce dal contorno di
   busto e testa **da dietro**? E **di lato**?
2. L'arto che colpisce punta lungo l'asse di una delle due camere? Quanto ne resta visibile
   in ciascuna (tabella §1)?
3. Il colpo ha una componente verticale o diagonale?
4. Il busto è a tre quarti o è finito di profilo pieno o di piatto?
5. L'arco è aperto in tutte e due le viste o in una è di taglio?
6. Coprendo l'arto che colpisce, il resto del corpo dice ancora fase e direzione?
7. Arto libero: fa da contro-posa visibile o è nascosto in fila dietro l'altro?
8. Passo o affondo in avanti: quale segnale lo rende visibile da dietro?

## 8. Checklist per chi revisiona

Sul video **in gioco** (regola 10, doc 2 §10), con **due riprese della stessa mossa**: camera
di gioco dietro, con il suo pitch e la sua distanza reali, e camera di lato. Colpo a segno e
colpo a vuoto. Dimensione di gioco, non ingrandita.

1. Frame di contatto (quello congelato dall'hitstop), **da dietro**: si vede cosa colpisce e
   verso dove?
2. Stesso frame, **di lato**: stessa domanda.
3. Carica tenuta e follow-through tenuto: stesse due domande.
4. Test miniatura (~120 px) in ognuna delle due viste.
5. Le sagome di carica, contatto e recovery sono diverse fra loro **in ogni vista**?
6. Scia e scintilla d'impatto: aiutano in tutte e due le viste o in una sono di taglio?
7. Da dietro, la reazione del bersaglio completa la lettura del colpo?
8. C'è un arto che sparisce per più di 2-3 f consecutivi in una vista durante una posa tenuta?

**Esito**: passa solo se le domande 1, 2 e 3 hanno risposta sì in **tutte e due** le viste.
Una vista sola non basta. Se una vista non può passare senza rovinare l'altra, l'esito è
"chiedi all'owner" con le due alternative, non una media.

---

## 9. Fonti

- Jonathan Cooper, *Game Anim: Video Game Animation Explained* (estratto "The 12 principles
  of animation in video games"): https://www.gamedeveloper.com/production/the-12-principles-of-animation-in-video-games
- Jamie Lozada, "Studying Game Animation Principles", 80 Level: https://80.lv/articles/studying-game-animation-principles/
- Rex Ahn, "1st-Person vs 3rd-Person Animation", Animworks: https://anim.works/?p=23594
- New Frame Plus, "How to Animate a Smash Bros Attack (Link)", trascrizione: https://amara.org/subtitles/I8Bnr4dEmv1u/en/1/download/How%20to%20Animate%20a%20Smash%20Bros%20Attack%20%20LINK.en.txt
- Dan Fornace (Rivals of Aether), "la silhouette è la cosa più importante": https://gamedeveloper.com/design/why-one-developer-says-fighting-game-devs-need-to-think-about-silhouettes
- Masahiro Sakurai sulle animazioni specchiate (2014): https://www.siliconera.com/?p=399745 ;
  SmashWiki, "Stance mirroring": https://ssbwiki.com/Stance_mirroring
- Junya C. Motomura, *GuiltyGearXrd's Art Style: The X Factor Between 2D and 3D*, GDC 2015,
  slide: https://www.ggxrd.com/Motomura_Junya_GuiltyGearXrd.pdf
- Mitchell, Francke, Eng, *Illustrative Rendering in Team Fortress 2*, 2007: https://www.cs.princeton.edu/courses/archive/fall07/cos597B/papers/mitchell-team-fortress.pdf
- Nintendo, "Ask the Developer: Kirby and the Forgotten Land", parte 2: https://www.nintendo.com/sg/interview/arzg/02.html
- Mihir Sheth, *Evolving Combat in 'God of War' for a New Perspective*, GDC 2019:
  https://gdcvault.com/play/1026423/Evolving-Combat-in-God-of . **Letto solo un riassunto**
  (https://videohighlight.com/v/hE5tWF-Ou2k), non il video: i dettagli vanno presi con riserva.
- Andreas Buehler, "Third-person camera view in games", Game Developer 2019: https://www.gamedeveloper.com/design/third-person-camera-view-in-games-a-record-of-the-most-common-problems-in-modern-games-solutions-taken-from-new-and-retro-games
- Daedalic, "Six ingredients for a dynamic third person camera", blog Unreal Engine: https://www.unrealengine.com/tech-blog/six-ingredients-for-a-dynamic-third-person-camera

Limiti della ricerca: non ho trovato una fonte primaria che tratti **insieme** camera dietro
e vista laterale per la stessa mossa. Le fonti dicono che deve funzionare da ogni angolo e
danno soluzioni per una camera alla volta; le regole su diagonali, verticale e archi inclinati
(§1-3) sono dedotte dalla geometria e vanno confermate guardando il gioco.

# Principi dei VFX di gioco: teoria generale e checklist

Quarto documento di teoria, gemello di quelli sull'animazione (`docs/notes/anim-theory/`). Richiesta owner
2026-11-19 02:05Z (cmsg_01CRvUdrWm1dJrM4q54YDBLNCY9bE6qgMhjbRePFkZtCLq): "indagare la teoria e i principi dei VFX allo
stesso modo dell'animazione". Thread cloud, Opus medium, solo ricerca, nessuna modifica al gioco.

**Solo teoria generale**, con la stessa regola dei documenti di animazione (owner 2026-11-07): niente correzioni di
effetti già fatti, niente regole nate da un solo effetto; i numeri sono **intervalli indicativi**, il verdetto è
sempre a occhio sul video in gioco (regola 10). Le regole operative del nostro kit restano in
`handoffs/stile-sheet-vfx-review.md`, `vfx-stile-v2-astrobot.md`, `vfx-stile-v3-categorie.md`; qui c'è il **perché**.
La checklist per le review Fable sta in `vfx-theory/gstn-vfx-reviewer.SKILL.md`.

Legenda: **[F n]** = detto da una fonte (§17). **[O]** = osservato nei riferimenti dell'owner (Astro Bot `astro_1-13`,
clip DK Bananza 2026-11-19). **[A]** = adattamento o ragionamento nostro, da verificare in gioco.
**[G]** = aggiunto dopo la peer review di Gemini (2026-11-19, `docs/notes/reviews/agy-vfx-theory/esito.md` su
`claude/locomotion-finish-1` e04141c7); cosa è stato accettato e cosa no è in §16-bis.
Frame: contati da 1; sim a 62,5 Hz, 1 frame ≈ 16 ms.

**Lezioni dell'owner 2026-11-19** (dopo i video 1080p del fumo v3; ambito: tutti i VFX, il punto 2 i gas). Sono
principi, non ricette; dove contraddicono il testo sotto, vincono loro e il testo è già allineato.
1. **La leggibilità non si compra uccidendo il realismo.** Opacità piena, taglia gonfiata o densità alta fanno
   "leggere" una nuvola ma la trasformano in un PNG piatto. Un gas si giudica su due domande insieme: si legge? sembra
   polvere (fumo, neve) vera? (§5.6-bis)
2. **La polvere vera si disperde**: mentre cresce sfuma fino ad alpha 0. Non si rimpicciolisce restando opaca (§3.13, §8.5).
3. **"Stile DK / Astro Bot" = comportamento realistico, stile solo nel colore e nella palette.** Non usare
   "stilizzato" per il comportamento: fisica, dispersione e luce sono reali (§7).
4. **Si giudica sempre contro il video di riferimento** (il dossier, `docs/notes/review/dossier-riferimento.md`), non
   solo contro la checklist: una checklist tutta "sì" non basta se accanto al riferimento non torna (§12.5).

## 0. Cosa c'era già (non ripetuto, solo richiamato)
- `vfx-clarity-ricerca.md` (11-17): Sakurai su hitstop e risalto dei personaggi, gerarchia Riot, portata onesta, ombre di 3D Land.
- `silhouette-vfx-ricerca.md` (11-15, approvato): la posa porta le fasi lunghe, i VFX le fasi brevi (frame attivi, contatto);
  review in due passate, VFX spenti e poi accesi.
- `stile-sheet-vfx-review.md`: regole di costruzione e checklist D 1-21 (v1/v2).
- `vfx-stile-v3-categorie.md` (11-18): tre categorie di materiale (A detriti solidi, B gassosi, C lampi emissivi).
- `ref-astrobot-vfx/NOTE.md` (+ clip DK Bananza): riferimenti dell'owner.
Questo documento li mette in un quadro unico e aggiunge ciò che mancava: forma, timing in fasi, strati, valore,
fumo e polvere, scelta tecnica, prestazioni, processo. Le contraddizioni trovate fra i documenti sono in §15.

---

## 1. Che cos'è un VFX di gioco: informazione prima, bellezza dopo
1. **Un effetto di gioco è un messaggio.** Prima dice cosa è successo, dove, quanto è grave e cosa fare; poi è bello.
   I VFX "insegnano come si gioca" [F1, F2]. Se non porta informazione, è rumore e va tolto [F3].
2. **Animare un effetto è animare energia** [F4]. Ogni effetto ha una causa (un pugno, un piede che frena, un corpo che
   cade) e un'energia che si scarica nel tempo. Forma, velocità e durata discendono da quella causa; per questo si
   pensa a **catene causa-effetto**: colpo → lampo → schegge → polvere che ricade [F5].
3. **Gerarchia d'attenzione**: l'attenzione che un effetto prende è proporzionale alla sua importanza di gioco.
   "Se tutto è importante, niente è importante" [F6]. Da noi: voto d'importanza per evento, due voti (chi fa / chi subisce).
4. **Il personaggio viene prima dell'effetto.** Un effetto troppo vistoso "oscura" i personaggi [F7]: deve colpire senza
   coprire. Nel nostro progetto la posa porta le fasi lunghe e i VFX le brevi (approvato 11-15).
5. **Il VFX si progetta con la mossa, non dopo** [F8]: i problemi di leggibilità nascono quando gli effetti arrivano tardi.

## 2. Forma (shape language)
1. **La forma segue l'energia** [F5]. Scarica improvvisa = forme appuntite, spigolose, raggiate. Moto che rallenta o
   energia dolce = forme tonde, morbide. Per questo il colpo è una stella o una scheggia, la polvere una sfera.
2. **La forma è il primo canale del messaggio** [F2, F3]: buono/cattivo, pericolo/innocuo si leggono dalla forma prima
   che dal colore. Miyamoto: pericolo = appuntito o in fiamme [F9]. Corollario: **stessa funzione di gioco = stessa
   forma** in tutti i kit; il colore del kit cambia solo la tinta secondaria [F6].
3. **Primario, secondario, terziario** [F10]. Il primario porta il messaggio di gioco: valore alto, silhouette netta,
   opacità piena, un punto focale. Il secondario dà sapore (scintille, frammenti): valore più basso, forme più semplici.
   Il terziario è atmosfera (fumo, foschia): basso contrasto, moto lento, mai in competizione col primario.
4. **Grande, medio, piccolo** [F11]. Una forma grande dominante, poche medie, qualche piccola. Mai tutte della stessa
   taglia: senza contrasto di scala l'effetto diventa "rumore uniforme".
5. **Evitare simmetria perfetta, gemelli e "denti di sega"** [F5]: pezzi speculari, spaziature regolari, cerchi perfetti
   di increspature sembrano meccanici. Variare taglia, spaziatura e percorso. Eccezione voluta: i burst radiali puliti
   dello stile Nintendo (ventaglio regolare con piccolo jitter) sono una scelta di stile, non un errore [A].
6. **Bordi duri e morbidi insieme** [F10]: tutto morbido = nebbia illeggibile; tutto duro = adesivo. Il primario ha
   bordo netto, l'atmosfera bordi morbidi.
7. **Silhouette dell'effetto** [F5]: il dettaglio che esce dalla silhouette principale si perde. Prima la massa grande,
   poi i dettagli.
8. **Direzione nella forma** [F10]: allungare nella direzione del moto, bordo d'attacco più chiaro e netto, coda più
   scura e morbida. Così una forma ferma "va" da qualche parte. Un colpo si orienta lungo la direzione del colpo o del lancio.
9. **Spazio negativo e ritmo** [F12]: buchi, pause e intervalli fanno leggere le forme piene; un effetto senza vuoti è una macchia.
   Alternare zone dense ad alta frequenza (scintille, schegge) a superfici lisce di riposo (una massa di fumo pulita) [G].
10. **Pochi e grandi > tanti e piccoli** [F13, F14]: il dettaglio va nella forma o nella texture, non nel numero di
    particelle. Conta la **densità** giusta, che Sakurai indica come il parametro più importante degli effetti [F15].
11. **Squash and stretch anche negli effetti** [G, F5, F20]: stelle, sbuffi e onde non sono rigidi; si schiacciano contro
    il punto di contatto e si allungano nel verso del moto (stretch dalla velocità) prima di espandersi. Un effetto che
    cresce solo in scala uniforme sembra un adesivo che si ingrandisce.
12. **Orientamento giusto per ogni forma** [G]: con camera in terza persona libera, uno sprite rivolto alla camera si vede
    sempre di faccia ma non ha direzione nel mondo; dischi a terra e onde d'urto vanno **allineati al piano** d'impatto,
    stelle e scie allungate **all'asse** del colpo; ciò che deve leggersi da ogni lato ha volume (mesh).

## 3. Timing: le tre fasi
1. **Anticipazione → climax → dissipazione** [F10, F16, F11]. Ogni effetto ha queste tre fasi; si progettano come curve
   d'intensità nel tempo, non proprietà per proprietà [F11].
   - *Anticipazione* (carica, wind-up): avverte che sta per succedere. Nei VFX legati a un personaggio di solito
     l'anticipazione la fa **la posa**, non l'effetto; l'effetto di carica serve solo per mosse lente o caricate [A].
   - *Climax* (impatto, pop): il massimo di valore, saturazione, taglia e numero di elementi. Cade **nel primo quarto
     della vita** dell'effetto [F10, F17].
   - *Dissipazione* (erosione, caduta): il resto della vita. Più lunga, più morbida, a contrasto basso, e non deve distrarre [F16].
2. **Impulso o ritmo** [F11]. *Impulso istantaneo*: nessuna anticipazione, si apre già al massimo; è la scelta del colpo
   (frame 1 = il più grande e chiaro). *Impulso a crescendo*: costruisce verso il picco (carica). *Ritmo regolare*:
   cicli calmi (fuoco d'ambiente, indicatori). *Ritmo spezzato*: intensità che varia nel tempo. Un effetto complesso
   li concatena (carica a ritmo → lancio a impulso → viaggio a ritmo → impatto a impulso).
3. **Mai curve lineari** [F10]. Partenza veloce e frenata (ease-out) per tutto ciò che esplode o viene spinto; ease-in
   solo per ciò che sparisce "risucchiato" (pop-out). Velocità costante = effetto morto.
4. **"Se ti sembra lungo, è troppo lungo"** [F10]. Gli effetti brevi tengono lo schermo leggibile e lasciano spazio
   all'effetto successivo; quelli lunghi coprono e costano (overdraw).
5. **La scala si comunica col timing** [F5]: grande e pesante = più lento, più frame; piccolo e leggero = veloce.
   Un colpo forte non è solo più grande, è anche un po' più lungo (in hitstop e dissipazione).
6. **Tenere la forma chiave, poi dissipare in fretta** [F5]: un effetto che si sfalda subito perde la forma; meglio
   tenerla e poi chiuderla rapida. Un "hold" non è mai immobile del tutto: piccola variazione per frame.
7. **Sovrapposizione e sfasamento** [F5]: gli elementi non partono e non finiscono tutti insieme; i secondari partono
   prima che il primario finisca. Stessa idea dell'overlap nel corpo.
8. **Il timing dell'effetto si aggancia alle pose chiave** [F16]: prima esistono le pose e i frame della mossa, poi
   l'effetto si costruisce sopra; nei picchiaduro deve coincidere col frame del danno [F11].
9. **Ritmo emotivo** [F11]: pulsazione più veloce di un battito = aggressivo; più lenta = amichevole (cure, raccolta).
10. **Bloccare il timing prima della rifinitura** [F11, F18]: forme grigie semplici + suono, si ritocca il timing finché
    "suona", solo dopo materiali e dettagli. Ritoccare il timing di un effetto rifinito costa molto di più.
11. **Follow-through degli strati secondari** [G, F20]: quando il corpo si ferma (fine colpo, hitstop), l'aria e la materia
    leggera spinte dal colpo continuano per inerzia nella direzione tangente e poi frenano. È l'overlap del corpo
    applicato all'effetto.
12. **Decadimento termico** dell'energia [G, F5]: nucleo bianco (massimo) → bordo giallo/arancio saturo → rosso scuro →
    fumo freddo. Vale per fuoco, esplosioni e colpi "caldi"; saltare un gradino toglie credibilità. Per elementi non
    termici (elettrico, ghiaccio) la stessa idea: bianco al nucleo, tinta satura, poi tinta spenta.
13. **Morte secondo la materia** [G, F10, owner 11-19]. *Energia e solidi*: si chiudono per erosione del bordo
    (soglia sulla maschera) o contrazione, non per velo; un'opacità che cala lenta lascia un velo grigio e overdraw.
    *Gas* (polvere, fumo, neve, vapore): si **disperdono** come quelli veri, crescono mentre l'opacità scende fino a 0;
    non si rimpiccioliscono restando opachi (sembra un oggetto che si sgonfia, non aria). Il velo si evita facendo
    arrivare l'alpha davvero a 0 entro la vita e abbassando il contrasto, non accorciando la crescita.

**Intervalli indicativi (dal nostro sheet, frame a ~60 fps)** [A]: lampo del colpo 1-2 frame; colpo intero 12-20 frame
(200-330 ms); polvere 15-27 frame (250-450 ms); anello d'atterraggio 15-21 frame; detriti < 1 s.

## 4. Volume e strati
1. **Un effetto buono è un composito di strati**, ognuno con un ruolo, una forma, un tempo propri [F19, F20]. Strati tipici
   di un impatto, in ordine di nascita [A]:
   | Strato | Ruolo | Vita tipica |
   |---|---|---|
   | Lampo / nucleo | dove e quando (il frame del contatto) | 1-3 frame |
   | Forma primaria (stella, onda, scheggia) | direzione e forza | 6-15 frame |
   | Detriti solidi | materia, peso, fisica | 20-60 frame |
   | Fumo / polvere | volume, materiale, aria | 15-40 frame |
   | Segno a terra / ombra / luce | profondità e posizione | quanto il primario |
   | Hitstop, tremito, suono | sensazione e ritmo | 4-12 frame |
2. **Gli strati si distinguono per contrasto**: un solo strato chiaro e netto, gli altri subordinati [F10]. Due strati di
   pari forza competono e l'occhio non sa dove andare.
3. **Il blend additivo da solo non basta** [F19]: si sposa con qualsiasi sfondo ma su fondo chiaro sparisce; serve un
   contrasto proprio (un bordo scuro, un corpo opaco, un valore diverso dallo sfondo).
4. **Volume con la luce, non col contorno** [F5]: il volume del fumo si dà con l'ombreggiatura e la luce della scena,
   non con un outline. Le masse grandi "inghiottono" le piccole mentre salgono.
5. **Composizione**: l'effetto guida l'occhio verso il punto importante (il contatto, il bersaglio), come una copertina [F1].
6. **Separare le fasi in strati guidati da eventi** [F16]: così si può allungare o accorciare una fase (per esempio con
   l'hitstop) senza rompere le altre. Da noi: eventi di presentazione → strati Niagara separati.

## 5. Leggibilità alla camera di gioco
1. **Più la camera è lontana, più conta il feedback del colpo** [F21]. Con camera larga (FOV ~86-95° orizz., da noi) un
   colpo piccolo rischia di sparire: serve una **taglia minima a schermo** [A].
2. **Leggere da dove guarda il giocatore**: le nostre viste di giudizio sono dietro (camera di gioco) e lato
   (approvato 11-12/11-15); un effetto piatto (sprite, disco) visto di taglio sparisce, quindi le forme del colpo devono
   avere volume o essere rivolte alla camera [A].
3. **La profondità si legge da terra** [F9-bis, A]: ombra di contatto, anello o segno sotto l'impatto in aria, luce
   proiettata a terra. In Astro Bot l'ombra netta sotto il personaggio è il primo indizio di profondità [O astro_2].
4. **Il personaggio resta leggibile**: l'effetto non copre il corpo né l'arto che colpisce; a VFX accesi la silhouette
   dei frame attivi si legge ancora [F7].
5. **Portata onesta** [F6]: l'effetto attivo non fa sembrare il colpo più corto o più piccolo della hitbox.
6. **Il primo frame conta**: un cambio visibile al frame 1 dopo l'input (posa o effetto) [F22].
6-bis. **Leggibile e credibile insieme** [owner 11-19]: per i gas la leggibilità viene da tempo, posizione, luce e
   colore del materiale, non da opacità piena o taglia gonfiata. Se per "leggere" la nuvola diventa un disco opaco
   o un PNG piatto, il realismo è perso e l'effetto è sbagliato anche se si legge.
7. **Densità con più giocatori**: in PvP gli effetti si sommano; gli effetti dell'avversario che minacciano me valgono
   più dei miei cosmetici [F6]. Effetti brevi e pochi strati restano la prima difesa.

## 6. Colore e valore rispetto allo sfondo
1. **Il valore (chiaro/scuro) è lo strumento dell'attenzione** [F1, F10]: l'occhio va al punto di contrasto di valore
   più alto. Test pratico: guardare il fermo immagine **in scala di grigi**: se il colpo non è il punto più chiaro (o
   più contrastato) dell'inquadratura, il colore non lo salverà [A].
2. **Corpo a valori medi, il massimo solo nel nucleo** [F10]: niente bianco o nero puri come base; il valore più alto
   è riservato al lampo del contatto. Così il lampo "buca" e il resto non abbaglia.
3. **Gradini di saturazione** [F10, F17]: ambiente muto e basso → personaggi → VFX con tetto più alto → UI. Gli effetti
   tagliano lo sfondo perché la loro saturazione sta sopra quella del mondo, non perché sono più grandi.
4. **Colori complementari: uno domina** [F10]: l'accento è piccolo e meno saturo. Pochi colori per effetto (analoghi o
   triade), un gradiente "stretto" nelle zone di luce per evitare grigi fangosi [F17].
5. **Contrasto e saturazione segnano le fasi** [F16]: climax = contrasto massimo e saturazione alta; dissipazione =
   contrasto basso, opacità bassa. Un fumo che resta saturo e contrastato a fine vita "non se ne va".
6. **Il colore porta lo stato** [F16, F6]: squadra, elemento, stordito, scudo rotto. Uno stato = una tinta, ovunque.
7. **Due sfondi obbligatori** (owner 11-18): ogni colpo si verifica su fondo chiaro e su fondo scuro o saturo. Su neve
   e cielo chiaro i VFX chiari spariscono: lì lavora il bordo saturo o il nucleo caldo, non il bianco [O DK, A].
8. **Gli effetti prendono la luce della scena** quando sono materia (fumo, polvere, detriti) e la **emettono** quando
   sono energia (lampi, scie) [O astro_1-5, F23]. Mischiare i due ruoli produce il "grigio piatto" o il "neon sporco".

## 7. Il look Astro Bot / DK Bananza: comportamento reale, palette di stile
1. **Tre famiglie di materia, tre trattamenti** [O, F23]:
   - *Energia* (lampi, stelle, scie, laser): emissiva, netta, breve, colori puri; legge su qualsiasi sfondo.
   - *Gas* (fumo, polvere, neve, vapore): realistica, morbida, traslucida, **illuminata** dalla scena, **colorata dal
     materiale** che l'ha generata (rosa sul nemico rosa, bianca sulla neve) [O astro_3].
   - *Solidi* (detriti, schegge, coriandoli, monete): mesh lucide, bordi arrotondati, ombre, rimbalzo fisico [O astro_4].
2. **Comportamento reale, stile nel colore** [F4, F5, owner 11-19]: il fumo sale e rallenta, la polvere si disperde e
   ricade, i detriti rimbalzano e si fermano, la luce della scena li modella. Quello che è "di stile" è la palette:
   colori puliti e saturi, materiale che tinge il gas, poche tinte per effetto. Non si semplifica il gas in sagome
   piatte da cartone: è la causa delle "nuvole come PNG grandi e piatti". Per i solidi restano lecite forme pulite e
   arrotondate (mesh lucide, §7.1), sempre con fisica vera. Evitare la parola "stilizzato" per il comportamento.
3. **L'effetto nasce dai dati del materiale** [F24]: in DK Bananza dove si rompe, quanto si rompe e di che materiale è
   pilotano l'effetto, e voce e suono sono sincronizzati sul colpo finale. Gli effetti di distruzione sono generati in
   automatico dalle texture dei materiali (800+ tipi). La sensazione tattile viene da effetti e suoni che **rispondono
   a ogni rottura**, non da animazioni fisse [F24]. Per noi: superficie colpita → colore e tipo di polvere/detriti [A].
4. **"È più divertente distruggere ciò che è bello"** [F25]: l'effetto vale quanto il contrasto con ciò che rompe;
   un ambiente povero rende povero anche il miglior VFX. (Ambito: livelli/spedizioni, non arene PvP.)
5. **Cosa si vede nella clip DK Bananza** [O, ripresa da TV, colori approssimativi]: a ogni colpo una stella breve
   arancio/gialla con scintille luminose, un'onda d'urto rapida a terra, blocchi di ghiaccio e neve che saltano via dal
   suolo, sbuffi di polvere sugli atterraggi; i colpi leggono bene sulla neve blu perché il nucleo è caldo e saturo,
   complementare allo sfondo freddo.
6. **Cosa si vede in Astro Bot** [O astro_1-5]: lampo bianco netto a raggi + streak colorate brevi + scintille, sopra
   polvere/foglie fisiche; ombra di contatto netta; fumo morbido traslucido colorato dal materiale; anelli emissivi
   saturi + coriandoli solidi lucidi; luce calda direzionale con rim light e foschia viola sullo sfondo.
6-bis. **Cosa dice Team Asobi (CEDEC 2025, solo fisica e grafica, nulla su fumo o materiali degli effetti)** [F32]:
   fino a ~7.500 particelle fisiche per scena a 60 fps (PS5) solo per oggetti piccoli e forme convesse semplici;
   frammenti di distruzione con numero limitato e **rimossi dopo un tempo** per tenere i 60 fps; fontane ibride (colonna
   = mesh, solo le gocce che cadono simulate); "sotto i 60 fps è un bug". Per noi vale il principio (tetto ai pezzi,
   vita breve, mesh per la parte prevedibile), non i numeri, che sono di una PS5 [A].
7. **Il "look Unreal generico"** si riconosce da: tanti puntini piccoli, bagliore diffuso ovunque, traslucido grigio non
   illuminato, rumore procedurale, colori smorti, scintille identiche dappertutto [A, già nello sheet D12].

## 8. Fumo e polvere: come si comportano
1. **Cause diverse, comportamenti diversi** [F5]. La *polvere* è spinta da una forza esterna e **ricade** per gravità.
   Il *fumo* sale perché è caldo, **rallenta** raffreddandosi, si **arrotola** dove incontra aria fredda. Il *vapore*
   sparisce presto (si assorbe nell'aria). La *neve* sollevata si comporta come polvere ma più leggera e luminosa.
2. **Burst poi frenata** [F5, F13]: partenza esplosiva (la massima velocità al frame 1), drag forte, quasi fermo prima
   di sparire. Un fumo a velocità costante sembra un nastro trasportatore.
3. **Polvere d'atterraggio e di frenata si allarga lungo il terreno** [A, F5]: è aria spinta fuori da sotto il corpo,
   quindi radiale e bassa; si alza solo un po' e ricade.
4. **Rotolamento**: masse che girano su se stesse in direzione coerente con la spinta; forze alternate danno l'oscillazione a S [F18].
5. **Si stacca, si spezza, si disperde** [F18, F5, owner 11-19]: il fumo si rompe in pezzi e si apre; i pezzi grandi
   assorbono i piccoli mentre salgono. Ogni massa **cresce mentre sfuma ad alpha 0**, come la polvere vera nell'aria;
   non si rimpicciolisce opaca e non lascia un velo che resta (l'alpha arriva a 0 dentro la vita, §3.13).
6. **Volume con la luce** [F5]: niente contorni; luce della scena dall'alto, ombra sotto, rim verso la camera. Il fumo
   colorato dal materiale si illumina, non diventa grigio: un fumo grigio su fondo chiaro "sporca" lo schermo [A,
   coerente col FAIL r8 "nuvola troppo grigia"].
7. **Non tagliare sui piani**: il fumo che tocca il pavimento o un muro deve sfumare (depth fade), altrimenti mostra la
   linea dura del quad [F26].
8. **Varietà di forma**: non cerchi uguali che salgono; masse di taglia diversa, sovrapposte, con buchi [F5].
9. **Versioni economiche del fumo semi-realistico** [G, A]: (a) gruppi di sfere mesh low-poly Masked che prendono luce e
   ombre da ogni angolo e non hanno overdraw (la soluzione v2, adatta al preset basso); (b) sprite traslucidi con luce
   finta a due toni (chiaro verso la luce, tinta d'ombra verso il basso, tipo half-Lambert) invece di mappe di luce
   precalcolate; (c) chiusura per erosione della maschera. Il flipbook illuminato con motion vector resta per il preset alto.
10. **Ancorato al terreno** [G]: la polvere d'appoggio nasce sul piano del suolo e segue la sua normale, non fluttua sopra.

## 9. Impatto, hitstop e frame d'impatto (Sakurai)
Le regole dell'hitstop sono già nei documenti (§0). Qui il rapporto **fra hitstop e VFX** [F21, F27, A]:
1. **Il frame del contatto è un evento unico**: nello stesso frame partono fermo, lampo, tremito, suono e inizio della
   reazione. Se il lampo arriva un frame dopo il fermo, il colpo sembra "molle".
2. **Durante l'hitstop** (rivisto dopo la peer review): il contrasto fra **corpi fermi** ed **effetto che scoppia** vende
   il peso del colpo. Gemini sostiene che in Smash, Street Fighter 6, Guilty Gear Strive e DMC5 lo **hit mark** (lampo,
   stella) avanzi a velocità piena mentre i personaggi sono congelati, e che un effetto rallentato a 0,15 su 8 frame di
   fermo avanzi di ~1 frame e sembri un errore di render [G]. Non l'abbiamo verificato su video (Sakurai letto solo da
   riassunti), ma il ragionamento è coerente con §3.2 (impulso istantaneo) [A]. Proposta: **energia** (lampo, stella,
   onda; categoria C) a velocità piena durante il fermo; **materia** (schegge, polvere, fumo; A e B) frenata o ferma,
   poi riparte col personaggio. Il nostro default attuale, `HitstopVfxTimeScale` = 0,15 per tutti gli effetti del colpo,
   è una **decisione dell'owner** (v2, 2A): cambiarlo spetta a lui (§15.4). Il rallentamento globale resta adatto allo
   slow-motion cinematografico (zoom finale), non all'hitstop ordinario [G].
3. **Proporzione**: più danno = hitstop più lungo, effetto più grande e un po' più lungo, con un **tetto**; proiettili
   meno hitstop; più giocatori = hitstop più corto (impostazione per modalità) [F27].
4. **Direzione**: la forma primaria segue la direzione del colpo o del lancio; il tremito è orizzontale a terra,
   verticale in aria, rispetto allo schermo [F27, A].
5. **Frame d'impatto (impact frame)** [F28]: uno o pochi frame stilizzati, spesso monocromi o a colori invertiti, sul
   contatto dei colpi più forti. Funzionano per contrasto e direzionalità. Uso: solo colpi di voto massimo (smash,
   finali), mai su colpi frequenti, sempre come impostazione disattivabile (sensibilità alla luce lampeggiante) [A].
6. **Scie delle lame e degli arti** [F29]: rendono il colpo potente e mostrano l'arco nei frame attivi, che il corpo da
   solo non mostra da ogni vista. La scia dura i frame attivi più pochi frame, non supera la recovery, e non nasconde
   l'arto. (Il video di Sakurai sui trail e sui pericoli da evitare è stato letto solo come riassunto.)
7. **Il colpo a vuoto è diverso dal colpo a segno**: whiff = solo scia; hit = scia + lampo + hitstop + detriti. Se a vuoto
   e a segno si somigliano, il colpo a segno perde valore [A].

## 10. Scelta tecnica: flipbook, sprite, mesh, ribbon, luce, decal
| Tecnica | Quando | Look | Costo principale |
|---|---|---|---|
| **Sprite** (quad rivolto alla camera, texture singola) | lampi, stelle, scintille, nuclei | netto se Masked/emissivo | overdraw se grande e traslucido; si riduce tagliando il quad sulla forma (cutout) [F14, F26] |
| **Flipbook** (sprite con sequenza di frame, sub-UV) | gas che si deforma: fumo, fuoco, esplosioni | ricco, "realistico" | memoria texture, overdraw; meno frame con i **motion vector** (es. da 121 a 32 frame, risoluzione doppia) [F30]; luce finta con mappe di luce precalcolate da 4 o 6 direzioni [F30]. Ma motion vector + mappe di luce = 2-4 sampler e calcoli su quad traslucidi: **solo preset alto**; nel basso sfere mesh o sprite con luce finta a 2 toni (§8.9) [G] |
| **Mesh particle** | detriti, schegge, onde d'urto, anelli, coriandoli | solido, illuminato, ombre | draw call per emitter, triangoli; quasi nessun overdraw se opaco |
| **Mesh con texture che scorre** | trombe d'aria, scie volumetriche, onde | pulito, continuo | uno shader; sostituisce centinaia di particelle prevedibili [F14, F16] |
| **Ribbon / trail** | scie di arti e lame, traiettorie | linea continua | vertici, traslucenza lungo la scia |
| **Luce dinamica breve** | lampo che illumina il pavimento e il personaggio | molto "tangibile" | costo per pixel della luce, pesante su GPU integrate [G]; solo preset medio/alto, poche, brevi, senza ombre; nel preset basso la sostituisce un **disco emissivo a terra** (finto rimbalzo di luce) + bloom del nucleo [G, A] |
| **Decal / disco a terra** | segno d'impatto, ombra degli effetti | ancora l'effetto al suolo | proiezione; da noi il decal blob è risultato invisibile, si usa il disco [vfx-stile-v2] |
Regole generali: **il dettaglio va nella texture, non nel numero di particelle** [F14]; "usa i sistemi di particelle
solo quando devi", meglio una mesh che scorre per ciò che è prevedibile [F16]; texture in scala di grigi a canali
impacchettati e colore dal parametro, così una texture serve molti effetti [F16].

## 11. Prestazioni sul "tostapane"
1. **L'overdraw è il primo nemico** [F14, F26, F10]: ogni strato traslucido sovrapposto ricalcola gli stessi pixel. Il fumo
   arriva facilmente a 5-20 strati. Raddoppiare lato di un quad = quadruplicare i pixel [F14]. Rimedi: meno strati,
   particelle più piccole a schermo, vita più breve, quad tagliati sulla forma, opaco/Masked dove si può, depth fade solo dove serve.
   Obiettivo indicativo in combattimento: **3-5 strati traslucidi** sovrapposti al massimo nel punto peggiore [G].
2. **Poche particelle per effetto** [F14]: 3-10 per effetti che possono partire in massa; un effetto da 30 particelle
   è sceso a 2 (lampo + onda) cuocendo il dettaglio in texture.
3. **CPU per pochi, GPU per tanti** [F26, F31]: Epic dice solo che la CPU conviene per emitter con poche particelle e
   che va misurato per piattaforma [F31]; una fonte indica il pareggio attorno a ~1000 particelle [F26], Gemini attorno a
   ~100-200 [G]. **Nessun numero è verificato**: per noi non cambia nulla, perché i nostri burst sono di decine di
   particelle (tetto 12 per burst nello sheet) e restano su CPU; si misura solo se un effetto supera ~100 [A].
   Ogni emitter ha un costo fisso: meno emitter, più renderer o array di mesh nello stesso emitter [F31].
4. **Niagara** [F31]: Effect Type per ogni sistema con livelli di scalabilità (preset grafici), culling per distanza e
   tetto di istanze, **pooling**, **bound fissi**, niente warmup, ombre e collisioni delle mesh spente se non servono,
   burst grandi spalmati su più frame.
5. **Shader** [F14, F26]: somme, prodotti, lerp costano poco; sin/cos/pow, SceneColor e rifrazione costano molto
   (+20-80 istruzioni). Ordine di grandezza per effetti grandi: 100-150 istruzioni; mobile 80-120. Conta anche il
   **numero di texture lette**: al massimo ~2 per uno shader traslucido grande; SceneColor e rifrazione vietati nel preset basso [G].
6. **Budget di tempo indicativi**: una fonte dà < 1-2 ms di tick Niagara su hardware debole [F26]; per il tostapane
   adottiamo come obiettivo **< 0,5 ms** per i VFX di combattimento, picchi fino a 1 ms [G]. Da misurare, non da assumere.
7. **Moduli costosi** da evitare in massa [F14]: collisione (sostituire con frenata a quota fissa / piano analitico),
   rumore (sostituire con curve di velocità), trail.
8. **Come si controlla**: viste Shader Complexity e Quad Overdraw, Niagara Debugger e `stat Niagara`, preset grafico
   basso acceso durante la cattura [A].

## 12. Processo
1. Il VFX nasce **nella scheda della mossa** (voto d'importanza, forma prevista, strati) [F8].
2. **Block-in**: forme grigie e timing col suono, giudicato in gioco; poi materiali, poi dettagli [F11, F18].
3. **Style guide** del gioco con: forme permesse, gamma di valore e saturazione, regole di timing, cosa è solido,
   gassoso, energetico [F17]. Da noi è lo sheet + v3; questo documento ne dà le ragioni.
4. **Review in due passate** (VFX spenti, poi accesi), due sfondi, due viste, a velocità reale e poi frame per frame.
5. **Un effetto si confronta con un riferimento della stessa classe** (colpo con colpo, polvere con polvere), **sul
   video**: prima si scrive il dossier del riferimento (`docs/notes/review/dossier-riferimento.md`), chi costruisce lo
   usa come bersaglio e chi revisiona lo usa come metro. La checklist non sostituisce il confronto [owner 11-19].
6. **Dubbi tecnici su Unreal** (Niagara, materiali, render): chiedere all'assistente AI integrato nell'Unreal Editor,
   che dà consigli specifici del motore (owner 11-19; serve l'editor aperto).

## 13. Checklist per chi costruisce un effetto
1. Che cosa comunica, a chi (chi fa / chi subisce), con che voto d'importanza?
2. Qual è la causa e quale energia scarica? La forma ne discende (appuntita/tonda)?
3. Quali strati, ognuno con un ruolo? Uno solo è il primario?
4. Timing: climax nel primo quarto, curve non lineari, durata che "non sembra lunga"?
5. Agganciato al frame giusto della mossa (contatto, partenza, appoggio)?
6. Energia emissiva, gas illuminato e colorato dal materiale, solidi lucidi con ombra: ogni pezzo nel suo ruolo?
7. Si legge in scala di grigi, su fondo chiaro e scuro, dietro e di lato, alla distanza massima di camera?
8. Non copre il personaggio, non mente sulla portata?
9. Durante l'hitstop rallenta e riparte col personaggio?
10. Costo: numero di particelle ed emitter, overdraw (≤3-5 strati), texture lette, preset basso?
11. Si schiaccia e si allunga nel verso del moto? Ogni forma ha l'orientamento giusto (camera, piano, asse)?
12. Muore secondo la materia: energia e solidi per erosione o contrazione; gas che crescono sfumando ad alpha 0;
    nessun velo grigio che resta?
13. Si legge **e** sembra il materiale vero (polvere, fumo, neve), senza opacità o taglia gonfiate?
14. Messo accanto al video di riferimento (dossier), torna? Taglie rispetto al personaggio, vita, dispersione, luce.

## 14. Checklist per chi revisiona
È la skill `vfx-theory/gstn-vfx-reviewer.SKILL.md` (12 domande sì/no giudicabili dal video + 2 controlli strumentali, etichette di FAIL, formato del verdetto).

## 15. Contraddizioni fra i documenti esistenti (da confermare, regola 13d)
1. `stile-sheet-vfx-review.md` D2 ("nessun fumo realistico") e D12 (il traslucido morbido come segnale di "Unreal
   generico") contraddicono la **v3** (categoria B: fumo realistico, traslucido, illuminato) e la scelta dell'owner
   11-18 (fumo morbido colorato dal materiale). Proposta: D2 e D12 valgono solo per energia e solidi; per i gas vale v3.
2. Sheet A "Materiale: mai Translucent" e "Morte: mai dissolvenza in trasparenza": superati dalla v3 per la sola
   categoria B (decadimento morbido di opacità permesso). Proposta: aggiornare lo sheet con l'ambito per categoria,
   con il vincolo di §3.13 (prima erosione o contrazione, l'opacità cala solo a fine vita) [G].
3. Sheet B "Colpo: lampo + toro neon": la memoria dell'owner dice "luce, non anelli" per il colpo (VFX v3).
   Proposta: lo sheet B segue la v3; l'anello resta solo se l'owner lo rivuole.
4. **Hitstop dei VFX** (da peer review): default 0,15 per tutti (decisione owner v2 2A) contro hit mark a velocità piena
   e materia frenata (§9.2). Proposta: A) energia a 1,0 e materia a 0,15 (consigliata, una seconda impostazione);
   B) tenere 0,15 per tutto. Si decide guardando le due versioni in gioco.
5. **Dissolvenza dei gas** (owner 11-19): lo sheet dice "mai dissolvenza in trasparenza" e la versione precedente
   di §3.13 permetteva ai gas di calare di opacità solo a fine vita. L'owner ha deciso: i gas crescono sfumando ad
   alpha 0 per tutta la dissipazione. Risolta; lo sheet va aggiornato per la categoria B.
Il coordinatore le porta all'owner; questo documento non cambia lo sheet.

## 16. Limiti
- Nessun video guardato: GDC 2017 (Keyser/Chamberlain), Sakurai (effetti, hit mark, trail), Lyndon GDC 2018 e Quitevis
  GDC 2026 letti solo come riassunti o abstract. *Elemental Magic* letto tramite una tesi che lo riassume [F5], non il libro.
- Nessuna fonte di Team Asobi sui VFX di Astro Bot; per DK Bananza solo le sessioni GDC/CEDEC 2026 su distruzione e voxel.
  Le osservazioni [O] vengono dai riferimenti dell'owner (la clip DK è ripresa da TV).
- I numeri di prestazioni [F26] sono di un singolo autore; valgono come ordine di grandezza, da misurare sul nostro hardware.

## 16-bis. Peer review di Gemini: cosa è entrato e cosa no
- **Accettato**: nome Chamberlin (verificato) e Oona Salla; principi mancanti (squash and stretch, follow-through
  degli strati, orientamento, decadimento termico, erosione invece del velo, contrasto di frequenza, impronta
  sull'ambiente come idea); trappole del preset basso (luci dinamiche, flipbook con motion vector); tetto di 3-5
  strati; numero di texture lette; budget < 0,5 ms come obiettivo; checklist del revisore ridotta da 26 a 12 domande
  + 2 controlli strumentali, con la domanda nuova su camera e orientamento.
- **Accettato come proposta all'owner**: hit mark a velocità piena nell'hitstop (§9.2, §15.4).
- **Non accettato o corretto**: (1) "Miyamoto è a pagina 3": verificato, la frase è a **pagina 2** (link F9 giusto).
  (2) Soglia GPU "~100-200 raccomandata da Epic": la pagina Epic non dà numeri; resta non verificato (§11.3) e non
  cambia nulla per i nostri burst. (3) Fonti Astro Bot: la sessione CEDEC 2025 esiste [F32], ma riguarda fisica,
  fluidi e distruzione; **non dice nulla** su clearcoat, fumo che campiona le sonde o stelle emissive di 1 frame, che
  Gemini le attribuisce; il relatore grafico è Hikaru Yamaguchi, non "Futoshi". Quelle affermazioni restano
  osservazioni, non fonti. (4) "Impronta ambientale" citata a Team Asobi: idea buona, fonte non trovata.
  (5) "Mesh GPU = 1 draw call": anche le mesh su CPU sono istanziate per emitter; la scelta CPU resta (§11.3).

## 17. Fonti
F1 vfxapprentice.com/blog/five-artistic-principles-gaming-vfx · F2 GDC 2017 "Visual Effects Bootcamp: Artistic Principles
of VFX", J. Keyser (Riot) e H. Chamberlin (Blizzard, Hearthstone), gdcvault.com/play/1024439 + thread realtimevfx.com/t/artistic-principles-of-vfx/4081 ·
F3 80.lv/articles/vfx-staples-shape-color-and-motion (Jevremović) · F4 J. Gilland, *Elemental Magic* vol. I-II, Focal Press
2009/2012 (recensione parkablogs.com) · F5 O. Salla, tesi Theseus 2017 theseus.fi/bitstream/handle/10024/137717 (riassume Gilland) ·
F6 leagueoflegends.com/en-us/news/dev/clarity-in-league + realtimevfx.com/t/anatomy-of-a-league-of-legends-missile-part-2/13062 ·
F7 gonintendo.com/contents/12076 (Sakurai, effetti e risalto dei personaggi) · F8 gdcvault.com/play/1034360 (Rocksteady, GDC 2024) ·
F9 iwataasks.nintendo.com/interviews/wiiu/super-mario-3d-world/0/2 · F9-bis iwataasks.nintendo.com/interviews/3ds/super-mario-3d-land/0/3 ·
F10 vfxapprentice.com/blog/10-league-of-legends-vfx-design-tips (dallo style guide di Jin ho Yang, Riot) ·
F11 vfxapprentice.com/blog/the-soul-of-effects-what-is-timing-in-vfx · F12 GDC 2026 "Controlled Chaos: 12 Principles of
Design for VFX", R. Quitevis (Beyond-FX), abstract · F13 vfxapprentice / Riot, "few large" (F10 punto 1) ·
F14 pocketgamer.biz "VFX optimisation in midcore games" · F15 gonintendo.com/contents/33516 (Sakurai, densità degli effetti) ·
F16 = F3 (fasi, strati guidati da trigger, scroll su mesh) · F17 vfxapprentice.com/blog/creating-vfx-style-guides-for-games ·
F18 design.tutsplus.com/tutorials/how-to-animate-explosion-effects--cms-30960 · F19 gonintendo.com/contents/9838 (Sakurai,
strati e additivo) · F20 GDC 2018 "Zip! Thwack! Ping! Animation Principles of VFX", M. Lyndon (SideFX), abstract ·
F21 gonintendo.com/contents/23556 (Sakurai, hit mark) · F22 tvtropes Masahiro Sakurai On Creating Games (riassunti) ·
F23 `ref-astrobot-vfx/NOTE.md` principio dell'owner (lampi emissivi, fumo illuminato, detriti solidi) e `vfx-stile-v3-categorie.md` ·
F24 dev.classmethod.jp/en/articles/cedec-2026-voxel (DK Bananza, CEDEC 2026) · F25 kotaku.com/donkey-kong-bananza-nintendo-gdc-2026-2000678021 ·
F26 adamkomarowski.github.io/unrealtechart/book/vfx · F27 sourcegaming.info/2015/11/11/thoughts-on-hitstop-sakurais-famitsu-column-vol-490-1 +
nintendowire.com (Eight Hit Stop Techniques, 2022-12-12) · F28 80.lv/articles/learn-how-to-add-accents-to-your-animations-with-impact-frames ·
F29 gonintendo.com/contents/30552 (Sakurai, blade trails) · F30 loronsodev.github.io/posts/explosion-fx (motion vector, luce TLRB) +
forums.unrealengine.com flipbook motion vectors · F31 dev.epicgames.com/documentation/en-us/unreal-engine/scalability-and-best-practices-for-niagara ·
F32 cgworld.jp/article/202510-cedec-astrobot.html (Team Asobi, CEDEC 2025: H. Yamaguchi, T. Yoshida) · G = peer review Gemini, `docs/notes/reviews/agy-vfx-theory/esito.md` (claude/locomotion-finish-1 e04141c7)

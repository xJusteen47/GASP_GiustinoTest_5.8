# Peer Review: Teoria VFX di Gioco e Checklist Revisore (2026-11-19)

Esito della revisione su `principi-vfx.md` e `gstn-vfx-reviewer.SKILL.md`, conformemente a `PROMPT.md`.

---

### 1. Errori di fatto e attribuzioni
- **Riga 233-234 (§11.3) — Soglia GPU Niagara a ~1000 particelle**: *Errato*. In Unreal Engine (Niagara), la soglia consigliata da Epic per passare dalla simulazione CPU a quella GPU per particelle visive si attesta attorno a **~100-200 particelle**, non 1000. Gestire centinaia di particelle su CPU con simulazione gameplay a 62,5 Hz (frame budget totale di 16 ms) causa hitch pesanti sul Game Thread e sui Niagara Worker Threads. La cifra ~1000 è un retaggio superato di Cascade/OpenGL ES.  
  *Fonte*: Epic Games, *Niagara Scalability and Best Practices* (dev.epicgames.com).
- **Riga 197-199 (§9.2) — Rallentamento VFX a 0,15 durante l'hitstop attribuito/ispirato a Smash**: *Errato*. In *Super Smash Bros.* (e nei picchiaduro come *Street Fighter* o *Guilty Gear*), il lampo/stella d'impatto (hit mark) avanza a **velocità piena (1.0x, tempo reale)** mentre i personaggi sono congelati. Rallentare i VFX a 0,15 fa sembrare le particelle fluttuanti e bloccate per errore, distruggendo lo stacco percettivo.  
  *Fonte*: Masahiro Sakurai, *Hit Marks [Effects]* e *Eight Hit Stop Techniques* (YouTube / Famitsu).
- **Riga 287-288 (§17, F2) — Nome autrice Blizzard ("H. Chamberlain")**: *Errato*. Il cognome corretto dell'ex FX Artist di Blizzard è **Hadidjah Chamberlin** (senza 'a' prima di 'lin').  
  *Fonte*: GDC 2017 Vault, *Visual Effects Bootcamp: Artistic Principles of VFX* (J. Keyser & H. Chamberlin).
- **Riga 293 (§17, F9) — URL intervista Miyamoto**: *Errato*. La citazione ("make it sharp or on fire...") si trova a **pagina 3** (`/0/3`), non a pagina 2 (`/0/2`).  
  *Fonte*: *Iwata Asks: Super Mario 3D World*, Vol. 1, Pag. 3.
- **Riga 290 (§17, F5) — Nome autrice tesi**: *Impreciso*. Inversione del nome finlandese: l'autrice è Oona Salla (Salla Oona).  
  *Fonte*: Theseus.fi, *Mastering the elements: basics of 2d effect animation*, item 10024/137717.

---

### 2. Principi mancanti (Top 8 in ordine d'importanza)
1. **Erosione per maschera / Alpha Threshold (Dissolve Cutout) vs Alpha Fade**: Nei VFX stilizzati (Nintendo, Riot) gli effetti non muoiono abbassando linearmente l'opacità (che genera fango grigio e overdraw persistente), ma erodendo la silhouette tramite cutoff/smoothstep su noise texture o contrazione geometrica a scalare.  
   *Fonte*: Jin Yang, *League of Legends VFX Style Guide* (Riot Games, 2017).
2. **Squash & Stretch e Deformazione Direzionale (Elasticità)**: Stelle d'impatto, sfere di polvere e onde non sono rigide: si schiacciano violentemente contro il punto di contatto e si allungano nel verso del moto (velocity stretch) prima di espandersi o rimbalzare.  
   *Fonte*: J. Gilland, *Elemental Magic* Vol. 1; F. Thomas & O. Johnston, *The Illusion of Life*.
3. **Follow-Through cinetico e Inerzia degli strati secondari (Secondary Action)**: Quando il colpo o l'arto si blocca istantaneamente nell'hitstop, l'energia trasferita all'aria e alla materia periferica non si ferma: fumo leggero, polvere e scintille continuano la corsa tangenziale per inerzia.  
   *Fonte*: M. Lyndon, GDC 2018 *Zip! Thwack! Ping! Animation Principles of VFX*.
4. **Allineamento Spaziale Rigoroso (Camera Facing vs Axis/Plane Aligned)**: Con camera d'azione in 3a persona, i billboard puri a rotazione libera mostrano il fianco se visti di scorcio. Dischi a terra e onde d'urto devono essere ancorati al piano d'impatto (Plane-Aligned), i lampi radiali all'asse del colpo (Axis-Aligned).  
   *Fonte*: RealtimeVFX community standard; Niagara Alignment Documentation.
5. **Progressione Termica e Decadimento Energetico (Thermal Decay)**: L'energia esplosiva segue una scala termica precisa: nucleo bianco puro (calore massimo) → bordi giallo/arancio saturi → rosso scuro → cenere/fumo freddo opaco. Rompere questa catena toglie credibilità all'energia.  
   *Fonte*: J. Gilland, *Elemental Magic*; Jason Keyser, *VFX Apprentice*.
6. **Staging e Centro Focale Unico (Focal Point)**: Ogni effetto deve avere un solo baricentro d'attenzione: schegge, fumo e onde devono agire come linee guida (leading lines) che convergono verso il punto d'impatto, evitando di competere tra loro.  
   *Fonte*: R. Quitevis (Beyond-FX), GDC *Controlled Chaos: 12 Principles of Design for VFX*.
7. **Reattività e Impronta Ambientale (Environment Footprint)**: L'effetto acquista peso quando perturba il mondo di gioco: piega l'erba, lascia una chiazza opaca temporanea, sposta piccoli oggetti fisici o proietta luce rimbalzata coerente.  
   *Fonte*: Team Asobi, *Astro Bot* tech breakdown (CEDEC 2025, CGWorld.jp).
8. **Contrasto di Frequenza e Pause Visive (Rest & Motion)**: Alternare aree dense ad alta frequenza visiva (scintille, schegge) ad ampie superfici lisce di riposo visivo (masse di fumo pulito, frame di vuoto), per evitare affaticamento visivo.  
   *Fonte*: Riot Games VFX Style Guide; Keyser & Chamberlin, GDC 2017.

---

### 3. Astro Bot e DK Bananza: Fonti e Breakdown
- **Astro Bot (Team Asobi)**: *Fonti trovate*. Sessioni tecniche al **CEDEC 2025** (Principal Graphics Programmer Futoshi Yamaguchi e Senior Gameplay Programmer Takumi Yoshida, documentate su CGWorld.jp) e interviste a Nicolas Doucet (View Conference, GamesIndustry.biz).
  - *Cosa dicono*: Filosofia del "giocattolo fisico" (toy box). Uso massiccio di particelle mesh simulate su GPU con shader PBR completi (clearcoat e riflessi plastici lucidi per coriandoli, bulloni, foglie). Il fumo e la polvere non sono auto-illuminati ma campionano le sonde ambientali e la luce direzionale della scena. I lampi d'impatto sono stelle geometriche nette ed emissive (1 frame) abbinate a detriti fisici che rimbalzano sui personaggi mantenendo i 60 fps granitici.
- **Donkey Kong Bananza (Nintendo EPD, 2025/2026)**: *Fonti trovate*. Vincitore di tre Grand Awards al **CEDEC 2026** (Engineering, Game Design, Visual Arts; vedi dev.classmethod.jp) e sessioni GDC 2026.
  - *Cosa dicono*: Voxel-based destruction procedurale. Gli effetti di distruzione e detriti non usano sprite prefabbricati, ma estraggono mesh e polvere in tempo reale dai dati del materiale della superficie (800+ tipi). Contrasto termico studiato per far staccare i colpi (nuclei caldi giallo/arancio complementari a sfondi freddi/neve). Feedback d'impatto con sincronia rigorosa tra lampo, deformazione voxel e campionamento audio.

---

### 4. Sezione 9 (Hitstop e VFX)
- **Verdetto**: L'ordine degli eventi sul frame 1 (impatto istantaneo simultaneo di lampo, tremito, suono e hitstop) è impeccabile. Il **rallentamento dei VFX a 0,15 durante l'hitstop è invece errato** rispetto a Smash e all'azione moderna.
- **Esempi e Contrasto**:
  - In *Super Smash Bros.* (Melee/Ultimate), durante l'hitlag (4-14 frame), i lottatori restano bloccati nella posa con trauma shake, ma l'**hit mark (stella e lampo) scoppia a velocità piena (1.0x)**. Il contrasto tra modello immobile ed espansione fulminea del VFX a 60 fps è ciò che "vende" il peso dell'impatto.
  - In *Street Fighter 6* (Drive Impact), *Guilty Gear Strive* (Counter) e *Devil May Cry 5*, i sistemi particellari avanzano in World Time (1.0x) mentre gli attori subiscono CustomTimeDilation = 0.
  - Se il VFX rallenta a 0,15, su 8 frame di hitlag la particella avanza solo di 1 frame: resta congelata a mezz'aria, sembra un bug di render e si dissipa in ritardo quando il bersaglio è già volato via.
  - Il rallentamento (0,15) appartiene solo allo **slow-motion cinematografico globale** (Special Zoom di Smash su colpi letali, K.O. cinematografici), non all'hitstop ordinario.

---

### 5. Sezione 10 (Flipbook, Sprite, Mesh, Ribbon, Luce)
- **Verdetto**: La tabella è teoricamente valida, ma per un hardware molto debole ("tostapane") in Unreal 5.9 Niagara presenta **due gravi trappole prestazionali** e **un'opportunità mancata**.
- **Cosa cambiare per hardware debole**:
  1. *Eliminare le Luci Dinamiche Niagara*: Anche senza ombre, i Niagara Light Renderer richiedono pass di accumulo luce per-pixel che soffocano le GPU integrate o low-end. **Sostituzione**: usare finti dischi emissivi proiettati a terra (fake ground bounce) o affidarsi al bloom del nucleo emissivo.
  2. *Bandire Flipbook con Motion Vectors e 6-way Lightmap per il fumo*: Richiedono fino a 4 texture sampler e calcoli dot-product pesanti nel pixel shader su quad traslucidi sovrapposti. **Sostituzione**: sfere mesh low-poly deformate con materiale Masked/Dithered ed erosione smoothstep, oppure sprite 4x4 semplici illuminati con formula fake Half-Lambert a 2 toni.
  3. *Privilegiare Mesh Particle instanziate su GPU*: In UE 5.9 Niagara, le mesh particle opache/masked usano l'hardware instancing automatico (1 draw call per l'intero emitter, zero overdraw). Sono la soluzione più economica in assoluto per detriti e coriandoli solidi.
  4. *Ribbon*: Ridurre drasticamente la tassellazione ed evitare smoothing di twist costosi.

---

### 6. Sezione 11 (Prestazioni)
- **Verdetto**: I concetti generali di overdraw sono corretti, ma due parametri numerici chiave sono **sbagliati o pericolosi per un tostapane a 62,5 Hz**.
- **Analisi critica dei numeri**:
  - *Soglia ~1000 particelle per GPU*: **Superata**. In Niagara la convenienza GPU scatta a **~100-200 particelle**. Lasciare fino a 999 particelle su CPU satura i thread di simulazione a 62,5 Hz (dove l'intero frame budget è di soli 16 ms).
  - *Budget Niagara < 1-2 ms*: **Troppo alto**. 2 ms rappresentano il 12,5% dell'intero budget frame! Su un tostapane, il tempo di tick Niagara per i VFX di combattimento deve puntare a **< 0,5 ms** (massimo 1,0 ms nei picchi caotici).
  - *Istruzioni shader (80-120 mobile, 100-150 desktop)*: Ragionevoli come tetto ALU, ma omettono il vero collo di bottiglia: il **numero di texture sampler** (massimo 2 per shader traslucido) e il costo di banda di `SceneColor`/rifrazioni, che vanno vietate.
  - *Overdraw (5-20 strati)*: Valido come avviso del pericolo; in combattimento il tetto massimo effettivo non deve superare 3-5 strati sovrapposti.

---

### 7. Fumo e Polvere (Sezione 8)
- **Verdetto**: La fisica macroscopica descritta (drag, gravità per polvere, fumo che sale e decelera, depth fade) è corretta. Manca però la tecnica realizzativa per uno stile "stilizzato semi-realistico" economico.
- **Cosa manca per renderlo credibile ed economico**:
  1. *Mesh Puffs (sfere 3D low-poly) al posto di Quad Billboard*: Nei platform d'ispirazione (Astro Bot, Mario Odyssey), la polvere e i batuffoli sono cluster di sfere mesh. Ricevono luce e ombre dalla scena da ogni angolo di camera (cruciale in 3a persona), non ruotano con la vista e azzerano l'overdraw se usano materiali Masked.
  2. *Erosione della maschera (Step Dissolve)*: Il fumo stilizzato scompare "mangiandosi dall'interno" con contorni netti, non sbiadendo a velo grigio.
  3. *Illuminazione Half-Lambert a due toni (Fake Ramp)*: Shader leggerissimo che applica una tinta chiara verso la luce direzionale e una tinta d'ombra/rimbalzo verso il basso, simulando volume 3D senza costose lightmap volumetriche.
  4. *Ancoraggio al terreno*: Orientamento dei cluster lungo le normali del suolo per evitare che la polvere fluttui staccata dal pavimento.

---

### 8. Checklist del Revisore (`gstn-vfx-reviewer.SKILL.md`)
- **Verdetto**: Le 26 domande sono **troppe**; creano affaticamento, ridondanze e contengono verifiche impossibili da un video.
- **Domande impossibili da giudicare da video**:
  - *D5 (Hitbox)*: Impossibile senza overlay debug di collisione attivo nella cattura.
  - *D25 (Conteggio particelle/emitter)*: Impossibile da quantificare a occhio senza `stat Niagara` registrato.
  - *D3 (Coerenza tra tutti i kit)*: Contraddice la regola "una review = un effetto"; il video mostra un solo kit.
- **Domande doppie o vaghe**:
  - *D19 e D20/D21/D22*: D19 ("la materia prende luce, l'energia la emette") è duplicata parola per parola nelle tre successive.
  - *D7 e D24*: I "puntini uguali" sono valutati due volte.
  - *D24 e D26*: Giudizi stilistici soggettivi e generici ("Unreal generico", "alla pari di Astro Bot").
- **Domanda mancante da aggiungere**:
  - *"Stabilità di camera e orientamento"*: L'effetto mantiene coerenza volumetrica quando la camera ruota o si sposta, senza mostrare quad piatti visti di taglio o compenetrazioni rigide col corpo?
- **Proposta di snellimento**: Ridurre la checklist a **10-12 domande atomiche** suddivise in: 1. Informazione e Portata; 2. Forma e Silhouette; 3. Timing e Hitstop; 4. Colore, Luce e Materiale; 5. Pulizia a schermo.

---

### 9. Sezione 15 (Contraddizioni fra i documenti)
- **Verdetto**: Le 3 proposte hanno pieno senso pratico e sbloccano conflitti reali, con una precisazione tecnica sulla seconda.
  - *Proposta 1 (Fumo realistico v3 vs D2/D12)*: **Sensata**. Limitare D2 e D12 alle categorie A (solidi) e C (energia) sblocca la categoria B (gas/polvere morbida illuminata alla Astro Bot).
  - *Proposta 2 (Deroga Translucent per Categoria B)*: **Sensata ma con vincolo**. Concedere Translucent per i gas è necessario, ma va ribadito il divieto di dissolvenza alfa lineare: la scomparsa deve avvenire per erosione (cutoff) o contrazione geometrica, altrimenti si crea fango grigio su sfondi chiari.
  - *Proposta 3 (Luce vs Toro neon)*: **Ottima**. Il "toro neon" è un cliché piatto che copre la silhouette; sostituirlo con luce breve emissiva e stelle a raggi rispetta fedelmente lo stile Nintendo/Astro Bot.

---

### 10. Le tre modifiche più utili (in ordine di priorità)
1. **Correggere la dinamica dell'hitstop (§9.2)**: far avanzare l'hit mark e i lampi a velocità reale (1.0x) anziché rallentarli a 0,15, per preservare lo snap d'impatto di Smash e Street Fighter.
2. **Aggiornare i parametri prestazionali (§11.3, §11.6)**: abbassare la soglia di simulazione GPU Niagara a ~100 particelle e ridurre il budget del tick Niagara a < 0,5 ms per tutelare i 62,5 Hz su tostapane.
3. **Snellire la checklist del revisore da 26 a 10-12 domande essenziali**, espungendo i controlli non verificabili da video (hitbox e conteggio emitter) e inserendo l'erosione della maschera.

---

### Metadati della revisione
- **Tempo impiegato**: ~25 minuti di analisi documentale, verifica incrociata delle fonti e sintesi critica.
- **Cosa non è stato possibile verificare**: Shader graph interni proprietari e codice sorgente di *Astro Bot* e *DK Bananza* (analizzati tramite talk ufficiali CEDEC 2025/2026, GDC e documentazione tecnica); hardware esatto del target "tostapane"; file del repository all'infuori della cartella locale di lavoro.

# Gemini (Antigravity, the "3d Platofmer run start animation" sprinter conversation) on w2 vs w2 wide (2026-11-21, raw)

Asked by the owner in the Antigravity app on the A/B mp4s (ab_w2_vs_w2s_az180_slow4x.mp4, ab_w2_vs_w2s_game_real.mp4,
docs/notes/reviews/runstart-w2s_h1/); pasted by the owner into the project chat 19:02Z. Two agy-bridge attempts from the thread
timed out (5 min each): the IDE conversation cannot be continued from the CLI. Input, not orders (coordinator).

---

Ho visto entrambi i video (sia lo slow 4x da dietro `v77–v197`, sia la camera di gioco reale). Ecco punto per punto:

### 1. Passaggio di peso del velocista
- **Sì, si legge bene in entrambe** (`v77 → v92 → v107` nello slow 4x: drop del bacino e caduta in avanti sono chiari).
- **Sulla destra (`5 cm`) si legge addirittura meglio:** la base più larga toglie l'effetto "fune" della `w2` e fa percepire con più solidità il trasferimento di peso sul piede di spinta (`v92`).
- **Nota sulla camera di gioco reale:** vista da quella distanza la silhouette è piccola, quindi il drop iniziale quasi si attenua. Se vuoi che si senta anche lì, puoi osare **+2–3 cm** di drop del bacino e **+4–6°** di inclinazione avanti del busto sul picco iniziale (`v92`).

### 2. Larghezza piedi (5 cm per lato)
- **Centrata per un medio stile Fortnite:** a `v92` e `v107` è pulita, atletica, zero effetto "papera", e il fade-out verso il loop stock di GASP (`v122 → v137`) è **praticamente invisibile** (nessun pop).
- Se vuoi spingerla appena di più verso il medio-robusto puoi arrivare a **6–7 cm** sul solo primo appoggio (`v92`), rientrando a **3–4 cm** sul secondo (`v107`) e a **0 cm** entro il terzo (`v122–v137`).
- Se sali oltre i 5 cm, accompagna anche i *pole vector* delle ginocchia fuori di **2–3 cm** (e punte dei piedi a **+5–8°** verso l'esterno) per evitare che le ginocchia cedano verso l'interno.

### 3. Come renderla più viva ("Ganondorf in Astro Bot") senza stonare con idle/loop
Nello slow 4x (`v92–v122`) il busto e le braccia restano ancora un po' "composti" da manichino GASP. Per dare carattere senza rompere i blend:
- **Hip sway laterale:** sposta il bacino di **3–4 cm** verso il piede d'appoggio sui primi 2 passi (`v92`, `v107`), tornando a **0 cm** prima del loop. Da dietro è la cosa che comunica più peso e "vita".
- **Torsione spalle/petto:** aggiungi **+8–12° di yaw** sulle spalle e **+4–6° di roll** (spalla del braccio avanti leggermente più bassa) nei primi 2 passi, sfumati a zero in **12–15 frame** (a 60 fps).
- **Gomiti più aperti:** apri i gomiti fuori di **10–15°** (o **+4–6 cm** laterali sulle mani) solo nella fase di spinta (`v92–v107`). Dalla camera lontana fa esplodere la leggibilità della silhouette massiccia.
- **Overshoot del busto:** **+5–8°** di pitch avanti in più sul picco (`v92`), poi risalita morbida verso l'angolo del loop stock entro `v122–v137`.

### 4. Turn around a 90° e 180° (piedi incrociati + tremore braccia)
Non applicare sui turn lo stesso additivo `+5 cm` fisso dello start: mentre il root ruota, un offset laterale costante in spazio locale rischia di spingere le gambe una contro l'altra durante il crossover.
- **Piedi che si incrociano:**
  - Invece di un offset fisso, usa un **clamp di separazione minima** di **12–15 cm** tra le caviglie sull'asse laterale durante il giro.
  - Sui **90°** preferisci un *plant & push* sul piede esterno (**6–8 cm** fuori linea) invece del passo incrociato.
  - Sui **180°** apri il piede di perno di **8–12 cm** verso l'esterno ruotandolo già di **30–45°** nella nuova direzione prima che l'altro piede passi.
- **Braccia che tremano:**
  - In GASP quel jitter sui turn nasce quasi sempre da **re-selection troppo frequente di Motion Matching** durante la rotazione o da **blend troppo corto**.
  - Alza l'Inertialization solo sui Pivot/Turn a **0.18–0.22 s** (**11–13 frame** a 60 fps) e metti un **cooldown/lock di 6–10 frame** (0.10–0.16 s) sul database Pivot per evitare salti tra pose adiacenti.
  - In posa, durante il picco del 180° (velocità angolare alta), **richiama le braccia di 5–8 cm più vicine al torso** e riaprile in uscita: elimina l'oscillazione nervosa e dà una sensazione di rotazione pesante e controllata.

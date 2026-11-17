# Telecamera stile Astro Bot per spedizioni/livelli: studio e piano UE (solo piano, niente costruito)
Thread cloud, Fable 5.2 high, 2026-11-17. Richiesta owner 14:44Z (cmsg_01CRvUdrWm1dJrM4q54YDBLNCqFtaiorumQRbABLXoBEn7). Ambito: telecamera di spedizioni/livelli (non le arene PvP, salvo decisione).
**[F]** = fonte; **[A]** = analisi mia a memoria dei giochi. LIMITE: non ho trovato una fonte di Team Asobi sulla telecamera di Astro Bot (2024) e in questo thread non ho guardato video: i numeri sotto sono punti di partenza da verificare su una cattura (vedi §5, passo C0).

## 1. Come si comporta la telecamera di Astro Bot [A]
- **Inquadratura:** dietro e sopra, inclinata verso il basso di circa 20-30°, abbastanza lontana che il personaggio occupi circa 1/8-1/10 dell'altezza dello schermo. Si vede sempre il terreno DAVANTI e l'ombra sotto i piedi. FOV stretto (Galaxy usa 45° di default [F1]): poco effetto grandangolo, distanze leggibili, aspetto "diorama".
- **Segue morbida:** in orizzontale insegue con un piccolo ritardo; in verticale NON segue il salto: resta sulla quota del terreno e si muove solo quando il personaggio esce da una fascia alta/bassa o atterra su un'altra quota (Galaxy ha proprio questi parametri: soglie `uplay/lplay` e ritardo `pushdelay` [F1]). È la cosa che più rende i salti leggibili.
- **Rotazione:** lo stick destro è libero quasi ovunque, ma la telecamera da sola gira poco e piano; non si ricentra di scatto dietro al personaggio. Il livello è costruito lungo un asse e la telecamera autoriale lo asseconda, quindi il giocatore tocca lo stick di rado.
- **Tratti autoriali:** corridoi a vista laterale (2.5D), vista dall'alto, torri attorno a cui si gira, binari che arretrano e allargano per i boss o i panorami. La transizione è una dissolvenza di posizione di mezzo secondo circa, mai un taglio durante il gioco.
- **Chiarezza prima del dettaglio:** Doucet: con troppo dettaglio "non vedi più il tuo percorso"; il percorso si legge da lontano perché "è dipinto così" [F2]. La telecamera lavora insieme al colore del livello.
- **Ostacoli:** gli oggetti fra telecamera e personaggio spariscono a retino invece di far saltare la telecamera in avanti (Odyssey usa dithering a pattern per questo [F3]).

## 2. Il modello da copiare: tipi di telecamera a dati + aree (Super Mario Galaxy) [F1]
Galaxy ha un file di telecamere per livello; ognuna è un tipo + parametri, attivata da **aree** (box) con priorità quando si sovrappongono, da punti di spawn o da eventi. Parametri: distanza, angolo verticale e orizzontale, offset, FOV, frame per arrivare (`camint`), soglie verticali. Tipi utili per noi:
| Tipo | Cosa fa | Uso |
|---|---|---|
| Follow | Dietro al personaggio, stick libero, deriva lenta | Default dei livelli aperti |
| Parallel | Distanza e angoli FISSI, trasla soltanto | Corridoi, vista laterale, dall'alto (Mario 3D World) |
| Rail | Posizione su una spline legata all'avanzamento | Percorsi, inseguimenti, boss |
| Tower | Orbita attorno a un punto fisso seguendo il personaggio | Torri, pianetini |
| FixedPoint / Fixed | Ferma, guarda (o no) il personaggio | Stanze, panorami |
| Planet | Follow che ruota col riferimento di gravità | Volumi Galaxy (milestone rinviata) |

## 3. Requisiti nostri
- **"Giù" unico e orientabile (owner 14:44Z).** Un solo fornitore di asse, `GSTN_DownAxis`: default = direzione della gravità, sovrascrivibile per area o livello (anche di LATO). Lo leggono ombra a picco, depth marker E telecamera (l'"alto" della telecamera = opposto dell'asse, con rotazione smorzata). Così marker e inquadratura non si contraddicono mai. La parte marker/ombra è del thread del depth marker; qui solo l'interfaccia comune.
- **Online:** la telecamera è locale a ogni client, mai nella sim, mai replicata. L'input relativo alla telecamera viene convertito in direzione mondo PRIMA di entrare nella sim (già così). Le aree si valutano sulla posizione del personaggio locale.
- **Opzioni del giocatore** (principio owner: il feel dell'input è regolabile): velocità stick, inversione assi, deriva automatica on/off, tasto di ricentro. Le aree autoriali possono bloccare lo stick.
- **Niente Project Settings, tutto a dati e usabile da Blueprint:** data asset `GSTN_CameraMode`, attore `GSTN_CameraVolume`, componente rail su spline.
- **Toaster:** costo trascurabile (una valutazione per frame, 1-2 trace per gli ostacoli).

## 4. Valori di partenza [A] (da regolare su video)
FOV 45-55. Distanza tale che il personaggio sia 1/8-1/10 dello schermo. Pitch 20-30° in giù. Ritardo orizzontale 0,15-0,25 s (molla critica). Fascia verticale: la telecamera non sale finché il personaggio non supera circa 1,5 volte la sua altezza sopra la quota di riferimento, o atterra. Anticipo (look-ahead) 0,5-1 m nella direzione di corsa, lento. Deriva di yaw 20-30°/s solo dopo 1 s di corsa laterale e solo in Follow. Transizione fra aree 0,4-0,7 s ease-in-out, interpolando attorno al personaggio (orbita), non in linea retta fra le due posizioni [F4].

## 5. Piano UE 5.9 e costo
Scelta tecnica, due strade:
- **A (consigliata se in 5.9 non è più Experimental):** costruire sopra il **Gameplay Camera System** di Epic (Camera Asset, Camera Rig a nodi, Camera Director, transizioni [F5]); il camera del GASP lo usa già e `Config/DefaultEngine.ini` lo cita (`gh api search/code q='GameplayCameras repo:…'`: 1 risultato). Era Experimental fino alla 5.7, con Beta/produzione attesa fra 5.8 e 5.9 [F6]: lo stato in 5.9 va verificato in editor. Noi aggiungiamo: Director che sceglie il rig da una pila di richieste a priorità (aree, eventi), nodi custom per fascia verticale, rail e asse "giù".
- **B (se è ancora Experimental o i nodi custom richiedono troppo C++ fragile):** sistema nostro leggero nel plugin (pila di modalità stile Lyra su PlayerCameraManager). Più codice, zero dipendenze instabili.

| Passo | Cosa | Macchina, modello | Stima |
|---|---|---|---|
| C0 | Verifica stato Gameplay Cameras in 5.9 + misura su video di Astro Bot (distanza, pitch, ritardi, fascia verticale; 8 fotogrammi) | fisso (sola lettura) Sonnet + cloud Fable | ~4 $ |
| C1 | Nucleo: `GSTN_DownAxis`, data asset modalità, Follow + Parallel, fascia verticale, pila a priorità, `GSTN_CameraVolume`, transizioni, opzioni giocatore, test sulla logica pura | cloud, Opus high | ~14-18 $ |
| C2 | Rail, Tower, Fixed, dissolvenza a retino degli ostacoli | cloud, Opus medium | ~8-10 $ |
| C3 | Montaggio su L_Test_Hub (3 aree di prova), taratura, video, review Fable | fisso, Sonnet + review cloud | ~8-10 $ |
Totale ~34-42 $. Planet (gravità) resta con la milestone dei volumi Galaxy.

## 6. Decisioni per l'owner
1. **Ambito.** A (consigliata) solo spedizioni/livelli; le arene PvP tengono la telecamera attuale. B anche PvP: +~8 $ e va rivisto il feel dei combattimenti.
2. **Quando.** A (consigliata) dopo l'ondata 1 del polish (M0-M5), partendo da C0. B subito: occupa un posto cloud e uno slot sul fisso.
Da confermare (13d): strada tecnica A/B dopo C0 (la scelgo io sul risultato salvo veto); valori di §4.

## Fonti
F1 lumasworkshop.com/wiki/Cameras (formato telecamere di Super Mario Galaxy) · F2 videogameschronicle.com "Astro Bot: How Team Asobi has turned Playroom into a galaxy-sized sequel" · F3 threadreaderapp.com/thread/924061000919285761 (dettagli Odyssey) · F4 gamedeveloper.com "Lessons from Suzy Cube: the camera system, part 1" · F5 documentazione Epic "Gameplay Camera System Overview" · F6 strayspark.studio "Gameplay Cameras plugin UE 5.7 production guide"

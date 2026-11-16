# Come i director passano frame data e intenzioni agli animatori (sintesi, 2026-11-16)

**In breve:** nessuno studio pubblica il proprio modello interno, ma le fonti pubbliche concordano su tre livelli, e la scheda li segue:
1. una **lista del moveset** (una riga per mossa: input, ruolo, stato),
2. una **scheda per mossa** con intenzione, pose chiave e frame,
3. i **dati di gioco** (finestre, hitbox, cancel) separati dall'animazione.

## Cosa fanno i riferimenti
- **Sakurai / Smash (GDC 2008, video "How to Animate a Smash Bros Attack").** Ogni attacco ha quattro fasi: Standby, Windup, Strike, Follow-through. L'anticipazione deve far capire la potenza ed essere leggibile da chi difende. Lo strike dura pochissimo ed è venduto dall'hitstop. Il follow-through è la fase più lunga ed è il momento in cui si può punire. Il director dà la **posa chiave e il frame** (es. Link: windup f2, strike f7, ritorno f28); le curve le fa l'animatore. Nei suoi video Sakurai corregge a mano le pose e il timing, frame per frame, nel tool interno. → §2 fasi e §3 pose della scheda.
- **Street Fighter / Tekken / frame data Smash.** Il linguaggio comune è **startup, attivi, recovery** (+ FAF e vantaggio on hit/block): chi gioca e chi implementa parlano in frame. → colonne Startup / Ultimo attivo / FAF; attivi e recovery calcolati.
- **Skullgirls (GDC Animation Bootcamp, Mariel Cartwright).** Il design fissa il **budget di frame** e l'animatore lavora dentro quel budget: pochi frame di anticipazione, pose estreme, smear, tenute. → Tenuta f, Smear, Ingresso nella §3.
- **Rivals 2 Workshop (documentazione pubblica del mod kit).** Un attacco è fatto di **finestre con nome** (Startup/Active/Recovery, lunghezza in frame) più **hitbox su osso** (offset, raggio) e on-hit properties, con un flag terra/aria e un'animazione di landing lag opzionale. → §5 hitbox (osso, raggio), §6 atterraggio, colonna Dove.
- **Brief di animazione standard (guide di produzione).** Lista con ID, priorità e **stato** (non iniziata → review → approvata → integrata). Brief per singola animazione: contesto prima/dopo, pose di entrata/uscita, durata e fps, beat chiave, riferimenti video, vincoli tecnici (root motion, blend). → intestazione della scheda e colonna Stato.

## Cosa aggiungiamo noi (regola 13 e workflow dell'11-16)
Tratti d'identità (2-3, con riferimento, controllati per primi), foglio delle curve condiviso, `?` = da confermare (evidenziato, raccolto in §9), stati del flusso pose owner → blocking → test → polish → buona/approvata, e i campi del nostro modello dati: takeoff window, landing track, max aria, ledge check, capsula, cancel.

## Fonti
- Sakurai, "How to Animate a Smash Bros Attack" (trascrizione): https://amara.org/subtitles/I8Bnr4dEmv1u/en/1/download/How%20to%20Animate%20a%20Smash%20Bros%20Attack%20%20LINK.en.txt
- Sakurai sulle animazioni d'attacco "snappy": https://www.gonintendo.com/contents/13757-sakurai-details-the-process-of-making-snappy-attack-animations
- Sakurai, Famitsu vol. 532 (dirigere team grandi): https://sourcegaming.info/2018/01/06/methods-for-directing-large-teams-sakurais-famitsu-column-vol-532/
- GDC "Developing Super Smash Bros. Brawl": https://www.gamedeveloper.com/game-platforms/best-of-gdc-developing-i-super-smash-bros-brawl-i-
- GDC Animation Bootcamp, Skullgirls: https://gdcvault.com/play/1020575/Animation-Bootcamp-Fluid-and-Powerful
- Rivals 2 Workshop, "Setting up an Attack": https://rivals2.com/workshop/?p=550 e "How to Make a Rivals 2 Attack": https://rivals2.com/workshop/?p=984
- Frame data nei picchiaduro: https://dev.to/wtschmidt/frame-data-in-fighting-games-40ie
- Documentazione e brief di animazione: https://mocaponline.com/blogs/mocap-news/game-animation-documentation-guide

Nota: di Sakurai e Skullgirls ho usato trascrizioni e riassunti pubblici, non i documenti interni (che non sono pubblici).

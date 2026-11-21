# Camera alla Mario / Astro Bot: ricerca, gap e piano (rev. 3)
Thread desktop senza editor, Opus high, 2026-11-21. Solo piano, niente codice. Base: `design.md` (stessa cartella, rev. 2 del 19/11, decisioni owner 1A 2A 3A) e il lavoro anchor di stanotte (`claude/project-thread-hiq1zx`, `docs/architecture/camera-anchor.md`). Qui c'è solo **ciò che è nuovo**: Bowser's Fury e 3D World, verifiche su Astro/Odyssey, gap aggiornato, piano rivisto.
Legenda fonti: **[F]** verificata (link in fondo) · **[N]** Neon/Gemini via agy, link non controllato · **[A]** mia deduzione, da guardare su video.

## 1. Come si comportano le camere (sintesi)
| | Astro Bot | Odyssey | 3D World | Bowser's Fury |
|---|---|---|---|---|
| Modello | Libera, ma livelli disegnati lungo l'asse camera | Libera + tratti autoriali (2D 8-bit, viste fisse) | **Parallela autoriale** a distanza fissa, il giocatore la sposta a scatti; Free Camera Mode solo in single [F1][F2] | Libera 360° in mondo aperto [N] |
| Inquadratura | Pitch 15-26°, FOV 80-95°, personaggio piccolo (misurato) | Media, dietro-sopra | Alta, lontana, "diorama" | Media; si allarga su Plessie e nelle battaglie giganti [N] |
| Verticale nei salti | Ferma a quota terreno, sale solo su salite lunghe [N] | Fascia verticale (come Galaxy) [A] | Quasi ferma (camera parallela) [A] | Come Odyssey [A] |
| Rotazione automatica | **Conflitto**: Neon dice che segue dietro in curva [N], io avevo letto "quasi mai" [A] → va guardato su video (P0) | Lieve | Nessuna: cambia per tratto | Segue dietro dolcemente in corsa e su Plessie, R ricentra [N] |
| Ricentro | Tasto con Camera Assist (accessibilità) [F3 design.md] | L ricentra dietro Mario | — | R |
| Ostacoli | Il livello evita di coprire | Pull-in sul mondo + retino sui prop [design.md F8] | Il livello evita di coprire; silhouette di Mario dietro i muri [A] | Come Odyssey [A] |
| Aree/binari | Tratti laterali, dall'alto, binari boss | 2D 8-bit (camera perpendicolare al muro) [N], stanze fisse | Un'inquadratura per tratto | Grandi eventi: camera che include Bowser [N] |
| Opzioni | Solo inversione (di sistema) e Camera Assist [N] | Sensibilità, inversione, giroscopio [N] | Inversione | Inversione, sensibilità [F4] |
| Tilt-shift | DOF macro sui primi piani [N] | Solo in Snapshot Mode [N] | DOF ai bordi [N] | — |

**Multigiocatore (3D World, il caso che ci serve):** con 2-4 giocatori sullo stesso schermo la camera **si allontana per tenerli tutti in quadro** fino a un limite [F5][F6]; Miyamoto/Koizumi spiegano che il multigiocatore su uno schermo è uno dei motivi della camera parallela fissa [F1][N]. Alcune recensioni la giudicano poco affidabile in multi [F7]. Chi resta indietro entra in una bolla [A, da gioco]. Lezione opposta: Mario Wonder segue un "capofila" senza zoom e schiaccia chi resta indietro, criticato [F8].

**Intento dichiarato da Nintendo (3D Land/3D World) [F1][F2]:** "una camera che si muove parallela a Mario a distanza fissa e non gira attorno al percorso" = ci si perde meno; nasce dalla camera a binario parallelo di Galaxy 2. Il 3D Mario moderno ha quindi **due famiglie**: libera (Odyssey/Fury/Astro) e parallela autoriale (3D Land/World). Il nostro plugin oggi ha solo la prima più le aree.

## 2. Gap rispetto alla camera di oggi
Oggi in gioco: camera GASP (Gameplay Cameras, boom +400) + `UGSTN_CameraAnchorComponent` (pivot che nessun cambio di capsula muove, air hold, tilt-shift). Il plugin `GSTNCamera` (aree, binari, Free, Arena 3-4 giocatori, 44 test) **non è ancora collegato a nessun personaggio**: P0 non è partito (nessun handover `camera-p0` su nessun branch).
| Comportamento | Mario/Astro | Noi oggi | Gap |
|---|---|---|---|
| Pivot pulito (capsula, 62,5 Hz, render 144 Hz) | — | **Fatto** (anchor, jitter-3; da compilare e provare) | Nessuno; il plugin deve leggere questo pivot (adapter P0) |
| Verticale (fascia salto) | Sì | Air hold nell'anchor (250 cm) | Doppio se si accende Free del plugin: tenerne uno (già deciso: plugin) |
| Ritardo/anticipo orizzontale | Piccolo ritardo, piccolo look-ahead | Ritardo 0, look-ahead spento | Molla asimmetrica + look-ahead lento (P1) |
| Segue dietro in corsa | Sì, piano (decisione 2A) | No | P1 |
| Ricentro / Camera Assist | Sì | No (API c'è, non collegata) | P1 |
| Distanza regolabile + curve pitch→distanza/FOV | 3D World a scatti, gli altri no | Distanza fissa | P1 |
| Collisione | Pull-in + retino prop + silhouette | Braccio GASP | P2 |
| Aree/binari del designer | Sì | Nel plugin, non collegate | P0-P4 |
| **Camera parallela a distanza fissa (3D World)** | Sì | No (Rail/FixedPoint non la fanno) | **Nuova modalità `Parallel`** (decisione 1) |
| Punti di focus / eventi grandi (Fury) | Sì | No | P3 (`AGSTN_CameraFocusPoint`, modo Frame) |
| Co-op stesso schermo | 3D World | Plugin ha `Arena` (pensato per PvP 3-4) | Decisione 2 |
| Tilt-shift | DOF leggero | **Fatto** (anchor) | Solo review |
| Impostazioni | — | Plugin in Project Settings | Data Asset (già deciso) |

## 3. Requisiti trasversali (valgono per ogni fase)
- **A dati:** un `UGSTN_CameraProfile` (Data Asset) per i default, sovrascritto dal livello e dalle aree; niente Project Settings; tutti i kit con lo stesso profilo.
- **Esposto a Blueprint:** aree e punti di focus sono attori piazzabili; funzioni BP `RequestRecenter`, `SetDistancePreset`, `PushCameraMode/PopCameraMode`, `SetFocusEnabled`; eventi `OnSectionChanged`, `OnCapsuleChanged` (già fatto); `Get Camera Pivot` (già fatto) per camere fatte a mano.
- **Tarabile dal designer:** ogni valore con unità e intervallo nel tooltip, curve (`UCurveFloat`) per pitch→distanza/FOV, `gstn.Camera.Debug` che mostra modalità, area e pesi attivi.
- **Sicura in multigiocatore:** solo presentazione, mai nella sim, mai replicata, mai su server dedicato, mai eventi durante il rollback; un componente per giocatore locale (split-screen ok); opzioni salvate per giocatore locale; le aree si valutano sul pivot del proprio giocatore.

## 4. Piano a fasi (rivisto; sostituisce §5 di design.md solo dove indicato)
| Fase | Cosa | Macchina, modello | Stima | Dipende da |
|---|---|---|---|---|
| P0 | Come da brief: plugin su un figlio di prova dell'Overlord, adapter anchor→soggetto, spike C1 (ordine tick) e C4 (control rotation), video base. **In più:** 8 fotogrammi da un video di Astro Bot in curva senza stick (chiude il conflitto §1) | fisso, Sonnet | 4-6 $ | fisso libero |
| P1a (**nuovo, si può fare subito**) | Solo codice e test headless nel plugin, senza editor: profilo Data Asset (via dai Project Settings), curve pitch→distanza/FOV, preset distanza 0,8/1/1,25, molla asimmetrica, look-ahead, segue-dietro (2A), ricentro/Camera Assist, stick latching | cloud, Opus high | 8-10 $ | niente |
| P1b | Collegamento al personaggio vero, taratura sui video, air hold unico | fisso, Sonnet | 3-4 $ | P0, P1a |
| P2 | Ostacoli: pull-in tarato + retino sui prop marcati | cloud Opus medium + fisso per il materiale | 5-7 $ | P1b |
| P3 | Punti di focus (Frame/Look/Reveal) + modalità `Parallel` se decisione 1A | cloud, Opus high | 11-15 $ | P1a |
| P4 | Livello di prova: 1 tratto parallelo, 1 laterale, 1 libero, 3 punti di focus; video e review Fable | fisso Sonnet + cloud Fable | 8-10 $ | P2, P3 |
| P5 (dopo) | Strumenti editor; camera co-op condivisa se decisione 2B | cloud, Opus medium | 6-8 $ (+6-8 $) | P4 |
Totale P0-P4: **~39-52 $** (prima 37-50; +2 $ per `Parallel` e la verifica Astro). Rischi: spike C1/C4 come in design.md; P1a scritto prima di P0 può dover cambiare l'adapter (costo basso: l'adapter è un file).

## 5. Decisioni per l'owner (rispondere "1A 2A 3A")
1. **Camera parallela alla 3D World.** **A (consigliata)** aggiungiamo la modalità `Parallel` (distanza e angolo fissi, solo traslazione, il giocatore la sposta a scatti di 45°); il designer la usa per area o per livello, la libera resta il default fuori dalle aree. B solo camera libera + aree attuali (Rail/FixedPoint), come Odyssey. C `Parallel` come default delle spedizioni, libera solo nell'hub. Costo: A/C +2-3 $ in P3, B 0. Resta fermo: niente.
2. **Co-op sullo stesso schermo nelle spedizioni.** **A (consigliata)** per ora no: ogni giocatore la sua camera (split-screen in locale, online ognuno la sua); il plugin resta pronto. B camera condivisa alla 3D World (si allontana per tenere tutti, limite di distanza, chi esce rientra) partendo dalla modalità `Arena`. Costo: A 0, B +6-8 $ in P5. Resta fermo: con A nulla; con B serve prima la regola di gioco per chi resta indietro (design delle mosse/progressione, tua).
3. **Quando partire.** **A (consigliata)** P1a subito in un posto cloud (solo codice e test, non tocca editor né animazioni), P0 appena il fisso è libero. B tutto aspetta il fisso come deciso il 19/11 (3A). C P0 subito sul portatile. Costo uguale; con A si guadagna circa un ciclo, con C il portatile resta occupato ~1 h.

## Fonti
F1 iwataasks.nintendo.com/interviews/3ds/super-mario-3d-land/0/1 (camera parallela a distanza fissa, da Galaxy 2) · F2 iwataasks.nintendo.com/interviews/wiiu/super-mario-3d-world/0/2 (Free Camera Mode in single) · F3 vedi design.md · F4 touchtapplay.com, impostazioni camera di 3D World + Bowser's Fury (orizzontale, verticale, sensibilità) · F5 primagames.com eGuide 3D World, multigiocatore (tutti in quadro fin dove la camera si allontana) · F6 siliconera.com "Super Mario 3D World's multiplayer is a step in the right direction" · F7 en.wikipedia.org/wiki/Super_Mario_3D_World (critiche alla camera in multi) · F8 thenerdstash.com, camera a capofila di Mario Wonder.
Neon (agy, Gemini Flash), sessioni 27f3d174 e b58fe0c6: diverse fonti citate sembravano generiche o inventate (es. pagine GamesRadar/PlayStation Blog non specifiche), perciò tutto ciò che viene solo da lì è [N] e non entra nei valori finché non è visto su video.

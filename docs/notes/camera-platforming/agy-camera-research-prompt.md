# Prompt per Gemini (Antigravity / agy): ricerca indipendente sulle telecamere da platform 3D (2026-11-19)

Da incollare ad Antigravity sul desktop. **Solo lettura e ricerca: non modificare file del progetto, non aprire l'editor,
non toccare `_ue59`.** Scrivi una sola risposta in `docs/notes/camera-platforming/agy-research-esito.md` in un worktree tuo
sul branch `claude/project-thread-mu9mh4` (commit + push su quel branch), in italiano, massimo ~250 righe.
Questa è una ricerca **alla cieca**: NON leggere i documenti di design camera del repo, servono per confrontare due ricerche indipendenti.

## Contesto
Gioco action/platform 3D in Unreal 5.9 (personaggio medio-pesante, picchiaduro alla Smash + livelli alla Mario 3D/Astro Bot,
gravità che può cambiare direzione, online listen server, hardware debole). Vogliamo una telecamera da platform 3D con
**distanza regolabile dal personaggio** e **punti di "autofocus" decisi dal designer** (punti/volumi che spostano l'inquadratura).
La telecamera di mira (ADS / spalla) è separata.

## Domande (numera come qui; per ogni punto: risposta breve + esempio concreto + fonte; distingui FONTE da OPINIONE)
1. **Astro Bot (PS5, 2024, Team Asobi)**: come segue il personaggio (ritardo orizzontale, cosa fa nei salti in verticale,
   ricentraggio automatico, quanto si muove da sola), pitch e distanza tipici, opzioni camera del menu, sezioni a camera fissa/
   binario e come si passa da una all'altra, ostacoli fra camera e personaggio. Interviste/talk di Team Asobi (anche CEDEC, GDC,
   Famitsu, 4Gamer) che parlano della camera.
2. **Donkey Kong Bananza (Switch 2, 2025)**: come gestisce la camera mentre si scava DENTRO il terreno (x-ray, silhouette,
   avvicinamento), distanza, auto-camera, opzioni nel menu, cosa dicono le recensioni sui problemi (motion sickness ecc.).
   Interviste Nintendo (Ask the Developer, ecc.) sulla camera.
3. **Super Mario Odyssey (2017)**: comportamento verticale nei salti (la camera resta alla quota del terreno?), auto-rotazione,
   zone a camera autoriale (2D 8-bit, viste fisse), occlusione con dithering, opzioni (velocità). Talk/interviste di EPD.
   Bonus: Super Mario Galaxy (file di camera con aree e tipi), Mario 3D World (camera fissa a sezioni).
4. **Principi generali**: riassumi in max 12 regole le più importanti di John Nesky "50 Game Camera Mistakes" (GDC 2014) e
   di Mark Haigh-Hutchinson *Real-Time Cameras*, soprattutto: lag/damping, look-ahead, pitch automatico, quando ruotare da sola,
   transizioni fra camere, collisione e occlusione, input relativo alla camera dopo un cambio di inquadratura.
5. **"Punti di interesse" / hint del designer**: come i giochi permettono al designer di indicare cosa guardare (Galaxy camera
   areas, Mario 64 Lakitu fixed points, Cinemachine Target Group / Camera Hints, God of War, Uncharted "look-at" hints, Ratchet & Clank).
   Che parametri hanno (peso, raggio d'influenza, priorità, se il giocatore può sovrascrivere)? Cosa rende un hint fastidioso?
6. **Distanza regolabile**: quali platform 3D permettono al giocatore di scegliere la distanza (zoom a scatti, slider, preset
   vicino/medio/lontano)? Come la combinano con la distanza decisa dal designer per area (moltiplicatore? offset? limite?)
7. **Unreal 5.9**: stato del plugin Gameplay Cameras (Experimental/Beta/produzione) in 5.7-5.9 e se è adatto a una camera da
   platform con volumi e hint, rispetto a una camera propria su PlayerCameraManager. Se non sai, scrivi "non so".
8. **Le 5 cose più importanti** per ottenere la "sensazione Astro Bot" con un personaggio medio-pesante, in ordine.

## Regole
- Se non sai, scrivi "non so". Niente numeri inventati: se stimi, scrivi "stima".
- Fonti come URL o titolo + autore + anno. Niente immagini di altri giochi nel repo.
- In fondo: tempo impiegato e cosa non hai potuto verificare.

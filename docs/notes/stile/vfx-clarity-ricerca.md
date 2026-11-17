# Chiarezza visiva dei VFX di combattimento: Sakurai, GDC, Astro Bot, 3D World
Thread cloud, Opus, 2026-11-17. Richiesta owner 20:59Z (cmsg_01CRvUdrWm1dJrM4q54YDBLNTXj47JUwjc1UmSpgekSyXQ). Solo ricerca, nessun codice.
Non ripete `stile-astrobot-nintendo.md`, `stile-sheet-vfx-review.md`, `silhouette-vfx-ricerca.md`: qui solo ciò che manca. Checklist aggiornata proposta: `handoffs/stile-sheet-vfx-review-proposta.md`.
**[F]** = detto da una fonte (in fondo). **[A]** = adattamento nostro allo stile Astro Bot / 3D World.

## 1. Cosa aggiunge Sakurai (oltre al riassunto già letto)
1. **Hitstop, dettagli** [F1, F2]: attaccante e bersaglio fermi per lo stesso tempo; più danno = più lungo, con un TETTO; modificatore per hitbox (punta della spada di Marth più lunga, attacchi elettrici più lunghi); **proiettili meno hitstop**; nel tutti-contro-tutti lo tiene **più corto del voluto** perché un terzo giocatore può colpire durante il fermo. Tremito: **a terra orizzontale, in aria verticale**, grande all'inizio e calante, hurtbox ferma, ampiezza secondo la distanza della camera. Il bersaglio passa dalla posa di sussulto a quella di dolore in **4 frame** durante il fermo; l'attaccante avanza lentissimo (disattivabile per mossa).
2. **Effetti vs personaggio** [F3]: un effetto troppo vistoso "sovrasta" i personaggi; deve restare d'impatto senza coprire. La chiarezza viene prima dello stile [F4].
3. **Effetti che spiccano** [F5]: si costruiscono a **strati**; il blend additivo li fa sposare con qualsiasi sfondo, ma da solo non basta: serve **contrasto** proprio.
4. **Feedback del colpo** [F6]: conta di più quanto più la camera è lontana. Feedback visivo **immediato** appena si preme il tasto (avvio) [F7].
5. **Esagerare** [F7, F8]: pose e deformazioni oltre il realistico vanno bene, anzi raccomandate.

## 2. Cosa aggiungono le fonti GDC e Riot
6. **Gerarchia d'attenzione** [F9, F10]: l'attenzione che un effetto prende deve essere proporzionale alla sua importanza di gioco; Riot dà a ogni abilità un voto 1-10, separato per chi la fa, per gli alleati e per i nemici. "Se tutto è importante, niente è importante."
7. **Stesso effetto di gioco = stessa forma** [F10]: chi radica ha sempre anello a terra + cilindro, cura verde e mai appuntita. Le skin cambiano colore, mai la forma; una skin non è mai meno chiara della base ("clarity parity") [F9].
8. **Mai mentire sulla portata** [F9]: la grafica non deve far sembrare un colpo più evitabile (più corto, più piccolo) di quanto è; i cosmetici non devono nascondere hitbox.
9. **Valori medi, saturazione non estrema** [F11]: niente nero e bianco puri come base; la gamma di valore più ampia si usa solo dove si vuole l'occhio. Effetti **brevi**: quelli lunghi coprono gli effetti successivi.
10. **Il caos nasce dal processo** [F12, Rocksteady GDC 2024]: i problemi di leggibilità nascono quando i VFX entrano tardi nel design; vanno pensati con la mossa.

## 3. Cosa aggiunge Nintendo (3D Land / 3D World) e Astro Bot
11. **Ombre come informazione di profondità** [F13]: in 3D Land l'ombra di monete e blocchi dice dove saltare; l'ombra del giocatore cade **sopra i nemici** per leggerne la posizione. Il gioco doveva capirsi **anche senza 3D stereoscopico**, e la densità grafica è stata **ridotta** apposta.
12. **Pericolo = appuntito o in fiamme** [F14] (Miyamoto): "i dettagli non contano". Camera a distanza e direzione fisse in 3D World per non perdersi e per il multigiocatore.
13. **Astro Bot**: identità del personaggio da pochi tratti grandi (occhi, oggetti portati) [F15]; arte sempre dopo che la meccanica è divertente [F16]. Nessuna fonte dello studio su VFX o leggibilità del combattimento: sotto è adattamento [A].

## 4. Adattamento: Smash → Astro Bot / 3D World [A]
- **Teniamo da Smash**: hitstop e tremito (regole §1, ma tremito orizzontale/verticale **rispetto allo schermo**, non al mondo, perché la camera gira), tetto, proiettili più corti, 3-4 taglie, zoom finale solo 1v1.
- **Cambiamo**: Smash usa molto additivo, raggi di luce e lampi a tutto schermo. Da noi il corpo dell'effetto resta **opaco Masked a 2 toni** (sheet A); l'additivo/emissivo resta **solo nel nucleo del frame 1-2** (è il "contrasto proprio" di Sakurai). Stelle e palline al posto di raggi e scintille sottili.
- **Profondità**: con camera 3D il colpo che lancia in profondità o in aria si legge solo con un segno a terra: anello/ombra sotto il punto d'impatto se il bersaglio è in aria (stessa idea del depth marker e delle ombre di 3D Land).
- **PvP**: hitstop più corto se ci sono più di 2 giocatori (dato del game mode, non project setting) e gerarchia "mio / avversario" come Riot: gli effetti dell'avversario che minacciano me prendono più attenzione dei miei cosmetici.
- **Taglia minima a schermo**: con la camera lunga e lontana dello stile (FOV 45-55) il colpo piccolo rischia di sparire; scala dell'effetto con la distanza della camera, come il tremito di Sakurai.

## Da confermare (regola 13d)
Risposta owner 21:04Z (cmsg_01CRvUdrWm1dJrM4q54YDBLN71xiz4w3nEXHP26d6eoeJK): (b) sì, due voti di importanza per evento (chi fa / chi subisce); (a) dipende dalla modalità: moltiplicatore di hitstop oltre 2 giocatori come impostazione per game mode.

## Limiti
I video di Sakurai non sono stati guardati: letti tramite riassunti di giornalisti e la sua colonna Famitsu sull'hitstop (fonte più dettagliata). Il video "Hit Marks [Effects]" non ha riassunti con regole concrete. Il talk Rocksteady letto solo come abstract. Nessuna fonte Team Asobi sui VFX.

## Fonti
F1 sourcegaming.info/2015/11/11/thoughts-on-hitstop-sakurais-famitsu-column-vol-490-1 · F2 nintendowire.com/news/2022/12/12/this-week-in-sakurai-12-5-12-11-fine-tuning-hit-stop-and-cheating-the-system ("Eight Hit Stop Techniques") · F3 gonintendo.com/contents/12076 (effetti e risalto dei personaggi) · F4 gonintendo.com/contents/10042 (chiarezza vs stile) · F5 gonintendo.com/contents/9838 (strati, additivo, contrasto) · F6 gonintendo.com/contents/23556 ("Hit Marks [Effects]", youtube.com/watch?v=B-P4ysHSjCg) · F7 tvtropes.org/pmwiki/pmwiki.php/WebVideo/MasahiroSakuraiOnCreatingGames · F8 game8.co (episodio "Manga-like Visuals") · F9 leagueoflegends.com/en-us/news/dev/clarity-in-league · F10 realtimevfx.com/t/anatomy-of-a-league-of-legends-missile-part-2/13062 · F11 vfxapprentice.com/blog/10-league-of-legends-vfx-design-tips · F12 gdcvault.com/play/1034360 (Hinkelbein, Rocksteady, GDC 2024) · F13 iwataasks.nintendo.com/interviews/3ds/super-mario-3d-land/0/3 · F14 iwataasks.nintendo.com/interviews/wiiu/super-mario-3d-world/0/2 · F15 gameshub.com (Doucet, "Nailing the Astro Bot aesthetic") · F16 blog.playstation.com/2024/09/09/astro-bot-how-team-asobi-created-a-unified-vision-for-fun

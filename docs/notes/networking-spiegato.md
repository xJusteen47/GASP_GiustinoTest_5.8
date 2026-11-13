# Il networking del gioco, spiegato (novembre 2026)

Per chi progetta mosse e livelli, non per chi scrive codice. Numeri misurati sul portatile, server dedicato + 2 client, 60 ms di
ritardo per direzione con 15 ms di variazione, a 0 % e 2 % di pacchetti persi (`docs/architecture/plans/fwd-prediction-2026-11-07.md`).

## 1. L'idea di fondo

Il server decide sempre. Ogni giocatore però non può aspettare la risposta del server per vedersi muovere: ci vorrebbero ~120 ms
(andata e ritorno) per ogni tasto. Il gioco quindi divide quello che vedi in tre tipi:

- **Predetto**: il tuo computer lo calcola subito, senza aspettare, e poi controlla con il server. Se il server dice altro, si corregge.
- **Interpolato**: il tuo computer lo mostra un po' nel passato, tra due stati già confermati dal server. È sempre corretto ma arriva in ritardo.
- **Presentazione predetta**: solo l'aspetto (animazione, flash) parte subito; la sostanza (danno, posizione, stato) aspetta il server.

## 2. Chi è cosa

| Cosa vedi | Tipo | Cosa significa per te |
|---|---|---|
| Il tuo personaggio (movimento, attacchi, schivate, abilità) | Predetto | Risponde subito al tasto. Raramente uno scatto quando il server corregge. |
| Il partner di un link (rider/carrier, prese, mosse di coppia), sullo schermo di chi lancia la mossa | Presentazione predetta (G1) | La reazione del partner parte insieme alla tua mossa; la posizione vera arriva dal server. |
| Il partner, sul suo stesso schermo | Predetto per sé, ma la mossa dell'altro arriva dal server | Vede la mossa dell'altro circa 250-280 ms dopo chi l'ha lanciata. |
| Altri giocatori e avversari | Interpolati | Sempre fluidi e corretti, ma ~50 ms "nel passato" più il viaggio dal server. |
| Minion (nemici di massa, M8) | Interpolati con le loro "marionette" (puppet) | Il server li simula tutti insieme; tu vedi la marionetta che li segue. |
| Il flash e le scintille quando colpisci un nemico | Presentazione predetta | Partono subito sul tuo schermo; se il server non conferma il colpo, si annullano. |

## 3. Il ciclo di una mossa predetta, passo per passo

1. **Tasto**: il tuo computer avvia la mossa nel frame stesso (il server non sa ancora niente).
2. **Presentazione**: animazione, suoni e, per le mosse di coppia, anche la reazione del partner (G1) partono subito.
3. **Viaggio**: l'input arriva al server dopo ~60 ms di rete più ~64 ms di "coda" del server (4 frame di input tenuti da parte per non
   restare mai senza). Il server esegue la stessa mossa sullo stesso frame.
4. **Conferma**: lo stato del server torna al tuo computer (~60 ms). Se coincide con quello che avevi predetto, non succede niente di visibile.
5. **Correzione (rollback)**: se non coincide (ad esempio il server ha visto un colpo che tu non avevi visto), il tuo computer torna allo stato
   del server e rigioca in un attimo i frame mancanti. Quello che vedi è uno **scatto** al frame giusto dell'animazione: come nei picchiaduro
   con rollback netcode, e come hai deciso (accettato, niente smussamento extra).
6. **Rifiuto**: se il server rifiuta la mossa (ad esempio il partner non poteva farla), la presentazione si ferma di colpo e il personaggio
   torna allo stato vero.

## 4. I valori di oggi

| Valore | Quanto | Perché |
|---|---|---|
| Frame di simulazione | 62,5 al secondo (16 ms) | Tutti, server e client, calcolano a passi fissi uguali. |
| Ritardo di interpolazione (buffer) | 50 ms (era 100) | Deciso da te. Abbassarlo ha avvicinato poco gli altri giocatori (~10-15 cm a passo di corsa); sotto i 50 ms il movimento si inceppa di più. |
| Coda di input del server | 4 frame (64 ms) | Lasciata così: svuotarla fa guadagnare 25-40 ms, ma il server resta senza input 3-4 volte più spesso. |
| Hop 1: dal tuo schermo al server, per la stessa mossa | ~180-220 ms | Rete (~60) + variazione + coda input (64) + allineamento dei frame. |
| Hop 2: dal server allo schermo del partner | ~70-130 ms | Rete (~60) + buffer (50) + allineamento. |

**Mosse di coppia, prima e dopo G1** (ms dopo il tasto di chi lancia la mossa):

| Mossa | Reazione del partner vista da chi la lancia: prima | Dopo G1 | Vista dal partner sul suo schermo |
|---|---|---|---|
| Boost jump (lancia il rider) | 333-377 | 38-68 | ~385-480 |
| Lancio del rider (lancia il carrier) | 553-590 | 239-246 | ~550-650 |

Il lancio del rider resta lungo per un motivo di design, non di rete: la mossa ha una preparazione (wind-up) prima del momento in cui il
rider parte. Accorciarla è una scelta di animazione.

## 5. La lag compensation: cosa fa al posto della predizione

Quando colpisci qualcuno che vedi interpolato (quindi un po' nel passato), il server non controlla il colpo contro la posizione di adesso:
**torna indietro nel tempo** a quello che vedevi tu quando hai premuto, e controlla lì. Così se sul tuo schermo il colpo prende, prende
davvero, anche se l'altro nel frattempo si è spostato. Vale anche per i minion: il server tiene una breve storia delle loro posizioni.

Il prezzo è dall'altra parte: chi viene colpito può essere colpito "dopo" essersi spostato sul suo schermo. Il gioco lo accetta perché è
meno fastidioso di mancare un colpo che si vedeva andare a segno.

## 6. Perché il partner non scende sotto ~170-190 ms

Perché il partner veda la tua mossa, l'informazione deve andare dal tuo computer al server e dal server al suo: due viaggi di rete
(~60 + ~60 ms con la rete di prova) più qualche frame per allinearsi. Nessuna predizione lo può evitare: il partner non può sapere che hai
premuto un tasto prima che il tasto gli arrivi. La predizione vera del partner (G3-G5) toglierebbe solo il buffer e un po' di allineamento:
da ~250-280 ms a ~200-240 ms. Quello che il gioco può fare, e G1 fa, è far vedere subito la reazione a **chi lancia** la mossa.

## 7. Cosa resta da fare

- **G2, video**: sul desktop si registrano le mosse di coppia in gioco e si controlla che la reazione anticipata si veda bene (la misura di G1
  è sul momento in cui il gioco decide di mostrarla; le mosse di prova potrebbero non avere un'animazione vera).
- **G3-G5, predizione vera del partner**: in pausa fino a un playtest PvP. Costerebbero $32-52 per 40-60 ms in meno solo sullo schermo del partner.
- **G3b, colpi tra partner**: se G3-G5 ripartono, serve prima che i colpi tra partner (prese PvP, lanciarazzi, hitgrab) si risolvano sul
  computer di chi viene colpito. È l'unica parte che tocca il cuore del combattimento (`GSTNCombatNP`).
- **Mossa rifiutata**: oggi non c'è uno scenario di prova per una mossa di coppia rifiutata dal server; il comportamento (stop di colpo) è
  coperto da un test interno. Se il rifiuto avviene dal lato del partner, chi lancia la mossa vede comunque la reazione fino alla fine.

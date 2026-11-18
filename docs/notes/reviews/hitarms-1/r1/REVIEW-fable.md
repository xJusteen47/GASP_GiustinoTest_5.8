# REVIEW ArmsV1 (braccia/mani hitstun) — revisore fable

**Esito: FAIL complessivo (non ancora parità Smash), ma netto miglioramento sul base: la lamentela "braccio dritto e rigido" è risolta.**

## Tratti (base = riga sopra, var = riga sotto)
1. **Gomiti piegati (rif. 60-110°)** — PASS. Lead a occhio 70-90°, trailing 60-70° in tutte le viste (close_az90 var hs+0..hs+36; hitstand_az45 var hs+0). Il base è dritto su entrambe le braccia (base hs+0..hs+36).
2. **Asimmetria braccia** — PASS. Lead alto all'altezza testa/spalla, trailing basso al fianco (close_az45 var hs+0/+1; hitstand_az45 var hs+0).
3. **Mani aperte e divaricate** — PARZIALE. Lead PASS (close_az45 var hs+0/+1, hs+22: dita aperte leggibili). Trailing FAIL "posa": nei close sembra semi-rilassata/incurvata, non splay (close_az45 var hs+4..hs+12; close_az90 var hs+6..hs+12).
4. **Primo frame già "dolore", estremo** — PARZIALE, FAIL "posa" sulla lettura. hs+0 è molto diverso dal base e forte come silhouette, ma il lead con avambraccio su e palmo aperto verso l'attaccante legge come "alt/saluto/guardia", non come arto sbalzato dall'urto (close_az45 var hs+0/+1; side_l_az45 var hs+0..hs+36 = mano tesa verso l'attaccante). In Sakurai le braccia sono trascinate via dal colpo, larghe e basse, col petto incassato.
5. **Chiarezza silhouette** — FAIL "posa" in due punti: (a) colpo da sinistra, side_l_az90 var hs+0..hs+6: mano/avambraccio lead davanti alla testa, copre il viso; hs+16..hs+36 avambraccio incrociato sul petto = "guardia". (b) trailing attaccato al corpo: mano sovrapposta ad anca/coscia in close_az90 var hs+4..hs+12 e hitstand_az90 var hs+0..hs+12.
6. **Testa e spalle che guidano** — testa/busto invariati (fuori ambito, leggono come nel base). Spalle alzate 12/7°: FAIL "posa", non percepibili in nessuna vista.
7. **Vita della posa nel tempo** — FAIL "dati montage": var hs+16, +22, +28, +36 sono praticamente identici in tutte le griglie; braccia congelate, nessun overshoot/assestamento. Gli additivi direzionali muovono il busto ma le braccia restano un blocco.
8. Uscita hs+44 (accovacciata): fuori ambito, non valutata.

## Suggerimenti numerici per il prossimo round
- **Lead spalla**: flessione -30/-35° (mano da altezza testa ad altezza petto/sterno) e abduzione orizzontale all'indietro +25/30°, così la mano resta dietro la linea del petto invece di puntare l'attaccante. Rotazione interna omero +20° (palmo non più rivolto all'attaccante). Gomito: tenere +60.
- **Trailing spalla**: abduzione +25/30° dal busto e estensione indietro +10°; gomito da +45 a +70/75°, così la mano si stacca dall'anca e crea spazio negativo.
- **Trailing mano**: polso indietro da 18 a 30°, spread dita +15° per dito, curl ridotto (dita quasi tese) come la lead.
- **Spalle/clavicole**: alzata da 12/7 a 20/12° più protrazione in avanti ~10° (petto incassato).
- **Colpo laterale**: con le correzioni lead sopra la mano non deve superare la linea del mento; verificare su side_l_az90 che il viso resti libero.
- **Chiavi (dati montage)**: nei 4 key mettere estremo a 0 s (gomiti +15° oltre il valore di tenuta, mani più indietro), assestamento a ~0.15-0.20 s, poi tenuta con piccola deriva (2-3° su gomito/polso) invece di hold piatto.

## Limiti delle miniature
- Splay "teso" dei primi 0.12 s vs splay rilassato: non distinguibile. Pollici e singole dita trailing non giudicabili con camera normale.
- Gradi stimati a occhio (±15°). Nessun frame di idle fornito: "molto diverso dall'idle" giudicato solo contro il base.
- Base e var sono run separate e in side_l la camera/attaccante differiscono: confronto di lettura, non frame-esatto.
- Nessun video: timing, pop d'ingresso e blend con additivi non valutabili da griglie.

## Domande dell'owner
- **"È della stessa qualità di un gioco action stilizzato alla Smash Bros?"** — **No.** Gomiti e asimmetria ci sono, ma il lead legge come gesto volontario (alt/guardia), il trailing è incollato al corpo e la posa resta congelata per ~20 frame.
- **"È alla pari di Smash o altri tripla A simili?"** — **No.** Manca il senso di arti trascinati dall'urto, l'overshoot e la pulizia di silhouette del riferimento Sakurai.

Consiglio: ArmsV1 è meglio del base (tenerlo come nuovo punto di partenza), ma serve un round V2 con i valori sopra prima dell'approvazione.

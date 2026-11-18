# REVIEW ArmsV2 (round 2) — revisore fable

**Esito: FAIL complessivo (non ancora parità Smash). V2 corregge quasi tutti i difetti di V1 ma ne introduce due nuovi: mano lead incollata ad anca/pancia e asimmetria ridotta.** (riga sopra = V1, sotto = V2)

## Tratti
1. **Gomiti piegati (60-110°)** — PASS. Entrambi ~70-90° (close_az45 V2 hs+0..hs+36; hitstand_az45 V2 hs+0).
2. **Mani aperte e divaricate** — PASS. Trailing ora chiaramente splay (close_az45 V2 hs+0..hs+8, hs+28); lead aperta ma spesso nascosta contro il corpo (vedi 5).
3. **Lettura "dolore", non guardia/saluto** — PASS. Sparito il gesto "alt" di V1 (close_az45 V1 hs+0 vs V2 hs+0); braccia basse e trascinate, petto incassato.
4. **Spalle/clavicole** — PASS. Shrug e protrazione ora visibili (close_az45 V2 hs+0/+1; close_az90 V2 hs+0..hs+8).
5. **Chiarezza silhouette** — FAIL "posa". Viso libero nel colpo laterale (side_l_az90 V2 hs+0..hs+6: risolto). Ma la mano lead ora poggia su anca/pancia: di profilo le braccia stanno dentro la sagoma del busto (close_az90 V2 hs+0..hs+12; hitstand_az90 V2 hs+0..hs+36) e nel colpo da sinistra legge come "mano sul fianco" (side_l_az90 V2 hs+16..hs+36; side_l_az45 V2 hs+1..hs+6). Lead fwd +17 cm è troppo poco: V1 era troppo, V2 è l'eccesso opposto.
6. **Asimmetria** — PARZIALE, FAIL "posa" in tenuta. Buona in hitstop (hitstand_az45 V2 hs+4..hs+8: trailing sbalzato fuori). In tenuta le due mani stanno alla stessa altezza anca con gomiti simili, posa "a cowboy" quasi speculare (close_az45 V2 hs+16..hs+36; hitstand_az45 V2 hs+12..hs+36).
7. **Primo frame estremo e diverso dall'idle** — PASS di fronte/45° (hs+-10 vs hs+0 nettamente diversi in tutte le griglie). Di profilo è meno estremo di V1 (hitstand_az90 V2 hs+0: braccia nel busto).
8. **Vita nel tempo** — PARZIALE. Overshoot e assestamento ora leggibili (hitstand_az45 V2 hs+0 → hs+8 → hs+16). Tenuta hs+16..hs+36 ancora quasi ferma: la deriva di 2.5° non si vede ("dati montage").
9. Uscita hs+44: fuori ambito.

## Suggerimenti numerici round 3 (delta rispetto a V2)
- **Lead spalla**: flessione +12° (mano da anca a costole basse/sterno, target fwd ~+26/28 cm dalla spalla, a metà fra V1 e V2) e abduzione +15° per staccare il gomito dal busto di profilo. Rotazione interna invariata.
- **Lead gomito**: da +60 a +80°, polso indietro 30-35°: avambraccio sollevato, mano ad almeno 15 cm dal bacino in ogni vista (controllare side_l_az90).
- **Trailing**: estensione indietro +10° e mano 8-10 cm più bassa/indietro; gomito invariato +72. Obiettivo: differenza di altezza fra le due mani ≥ 25 cm in tenuta.
- **Deriva tenuta**: da 2.5° a 5-6° su gomiti/polsi, fase sfalsata fra le due braccia (~0.2 s) così non respirano all'unisono.
- **Hitstop**: tenere i +15°/+25 % attuali; non toccare shrug 20/12 e protrazione 10.

## Limiti
- Video non guardati (nessun frame estratto): timing, pop d'ingresso e blend con gli additivi restano non giudicati.
- Lead nascosto da anello/particelle in close_az45 V2 hs+4..hs+12. Mano "tesa" vs "rilassata" non distinguibile; pollici non giudicabili.
- Gradi a occhio ±15°; V1 e V2 sono run separate con camera leggermente diversa in side_l.

## Domande dell'owner
- **"È della stessa qualità di un gioco action stilizzato alla Smash Bros?"** — **No**, ma vicino: lettura di dolore, gomiti, mani e spalle ci sono; mancano silhouette di profilo e asimmetria in tenuta.
- **"È alla pari di Smash o altri tripla A simili?"** — **No.** La mano sul fianco/pancia e la tenuta quasi speculare e ferma non reggono il confronto col riferimento Sakurai.

Consiglio: V2 è il nuovo punto di partenza (meglio di V1); un terzo round mirato solo al braccio lead e alla deriva dovrebbe bastare.

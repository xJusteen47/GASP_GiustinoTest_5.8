# Audit degli hit generici della vittima (cloud, sola lettura, 2026-11-23)

Nessuna modifica, nessuna cattura. Criteri di oggi: regole 10, 13, 15b. Etichette: **gioco** (codice/ABP), **montage** (tempi, blend,
sezioni), **dati** (chiavi della clip corrette da script, senza toccare la posa), **posa** (lavoro d'animazione, giro di review).
Fonti (tutte lette in sola lettura): `react-v3/review.md` (v1falr 06277fc1), `react-hook-4/review.md`, `react-hook-6-review.md`,
`m1-reaction-bones/README.md`, `night-abp.md`, `night-knockdown.md` (integ-2026-11-23 e0fcf520), `hit-reactions-1.md`,
`hit-react-directional.md`, `hit-arms-1.md` (claude/hit-arms-1 933b42ec), `getup-momentum-2.md` (qjfnc0 e16db0f4).

## Fatti comuni a tutte e 8
- **Nessuna è approvata o bloccata**; nessuna ha tratti d'identità (13a) né sheet delle curve (13c); mai passate dai controlli automatici 15b.
- **Root a ~16 cm dall'origine** in tutte le clip di reazione (night-abp, misurato): al blend-out il root torna sotto la capsula e trascina
  i piedi (~11-12 cm, lo "scivolamento") e abbassa il bacino di 4-5 cm. Non è Offset Root Bone (spento, identico). È **dati** (chiavi del
  root) oppure **gioco** (bloccare il root delle pose di slot nell'ABP): stessa causa, due posti dove correggerla.
- **Accovacciamento dopo il montage** (~25 frame) anche dopo HitStand e Tumble, non dopo Splat (react-v3). Parte è il root sopra; dopo
  HitStand react-v3 vede un accovacciamento profondo (petto sulle ginocchia), più dei 5 cm misurati: probabile seconda causa **gioco**
  (motion matching che sceglie una posa d'atterraggio per lo stato/tag attivo al frame d'uscita). Da provare con una cattura dopo il fix del root.
- **Chiavi di scala non unitarie** (testa 1,15, cosce 1,12, piedi 0,89) misurate su Tumble, HitStand, GroundHit, Splat (m1, 11-17).
  La pipeline è corretta (`ue_rig.py::_unit_scale`), ma **non risulta** che le 8 clip in gioco siano state ricotte: **dati**, da verificare.
- In gioco l'HitStand usato è ancora la base o ArmLow: il set `DA_ReactionAnims_HitArms` (ArmsV3, scelta owner 1A del 18/11) non è mai stato
  applicato (`hitarms_swap.py` non eseguito, branch hit-arms-1 non in integ). Il set variante è una copia del 18/11 16:00 (rischio: modifiche
  dell'owner a DA_ReactionAnims dopo quell'ora non arrivano in gioco).

## Scheda per clip
| Clip | Stato | Difetti aperti | Tipo |
|---|---|---|---|
| **HitStand** | PASS react-v3; ArmLow PASS; ArmsV3 "difendibile" (r3), mai in gioco | accovacciamento dopo (~25 f); piedi scivolano ~12 cm nei primi 20 f di hitstun; colpo da lato: una posa sola non regge (braccio guida sbagliato); clip di 4 key, posa congelata nella tenuta; additivo non riparte al 2° colpo; uscita brusca dall'hitstun (pop) | gioco + dati (root); gioco (scelta/specchio per lato, già 2A); posa (vita nella tenuta, risolta da ArmsV3); gioco (additivo) |
| **Tumble** | PASS | taglio secco all'ingresso; clip in place a ~90 cm (va bene in aria, causa la caduta verso GroundHit); accovacciamento dopo | montage (blend d'ingresso); gioco/dati (root) |
| **Splat** | PASS con riserva | mai catturata dalla telecamera dell'attaccante (davanti); bacino 10-15 cm staccato dal muro con spalle a contatto; testa e mano da controllare per 2-5 cm dentro il muro | cattura; posa; dati (controllo compenetrazioni 15b) |
| **GroundHit** | PASS solo come sequenza dopo i fix di codice | un frame di posa d'impatto ~90 cm sopra il pavimento (react-hook-6, dopo il fix del pop di react-hook-4: va riverificato); rimbalzo della clip non visibile; ora il gioco fa già un rimbalzo di ~18 cm (night-knockdown): possibile doppio rimbalzo o rimbalzo che si contraddice | montage/gioco (GroundHit che parte prima del contatto o f0-f2 dall'altezza del Tumble); posa solo se dopo il fix il rimbalzo non legge |
| **LieUp** | mai giudicata da sola; pop di gap risolto (react-hook-4) | root 16 cm; scala da verificare | dati |
| **Roll** | PASS | gambe verticali a f3-f4 possono leggersi come rotolata all'indietro (tuck); warp direzionale (getup-momentum-2) mai guardato da un revisore; perno sulla schiena oltre ~60° probabilmente strano | posa (facoltativa); gioco (in corso nel thread get-up) |
| **RollBack** | PASS | diagonale che parte tardi (f2-f4 gambe ancora al centro); corsa 65 vs 80 cm accettata | posa (facoltativa) |
| **GetUp** | PASS dopo i fix di codice | primi 3 frame molli; accovacciamento dopo la rialzata (v81-v100); round 2 velocità d'uscita in corso sul portatile | posa o montage (blend d'ingresso, da capire con la cattura); gioco/dati (root); gioco (get-up thread) |
| Additivi direzionali (8) | 4 PASS (Front, Right, Left, Air front); 4 mai visti (Ground_Back, Air_Back/Left/Right) | Ground_Back non colpisce mai nella cattura; il giro verso l'attaccante fa leggere ogni colpo forte come frontale | cattura; decisione owner aperta (giro) |

## Piano di rifacimento (in ordine; costi dai reali di INDICE/costi.csv)
**Fase 0, dati, fisso headless, Haiku/Sonnet, ~2-3 $, ~15 min.** Uno script su tutte le 8 clip: chiavi di scala (`react_bones_m1.py`),
offset del root, controlli 15b (caviglie, punte, ossa +5%, tremolio gomiti/polsi, compenetrazioni). Solo lettura, un file di esito.
Se la scala è ancora sbagliata, si ricuociono le clip con `_unit_scale` (stesso thread, ~1 $ in più).

**Fase 1, gioco/montage, un thread di codice, Opus medium, ~8-12 $ + cattura ~4 $ (stima da getup-momentum, night-knockdown).**
1. Root: consigliata la correzione **dati** (azzerare l'offset orizzontale del root nelle 8 clip con script), perché resta nelle clip di
   reazione e non tocca l'ABP condiviso; l'alternativa è il lock nell'ABP (night-abp A). Una cattura prima/dopo dice se sparisce anche
   l'accovacciamento dopo HitStand; se no, seconda causa (stato al frame d'uscita) nello stesso thread.
2. GroundHit: farlo partire all'altezza del Tumble o qualche frame prima del contatto; decidere quale rimbalzo vale (gioco 18 cm o clip).
3. Blend d'ingresso del Tumble; uscita dall'hitstun senza pop; additivo che riparte al 2° colpo.
4. Applicare `hitarms_swap.py` (scelta 1A già data) e la scelta/specchio della posa HitStand per lato (scelta 2A già data).
Poi **una sola sessione di cattura** di tutte le sequenze, da dietro e di lato più la telecamera dell'attaccante per Splat (15e).

**Review cloud (regola 10), Fable/Opus, 3-4 $ l'una:** HitStand, Tumble, Splat, sequenza knockdown (GroundHit+LieUp+GetUp), Roll+RollBack
come una sola review dei get-up già in coda. Circa 4-5 review = **12-20 $**.

**Fase 2, posa, solo per le voci che dopo la Fase 1 sono ancora FAIL "posa".** Prima i tratti d'identità e lo sheet delle curve (13a, 13c).
Probabili: GetUp primi 3 frame, Splat bacino sul muro, eventualmente rimbalzo GroundHit; facoltative: tuck del Roll, diagonale del RollBack;
pose laterali di HitStand se lo specchio non basta; 4 additivi mai visti (prima catturarli). Costo reale per clip: 5-8 $ a giro di build
+ 3-4 $ di review, 1-3 giri: **8-25 $ a clip** (hit-arms: 3 giri 18-25 $).

**Totale:** Fase 0+1 con review circa **25-40 $**, senza pose nuove. La Fase 2 dipende dalle review: con 3 clip a un giro ~25-35 $ in più.
Il grosso dei difetti segnalati (piedi, accovacciamento, GroundHit) è gioco/dati, non posa.

## Da portare all'owner (13d)
1. Dove correggere il root: dati nelle clip (consigliata) o lock nell'ABP.
2. Quale rimbalzo a terra vale: quello di gioco (~18 cm) o quello della clip.
3. Il giro verso l'attaccante sui colpi forti da dietro/lato (aperta dal 16/11).

# Loop di corsa Overlord dai riferimenti di Ganon: corsa base + sprint 1 (piano)

Owner 2026-11-21 02:08Z (ambito: loop di corsa dell'Overlord, stadio 1 "corsa" e stadio 2 "sprint 1"):
la camminata di Ganon è troppo poco "corsa", la corsa è troppo "sprint": la nostra corsa base è una via di mezzo;
lo sprint 1 può essere un facsimile della run di Smash; **le clip legacy non si buttano**. Commento sul run start
variante 0 (busto eretto, gomiti più bassi, "sembra un cartone anni 60") = vale anche per la corsa base.
Solo piano; l'implementazione è un thread sul fisso. Lo sprint 2 (braccia aperte) aspetta il riferimento dell'owner.

## 1. Cosa mostra il video (IMG_9598.mov, 30 fps, telefono puntato sullo schermo, tempi +-1 frame)
Griglie: `handoffs/overlord-run-loop-ref/` A_ganon_walk_keys.jpg, B_ganon_run_keys.jpg (pose chiave etichettate),
walk15.jpg e run30.jpg (sequenze intere, ritaglio che segue Ganon). Il passo si conta da appoggio largo ad appoggio largo.

| | Camminata (0,7-3,2 s) | Corsa (dash 5,57-5,80 s, loop da 5,83 s) |
|---|---|---|
| Ciclo (2 passi) | ~23 f = **0,75-0,77 s** (appoggi f33, 44, 55, 67, 78, 89) | ~16 f = **0,53 s** (appoggi f183, 191, 199) |
| Volo | nessuno, sempre un piede a terra | lungo, ~35-40 % del passo |
| Busto | quasi eretto, ~10° avanti, petto in fuori | ~20-25° avanti + spalle curve, testa avanti e in giù ("toro") |
| Ginocchio al passaggio | ALTO, calpestio pesante (f61, f73) | alto, gamba di spinta tutta tesa (f189) |
| Braccia | gomiti ~90°, pugni alla pancia, oscillazione corta ~15-20° | pistone: pugno avanti fino al mento, gomito dietro la schiena, ~45° |
| Testa | ferma, sguardo avanti | in avanti, mento giù |
| Altro | mantello trascinato | polvere a ogni spinta; il dash d'avvio è quasi orizzontale (~60°, f171-174) |

## 2. Spec STADIO 1 - corsa base (via di mezzo)
Tratti d'identità (regola 13a, la review li controlla per primi):
1. **Busto eretto e petto fiero** (lean <= 12°), con il **ginocchio che calpesta** della camminata (rif. A f61/f73).
2. **Gomiti bassi, pugni alla pancia** che pompano corti: niente braccia alte da cartone anni 60 (owner 02:08Z).
3. **Peso**: a ogni appoggio il bacino scende e la testa si muove meno del bacino.

| Valore | Camminata ref | **Corsa base** | Run ref |
|---|---|---|---|
| Ciclo | 0,76 s | **0,64 s (~19 f a 30 fps; 2 passi da 0,32 s)** | 0,53 s |
| Volo per passo | 0 | **~15-20 % (2 f)** | 35-40 % |
| Lean busto | ~10° | **8-12°**, mai oltre 12 | 20-25° |
| Ginocchio al passaggio (coscia dalla verticale) | ~55° | **50-60°** | ~70° |
| Braccia | 15-20°, pugni alla pancia | **±25-30° dalla spalla, gomito 90-100°, gomito mai sopra le costole basse, pugni tra ombelico e sterno, un po' larghi (massa, nota v4r7)** | ±45°, pugno al mento |
| Bacino | | **calo 4-6 cm 1-2 f dopo l'appoggio; risalita in spinta** | |
| Testa | ferma | **mento leggermente giù, bob <= metà del bacino** | avanti/giù |
| Larghezza tracce piedi | larga | **~+25 % sullo stock** (bruto), senza incrociare | |

Sheet delle curve (regola 13c): appoggio = contatto secco (niente ease in sul piede), discesa del bacino ease out
in 2 f, braccia con follow-through di 1-2 f sul polso rispetto al gomito, busto con micro-torsione opposta al bacino
(±6-8°), overshoot 0. Fasi: appoggio A 0 %, giù 12 %, passaggio 25 %, su/volo 40 %, appoggio B 50 % (simmetrico).

## 3. Spec STADIO 2 - sprint 1 (facsimile run di Smash)
Tratti d'identità:
1. **Toro che carica**: busto 20-25° avanti con spalle curve e testa avanti, mento giù (rif. B f183, f193).
2. **Pistone delle braccia**: pugno avanti fino al mento, gomito dietro la schiena (rif. B f187).
3. **Volo lungo** con la gamba di spinta tutta tesa (rif. B f189, f197).

| Valore | **Sprint 1** |
|---|---|
| Ciclo | **0,53 s (16 f; passi da 0,27 s)** |
| Volo per passo | **35-40 %** |
| Lean | **20-25° + curva delle spalle**; cambio di posa rispetto allo stadio 1 >= 12° = "si è piegato" (decisione owner 2A) |
| Braccia | **±45°, gomito 80-90°**, pugno avanti all'altezza del mento, gomito dietro oltre la schiena |
| Ginocchio al passaggio | **~70°**, gamba di spinta tesa |
| Testa | in avanti, mento giù, bob piccolo |
Curve: spinta esplosiva (ease out nella spinta), atterraggio secco, braccia in controfase pura, overshoot del pugno
avanti 1 f. VFX: sbuffo a ogni spinta, come già nel piano degli stadi (S2), non in questa clip.
Il dash d'avvio di Smash (quasi orizzontale) NON entra: è un'altra cosa (run start / entrata dello sprint 2).

## 4. Come si fanno (thread sul fisso, Cascadeur)
1. Prima: chiedere all'Unreal Assistant come GASP stock sceglie tra Run Loops (BS_Relaxed_Run_Loop_F_Slopes) e
   Sprint Loops (M_Relaxed_Sprint_Loop_F) e se un BS nostro può sostituire il BS delle pendenze (regola 14).
2. Base: in Cascadeur importare **M_Relaxed_Run_Loop_F** (campione piano del BS) per la corsa base e
   **M_Relaxed_Sprint_Loop_F** per lo sprint 1, sullo scheletro dell'Overlord, stessa pipeline del run start v4r7
   (Saved/AnimPipeline/runstart/). Si tengono root motion e appoggi dello stock, si cambia la parte sopra e il tempo.
3. Tempo: si ritimano a 0,64 s e 0,53 s. La lunghezza del passo = velocità dello stadio (MaxSpeed in DA_Locomotion_Default >
   Run Stages, da leggere) x durata del passo (0,32 / 0,27 s), così il root combacia con la capsula e lo stride warping
   lavora vicino a 1. Piedi piantati senza slittare.
4. Pose: 8 chiavi per clip (appoggio/giù/passaggio/su per piede) dalle tabelle sopra, poi AutoPhysics per peso e bacino;
   il mantello resta alla cloth.
5. Export FBX -> `Content/GSTN/...` con nomi `AS_GSTN_Overlord_RunLoop_F_v1` e `AS_GSTN_Overlord_Sprint1Loop_F_v1`.
   Curve con Tools/GAS/routing/curves_author.py: contact_l/r, **phase nella convenzione stock (triangolo +1 appoggio L,
   -1 appoggio R, come chiede la divergenza 19)**, movedata_speed, enable_warping = 1. Nessun sync marker (lo stock non li ha).
6. Righe chooser: nel chooser annidato per kit (override per skin) SENZA pose match nel genitore, riga nostra sopra la riga
   stock: Gait Run -> corsa base, Gait Sprint -> sprint 1; fallback = stock. Stride warping acceso come stock.
7. Pendenze: lo stock in avanti è un BS con pendenze. v1 = BS nostro con lo stesso layout: campione piano = la nostra clip,
   campioni in salita/discesa = la nostra clip + il delta stock (pendenza meno piano) come additivo, metodo valido per
   qualsiasi set. Test sulle pendenze obbligatorio (principio owner).
8. Legacy: nessuna clip si cancella; le righe vecchie si disattivano, non si tolgono. Righe in gasp-divergenze.md prima del commit.
9. Controllo isolato della clip (clip-isolation) PRIMA dell'in-game, poi cattura base + variante, ASSERT: OK, review cloud
   (regola 10): una review per clip, vita prima dei numeri.
10. Raccordo: il run start v4r7 (bloccato) passa al loop. Se il raccordo stona con la nuova corsa base, si fa una variante
   del run start, non si tocca la v4r7 (regola 13b).

## 5. Costo stimato
Questo thread ~3 $. Thread fisso (Opus high per curve e righe, Fable giudica le pose, catture a subagenti):
corsa base ~8-10 $, sprint 1 ~6-8 $ (più vicino allo stock), BS pendenze ~3 $, 2 review cloud ~2 $ l'una.
**Totale ~20-25 $**, in linea con S3 del piano degli stadi (20-25 $ con la 3B). Sprint 2 escluso.

## 6. Da confermare (al coordinatore, regola 13d)
- Commento owner sulla variante 0 del run start (busto eretto, gomiti più bassi): la v4r7 è bloccata, quindi serve una
  variante nuova del run start (v4r9) con la stessa parte alta della corsa base. Consigliato: farla nello stesso thread,
  dopo la corsa base, così il raccordo esce coerente.
- Il ciclo 0,64 s della corsa base è una via di mezzo calcolata; si regola a vista sul video in-game.

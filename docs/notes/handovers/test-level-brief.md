# Brief TH-1: livello di test giocabile `L_Test_Hub` (fisso, RC con l'editor dell'owner, Sonnet, ~$5-8)

**Scopo.** Una mappa dove l'owner prova a mano tutto ciò che è costruito: colpi M4, difesa/link M6, piggyback M7 (mount, drop, colpito da
montato, lancio del rider, boost), spin/drill, traversal, mosse pilot/overlord con le animazioni correnti. Primo controllo *renderizzato* di
cose viste finora solo a `-nullrhi`. Leggi solo: questo brief, [test-maps.md](../../architecture/plans/test-maps.md) §0, `docs/architecture/testing.md`
(sezione listen/PIE), `Tools/GAS/build_test_combat_map.py`, `Tools/GAS/build_wall_slide_ab_map.py` e gli `prepare_editor()` degli scenari.

**Mappe esistenti.** `L_Test_Combat` (TM2 parziale: muro, vetro, KO zone, 3 trampolini; da script), `L_WallSlideAB` (corsie wall slide A/B; da
script), `DefaultLevel_Harness` (harness), più le mappe del sample GASP. CAM, INT, DM, WS2, TM sono **milestone**, non mappe (DM ha solo un
prototipo su un branch). **Scelta: una mappa nuova unica**, non sotto-livelli: le due mappe hanno pavimento, PlayerStart e GameMode
propri nello stesso punto e l'harness vuole un livello solo. Lo script nuovo **importa i builder** di quelle due mappe e li chiama con un offset.

**Prerequisiti (passo 0 della [sequenza](../../architecture/plans/sequenza-2026-11-06.md)).** Checkout principale su `feature` con M7 e SD uniti, compilato;
nel `.uproject` accesi GSTNDefence, GSTNLink, GSTNSpinDrill, GSTNPiggyback, GSTNPlatform, GSTNLevelObjects (controlla, non toccare la modifica
non committata dell'owner: se manca un plugin, chiedi). GameMode `GM_GASNPPTest`.

**Layout** (pavimento blu con tacche da 1 m, cartello `TextRender` per stazione, tabellone in piazza; colori di test-maps.md §0):

| Zona | Dove | Contenuto | Si prova |
|---|---|---|---|
| Z0 Piazza | origine, 40×40 m **vuota** | 4 PlayerStart a 3 m, tabellone con zone e comandi | spawn; le corsie dell'harness (−15 m X/Y) restano libere |
| Z1 Colpi M4 | +X 30 m | builder di `L_Test_Combat` (muro, vetro, KO, trampolini) + corsia knockback 30 m e righello 10 m | hit, hitstun, knockback, splat, rottura vetro, ring-out |
| Z2 Duello M6 | +X 70 m | arena tonda 20 m con bordi KO | scudo, parry, prese, lanci, tech, mash |
| Z3 Piggyback M7 | +Y 30 m | pad 20×20 m, sporgenza 4 m, muro, tacche per distanza lancio e altezza boost | mount/dismount/drop, colpo da montato, throw rider, boost jump |
| Z4 Spin/drill | +Y 70 m | anello di 4 bersagli, piattaforma 3 m per la picchiata, blocco non trapanabile | spin a terra/aria, cancel opposto, latch, rimbalzo |
| Z5 Traversal | −X 40 m (oltre le corsie) | 2 corsie del builder `L_WallSlideAB`, gradini, salti, rampe (sottoinsieme TM1), piattaforma mobile + `gstn.Platform.SpawnTestFighter` | wall slide, salto, piattaforme |
| Z6 Animazioni | −Y 40 m | uno `SkeletalMeshActor` per mossa in loop (jab v12, ftilt v31, dair corrente) con etichetta; se la mossa è nel kit, anche dal vivo su un bersaglio | lettura delle mosse in gioco (con `slomo 0.25`) |

**Bersagli.** Non esistono manichini: i bersagli **statici** sono i pawn dei client PIE parcheggiati da helper; i **mobili** sono un client
che cammina in tondo (input iniettato) e il test fighter della piattaforma. **Helper** nuovo `Tools/GASNPPTest/test_hub.py` (solo Python, chiama
l'harness esistente): `prepare()` (kit di mount/spin/link creati in memoria dai `prepare_editor()`; avvisa se due si contraddicono), `kit(i, nome)`,
`park(i, zona)`, `walker(i, on)`, `mount(rider, carrier)`, `fighter(zona)`, `selftest()` (ogni player in ogni zona, un'azione per zona, su file).
Una pedana di cambio kit in gioco richiede codice C++: domanda D1 della sequenza (default: solo Python).

**Passi (thread RC sul fisso; il thread non salva mai mappe).**
1. Thread: audit di 10 minuti: attori disponibili (`AGSTN_Trampoline`, `AGSTN_KOZone`, `AGSTN_TestSurface`, piattaforme scriptate), kit e mosse
   animate esistenti (AS_/AM_ e se sono legati a un kit). Scrive su file cosa manca; se manca qualcosa di bloccante, lo segnala, non lo costruisce.
2. **Owner**: salva il proprio lavoro, poi *File > New Level > Empty Level* (nessun salvataggio).
3. Thread: `build_test_hub_map.run()` riempie il livello aperto (idempotente: cancella solo gli attori con il suo tag); mai `save_*`.
4. **Owner**: *Ctrl+S* → salva come `/Game/Levels/Test/L_Test_Hub`. Dopo ogni correzione del thread, di nuovo Ctrl+S.
5. Thread: Play listen con **3 player** (host + 2 client, Run Under One Process; `setup_editor(True)` per GameMode e throttling), `test_hub.prepare()`,
   `selftest()`; message log senza errori. Poi `setup_editor(False)` e ripristino delle PIE settings.
6. Thread: commit di script, helper e (dopo il salvataggio dell'owner) della `.umap` su un branch; l'owner fa il push.

**Come la usa l'owner.** Play (listen, 3 player), finestra host col gamepad; nella console Python dell'editor `import test_hub as h; h.prepare()`,
poi `h.park(1,'Z1')`, `h.mount(0,1)`, `h.kit(0,'KIT_Test_Mount')` ecc. Il tabellone in piazza riporta questi comandi. Lo spin vuole la rotazione dello stick.

**Coperto finora solo a `-nullrhi` (guardare qui per la prima volta).** Tutti gli scenari spin/drill (portatile, presentazione minima D11); mesh
del rider sul proprio client (finding B) e salto di 74 cm all'attacco (finding A, M7-13), nodi IK dell'AnimBP ancora TODO (posa del rider
probabilmente sbagliata: atteso); test fighter delle piattaforme. Mai provati: server dedicato, suite DEFAULT/PARTS/DEFENCE nelle 3 configurazioni.

**Fatto quando:** mappa salvata dall'owner e caricata senza errori, `selftest()` verde in tutte le zone, tabellone leggibile, handover ≤ 30 righe in
`docs/notes/handovers/test-hub.md` con costo e l'elenco di ciò che non ha funzionato (diventa il brief di TH-2).

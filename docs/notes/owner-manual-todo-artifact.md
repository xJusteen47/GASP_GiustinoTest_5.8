# Cose da fare a mano (copia dell'artifact)

Copia in repo dell'artifact https://claude.ai/artifact/XjMiMf16f85qP5TvqPCaSk, aggiornata 2026-11-11 (M10 slice 8). L'artifact è la versione di riferimento.

## 1 · Git

- [ ] Porta weakpoint 3/3b e M10 slice 1-7 (oggetti: raccogli, posa, lancia, cadono se colpito; tasti B/RT già nei kit di prova, prompt "B"; slice 4: oggetti a terra nel probe di interazione, colpi lanciati con `Combat.Source.Thrown`; slice 5: l'oggetto in mano è un legame Hand di GSTNLink, B prende l'oggetto inquadrato, credito del lancio anche sui minion; slice 6: peso e taglie, impatti Rompi/Rimbalza/Attacca, fix del fork sugli attributi dei proxy; slice 7: un colpo per bersaglio per lancio anche dopo i rimbalzi, fuoco amico per tipo di colpo, le due impostazioni di raccolta; slice 8: mira di spalla per il lancio, oggetti che esplodono, l'oggetto lanciato parte subito dalla mano sul tuo client) su `feature/gas-npp-5.8` (tip di `claude/desktop-m10-8`, sha esatto nell'artifact, sostituisce `f9d5e0a3`), poi `git pull` e build completa da Rider (header di GSTNLink e GSTNInteract cambiati, nuova dipendenza GSTNHeld → GSTNLink):
  ```
  cd "C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2"
  git fetch origin claude/desktop-m10-8
  git push origin <sha>:refs/heads/feature/gas-npp-5.8
  ```

## 2 · Editor (salva tu con Ctrl+S)

- [ ] **Recoil segnaposto in PIE** con la mossa di test `KIT_Test_StrikeKinds` (solo test: swing da destra frame 8-14, affondo dritto 22-26, nessun recoil proprio):
  1. Editor nella cartella principale, mappa `L_Test_Hub`.
  2. Console Python: `import sys; sys.path.insert(0, r"C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2\Tools\GASNPPTest"); import test_hub as h` poi `h.prepare()`.
  3. Play (listen, 3 giocatori), aspetta i manichini.
  4. `h.strike_kinds(0)`: kit di test, ogni colpo rimbalza, un manichino 120 cm davanti.
  5. Attacco (tasto sinistro / X): lo swing prende il manichino, finisce lo swing, poi rinculo Swing (`…B_A…`, 30 frame).
  6. Reticolo acceso (click stick destro), poi stick destro tutto a destra mentre attacchi (o gira di ~30° a destra): lo swing manca, l'affondo prende, rinculo Stab subito (`…B_V…`, 24 frame).
  7. `h.strike_kinds(0, False)`, ferma il Play, `h.restore()`. Niente da salvare.
- [ ] Prova sterzata e reticolo con un pad: reticolo spento = stick destro solo camera, il sinistro gira il personaggio; reticolo acceso = in avvio lo stick destro gira la mossa (max 20°) e vince sul sinistro. Reticolo solo PvE/TDM, freccia ai piedi, azioni automatiche.
- [ ] Oggetti coi tasti veri (M10 slice 3) in PIE: mappa `L_Test_Hub`, Python `import test_hub as h` + `h.prepare()`, Play listen 3 giocatori (kit `KIT_Test_Hits`, ha già i tasti). Console host: `gstn.Held.Spawn 0`. Avvicinati: compare "B" sopra il cubo. B = prendi, B di nuovo = posa, RT tap = lancia verso lo stick sinistro, RT tenuto = reticolo e lancio verso la camera al rilascio. Lontano dal cubo B fa quello di prima. `gstn.Held.Report` mostra `carrying=` e `target=`. Slice 5: con due cubi vicini gira lo stick verso uno dei due: la "B" sta sopra quello e B prende proprio quello. Kit con i tasti: `KitsWithDefaultBindings` in `Plugins/GSTNHeld/Config/DefaultGSTNHeld.ini` (o Project Settings → Plugins → GSTN Held). Slice 6: `gstn.Held.Spawn 0 120 Medium 50` (Medio, corsa e salto al 70 %, niente scudo né attacchi aerei), `Heavy 50` (solo cammina, posa, lancia), `Huge 100` (B non lo prende); quinto argomento `Break`/`Bounce`/`Stick` per gli impatti (Break ricompare dopo 3 s; da slice 7 un rimbalzo non ricolpisce chi il lancio ha già colpito). Slice 7: in Project Settings → Plugins → GSTN Held (Interact) `Pickup Takes Focused Object` spento = sempre il più vicino; `Focus Miss Falls Back To Nearest` spento = mano vuota se l'inquadrato lo prende prima un altro (tutte e due accese di default). Slice 8: `gstn.Held.Spawn 0 120 Light 5 Explode` = il cubo esplode dove colpisce (raggio 2,5 m, colpisce tutti, poi ricompare dopo 3 s); RT tenuto: la camera dovrebbe andare sopra la spalla, ma serve che il pawn passi i tag della mira alla camera (non ancora collegato), quindi per ora vedi solo il reticolo e il lancio verso il mirino; da client il cubo deve partire subito dalla mano senza scatti. In Project Settings → Plugins → GSTN Held `Aim View` = `Keep View` torna al comportamento di prima. Niente da salvare.
- [ ] Fuoco amico per modalità (slice 7): nelle regole della modalità (`UGSTN_CombatRules`, Teams) `Friendly Fire Sources` con `Combat.Source.Thrown` = gli oggetti lanciati colpiscono anche gli alleati, il corpo a corpo no. Vuoto (default) = alleati ignorati.
- [ ] Strike Kind per gruppo di hitbox (`Override Strike`, Swing/Stab); recoil per attacco sulla traccia Hitbox; `Stab Only` sui weakpoint interni.
- [ ] `L_MinionYard` con `setup_yard`; aggancio tinta P10; contenuto TM3; `L_Test_Combat` (facoltativo); pair-move in PIE; `gstn.Net.SimProxySendEveryNTicks 2`;
  binding del tasto di sparo; `gstn.Projectile.Fire` / `Ability 0 1`; ghost del proiettile con lag; harness rosso su `DefaultLevel` nel worktree m7 (mappa, non codice); Oodle (facoltativo).

## 3 · Decisioni

- [ ] Leve weakpoint §6 (scala hurtbox, priorità, magnetismo PvE).
- [ ] M10: un oggetto posato o atterrato si può riprendere dopo 0,64 s (ritardo di rete dei record): va bene o lo vuoi più corto? Danni e archi veri per tipo di oggetto; quali kit hanno i tasti degli oggetti (ora solo i kit di prova; c'è l'opzione "tutti i kit").
- [ ] G6 fatto con valori di prova (Leggero -10 %, Medio -30 %, Pesante -50 % senza salto, Enorme -70 % e capacità; lancio x1/0,85/0,7/0,5; capacità di default 50 kg): dimmi i valori veri e quali oggetti si sollevano. Impatti: un rimbalzo può colpire di nuovo chi ha già preso il lancio; Break si rompe anche su un nemico. Va bene?

## 4 · Animazioni

- [ ] Rinculo Swing e Stab (oggi segnaposto `…shove_stand_B_A…` / `…B_V…`). Blend della mano per il caso "faccia di taglio" (più avanti).

## 5 · Pulizia branch

- [ ] GitHub: branch vecchi elencati nell'artifact; dopo il push anche `claude/desktop-weakpoint-3` , `claude/laptop-weakpoint-3b`, `claude/desktop-m10-1`, `claude/desktop-m10-2`, `claude/desktop-m10-3`, `claude/desktop-m10-4`, `claude/desktop-m10-5` e `claude/desktop-m10-6`. Portatile: `git branch -d claude/laptop-projectile-1`.

## Fatto

- 2026-11-10 · M10 slice 7: un rimbalzo non ricolpisce chi il lancio ha già colpito (opzione per oggetto); fuoco amico per tipo di colpo nelle regole della modalità (`Friendly Fire Sources`); le due impostazioni di raccolta decise da te (inquadrato o più vicino; conteso: mano vuota o il più vicino), accese come prima. Branch `claude/desktop-m10-7` (sha nell'artifact).
- 2026-11-10 · M10 slice 6: peso e taglie (G6: carico sul legame Hand, rallenta corsa e salto su ogni macchina, comandi tolti per taglia, Enorme solo con capacità, lancio più debole per taglie grandi) e impatti Atterra/Rompi/Rimbalza/Attacca (parte di G7); corretto un bug del fork GAS-NPP (finito un effetto su una statistica, gli altri client la vedevano a 0). Headless 1495/0, harness PASS oggetti 4/4 listen 2 % e dedicato 0 %, presa 5/5 a 2 %. Branch `claude/desktop-m10-6`. Restano: mira con la spalla e fuoco amico (G7), ghost dell'oggetto (G8), nemici lanciati (G9), debug ed editor (G12-G13).
- 2026-11-10 · M10 slice 5: l'oggetto in mano è ora un legame Hand di GSTNLink (G1: il watchdog sa giudicare un partner che non è un combattente); B prende l'oggetto che il probe inquadra (il più vicino libero se un altro lo prende nello stesso frame); KO dei minion da oggetto lanciato con `Combat.Source.Thrown` e credito a chi lancia. Headless 1490/0, harness PASS oggetti listen 2 % e dedicato 0 %, presa 5/5 a 2 %. Branch `claude/desktop-m10-5`. M10 non è finita: restano peso e taglie (G6), impatti e mira (G7/G8), nemici lanciati (G9), debug ed editor (G12-G13).
- 2026-11-10 · M10 slice 4: gli oggetti a terra entrano nel probe di interazione (un solo bersaglio di B per frame, uguale su ogni macchina, anche accanto a un nemico da afferrare); i colpi degli oggetti lanciati hanno il tag `Combat.Source.Thrown` e danno il credito a chi lancia anche se il suo pawn non c'è più. Headless 1489/0, harness PASS listen 2 % e dedicato 0 %. Branch `claude/desktop-m10-4`.
- 2026-11-10 · M10 slice 3: tasti B/RT degli oggetti collegati in automatico ai kit di prova (nessun Blueprint toccato); RT con l'oggetto in mano ora lancia davvero (prima vinceva lo smash forte); B prende solo con un oggetto vicino, altrimenti fa quello di prima; prompt "B" sopra l'oggetto. Headless 1484/0, harness PASS listen 2 % e dedicato 0 %. Branch `claude/desktop-m10-3`.
- 2026-11-10 · M10 slice 2: l'oggetto in mano è nello stato di sync del personaggio (predetto), cade se chi lo tiene è colpito, lanciato in aria, afferrato o KO; tasti di default come dati: B prende e posa, RT tap lancia verso lo stick sinistro, RT tenuto mostra il reticolo e lancia verso la camera. Headless 1481/0, harness PASS listen 2 % e dedicato 0 %. Branch `claude/desktop-m10-2`.
- 2026-11-10 · Decisione presa (tua): tasti degli oggetti come sopra, tutto da rivedere col playtest. Nemici presi come oggetti e lanci alla smash: design per M11.
- 2026-11-10 · M10 slice 1: plugin `GSTNHeld`, raccogli, tieni, lancia (colpo tramite `GSTNProjectile`, credito a chi lancia), atterraggio, di nuovo raccoglibile; headless 1477/0, harness PASS listen e dedicato a 0 % e 2 %. Branch `claude/desktop-m10-1`.
- 2026-11-10 · Weakpoint slice 3b: stick destro gira il personaggio solo col reticolo acceso e allora vince sul sinistro (decisione presa); reticolo nell'input predetto, pronto per le mosse ranged mirabili (headless 1475/0). Branch `claude/laptop-weakpoint-3b` (`ceb9d1c2`).
- 2026-11-10 · Weakpoint slice 3 (sterzata predetta via Mover, reticolo PvE/TDM, freccia, azioni automatiche, mossa di test per i recoil; headless 1474/0, harness PASS). Branch `claude/desktop-weakpoint-3`.
- 2026-11-09 · Push di `92e9b029` su `feature/gas-npp-5.8` (fatto da te). Voci precedenti: vedi artifact.

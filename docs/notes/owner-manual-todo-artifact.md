# Cose da fare a mano (copia dell'artifact)

Copia in repo dell'artifact https://claude.ai/artifact/XjMiMf16f85qP5TvqPCaSk, aggiornata 2026-11-10. L'artifact è la versione di riferimento.

## 1 · Git

- [ ] Porta la weakpoint slice 3 su `feature/gas-npp-5.8` (tip di `claude/desktop-weakpoint-3`, sha esatto nell'artifact, sopra `92e9b029`), poi `git pull` e build completa da Rider:
  ```
  cd "C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2"
  git fetch origin claude/desktop-weakpoint-3
  git push origin origin/claude/desktop-weakpoint-3:refs/heads/feature/gas-npp-5.8
  ```

## 2 · Editor (salva tu con Ctrl+S)

- [ ] **Recoil segnaposto in PIE** con la mossa di test `KIT_Test_StrikeKinds` (solo test: swing da destra frame 8-14, affondo dritto 22-26, nessun recoil proprio):
  1. Editor nella cartella principale, mappa `L_Test_Hub`.
  2. Console Python: `import sys; sys.path.insert(0, r"C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2\Tools\GASNPPTest"); import test_hub as h` poi `h.prepare()`.
  3. Play (listen, 3 giocatori), aspetta i manichini.
  4. `h.strike_kinds(0)`: kit di test, ogni colpo rimbalza, un manichino 120 cm davanti.
  5. Attacco (tasto sinistro / X): lo swing prende il manichino, finisce lo swing, poi rinculo Swing (`…B_A…`, 30 frame).
  6. Stick destro tutto a destra mentre attacchi (o gira di ~30° a destra): lo swing manca, l'affondo prende, rinculo Stab subito (`…B_V…`, 24 frame).
  7. `h.strike_kinds(0, False)`, ferma il Play, `h.restore()`. Niente da salvare.
- [ ] Prova sterzata (stick destro in avvio, max 20°), reticolo (click stick destro, solo PvE/TDM) e freccia ai piedi con un pad. Azioni create in automatico.
- [ ] Strike Kind per gruppo di hitbox (`Override Strike`, Swing/Stab); recoil per attacco sulla traccia Hitbox; `Stab Only` sui weakpoint interni.
- [ ] `L_MinionYard` con `setup_yard`; aggancio tinta P10; contenuto TM3; `L_Test_Combat` (facoltativo); pair-move in PIE; `gstn.Net.SimProxySendEveryNTicks 2`;
  binding del tasto di sparo; `gstn.Projectile.Fire` / `Ability 0 1`; ghost del proiettile con lag; harness rosso su `DefaultLevel` nel worktree m7 (mappa, non codice); Oodle (facoltativo).

## 3 · Decisioni

- [ ] Leve weakpoint §6 (scala hurtbox, priorità, magnetismo PvE).
- [ ] In attacco lo stick sinistro deve smettere di girare il personaggio? Oggi "litiga" con la sterzata.

## 4 · Animazioni

- [ ] Rinculo Swing e Stab (oggi segnaposto `…shove_stand_B_A…` / `…B_V…`). Blend della mano per il caso "faccia di taglio" (più avanti).

## 5 · Pulizia branch

- [ ] GitHub: branch vecchi elencati nell'artifact; dopo il push anche `claude/desktop-weakpoint-3`. Portatile: `git branch -d claude/laptop-projectile-1`.

## Fatto

- 2026-11-10 · Weakpoint slice 3 (sterzata predetta via Mover, reticolo PvE/TDM, freccia, azioni automatiche, mossa di test per i recoil; headless 1474/0, harness PASS). Branch `claude/desktop-weakpoint-3`.
- 2026-11-09 · Push di `92e9b029` su `feature/gas-npp-5.8` (fatto da te). Voci precedenti: vedi artifact.

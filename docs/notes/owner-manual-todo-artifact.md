# Cose da fare a mano (copia dell'artifact)

Copia in repo dell'artifact https://claude.ai/artifact/XjMiMf16f85qP5TvqPCaSk, aggiornata 2026-11-10. L'artifact è la versione di riferimento.

## 1 · Git

- [ ] Porta le weakpoint slice 3 e 3b su `feature/gas-npp-5.8` (tip `ceb9d1c2` di `claude/laptop-weakpoint-3b`, contiene `7487fa51`, sopra `92e9b029`), poi `git pull` e build completa da Rider:
  ```
  cd "C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2"
  git fetch origin claude/laptop-weakpoint-3b
  git push origin ceb9d1c2:refs/heads/feature/gas-npp-5.8
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
- [ ] Strike Kind per gruppo di hitbox (`Override Strike`, Swing/Stab); recoil per attacco sulla traccia Hitbox; `Stab Only` sui weakpoint interni.
- [ ] `L_MinionYard` con `setup_yard`; aggancio tinta P10; contenuto TM3; `L_Test_Combat` (facoltativo); pair-move in PIE; `gstn.Net.SimProxySendEveryNTicks 2`;
  binding del tasto di sparo; `gstn.Projectile.Fire` / `Ability 0 1`; ghost del proiettile con lag; harness rosso su `DefaultLevel` nel worktree m7 (mappa, non codice); Oodle (facoltativo).

## 3 · Decisioni

- [ ] Leve weakpoint §6 (scala hurtbox, priorità, magnetismo PvE).

## 4 · Animazioni

- [ ] Rinculo Swing e Stab (oggi segnaposto `…shove_stand_B_A…` / `…B_V…`). Blend della mano per il caso "faccia di taglio" (più avanti).

## 5 · Pulizia branch

- [ ] GitHub: branch vecchi elencati nell'artifact; dopo il push anche `claude/desktop-weakpoint-3` e `claude/laptop-weakpoint-3b`. Portatile: `git branch -d claude/laptop-projectile-1`.

## Fatto

- 2026-11-10 · Weakpoint slice 3b: stick destro gira il personaggio solo col reticolo acceso e allora vince sul sinistro (decisione presa); reticolo nell'input predetto, pronto per le mosse ranged mirabili (headless 1475/0). Branch `claude/laptop-weakpoint-3b` (`ceb9d1c2`).
- 2026-11-10 · Weakpoint slice 3 (sterzata predetta via Mover, reticolo PvE/TDM, freccia, azioni automatiche, mossa di test per i recoil; headless 1474/0, harness PASS). Branch `claude/desktop-weakpoint-3`.
- 2026-11-09 · Push di `92e9b029` su `feature/gas-npp-5.8` (fatto da te). Voci precedenti: vedi artifact.

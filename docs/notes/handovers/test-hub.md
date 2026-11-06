# Handover TH-1: livello di test `L_Test_Hub` (fisso, editor dell'owner, 2026-11-06)

- Worktree **m7**, branch `claude/project-thread-f6jper` da M7-15 (`8154056`): `feature` non ha ancora M7. Commit `32f8f7f` builder/helper/audit,
  `02fc39f` pedane, `2867b09` automazione. La `.umap` si committa dopo il Ctrl+S dell'owner. Push: owner. `Content/AnimPipeline` in m7 = giunzione.
- `Tools/GAS/build_test_hub_map.py`: riempie il livello aperto (mai salva la mappa), idempotente sul tag `TestHub`, 284 attori: Z0 piazza,
  tabellone, 7 pedane kit + pedana rossa debug, Z1 colpi (stazioni di L_Test_Combat + corsia 30 m), Z2 arena 20 m con KO, Z3 piggyback,
  Z4 spin/drill, Z5 traversal (2 corsie WallSlideAB, gradini, salti, rampe), Z6 jab v12 / ftilt v31 / dair v4 VarM in loop, segnaposto manichini.
- **Niente Python a mano** (richiesta owner): `Config/DefaultEngine.ini` StartupScripts → `Tools/GASNPPTest/hub_startup.py` →
  `test_hub.auto_start()`. Aperta L_Test_Hub: contenuti di test in memoria (prepare); a ogni Play in quella mappa: 10 manichini (`summon` sul
  server + AI controller), regole colpi (`hit_rules_setup`), link step, kit di base ai giocatori; pedane: cambio kit / toggle
  `gstn.Combat.Debug.Hitboxes`+`Hurtboxes`. Altre mappe: niente (contenuto ripristinato quando si cambia livello). Play 3 giocatori listen
  resta salvato nelle impostazioni PIE dell'editor.
- Scelte (regola 6): manichini spawnati a runtime perché un pawn Mover piazzato rende la mappa non salvabile (riferimento privato alle
  transizioni dei movement mode del CDO); pedane in Python = seme di D1.
- Verifica: Play listen 3 giocatori, `selftest()` **PASS** in tutte le zone (prima dei manichini a runtime); manichino piazzato colpito: danno 14,
  StaggerInPlace; pedane Beta/Link applicate a mano libera.
- **Crash** (18:16): cambio kit su un manichino nello stesso tick dello spawn → access violation in `FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>`
  (`FGSTN_LinkSnapshotContributor::Contribute` ← `ReadSnapshotEntry` ← `EnsureSnapshot` ← `SimulationTick`). Aggirato (i manichini tengono il
  kit di base). Bug C++ da guardare: il contributor non regge un pawn appena creato con loadout cambiato.

**Prossimo, serve il fisso (dopo le 04:00):**
1. Owner riapre l'editor su m7 e L_Test_Hub → `build_test_hub_map.run()` → Play: verificare che l'avvio automatico parta, che i manichini
   a runtime prendano colpi, selftest → Ctrl+S → commit della `.umap` e dei MI in `Content/GASNPPTest/TestMaps/Hub`. (Se non fatto in TH-1.)
2. TH-1b kit a terra (`claude/project-thread-0t6tfg`, C++): cherry-pick su m7, build a editor chiuso, sostituire le pedane Python con
   `AGSTN_KitPickup` (Sonnet, ~$2-4).
3. Fix del crash sopra (Opus medium, ~$3-5) prima di cambiare kit ai manichini.

**Non funziona / da guardare (brief TH-2):** manichini senza respawn dopo KO; orientamento delle scritte non controllato dall'owner; attacco da
montato senza binding `Role.Rider` (M7-06); posa rider (IK TODO); blocco Z4 solo segnaposto (`h.drill_immune`); mosse di Z6 in nessun kit;
`IC_GASNPP_Test.uasset` salvato su disco coi bottoni Shield/Spin (non committato: `git checkout` a editor chiuso); `Content/Levels/L_Test_Hub.umap`
è un redirector (non committato); server dedicato e suite nelle 3 configurazioni non provati.

Costo stimato: ~250k token di contesto, ~$12-15 (sopra la stima: manichini, pedane, automazione e crash su richiesta dell'owner).

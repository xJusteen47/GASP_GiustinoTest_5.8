# Handover TH-1: livello di test `L_Test_Hub` (fisso, editor dell'owner, 2026-11-06)

- Worktree **m7**, branch `claude/project-thread-f6jper` da M7-15 (`8154056`): `feature` non ha ancora M7 (passo 0 non fatto). Commit
  `32f8f7f` (builder, helper, audit), `02fc39f` (manichini, pedane, setup automatico), poi la `.umap` dopo il Ctrl+S dell'owner. Push: owner.
- `Content/AnimPipeline` nel worktree m7 è una giunzione verso il checkout principale (non versionata), come in `_feature`.
- `Tools/GAS/build_test_hub_map.py`: riempie il livello aperto (mai salva la mappa; rifiuta un livello che non sia nuovo o L_Test_Hub),
  idempotente sul tag `TestHub`, 282 attori: Z0 piazza + tabellone + 7 pedane kit, Z1 colpi (stazioni di L_Test_Combat + corsia 30 m),
  Z2 arena 20 m con KO, Z3 piggyback (sporgenza 4 m, righelli), Z4 spin/drill, Z5 traversal (2 corsie WallSlideAB, gradini, salti, rampe),
  Z6 jab v12 / ftilt v31 / dair v4 VarM in loop. Salva solo i MI colore in `/Game/GASNPPTest/TestMaps/Hub`.
- `Tools/GASNPPTest/test_hub.py`: `prepare()` (una volta per sessione, prima di Play) + watcher che a ogni Play mette le regole dei colpi
  (`hit_rules_setup`), i link step e il kit di base su giocatori e manichini, e applica il kit della pedana su cui sale un giocatore.
  Comandi: `kit park walker mount mounted spin drill_immune fighter selftest`.
- Scelta (regola 6): manichini = `BP_GASNPP_TestCharacter` piazzati (AI del server), nessun C++; pedane = Python lato server (stesso percorso
  del cambio loadout C15), seme di D1. I kit a terra di TH-1b (`claude/project-thread-0t6tfg`, C++ nuovo) NON sono uniti: chiedono una build
  a editor chiuso. Prossimo: unirli e sostituire le pedane Python con `AGSTN_KitPickup` (Sonnet, fisso, ~$2-4).
- Verifica: Play listen 3 giocatori, `selftest()` **PASS** in tutte le zone (posizioni, attacco, scudo, mount formato sul server, spin visto
  sul server, test fighter, mesh in loop); manichino colpito: danno 14, StaggerInPlace; pedana provata: Beta e Link applicati.
- Prima prova fallita (mount sposta il carrier prima del controllo posizione in Z3): corretto l'ordine, seconda prova verde.

**Non funziona / da guardare (brief TH-2):**
- I manichini non si rialzano né ritornano dopo un KO (nessun respawn per pawn AI).
- Il tabellone e le scritte sono TextRender: orientamento non controllato a occhio dall'owner.
- Attacco da montato: nessun kit ha il binding `Role.Rider` (M7-06). Posa del rider: nodi IK AnimBP ancora TODO.
- Il blocco di Z4 è solo un segnaposto: "non trapanabile" esiste solo come effetto su un giocatore (`h.drill_immune`).
- Le mosse di Z6 non sono in nessun kit: niente versione dal vivo su un bersaglio.
- `LevelEditorPlaySettings` non è esposto a Python: il Play a 3 giocatori va scelto a mano (o da Rider `ue_play`).
- Server dedicato e suite DEFAULT/PARTS/DEFENCE nelle 3 configurazioni: non provati qui.

Costo stimato: ~180k token di contesto, ~$8-10 (sopra la stima $5-8 per il giro manichini/pedane chiesto dall'owner).

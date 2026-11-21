# Stato reale delle milestone (audit cloud, sola lettura, 2026-11-21)

Fonti: `origin/feature/gas-npp-5.8` (e79e401a, 2026-11-16), linea di gioco attuale `claude/runstart-generic-desktop`
(b2c7d5f5, contiene feature + 607 commit, quasi tutti animazione/loco), handover in `docs/notes/handovers/`,
`docs/notes/takeover-state.md`, `docs/architecture/plans/`, cartella `Plugins/`. La roadmap HTML del progetto
(`milestones/milestones.html`) è ferma al 23/10 ed è superata da questa nota.

## Risposta breve: M10 e M11 sono fatte
- **M10 oggetti tenuti e lanciati (GSTNHeld)**: slice 1-10 fatte (handover `m10-slice1..10.md`). Slice 10 (2026-11-11):
  "M10 is complete except the G12/G13 items listed as not done"; suite 50/50, headless 1498/0, harness held 8/8.
  Resta solo rifinitura: attacco alla mano + blend di rilascio (spike S2), anteprima dell'arco (modulo editor), prompt di
  interazione, validatore HoldFrames, video G12, wind-up mai visto in PIE (spedito a 0 = lancio istantaneo).
- **M11 kit in gioco (GSTNPickups, GSTNKitMods, GSTNBackpack, GSTNLoadoutMenu)**: slice 1-20 + verify + swap + merge su 5.9
  (`m11-slice*.md`, `m11-verify-1.md`, `m11-merge-ue59.md`, `m11-loadout-ue59.md`, `pad-menu-1.md`). Raccolta/scambio kit,
  mod, zaini (JetPack, ShieldDome nell'hub), schermata del carico col pad (sistemata in `pad-menu-1`). Restano: zaino vero
  che segue la schiena (serve una decisione sul componente look, `backpack-back.md`), righe arma di supporto/super nella
  schermata, nomi leggibili di mod/zaini, avvio "tutti pronti" (va con M12).
- Entrambe sono nella linea che l'owner usa per il playtest (plugin presenti su `claude/runstart-generic-desktop`).

## Tabella
| Milestone | Stato | Prova | Cosa resta |
|---|---|---|---|
| M0-M2 | fatte | roadmap 23/10 | - |
| M3 abilità, kit, salto | fatta | C0-C19 (HANDOVER 14); `m3-suite.md` suite 50/50 | flake noti `hit_on_wall_hit`, `jump_squat` (non in lista flaky) |
| M4 colpi | fatta | HANDOVER 33 "M4 H13b done, M4 closed" | - |
| M4b parti distruttibili | quasi fatta | P0-P9, P7b, P13-P19 (`parts-rest.md`) | P10-P12 presentazione (P11/P12 = fase estetica), P17 stazioni; debito perf parti minion 1,88 us vs 1,8 |
| M5 editor timeline/debug | quasi fatta | E1-E17, E19 hook (`m5-rest.md`) | E18 Rewind Debugger (deciso "mai", D7); categoria debugger e BakeCheck mai visti a schermo; anteprima/forme nell'editor |
| M6 difesa e link | fatta | HANDOVER 61 "M6 closed", F15 follow-up | SDI/scudo PvP da giudicare in M14 |
| M7 piggyback | fatta | M7-01..M7-15 (`m7-piggyback.md`) | FP (predizione partner): G0-G2 fatte, G3-G5 in pausa fino a un playtest PvP |
| M8 minion, 40+ nemici | fatta (codice) | G0-G12b (`m8-minion.md`), leve netcode 3-4 | "Launched hover" non risolto; contenuto TM3 mai fatto |
| M9 adattatore GAS | saltata | opzionale, consigliato di non farla | - |
| M10 oggetti tenuti | fatta | vedi sopra | rifinitura G12 |
| M11 kit in gioco | fatta | vedi sopra | zaino sulla schiena, righe extra |
| M12 modalità + livelli | **non iniziata** | nessun GSTNMission/GSTNRespawn/GSTNModeVerbs su nessun branch | tutto G0-G8; **D5 (Game Features + Experiences, tocca .uproject) aspetta il sì dell'owner** |
| M13 spedizione | non iniziata | idem | tutto; dopo M12, su via dell'owner |
| M14 versus | non iniziata | idem (codice camera duello esiste in GSTNCamera, test Core) | tutto; dopo M12 |
| T superfici traversal | parziale | T1, T2, T3, T5, T6, T9 test (`traversal-rest.md`) | T4 lancio da superfici mobili, T7 ciclo vita + lag comp, T8 piattaforme minion |
| CAM camera | parziale | P1a in gioco ON di default (memoria 11-21), tilt-shift; plugin GSTNCamera ancora sperimentale, su nessun pawn | camera su pawn vero = M12 G5 |
| INT interazione | parziale | GSTNInteract sperimentale, usato da held/pickup ("Tieni B") | focus unico + prompt vero = M12 G5 |
| DM marker profondità | parziale | plugin GSTNDepthMarker sperimentale (0.1), pseudocodice in project files | non acceso nel gioco |
| WS2 wall slide v2 | parziale | brief SLIDE (2026-11-08) in attesa del sì; momentum slide a terra scritto in cloud non compilato (`momentum-slide.md`) | decidere |
| TM mappe di test | parziale | L_Test_Hub (TH-1/TH-2) è la mappa di avvio | TM3 contenuto nemici |
| Fuori roadmap, fatti | - | combat plugin esposto ai BP (CP0-CP6), pushbox P1-P5, proiettili, weakpoint, spin/drill, react hook | - |

## Proposta: lavoro in parallelo in cloud (max 2 thread, fisso e portatile occupati)
Limite: in cloud non c'è Unreal, quindi il codice si scrive e si testa in logica Core ma si compila dopo su una macchina
(~30-45 min di build + test headless quando una si libera). Per questo conviene scegliere lavoro con molta logica pura.

1. **Cloud A, M12 parte 1 (Opus high, 8-12 $)**: aggiorna `m12-modi-livelli.md` al repo di oggi (M10/M11 fatte, drop del kit
   alla morte, ServerRespawn, D-1/D-2), poi scrive `GSTNRespawn` Core (regole vite/checkpoint/drop kit, KO per vita e per zona)
   e `GSTNMission` Core (obiettivi, catene, test "10 kill con rollback = 10"). Se D5 = sì, prepara anche G1 (Experiences).
   Poi build + test sul portatile quando libero (~6-10 $ di verifica).
2. **Cloud B, pulizia residui (Sonnet medium, 4-6 $)**: M10 prompt interazione + validatore HoldFrames; M11 righe arma di
   supporto/super e nomi leggibili; i due flake noti nella lista flaky; driver headless che caricano DefaultLevel
   (`m11-merge-ue59.md`). Compilazione insieme al thread A per una sola build.
3. **Dopo A**: M14 G0-G1 in cloud (Opus medium, ~6-10 $), poi M12 G5-G7 sul fisso (camera e interazione su pawn vero, livello
   d'esempio) quando l'animazione libera l'editor. M13 resta ultima, su via dell'owner.

Decisione che serve all'owner: **D5** (A = sì Game Features + Experiences come da piano, consigliata; B = no, modalità
con GameMode classico, meno modulare). Senza risposta il thread A parte comunque dalla parte Core, che non dipende da D5.

COSTO: ~2 $ | 0 minuti desktop | 0 token review

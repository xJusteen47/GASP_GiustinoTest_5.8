# review-invarianti-1 (cloud, 2026-11-24, Opus medium, budget 6 $): INVARIANTI, test, pacchetto livello 2, ereditati bloccanti
Branch claude/project-thread-y073op da wwnerm 891c65de (btrk7b + vjbryc). Solo testo/script. Riuso: review_pack (griglie, slowmo tile, allineamento), motion_overlay (schede velocita'), no_stop_gate; nessuna copia.
## Fatto
1. `Tools/AnimPipeline/contract/invarianti-review.md`: fonte di INV-01..INV-12 col perche'; sezione `## INVARIANTI` in testa a
   reviewer project.md, executor project.md, cheap SKILL.md (+ rimando nei due SKILL.md). Cheap: tolti "max 8 immagini" e
   "pacchetto compatto" dal giudizio (livello 2 identico alle skill originali).
2. `tests/test_review_invariants.py` (22 test): sezione e id in ogni skill e nel contratto, INVARIANTI prima sezione, nessun
   tetto immagini sul livello 2 (regex, ammesso solo se la riga dice livello 1), pacchetto incompleto se manca un passo
   (identita', video, vista, frame scoperto, clip, pose chiave, base, motivo tenuta), ereditati bloccanti, motivo tenute.
3. Ereditati di nuovo BLOCCANTI (revert btrk7b p.3): `capture_move.inherited_blocks` ora solo informa; sblocca solo una riga
   dell'owner (parole tra virgolette, riga esatta) in anim-approvate.md `## Esenzioni dei controlli` (sezione aggiunta, vuota).
   stop_and_go e hold_reason mai esentabili. Testo skill aggiornato (reviewer, executor p.3).
4. Tenute (owner 24/11): `animpipe check` blocco `hold_reason` (posa "hold" senza hold_reason, holds senza hold_reasons);
   cancello `no_stop` rifiuta holds senza `no_stop.hold_reasons`; il pacchetto le mette in "da giudicare a video". Nessuna
   carta esistente colpita (scansione di cards/).
5. `gstn_anim_mcp/final_review_pack.py` + tool MCP `anim_review_pack` (12o, in coda): mp4 reale/slow4x, slowmo di TUTTA la
   finestra 1 tick per cella in az90/az180/game a riquadri da 32 (base sopra), copertura frame, schede velocita' di OGNI clip,
   pose chiave 512 px, critico_brief.md, `review_pack.json` `complete`, `check_pack()`; `anim_capture_move` lo costruisce sempre (`final_pack`, `review_ready`; nuovi argomenti `identity`, `key_frames`). INV-11 chiama
   `python -m animpipe life <joints> --json` se esiste (thread "Restore OG life metrics", 1uvcac, accordato).
6. Proposta regole 4 e 10: `docs/notes/review/proposta-regole-4-10-2026-11-24.md` (la incolla l'owner).
PROVA: `cd Tools/AnimPipeline; python -m pytest -q tests/ --ignore=tests/test_casc_roundtrip.py`: 190 pass / 11 fail; i 11 falliscono
identici senza le mie modifiche (git stash: 168/11; file locali mancanti, pwsh).
## Aperto
- K7: legibility, contact_faf, hitstop_snap 14->15 ora bloccano K8. Sul PC: decidere per ciascuno se la misura sbaglia
  (correggere + ritarare su K7 con log) o se K7 ha il difetto (correggere in K8 o esenzione dell'owner). Non diagnosticabile
  in cloud: manca Saved/AnimPipeline/refs/ufd/OverlordDropkick_timing.csv.
- Pacchetto mai provato su una cattura vera; prima cattura: passare identity e key_frames, elencare tutte le clip.
- Copie skill in project files anim-skills/ e account non aggiornate; voce INDICE-PROGETTO al merge.
SUBAGENTI: 0
COSTO: ~4,5 $ (stima) | 0 minuti desktop | 0 token review

# T2 anim_layer (MCP pose layers for flash agents) - 2026-11-24, cloud, Opus high, branch claude/project-thread-bt9fds from 7zhryd
OBIETTIVO: l'agente scrive l'intento ("gomito destro 10 gradi piu' fuori a f13, morbido su f1-f21"), lo strumento fa
quaternioni, curve e vincoli; ogni passo annullabile; mai chiavi step. Brief: handoffs/skill-anim-unica-brief-2026-11-24.md (T2).
RIUSO: helper quaternioni di follow_through.py; _snapshot_asset, _clip_frame_count, _repo_root e manifest di anim_ops;
schema Run-SparseAnimStep.ps1 + script _ue di follow_through_ue.py; inviluppo = la curva dei passi w3 (ss(a,b)*(1-ss(c,d))).
FATTO (Tools/AnimPipeline/gstn_anim_mcp/):
- anim_layer.py (puro): strato = parte + movimento (out/in, forward/back, turn_*, tilt_*, chin_up/down, flex/extend; parole
  italiane accettate) + gradi + start/peak/end. Assi dal CORPO a ogni frame (anche, o spalle con frame=chest; clip = assi
  asset), non dal moto del bacino. Rotazione del solo osso attorno al suo giunto: figli seguono, lunghezze invariate.
  start/end e fuori finestra identici al bit (ancore). Rampa < 2 frame rifiutata (= chiave step) salvo bordi clip.
  Segno deciso una volta al picco: il codice w3 lo sceglieva per frame e poteva invertirsi a meta' strato (test lo mostra).
  Gambe/bacino/root rifiutati (piedi scivolerebbero: si usa anim_run_intent con IK). gradi/frame > 6 = MISURA, non blocco (2A).
- anim_layer_ops.py: pila per variante in Tools/AnimPipeline/cards/layers/<dst>.json (sorgente versionabile). Azioni
  add (dry_run), undo (ultimo o to=id), list, dump (UE, una volta per clip: poi i cm si misurano offline), build (UE: dst = src
  + pila, conservazione sui dati, PASS/FAIL). Rifiuta dst = src e dst in docs/notes/anim-approvate.md.
- anim_layer_ue.py: dump e build; verifica FK della tabella PARENTS contro la posa del motore (errore se > 0,5 cm).
- mcp_server.py: tool anim_layer in coda (prefisso fisso per la cache); 3 test che fissavano "ultimi 4 tool" aggiornati.
PROVE: `python -m pytest -q tests/test_anim_layer.py` = 15 passed. Equivalenza col codice w3 di c1 passo-02 (22a318f1),
c1 passo-03 (d5cfea6a), c2 passo-03 (f3978762) su manichino sintetico: differenza max 3e-14 cm/gradi; testa 6,5 gradi a f13
(= esito.json head_up_f13_deg). Suite intera: 11 rossi preesistenti d'ambiente, identici prima e dopo (diff liste FAILED vuota).
PROVA FLASH (Haiku al posto di DeepSeek flash: nessuna chiave DeepSeek in cloud), solo descrizione tool + 3 istruzione.json:
docs/notes/anim-layer/flash-dryrun-w3-2026-11-24.json. 8 strati, tutti validi, conservazione OK, stessi giunti nella stessa
direzione del w3 (test TestFlashDryRun). Ampiezze DIVERSE (gomito_r 3,6 cm fuori contro 6,6 del w3): le istruzioni non
danno numeri ("leggermente"). Per avere gli stessi numeri l'istruzione deve dare il bersaglio (verifica.misura in cm o
gradi) e l'agente cicla add dry_run sul dump finche' moved_joint_at_peak lo raggiunge. Haiku non ha toccato f0 (pre-piega):
la descrizione ora dice che start=0, peak=[0,k] cambia f0.
NON FATTO / PER T3 (fisso): mai girato in UE. 1) anim_layer dump su AS_GSTN_RunStart_Generic_w3 (verifica FK, nomi
neck_02/middle_metacarpal); 2) prova numerica vera: rigenerare w3 con W3_ARM_R_OUT=0 (runstart_w3_ue.py) come src, pila
STEP_C1_02 di tests/test_anim_layer.py, build, confronto con w3 (atteso ~0); 3) poi la PROVA dropkick K6 a strati.
Rischi: add_bone_curve per ossa senza traccia non provato; get_animation_track_names da verificare in 5.9.
INDICE-PROGETTO.md non e' su questo branch (claude/indice-progetto): voce da aggiungere "anim_layer: strati di posa MCP".
SUBAGENTI: 1 (prova a secco flash, Haiku)
COSTO: ~4,5 $ (stima) | 0 minuti desktop | ~64k token subagente

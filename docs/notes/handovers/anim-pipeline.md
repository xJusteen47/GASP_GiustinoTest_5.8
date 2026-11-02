# Handover anim pipeline: checkpoint 2, motore unico (2026-11-02, branch claude/project-thread-0pzf0o)

Esito: **il motore gira end-to-end su jab v11** (scheda -> Control Rig -> bake 60 fps -> controlli 3 classi -> trails), un
giro completo < 1 min (build 10 s, check 35 s, trails 2 s). Checkpoint 1 (spike, GO a 60 fps) è nella storia git di questo file.
Comandi (`Tools/AnimPipeline`, `python -m animpipe ...`): `build <card>`, `check <card>`, `trails <card>`; risultati in
`Saved/AnimPipeline/animpipe/<move>_{build,check}.json` + `<move>_check.txt` (una riga per controllo), immagine in `sheets/`.

- `build` (`ue_rig.build`): bake della `source_anim` sul `CR_Mannequin_Body`, tiene solo i frame delle pose, tangenti per
  profilo di spaziatura (`charge_long`/`arrive_1_2`/`hold_recoil` = piatte pesate; `limb_led_recovery` = smart auto),
  ritardi per parte (sposta le chiavi interne dei controlli), piedi IK `planted`: deriva <= 1 cm / 2° riportata alla chiave 0.
  Jab v11: 36822 -> 9690 chiavi; alle pose 0,0 cm sulle mani, testa 2,75 (ritardo 1 frame dichiarato), piedi 0,96;
  fuori dalle pose max 4,7 cm (mano), contro i 7,35 della sola riduzione del checkpoint 1.
- `check`: blocca bone_lengths, clearance, legibility, contatto/FAF, piedi piantati, pugni chiusi; misura motion_curves(+ref),
  motion_check, spaziatura (progresso a metà segmento), distanza dalla sorgente; occhio: 3 voci per la griglia. Gli script
  vecchi girano invariati come sottoprocessi. `move_sheets.json[move]` è generato dalla scheda a ogni check.
- Jab v11 build: FAIL su legibility (braccio dietro copre il pugno, 1 frame) e bone_lengths (4 ossa, max clavicola_r 1,29 cm).
  Sorgente densa con gli stessi controlli: legibility PASS, bone_lengths FAIL già lì (8 ossa, max 0,98). Nessun auto-fix:
  v11 è una scheda retrofittata (pose = confini delle finestre), il test vero è v12 / v3.1 con le pose Step.
- Trails: fatti offline dalle ossa campionate (matplotlib, 3 viste, build su sorgente in grigio): niente editor, 1 immagine.

Decisioni prese senza owner (consigliate): resto a 60 fps; accetto 1 cm su colonna/ginocchio nei round trip delle mosse vecchie;
calibrazione delle tangenti in editor tolta (evaluate_keys inaffidabile), la spaziatura si misura sul bake.
Gotcha: da Git Bash serve `MSYS_NO_PATHCONV=1` (altrimenti `/Game/...` diventa un percorso Windows; `check` lo imposta);
`add_key` sui canali del rig perde i valori: si modificano le chiavi esistenti (remove/set_value/set_time); `twist_chain`
è n/a sui build (legge i punti Cascadeur `<take>_targets_raw.json`); `refs/` nel worktree è una junction verso `Saved/AnimPipeline/refs`.

Prossimo thread (fase B): import delle pose Step del proprietario (`poses[].source.file`) sui controlli al posto di
`source_anim`; equilibrio come regolazione del bacino alle chiavi; twist_chain sulle ossa; regressione v12 e v3.1.

Domanda per il proprietario: bone_lengths a 0,1 cm boccia anche v11 denso (stretch del rig/bake). Consiglio: sui build da rig
spegnere gli stretch switch del rig e tenere 0,1 cm; in alternativa "non peggio della sorgente". Applico il primo nella fase B.
Editor: aperto da riga di comando, chiuso col kill (asset salvati esplicitamente, nessun salvataggio di mappa o Blueprint).
Costo stimato: Opus ~60 chiamate, ~115k contesto finale, circa $6-8; nessun subagente.

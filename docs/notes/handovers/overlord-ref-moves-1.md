# Handover overlord-ref-moves-1: mosse di Ganon e Ike sull'Overlord (2026-11-25, verificato 2026-11-26)

Branch `claude/project-thread-sznhpw` (base `main` `152d25e6`, unito con `origin/claude/project-thread-k3aapg` `d6122fe7` al merge commit `2ca1a807`).
Dettagli, numeri e fonti: `docs/notes/overlord-ref-moves.md`.

## Cosa
- `Tools/GAS/overlord_ref_moves_data.py` (Python puro, test `Tools/GAS/tests/test_overlord_ref_moves_data.py`, 4 OK): frame data e colpi di
  Ultimate (UFD + SmashWiki), conversioni del progetto, percorsi delle hitbox centrati sull'asse di sguardo (richiesta owner 10:22Z).
- `Tools/GAS/setup_overlord_ref_moves.py`: clip capsula (in place), montage, timeline (`setup_guard`), GA figli di `UGSTN_GA_TestKitStrike`
  sotto `Characters/Overlord/Moves/Ref`; binding su `KIT_Test_Overlord` (F.S = Strong.Directional a terra, N.S = Strong.Neutral a terra,
  N.L Aerial = Attack.Neutral in aria, N.S Aerial = Strong.Neutral in aria); MoveOverrides dell'ABP capsula. Il kit Playtest NON è toccato.
- Bersagli: merge di `claude/project-thread-k3aapg` (`gstn.TestCubes.Spawn`); i colpi portano `GSTN_PartHitData(Penetration=...)`.
- `greybox/make_capsule_clips.py`: importabile senza eseguire; i montage che suonano già una clip capsula la tengono come override.
- `gstn_content.py`: regole cartella per `AM/AS_OverlordRef_*` e `GA/TL_Overlord_Ref*`.

## Verifica sul fisso (2026-11-26, lock `_ue59` acquisiti e rilasciati a `LIBERO`)
- **Fix regressione `914e5d03` (`3c263e0f`)**: ripristinati `Tools/GAS/build_test_hub_map.py` e `Tools/GASNPPTest/test_hub.py` ai path `/Game/GSTN/...` di `origin/main` (`152d25e6`).
- **Fix 60 fps in `Tools/GAS/setup_overlord_ref_moves.py` (`f9bd4c88`)**: il primo run di `Save-Assets.ps1` (`34s`, `exit=3`) ha fallito su `AS_GSTN_Greybox_Ref_NLAirGanon` (55 frame a 60 fps ricampionati al default 30 fps con residuo `0.5` in `AnimSequence.cpp:4970` / `AnimCompressionTypes.cpp:993`). Impostando temporaneamente `AnimationSettings.default_frame_rate` a `60/1` attorno a `create_asset`, il secondo run ha creato/salvato tutti i 16 asset, `KIT_Test_Overlord`, `ABP_GSTN_GreyboxCapsule` e `Tools/GAS/setup_guard_ledger.json` (`e4d1986d`):
  - `.\Tools\CI\Save-Assets.ps1 -Script Tools\GAS\setup_overlord_ref_moves.py -Tag overlord_ref_moves` -> `Saved\Logs\ci\save_overlord_ref_moves.log.esito`: `ESITO: OK` (`14s`, `precheck: OK`).
  - `Saved\OverlordRefMoves\setup_overlord_ref_moves.txt`: 0 `ERROR`, `saved 16 move assets, kits ['KIT_Test_Overlord'], capsule ABP`; `F.S` (`Hitbox: 2, Landing: 0`, Ground), `N.S` (`Hitbox: 1, Landing: 0`, Ground), `N.L Aerial` (`Hitbox: 4, Landing: 3`, Air), `N.S Aerial` (`Hitbox: 2, Landing: 3`, Air); i 4 nuovi binding risultano in testa a `KIT_Test_Overlord` (prima dei binding arma `WPN_Test_Overlord`, nessun strong precedente sovrascritto).
  - Verifica serializzazione `GSTN_PartHitData(Penetration=6/4/2/4)` in `TL_Overlord_Ref*.export_text()`: tutte e 4 `True` (`pie_overlord_ref.log.ue.log`).
- **Build & Test (`GASP_Giustino2_testcubes` @ `2ca1a807`)**:
  - `python -m pytest Tools/GAS/tests/test_overlord_ref_moves_data.py Tools/GAS/tests/test_setup_guard.py -v` -> `19 passed in 0.33s` (4/4 `test_overlord_ref_moves_data.py` + 15/15 `test_setup_guard.py`).
  - `.\Tools\CI\Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\build_sznhpw.log` -> `ESITO: OK` (`2s`, `REGRESS: OK 8 locked versions, 13 files unchanged`).
  - `.\Tools\CI\Run-Timed.ps1 -Preset Test -Log Saved\Logs\ci\test_overlord_ref.log -Tests GSTNParts.Minion` -> `ESITO: OK` (`36s`); `Check-TestLog.ps1 -MinTests 11` -> `ESITO TEST: PASS  ok=11 fail=0 totale=11 (minimo 11)`.
- **PIE su `/Game/GSTN/Maps/L_Test_Hub` (`Saved\Logs\ci\pie_overlord_ref.log`)**: `ESITO: OK` (`81s`, `precheck: OK`), pedana `OVERLORD JAB` verificata a `(800, 1050)` e `KIT_Test_Overlord` equipaggiato (`OK`), `gstn.Combat.Debug.Hitboxes 1` e `gstn.Greybox 1` attivi, 14 screenshot ispezionati (`Shot showui` con capsula greybox + hitbox rosse visibili) e `gstn.TestCubes.Report` verificato su `0,1,3,6,10,15` e `0,2,4,6,10,15`:
  - `F.S` (`GA_Overlord_RefFSGanon_C_0`, tl 9, pen 6): H3 `band=0 100% 24.00`, H6 `band=1 65% 15.60`, H10 `band=2 ricochet=1 0.00`.
  - `N.S` (`GA_Overlord_RefNSIke_C_0`, tl 12, pen 4): H1/H2/H3 `band=0 100% 13.50`, H4 `band=1 65% 8.77`, H6/H10 `band=2 ricochet=1 0.00`.
  - `N.L Aerial` (`GA_Overlord_RefNLAirGanon_C_0`, tl 10, pen 2): H1 `band=0 100% 7.00`, H2 `band=1 65% 4.55`, H3 `band=2 ricochet=1 0.00`.
  - `N.S Aerial` (`GA_Overlord_RefNSAirIke_C_0`, tl 11, pen 4): H3 `band=0 100% 7.50`, H4 `band=1 65% 4.88`, H6 `band=2 ricochet=1 0.00`.
  - Log: `Saved\Logs\ci\pie_overlord_ref.log.ue.log`; screenshot e report temporanei in `C:\Users\jacop\.gemini\antigravity\scratch\teamwork_tmp\f03fd16a-8899-40b2-bccb-a86199e3b8e0\shots\`.

## Da confermare con l'owner (note reversibili)
- Penetrazioni proposte e provate in gioco: `F.S 6`, `N.S 4`, `N.L Aerial 2`, `N.S Aerial 4`. Scala `1 unità Smash = 9.5 cm`.
- `fsmash` senza carica (hold al frame 14 non incluso); `ftilt` solo variante dritta/avanti.
- Su `KIT_Test_Overlord` i 4 binding reference sono messi in testa al kit (un eventuale strong a terra del kit verrebbe sostituito, backup automatico salvato in `Saved/Backups/GAS/20261127-001858/`).

BRANCH: claude/project-thread-sznhpw
ESITO: PASS asset salvati (14s), merge k3aapg, build OK (2s), GSTNParts.Minion 11/11 + pytest 19/19 verdi, PIE L_Test_Hub con 4 mosse + TestCubes + Hitboxes verificato (81s)
SUBAGENTI: 2 cloud (2026-11-25, frame data) + 2 explorer + 1 worker (M4, 2026-11-26)
COSTO: ~6 $ | 25 min desktop | 0

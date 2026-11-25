# Handover overlord-ref-moves-1: mosse di Ganon e Ike sull'Overlord, bersagli a durezza diversa (2026-11-25)

Branch `claude/project-thread-sznhpw` da `main` 152d25e6. Thread cloud, macchine spente: NIENTE eseguito nell'editor.
Dettagli, numeri e fonti: `docs/notes/overlord-ref-moves.md`.

## Cosa
- `Tools/GAS/overlord_ref_moves_data.py` (Python puro, test `Tools/GAS/tests/test_overlord_ref_moves_data.py`, 4 OK): frame data e colpi di
  Ultimate (UFD + SmashWiki), conversioni del progetto, percorsi delle hitbox centrati sull'asse di sguardo (richiesta owner 10:22Z).
- `Tools/GAS/setup_overlord_ref_moves.py`: clip capsula (in place), montage, timeline (`setup_guard`), GA figli di `UGSTN_GA_TestKitStrike`
  sotto `Characters/Overlord/Moves/Ref`; binding su `KIT_Test_Playtest` (F.S = Strong.Directional a terra, N.S = Strong.Neutral a terra,
  N.L Aerial = Attack.Neutral in aria, N.S Aerial = Strong.Neutral in aria); `PS_Test_TargetH{0,2,4,6,8,12}`; MoveOverrides dell'ABP capsula.
- `build_test_hub_map.py`: cubi e cartelli `penetration_targets`. `test_hub.py`: `spawn_targets` (manichini dentro i cubi, parti attaccate
  su ogni mondo, regole con PartResolve per la sessione, rimessi a posto se spinti, scritta RIMBALZO). `dummies()` esclude i bersagli.
- `greybox/make_capsule_clips.py`: importabile senza eseguire; i montage che suonano già una clip capsula la tengono come override.
- `gstn_content.py`: regole cartella per `AM/AS_OverlordRef_*` e `GA/TL_Overlord_Ref*`.

## Sul fisso (un thread, lock `_ue59`)
1. Editor chiuso: `.\Tools\CI\Save-Assets.ps1 -Script Tools\GAS\setup_overlord_ref_moves.py` -> `ESITO: OK`; leggere
   `Saved/OverlordRefMoves/setup_overlord_ref_moves.txt` (binding prima/dopo, warning su comandi fuori dal command set).
2. Rigenerare L_Test_Hub con `build_test_hub_map.py` (mappa binaria condivisa: avvisare l'owner).
3. Play: provare le 4 mosse sui bersagli; `gstn.Combat.Debug.Hitboxes 1` per vedere gli archi. Max 2 tentativi di fix per errore.
Non verificato: nomi Python di enum/proprietà (`SAKURAI3D`, `extension_data`, `remove_all_bone_tracks`), testo dell'InstancedStruct
`GSTN_PartHitData`, che `UGSTN_GA_TestKitStrike` parta in aria con ground rule Air, la resa delle pose capsula.

## Da confermare con l'owner
- Penetrazioni proposte: F.S 6, N.S 4, N.L Aerial 2, N.S Aerial 4. Scala 1 unità Smash = 9.5 cm.
- fsmash senza carica; ftilt solo avanti; N.S sostituisce sul kit Playtest lo strong neutro che c'era (tornare indietro: backup del kit).
- Il kit Playtest è toccato anche dal thread delle prese di Ganon: eseguire gli script uno dopo l'altro, mai insieme.

ESITO: PARZIALE codice e dati scritti, test Python OK, non eseguito in Unreal (macchine spente)
SUBAGENTI: 2, raccolta frame data da UFD e SmashWiki, stesso modello
COSTO: ~6 $ | 0 min desktop | 0

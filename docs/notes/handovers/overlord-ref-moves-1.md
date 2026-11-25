# Handover overlord-ref-moves-1: mosse di Ganon e Ike sull'Overlord (2026-11-25)

Branch `claude/project-thread-sznhpw` da `main` 152d25e6. Thread cloud, macchine spente: NIENTE eseguito nell'editor.
Dettagli, numeri e fonti: `docs/notes/overlord-ref-moves.md`.

## Cosa
- `Tools/GAS/overlord_ref_moves_data.py` (Python puro, test `Tools/GAS/tests/test_overlord_ref_moves_data.py`, 4 OK): frame data e colpi di
  Ultimate (UFD + SmashWiki), conversioni del progetto, percorsi delle hitbox centrati sull'asse di sguardo (richiesta owner 10:22Z).
- `Tools/GAS/setup_overlord_ref_moves.py`: clip capsula (in place), montage, timeline (`setup_guard`), GA figli di `UGSTN_GA_TestKitStrike`
  sotto `Characters/Overlord/Moves/Ref`; binding su `KIT_Test_Overlord` (F.S = Strong.Directional a terra, N.S = Strong.Neutral a terra,
  N.L Aerial = Attack.Neutral in aria, N.S Aerial = Strong.Neutral in aria); MoveOverrides dell'ABP capsula. Il kit Playtest NON è toccato.
- Bersagli: niente qui (coordinatore 10:26Z): li fa `claude/project-thread-k3aapg` (`gstn.TestCubes.Spawn`); i colpi portano la penetrazione.
- `greybox/make_capsule_clips.py`: importabile senza eseguire; i montage che suonano già una clip capsula la tengono come override.
- `gstn_content.py`: regole cartella per `AM/AS_OverlordRef_*` e `GA/TL_Overlord_Ref*`.

## Sul fisso (un thread, lock `_ue59`)
1. Editor chiuso: `.\Tools\CI\Save-Assets.ps1 -Script Tools\GAS\setup_overlord_ref_moves.py` -> `ESITO: OK`; leggere
   `Saved/OverlordRefMoves/setup_overlord_ref_moves.txt` (binding prima/dopo, warning su comandi fuori dal command set).
2. Play: kit dalla pedana OVERLORD JAB, `gstn.TestCubes.Spawn`, provare le 4 mosse sui cubi; `gstn.Combat.Debug.Hitboxes 1` per vedere gli archi. Max 2 tentativi di fix per errore.
Non verificato: nomi Python di enum/proprietà (`SAKURAI3D`, `extension_data`, `remove_all_bone_tracks`), testo dell'InstancedStruct
`GSTN_PartHitData`, che `UGSTN_GA_TestKitStrike` parta in aria con ground rule Air, la resa delle pose capsula.

## Da confermare con l'owner
- Penetrazioni proposte: F.S 6, N.S 4, N.L Aerial 2, N.S Aerial 4. Scala 1 unità Smash = 9.5 cm.
- fsmash senza carica; ftilt solo avanti. Su KIT_Test_Overlord un eventuale strong a terra del kit viene sostituito (backup del kit).

ESITO: PARZIALE codice e dati scritti, test Python OK, non eseguito in Unreal (macchine spente)
SUBAGENTI: 2, raccolta frame data da UFD e SmashWiki, stesso modello
COSTO: ~6 $ | 0 min desktop | 0

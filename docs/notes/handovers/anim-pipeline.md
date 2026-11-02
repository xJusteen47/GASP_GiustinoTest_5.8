# Handover anim pipeline: spike fase A, checkpoint 1 (2026-11-02, branch claude/project-thread-mkwsb0)

Esito: **GO sul Control Rig a 60 fps; NO-GO su 62,5 fps.** Codice: `Tools/AnimPipeline/animpipe/` (`python -m animpipe ...`),
scheda `Tools/AnimPipeline/cards/jab_v11.json`. Risultati completi: `Saved/AnimPipeline/animpipe/*.json` (non versionati).
Asset: `/Game/AnimPipeline/Rig/{LS_,AS_}JabV11_{RT60,Sparse}` (Content non versionato).

Punti del brief §3:
1. Rig: `CR_Mannequin_Body` del GASP funziona così com'è sul UEFN mannequin: 22 controlli, 985 canali. FK/IK ok.
2. Round trip jab v11 denso (anim -> bake sui controlli -> AnimSequence): effettori entro 0,06 cm (mani, testa, bacino,
   piedi, pugno), ma spine_05 0,96 cm e polpacci 0,58 cm (IK spline della colonna e pole del ginocchio del rig): sopra lo
   0,5 del brief, dentro l'1 cm della regressione §4.6. Accettato (le mosse nuove nascono sul rig). Chiavi solo ai 10 frame
   della scheda: 37430 -> 9850 chiavi, pugno a 7,35 cm al frame 4 (atteso: v11 è una curva densa di Mario; è una misura).
3. Tangenti e pesi per chiave su un controllo del rig da Python: OK (`animpipe tangent`, readback e curva cambiata).
   Animation Assistant via MCP non provato (unreal-mcp non connesso in questa sessione): API diretta basta.
4. Motion trails: NON fatto, rimandato al prossimo thread.
5. 62,5 fps: l'export rifiuta ("62,5 fps non multiplo o fattore di 30"). Si resta a 60 con `round(G x 0,96)`.
6. Un passaggio completo (bake + export + confronto) = 8 s.

Gotcha (già nel codice): `SequencerTools.export_anim_sequence` non trova lo spawnable -> usare
`ControlRigSequencerLibrary.export_anim_sequence_from_sequencer` con la sequenza aperta e `set_current_time(0)` prima del
bake; dopo un save un dialogo di source control blocca il bridge qualche secondo (retry in `_sample`); l'export include
il frame di fine playback.

Scheda: le finestre vengono dal CSV (`refs/ufd/*_timing.csv`), `card.to_move_sheet` rigenera la voce v11 identica a
quella scritta a mano in `move_sheets.json`. Per jab v11 le "pose" sono i confini delle finestre (mossa nata densa).

Prossimo thread: motion trails (punto 4), poi fase B: `animpipe build <scheda>` che importa le pose Step del
proprietario sui controlli ai frame della scheda e applica tangenti per profilo di spaziatura; regressione v12/v3.1.

Domande per il proprietario: (1) 62,5 fps forse richiede di cambiare il frame rate di animazione nelle Project Settings:
vale la pena provarlo o restiamo a 60 (consigliato)? (2) ok 1 cm su colonna/ginocchio nel round trip delle mosse vecchie?
Editor: aperto da riga di comando e chiuso (processo terminato, nessun salvataggio di DefaultLevel/NewBlueprint).
Costo stimato: Opus ~45 chiamate, ~75k contesto finale, circa $4-5; nessun subagente.

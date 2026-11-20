# skid-walkturn-2 (portatile, 2026-11-20, Opus medium) - FERMO: UE 5.9 del portatile troppo vecchio per gli asset
Branch claude/skid-walkturn-2 (da claude/skid-walkturn-1 ad43d876), checkout GASP_Giustino2_n216qs (copia 5.9 con contenuti GASP;
i 2 falsi positivi LFS PM_BoostJump/PM_ThrowRider messi in `git stash` lì, non persi). Cartella principale non toccata (VFX non committato).

## Fatto
1. Build editor OK (Saved/Logs/ci/build_wt2.log, 571 s; rebuild cvar build_wt2b.log 34 s).
2. Cvar `gstn.Skid.Sim` (0 = la nostra skid sim non parte, si vede il pivot GASP): GSTN_CombatSimRunSkid.cpp, 97ec0536.
3. Owner: editor piccolo nelle catture low-res: vd2_mover.py `EDITOR_WIN` (default 960x540, `full` per video owner),
   speed-up.md §4, a1a75dd9. NON ancora provato in una cattura.
4. Unreal Assistant (3 domande, risposte in docs/notes/assistant/2026-11-20_pivot.md), verificato su Chooser.cpp 5.9 r.740-800:
   - riga nested DENTRO una tabella con PoseSearchColumn: i suoi asset non entrano nel DB interno, non vince mai;
   - tabella SENZA PoseSearchColumn: se la nested non dà risultati si passa alla riga dopo (fall-through), poi Fallback.
     Nota sorgente: le output column del parent vengono scritte PRIMA di ChooseMulti, anche se la nested fallisce.
   - schema consigliato: in `Locomotion Transitions` (parent, niente pose search) una riga Evaluate/Nested
     `CHT_GSTN_PivotSet` SOPRA `Run F Loco Transitions`/`Sprint Loco Transitions`, con colonne stock copiate
     (Speed2D, FutureFacingDelta, IsPivoting, MovementDirection_Recent, output struct) + Gait + tag kit/skin
     (GameplayTag column, skin prima del kit: fall-through skin -> kit -> stock) + PoseSearchColumn PROPRIA.
     Clip: root motion ON, curve e notify `Transition:` copiati dalla clip pivot stock, ExcludeFromDatabase sulla coda.
   Coincide con la risposta avuta sul fisso (commit locale a7496388, non pushato).
5. Script sola lettura Tools/GAS/pivot/pivot_rows_dump.py (righe pivot della base stock): NON eseguito con successo.

## Blocco (regola 6: manca lo strumento)
UE 5.9 del portatile = CL 60351404; gli asset salvati sul fisso (CHT_GSTN_StockMover_PoseMatch, clip Mixamo) sono CL 60738873:
"salvato con una versione del motore più recente" (Saved/Logs/ci/pivot_dump_ue.log). Serve aggiornare UE 5.9 dal launcher
Epic sul portatile (owner), poi rebuild incrementale. Tentativi: 1 (dump).

## Prossimo
Aggiornato il motore: dump righe pivot -> CHT_GSTN_PivotSet + riga in Locomotion Transitions (base 1, gstn.Loco.Base 1), riga 11
in gasp-divergenze.md PRIMA del commit; tag kit/skin richiede una proprietà leggibile dal contesto ABP (decidere C++ vs BP);
clip sprint = skid attuale serve prep_gasp_clip (root motion). Poi cattura base (Skid.Sim 0, stock) vs variante, ASSERT, review.

COSTO: ~6 $ | 0 min desktop | 0 token review

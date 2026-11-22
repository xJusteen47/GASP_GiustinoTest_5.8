# runstart-v6-desktop: run start v4r6 (più dritto) + curve stock A/B (desktop, 2026-11-20)
Branch claude/runstart-v6-desktop (da claude/runstart-v5-desktop 98c3b9f6). _ue59 lock preso e liberato. Ricontrollo (regola 11):
Saved/RunStartV5/warp1p/assert.txt letto, ASSERT: OK f95-170 93 % come scritto.

## 0. v4r5 non era committata
AS_GSTN_RunStart_v4r5 e le righe (CHT, DA_Locomotion_Default) erano solo nel working tree: committate in bcc3463e. Il tag
approved/runstart-v4r5 (af857cff) NON contiene la clip: il contenuto bloccato è bcc3463e. DA_Locomotion_Default era cambiato dopo
la cattura v5 (20:06Z, forse playtest owner): con il DA attuale lo stage di corsa resta 0 (~475 cm/s) nel mv_run_start_v5.

## 1. Curve (owner: come GASP stock)
- Chi legge `phase`: Curve channel `Phase` di PSS_Relaxed_Loops / PSS_Default_Mover, campioni t e t-0,033 s (pss_channels_dump.py ->
  Saved/Routing/pss_channels.txt); l'Unreal Assistant non poteva ispezionare gli asset (Saved/Assistant/replies.json).
- Convenzione stock: triangolo +1 appoggio sinistro, -1 destro (curves_plants_check.py, Saved/Routing/plants_*.json, entro 1 frame).
- curves_author.py: kind start_stock (movedata_speed dal root, enable_warping 0->1 nel 10 %, phase stock), CURVES_PHASE=ours.
- A/B catture pulite (stesso DA), scivolamento piedi f160-200: v4r5 14,5 cm, v4r5c (curve+phase stock) 34,5, v4r5d (curve stock,
  phase nostra) 19,1; f80-240 L/R: v4r5 36,4/33,4, v4r5d 30,3/43,4 (runstart_v4_evidence.py, docs/notes/reviews/runstart-v4r5c, -v4r5d).
  movedata_speed non cambia il play rate (tempo clip per frame = 1,0 in entrambe). Con la phase stock il MM entra nel loop su un
  frame con valore giusto ma direzione opposta: costo traiettoria. Riga 14 in gasp-divergenze.md. Esito: le curve stock non
  migliorano; v4r5c e v4r5d restano come varianti (la v4r5 bloccata non è toccata).

## 2. v4r6 (Cascadeur)
RunStart_v4r6.casc = v4r5 + runstart_upright_casc_v4r6.py: corpo sopra spine_02 indietro 15° (10° a f0), testa di nuovo avanti 75 %,
sfuma f29-45; spine_02->collo 50-53° -> 35-38°. Clearance braccia min +1,8 cm. FBX -> AS_GSTN_RunStart_v4r6 (runstart_v4_ue.py,
RSV3_EXIT_AT 1.0667), root 977,8 cm. Curve: set della v4r5 (kind start); con start_stock il piede destro scivolava 43 cm.
Righe: v4r5 off, v4r6 on (Saved/AnimPipeline/rows/runstart_v4r6_new.txt). Per tornare alla v4r5: runstart_curves_variant_ue.py via
Save-Assets con RSC_SRC=v4r5 RSC_DST=v4r6 RSC_ROWS=stock (ricrea le curve start_stock su v4r6: poi curves_author kind start).
Cattura Saved/RunStartV6/v4r6s: ASSERT: OK f95-170 93 % (min 0,56). Review Fable r7: PASS con riserve (review_fable_r7.md + addendum).
Video owner docs/notes/reviews/runstart-v4r6/owner/runstart_v4r5_vs_v4r6(_025x).mp4.

## Aperti
1. posa (Fable, minore): v096 mento forse alto; v128-158 inclinazione non monotona mentre la variante sfuma.
2. gioco: nel loop (da f152) il personaggio torna stock e si appiattisce; scivolamento piedi 30-40 cm resta (anche v4r5).
3. Coda: start indietro/girati (B/LL/RL) nello stile vincente, thread nuovo. Merge hiq1zx/xmve9g su _ue59 non fatto (budget).
COSTO: ~17 $ | ~25 min desktop | review ~0.08M token

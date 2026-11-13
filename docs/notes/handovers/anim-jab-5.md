# Handover: jab Overlord v3 costruiti e visti in gioco, verdetto FAIL (ANIM-JAB-5, 2026-11-12)
Branch locale `claude/desktop-anim-jab-5` (da jab-4, worktree `GASP_Giustino2_m7`), su origin come `claude/project-thread-v1mo72`. Niente C++, nessuna mappa salvata.
**Non mostrare all'owner come pronta.** Giri di correzione sul video: 0 su 2 (chiuso per soglia di contesto). Editor rilasciato.

## Verdetto (griglie v2 sopra / v3 sotto, stessi frame, guardate: `cmp_jab1_profile`, `cmp_jab1_game`, `cmp_jab2_profile`, `cmp_jab2_tail`, `cmp_hit_profile`; NON guardata: `cmp_jab2_game`)
- **Jab 1: FAIL.** Meglio della v2: caricamento vero (busto girato, mano alla testa, f2-f3), braccio lungo e orizzontale sulla linea del bersaglio, petto di lato,
  rientro pulito all'idle eretto. Non passa: (a) R1, sul colpo e nella tenuta f5-f12 le gambe sono piegate (ginocchia 41°/50°), si legge ancora "squat";
  (b) tenuta f5-f12 ferma (T1 §8: la tenuta mobile di 1,5 cm non si vede); (c) dalla telecamera `game` (dietro al personaggio) il braccio punta dentro lo
  schermo e sparisce: la sagoma è una colonna. R6 del piano presume una telecamera di lato: **chiedere all'owner da quale telecamera giudica**.
- **Jab 2: FAIL in gioco.** Sagoma del caricamento grande e alta, passo di 30 cm presente (f3 0,6 → f8 29,9, poi fermo, nessun ritorno), sul colpo la mano
  resta sul bersaglio per tutto l'hitstop (a9,33 v41-v45), osservazione, tremore e pugno leggibili. Non passa: (a) **la capsula scende di 12 cm (z 100 → 88,1) a f3,
  quando parte il root motion, e resta bassa anche dopo la mossa**; l'IK dei piedi piega le gambe: affondo e osservazione diventano una seduta, e a fine mossa
  le gambe restano un po' piegate. È lato gioco, non pose; (b) caricamento f2-f6 quasi identico (il piano dice "non si ferma mai"); (c) mano sul colpo bassa
  (z 102, anca del manichino) e dentro il manichino; (d) sul colpo l'avanzamento è 34,1 cm (4 cm in più sul frame dopo l'hitstop).
- Numeri: `capture_numbers.md`. Video: `jab1_ingame.mp4`, `jab1_jab2_ingame(_slow).mp4`, `jab1_jab2_hit_ingame(_slow).mp4` in `docs/notes/reviews/overlord-jab-v3/`.

## Fatto
- Schede `Tools/AnimPipeline/cards/overlord/jab/overlord_jab{1,2}_v3.json` + `overlord_jab_v3_author_notes.md` (segni confermati; l'idle è già girato 24° a destra).
  Key sparsi (13 e 17 pose), `part_delays` 0 (spostavano il contatto: il ritardo della catena è nei key). `stick_preview.py` portato da jab-1.
- Passo del jab 2 **solo dati**: `edits` su `root_ctrl` (+Y 30 da f9), piede destro piantato nel mondo; `enable_root_motion` per mossa in `setup_overlord_jab_kit.py`
  (`ANIM_VERSION='V3'`, `BLEND_OUT_SECONDS=4/60` provvisorio, play rate 62,5/60 mantenuto). Le copie V2 restano su disco, non referenziate.
- Tremore: 78 key su `hand_r_fk_ctrl` e `lowerarm_r_fk_ctrl` (20%), sezione "Manuale" di `LS_OverlordJab2V3_Build`, periodo 4 frame, 5° → 0,5°.
- `Tools/GASNPPTest/pie_capture_compare.py`: griglie v2/v3 allineate sul frame 0 della mossa. Frame grezzi in `Saved/AnimJab5/capture` (non committati).

## Check numerici contro teoria (vince l'occhio; dichiarati)
`contact_faf` jab 2 FAIL (picco a f0: parte dal braccio teso del jab 1 e il check misura senza il root); `legibility` FAIL (regole da pugno, ma "braccio nascosto
alla telecamera di gioco" è vero, vedi sopra); `clearance` jab 1 FAIL residuo: mano destra −2,5 cm in `spine_05` a f3 e −1,5 cm in `clavicle_l` a f4
(2 tentativi fatti, f2 risolto: prossimo passo +3/4 cm su Y di `poses["4"].arm_r.hand`); `motion_check` (misura) FAIL: catena rigida, niente overlap.

## Prossimo thread (in ordine)
1. CODICE/sim: perché il root motion del montage abbassa la capsula di 12 cm e non la rialza (`FLayeredMove_SyncMontageRootMotion` contro l'altezza da terra del
   walking mode; vedi `displacement.md` §1C). Finché non è chiuso, il jab 2 in gioco non mostra le pose della scheda. Alternativa senza codice: passo solo in `pelvis.loc`.
2. Pose, giro 1: jab 1 gambe lunghe sul colpo (bacino meno basso, piede sinistro arretrato o destro più avanti), tenuta che respira, clearance f3-f4;
   jab 2 caricamento che continua a salire f3→f6, mano sul colpo all'altezza del petto, affondo con gamba dietro lunga. Poi ricattura con gli stessi comandi.
3. Domanda all'owner: telecamera di giudizio (dietro o di lato). Scelte ancora sue: passo 30 cm col piede sinistro, pugno dopo il tremore, pestone destro nel jab 1.

Costo stimato: ~150k token di contesto (Fable) + subagenti: Opus high 2 (~450k token), Sonnet 2 (~240k token). "Cose da fare a mano": nessun passo nuovo.

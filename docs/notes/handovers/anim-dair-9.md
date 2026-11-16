# Handover: down-air dell'Overlord v8 costruito e visto in gioco, verdetto FAIL (ANIM-DAIR-9, 2026-11-12)
Branch locale `claude/desktop-anim-dair-8` nel checkout principale `GASP_Giustino2`, su origin come `claude/project-thread-woqa0y`.
Niente C++, nessuna mappa salvata, `stash@{0}` e `NewBlueprint1/2.uasset` non toccati. Editor rilasciato (solo lanci brevi, tutti chiusi).
**Non mostrare all'owner come pronta.** Giri di correzione sulle pose: 0 su 2 (chiuso per soglia di contesto).
## Verdetto: FAIL (REVISE). Revisione completa in `docs/notes/reviews/overlord-dair-v8/review.md` (11 griglie v7 sopra / v8 sotto, 7 mp4)
Meglio della v7 su 8 punti su 10, peggio su nessuno. Passano: carica, spacing del colpo in 2 frame (resta pesante, arresto secco),
frame di hitstop (chiave pulita a15 da dietro e di lato), sagoma di lato. Non passano:
- **Dopo l'impatto (a16-a27): FAIL.** Il rimbalzo dei piedi non si vede (≤1 px a 360 px), circa 11 frame letti come fermo immagine.
- **Tenuta vista da dietro: PARZIALE.** Busto ruotato e un gomito fuori, ma sotto il bacino gambe dritte, unite e speculari: una colonna.
- Recovery in sequenza presente ma arriva veloce dentro il blend-out (gambe che cambiano in 2 frame a f38-39). Carica debole da dietro.
## Correzioni per il giro 1 (assi card: +X sinistra, +Y avanti, +Z alto; key 0, 3, 37, 42 intoccabili)
1. a17→a23 gambe: rigide fino a 17, poi le ginocchia cedono e i piedi salgono 8-10 cm a a19-20 (ora 3,5), giù entro a23, una sola oscillazione.
2. a17→a26 bacino e colonna: bacino -8/-10 cm Z, arco che si chiude di 8-10°, testa per ultima, continuo fino a 26.
3. a8→a10: ginocchia e gomiti dentro di 6-8 cm su X, testa -5 cm Z (la palla si stringe del 15% vista da dietro).
4. a28→a37: 80% del tragitto entro a34 con slow-in sulla 37; lato kit provare blend-out di 8 frame (`BLEND_OUT_SECONDS`).
5. **Serve l'OK dell'owner (toccano la sua chiave ruotata a15):** ginocchio sinistro piegato 15-20° e fuori 8-10 cm, piede sinistro 10 cm
   più alto, gamba destra dritta; gomiti fuori 10-12 cm per lato e 5 cm più alti, braccia sempre incrociate. Senza, da dietro resta una colonna.
## Trovato in gioco (non sono pose)
- **Le build di animpipe lasciano `ik_foot_l/r` fermi alla posa di riferimento**; l'ABP di gioco ci appoggia i piedi, quindi le gambe non
  seguivano l'animazione (vale anche per il jab: è lo "squat"). Corretto solo sulle copie del kit (`_bake_ik_feet` in
  `Tools/GAS/setup_overlord_dair_kit.py`). **La correzione vera va in animpipe build/export per tutte le mosse.**
- Montage con blend-out di default (0,25 s) si mangiava la recovery: ora 4 frame. Salto di prova: picco 233 cm, 37 frame di caduta.
- Capsula: spawn a z 100, dopo QUALSIASI atterraggio z 88,1 (anche un salto senza mossa): non dipende dal root motion del montage.
- Nessun landing cancel: atterrando a metà mossa il personaggio resta sulle punte circa 8 frame, poi si accovaccia e scatta all'idle.
- Hitstop: all'inizio la posa torna da a15,9 a a15,0 e alla fine salta un frame (a16,2→a18,2); non visibile, è la sincronia montage/timeline.
- Colpo a segno: i piedi scendono dietro il manichino con uno spazio visibile (hitbox 40 cm provvisoria), il manichino non reagisce.
- Dalla camera di gioco (dietro) nessun arto sparisce nello schermo: il colpo è verticale.
- `animpipe check` v8: bloccanti `legibility` (regole da pugno), `source_fidelity` (path delle pose owner fuori da refs, come v7), `co_motion` (braccio destro 7-11).
## Prossimo thread (serve l'editor): correzioni 1-4 nella card, poi `author`/`build`, kit con `refresh=True`, cattura e nuova revisione
- Kit: `UnrealEditor-Cmd -run=pythonscript -script=Tools/GAS/setup_overlord_dair_kit.py` con `OVERLORD_DAIR_ANIM=<V8_Build>,<V7_Build>`.
- Cattura: `link_mp.patch(True)`, editor su `L_Test_Hub` con `-ExecCmds="py <boot>"`, `PIE_CAPTURE_KIT`/`PIE_CAPTURE_OUT`, poi `link_mp.restore()`.
- Griglie: `python Tools/GASNPPTest/pie_capture_compare.py <cap_v7> <cap_v8> <out> --old-label=v7 --new-label=v8 --set=dair`. Frame grezzi in `Saved/AnimDAir/capture_v{7,8}_r5`.
- In gioco oggi (non approvata): `L_Test_Hub`, kit `KIT_Test_OverlordDAirV8` (o `V7`), salto e poi giù + attacco in aria.
Tentativi su blocchi: cattura 2 (giocatori PIE), 1 (bake IK). Costo stimato: ~155k token di contesto Fable + un subagente Fable di revisione (~90k).

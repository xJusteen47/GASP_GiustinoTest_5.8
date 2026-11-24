# dropkick-tb-4 (portatile, notte 2026-11-21, Opus) - tech bounce del dropkick rifatto (r4): Fable PASS al giro di posa 2
Branch claude/project-thread-xbjsuj (da wc8ksn c1b49823), checkout GASP_Giustino2_n216qs. Owner kit KIT_Test_Playtest NON toccato (gioca ancora r3).

## Fatto
1. Base oggi (r3 nel kit owner): Saved/DropkickTB4/base2, docs/notes/reviews/dropkick-tech-bounce-v4/base/. Nessun corpo sotto il pavimento.
2. Design Fable (regista): v4/design-r4.md. Piano: Tools/AnimPipeline/cards/overlord/dropkick/r4/make_r4_plan.py (altezze MONDO convertite
   col salto della capsula misurato in base2) -> pose_plan -> animpipe author + build (AS_OverlordDropkickTechBounceR4).
3. Variante: setup_dropkick_tech_bounce.py ROUND=R4 FRAMES=12 BLEND_OUT=1 -> KIT_Test_PlaytestTBR4, TL/GA/AM/AS ...R4, WPN_Test_PlaytestTBR4.
   Due bug dello script corretti: la classe del round non veniva registrata (match sul prefisso trovava R3: giocava il bounce generico);
   il blend-out automatico del montage dava la posa a GroundHit 3 frame prima (r3 mostrava 11 frame su 15).
4. Giro 1 (Saved/DropkickTB4/r4d, ASSERT OK): Fable FAIL stretto (v4/r4/review-r4.md). Giro 2 (r4e, ASSERT OK, pose f0-f3 e f8-f11):
   **Fable PASS** (v4/r4b/review-r4b.md): tratti ok, f0 il peso arriva sul contatto (testa 41 -> 27), niente sit-up, stacco leggibile
   sullo zoom 4x della camera di gioco (grid_low_whiff_game_zoom.png). Video: r4b/dropkick_low_{whiff,hit}_{realspeed,slow4x}.mp4.
5. Laptop: animpipe senza Antigravity ne' /Game/AnimPipeline/Pilot (editor_session.py, template = copia locale di R3, Pillow nel preview).
6. Proposta auto-cancel landing senza codice runtime: docs/notes/mosse/auto-cancel-landing-proposta.md.

## Per l'owner (decisioni)
1. Mettere r4 nel kit playtest? A) si' (setup_playtest_dropkick_knockdown.py con PLAYTEST_DROPKICK_OWN_BOUNCE=R4, consigliata dopo che
   guardi il video) B) no.
2. Il frame di contatto e' ancora l'ultimo frame del dropkick (a59, inclinato, piedi al pavimento): A) la clip del bounce parte sul frame di
   touchdown (dato del dropkick, consigliata) B) si riposa a59.
3. GroundHit (asset owner) fa un salto della testa di ~22 cm in 1 frame dopo il bounce: A) accettare come secondo impatto B) entrare in GroundHit dal suo f2.

## Aperto / non fatto
- Gioco: hit vs whiff stessa clip, pelvi 1-5 cm piu' alta in hit; dropkick da salto alto: il move finisce in aria (timeout) e non rimbalza.
- Polish opzionale (review r4b): pelvi f1-f3 16/15/16 (oggi 19/22/21), rompere il plateau all'apice f7-f9.
- Cascadeur sul pivot: il portatile ha Cascadeur ma lo script server va avviato a mano (Commands > MCP > Start script server): non fatto.
- Stomp landing, ftilt, air idle/crouch: nessun brief con le parole dell'owner (coordinatore: solo proposte); run start indietro saltati.
- Check animpipe (legibility/contact_faf/fists/co_motion) pensati per gli attacchi: falliscono per costruzione su questa clip.
- Asset solo locali sul portatile: /Game/AnimPipeline/Pilot/AS_Pilot_GanonFTilt_V2 (stand-in), Rig/.../AS_OverlordDropkickTechBounceR4.

Prove: assert Saved/DropkickTB4/r4e/assert.txt (pie_capture_move.py + capture_assert), numeri in v4/r4b/frames.md (frames.jsonl r4e).
COSTO: ~35 $ (stima) | 0 min desktop | ~330k token review/regia Fable

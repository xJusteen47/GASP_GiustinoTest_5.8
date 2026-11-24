# fthrow-overlord-1 (portatile, 2026-11-23/24, Opus medium) - F. throw Overlord v1: clip PASS ai check, cattura NON fatta (presa non avviene)
Branch claude/project-thread-y270ci (da jcugce = integ-2026-11-23 + fix skill), checkout n216qs (build C++ 593 s ESITO OK).
OBIETTIVO: F. throw (presa a due mani + testata) dal piano Zetterburn, owner 1C 2A 3A. Riuso: animpipe author/build/check, Poser,
GSTN_GA_LinkThrowForward (mira istantanea), pie_capture_move. Dossier docs/notes/reviews/fthrow-overlord/dossier.md.
## Fatto
- Carte: cards/overlord/fthrow/make_fthrow_cards.py -> overlord_fthrow_v1.json (17 chiavi, mani in coordinate mondo ai LATI del collo,
  impatto a19 = g20, FAF a44 = g46, clip 51 f, passo 30 cm). Arco fronte: picco a14 -> impatto a19 42 cm avanti e 14 giu', ultimi 2 frame 57 %.
- Giro 1 (2 tentativi): catena cinetica, contatto/FAF, piedi; testa nella mano dx -4,85 cm (polsi davanti al collo). Giro 2 (2 tentativi):
  polsi ai lati, esenzione pugni unica 0-38 (due voci con lo stesso check si sovrascrivono!), bacino che parte a a16.
- Check giro 2 (reviews/fthrow-overlord/v1r2/check_r2.txt): PASS clearance, bone_lengths, contact_faf (picco a20), planted 0,07 cm, fists,
  hitstop_snap, co_motion. legibility = n/a (accetta solo braccia/gambe come arto d'attacco). motion_check (misura): overlap bacino>petto 0,33
  nello scatto, recupero senza overlap, gomiti alzati 51-63 gradi nella presa (voluto), sbilanciamento a17 (affondo dichiarato).
- Nel progetto (Content/GASNPPTest/Combat/Overlord, nuovi): AS_OverlordFThrowV1, AM_OverlordFThrowV1, GA_Overlord_FThrowV1 (figlio di
  GSTN_GA_LinkThrowForward: totale 52 g, impatto 20, FAF 46, hitstop 8; danno/knockback segnaposto). Nessun kit modificato.
## Cattura: FAIL di strumento, non di posa (reviews/.../v1r2/capture.log, assert.txt)
- cards/overlord/fthrow/cap/capture_v1.ps1: Interact a v20, Attack + stick a v65 su KIT_Test_Overlord contro BP_GASNPP_TestCharacter a 80 cm.
  La presa NON parte (nessun tag di link, history solo "+H"), quindi Attack fa il jab: ASSERT FAIL (AM_OverlordJab1V3).
- Causa probabile: KIT_Test_Overlord non ha gli slot di presa o il manichino non e' un bersaglio (link_scenarios usa KIT_Test_Link +
  link_setup()/grab_content_setup). Le proprieta' di GSTN_LinkSettings NON sono visibili a Python: il lancio F. e l'HoldPoint vanno in un ini
  locale Saved/Config/WindowsEditor/GSTNLink.ini (lo script lo scrive e lo toglie).
- Prossimo (thread strumento, Sonnet): sequenza fthrow_hit con il setup di link_scenarios (kit con presa + bersaglio), verificare il
  tag del link in frames.jsonl, poi cattura dietro+lato+game e review cloud. Altezza: pawn z 100 in cattura -> mezza capsula ~100, quindi
  HoldPoint Z = 136 - 100 = 36 (FTHROW_HOLD_POINT=38,-5,36), non 46.
## Voci "gioco" in coda codice
- Ancora del collo fissa: la vittima non segue le mani (tirata di 10 cm del piano impossibile senza ancora per frame).
- Vittima stessa taglia con collo a 136 cm: ~9 cm sotto il pavimento se dritta; serve la posa "tenuto per il collo" del bersaglio.
## Prove
Check: `python -m animpipe check cards/overlord/fthrow/overlord_fthrow_v1.json` (cwd Tools/AnimPipeline). Cattura: capture_v1.ps1 -> assert.txt.
Lock n216qs liberato, editor chiuso, ini temporaneo rimosso.
SUBAGENTI: 0
COSTO: ~10 $ (stima, 2 giri) | 75 minuti desktop (build 10, editor animpipe 25, catture 4 di cui 2 a vuoto, attese coda non contate) | 0 token review

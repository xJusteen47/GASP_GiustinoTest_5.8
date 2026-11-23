# fthrow-overlord-1 (portatile, 2026-11-23, Opus medium) - F. throw Overlord v1, PARZIALE: fermo al tentativo 2 di 2 (regola 5)
Branch claude/project-thread-y270ci (da jcugce = integ-2026-11-23 + fix skill), checkout n216qs (build C++ 593 s ESITO OK).
OBIETTIVO: F. throw (presa a due mani + testata) dal piano Zetterburn, owner 1C 2A 3A. Riuso: animpipe author/build/check, Poser,
GSTN_GA_LinkThrowForward (mira istantanea), pie_capture_move. Dossier docs/notes/reviews/fthrow-overlord/dossier.md.
## Fatto
- Carte: cards/overlord/fthrow/make_fthrow_cards.py -> overlord_fthrow_v1.json (16 chiavi, mani in coordinate mondo sul collo,
  impatto a19 = g20, FAF a44 = g46, clip 51 f). Arco fronte: picco a14 (y -11.6, z 163) -> impatto a19 (y 30.4, z 148.9); ultimi 2 frame
  57 % del percorso (report offline_report.txt in reviews/fthrow-overlord/v1).
- Check finale (check_v1c.txt): PASS bone_lengths, contact_faf (picco a20), planted_feet 0.07 cm, hitstop_snap, co_motion.
  FAIL: clearance hand_r|head -4.85 cm @a20 (-1.8 @a19); fists 0.8 @a0 (la presa e' semi-aperta per scelta: manca l'esenzione 0-20);
  legibility = n/a (conosce solo braccia/gambe come arto d'attacco: arm_r fa da segnaposto, regole del pugno non giudicano una testata).
- Pronti ma NON eseguiti (niente cattura con un check di compenetrazione fallito, regola 15b): Tools/GAS/setup_fthrow_variant.py
  (clip, AM, GA_Overlord_FThrowV1: impatto 20, FAF 46, hitstop 8), Tools/GASNPPTest/fthrow_capture_pre.py (fallback F. + hold point in
  memoria, nessun kit toccato), pie_capture_move: PIE_CAPTURE_PRE_PY e sequenza 'fthrow_hit' (Interact, poi Attack + stick a +45).
## Diagnosi del FAIL rimasto (per il prossimo giro, 1 build)
1) testa dentro la mano destra a19-a20. 2) i polsi stanno DAVANTI al collo della vittima (GRIP y = collo -8 cm, cioe' alla stessa
profondita' della faccia della vittima): la fronte che arriva sulla faccia scende sulle proprie mani. 3) polsi ai LATI del collo
(GRIP y = collo -2, x +-11, z -7) + esenzione fists 0-20 ("presa"). Poi setup_fthrow_variant, cattura (dietro az180 + lato az90 + game,
run hitbox separato), review cloud.
## Voci "gioco" in coda codice
- Ancora del collo fissa: la vittima non segue le mani (tirata di 10 cm del piano impossibile senza ancora per frame).
- Vittima stessa taglia con collo a 136 cm: ~9 cm sotto il pavimento se dritta; serve la posa "tenuto per il collo" del bersaglio.
- HoldPointCm per il F. = (38, -5, 136 - mezza altezza capsula): mezza altezza da verificare in editor (fthrow_capture_pre usa 90).
- motion_check (misura): gomiti alzati 57-63 gradi nella presa (presa ad altezza spalla, voluta), restano 2 catene rigide.
## Prove
Numeri da Saved/AnimPipeline/animpipe/overlord_fthrow_v1_check.txt (copia in reviews/fthrow-overlord/v1), comando
`python -m animpipe check cards/overlord/fthrow/overlord_fthrow_v1.json` (cwd Tools/AnimPipeline). Lock n216qs liberato.
SUBAGENTI: 0
COSTO: ~6 $ (stima) | 45 minuti desktop (build 10, editor animpipe 15, attese coda 30 non contate) | 0 token review

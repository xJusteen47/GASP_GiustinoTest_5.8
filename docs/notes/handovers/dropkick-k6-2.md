# dropkick-k6-2 (portatile, 2026-11-21, Opus) - K6 giro di posa 2 di 3
Branch claude/project-thread-l1dcng (da jx1fv2 d3d47d18), checkout GASP_Giustino2_n216qs. K5 e K6 r1 (pack k6a) intatti in git.

## Fatto
1. k6/make_k6_cards.py: startup chiavi 0/2/4/5/7/11/14/15 (apertura a3, chiusura a5, tenuta a6, rilascio a8; chiave 8 tolta),
   settle con ampiezza doppia (picco +11, rimbalzo -3,5, testa +11 due frame dopo, avambracci che si staccano 5 cm e ricadono ultimi).
2. Build reale x2 (tentativo 1: apertura che sembrava la guardia; tentativo 2: apertura "ad ali"). Clearance PASS in UE sulle 3 clip.
3. Catture k6/capture_k6r2.ps1 (viste az90, az180, az315, game; alta a 350 cm): ASSERT OK x3. Pacchetto reviews/dropkick-kick-k6/k6r2b
   (README con cosa e' cambiato). Le catture del tentativo 1 sono in Saved/DropkickK6/r2v1 (non versionate), pack k6r2 non committato.
4. Editor aperto da me e chiuso da me (uscita exit 3 = assert noto alla chiusura dopo i salvataggi, asset salvati 22:10).

## Aperto
- Review cloud (regola 10) su k6r2b: tratti, poi vita (movimento braccia a3-a8, settle), poi tecnico. E' il giro 2 di 3.
- Rischio noto: apertura a3 contenuta da dietro (braccio corto 26,7 cm); se FAIL su questo, al giro 3 alzare di piu' i gomiti
  o muovere anche il busto (torsione), non stringere di piu' il press (clearance gia' a ~0,5 cm).
- Script di cattura veloci del coordinatore (claude/runstart-generic-desktop) non usati: catture gia' fatte.
Prove: clearance da Saved/AnimPipeline/animpipe/overlord_dropkick_*_k6_check.txt (riga "block clearance"); ASSERT da
k6r2b/assert_lines.txt; spostamenti dei gomiti dal Poser offline (%TEMP%/k6r2/try3.py, try5.py, non versionati).
COSTO: ~6 $ (stima) | 0 min desktop | 0 token review

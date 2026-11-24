# fthrow-capture-fix-1 (cloud, 2026-11-24, Opus medium) - strumento di cattura F. throw: setup di presa dei test di rete. Cattura NON ancora fatta
Branch claude/project-thread-7ogxes (da claude/project-thread-y270ci 01262dc5). Posa e timing NON toccati (impatto 20, FAF 46, hitstop 8).
OBIETTIVO: la sequenza fthrow_hit forma la presa e Attack + avanti gioca GA_Overlord_FThrowV1 (v1r2: nessuna presa, jab, ASSERT FAIL).
Riuso: link_scenarios.prepare_editor/link_setup, KIT_Test_Link (contenuto F13), GSTN_LinkContentLibrary.SetKitThrows, hook PIE_CAPTURE_PRE_PY.
## Causa (lettura del codice, non verificata in editor)
- KIT_Test_Overlord non ha slot di link: Interact non ha mossa di presa, quindi Attack resta il jab.
- KIT_Test_Link lega il SUO lancio avanti (GA_ThrowForward_Test) nelle righe del kit: il FallbackThrows dell'ini non basterebbe;
  serve Throws.Forward dell'estensione link del kit (ApplyKitThrows lo lega al posto della riga).
## Fatto
- cards/overlord/fthrow/cap/fthrow_capture_pre.py (prima di Play): prepare_editor + link_setup, KIT_Test_Link Throws.Forward =
  GA_Overlord_FThrowV1 SOLO IN MEMORIA (l'editor di cattura esce senza salvare). Back/Up/Down null = fallback.
- Tools/GASNPPTest/pie_capture_move.py, fthrow_hit: 'dummy_kit' (PIE_CAPTURE_DUMMY_KIT, default KIT_Test_Link, come i pawn dei test
  di rete) applicato al piazzamento del manichino; 'link_probe': in capture.log "grab probe" 2 frame prima di Interact e
  "host Body link ... FORMED/NOT FORMED" 15 frame dopo.
- capture_v1.ps1: -Kit KIT_Test_Link, PIE_CAPTURE_PRE_PY, HoldPoint default 38,-5,36, uscita in captures/fthrow-overlord/v1r3,
  stampa le 2 righe di link prima dell'assert. Ini locale GSTNLink.ini come prima (scritto e tolto).
- Verifica: solo py_compile dei due .py (nessun editor in cloud).
## Prossimo (portatile, quando il coordinatore libera l'editor; tentativo 1 di 2)
1. checkout di questo branch nel worktree n216qs (il ps1 usa quel percorso), niente build C++ (nessun C++ cambiato).
2. capture_v1.ps1. Leggi prima le righe grab probe / Body link: NOT FORMED + refused <motivo> = problema di presa (es. Team);
   FORMED ma ASSERT FAIL owned by AM_*Test* = Throws.Forward non preso (controlla la riga "fthrow pre" del log).
3. ASSERT OK -> review cloud (regola 10) solo sulla finestra presa/lancio: KIT_Test_Link ha locomozione e aspetto suoi.
## Rischi
- Se la presa con il manichino resta rifiutata, tentativo 2: PIE_CAPTURE_DUMMY_KIT='' (kit proprio del manichino) o distanza 90 cm
  (PIE_CAPTURE_GRAB_DIST, come link_scenarios.place).
SUBAGENTI: 0
COSTO: ~1,5 $ (stima) | 0 minuti desktop | 0 token review

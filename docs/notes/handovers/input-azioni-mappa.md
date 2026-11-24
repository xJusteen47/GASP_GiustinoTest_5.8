# Handover: mappa input e verifica strategia "tutto ad azioni" (cloud, 2026-11-23)

Branch `claude/project-thread-uvefj5` (il coordinatore aveva proposto `claude/input-azioni-mappa`: le regole della sessione
permettono il push solo su questo). Sola lettura, nessun codice modificato. Base `main` a5b2095.

OBIETTIVO: mappa di cosa è legato ad azioni/comandi, tasti, IMC; verifica indipendente dell'ipotesi dell'owner.
ESITO: fatto. Documento `docs/notes/input-azioni-mappa.md` (riassunto 10 righe, tabella, letture dirette, analisi, piano P0-P5, 3 decisioni).

Riuso dichiarato: INDICE-PROGETTO §5.10, input.md, gas-npp.md, combat.md §12.7, handover pad-menu-1 e spin-drill, readme NP.

Risultati chiave:
- Il codice C++ di gioco non legge tasti fisici (grep in §2 del documento). Il combattimento passa già da azione -> bottone logico -> FGSTN_InputCmd.
- Tre canali di input in rete in parallelo (comando GSTN, input Mover scritto dal Blueprint GASP, FAbilitySimInputCmd del fork).
- Salto dal muro legge bIsJumpJustPressed di Mover (GASP IA_Jump), probabilmente senza tasto nel layout di gioco: DEDOTTO, da verificare (P0).
- Tasto diretto L3/Z nel Blueprint GASP (volo di debug); gstn.Wall.Jump/Detach scrivono globali lette nella simulazione (non Shipping).
- L'ipotesi è giusta nello spirito ma l'azione Enhanced Input è locale: la robustezza viene dal comando unico per frame.

Da confermare (al coordinatore): priorità di IMC_GASNPP_Test (5 negli script, 10 in pad-menu-1); "L3 dash" dell'owner vs L3 = run boost oggi.
Limiti: Blueprint e IMC non leggibili dal cloud (LFS), quindi tutto ciò che sta nei Blueprint è dedotto da script e handover.

PROVE: tasti da `Tools/GASNPPTest/movement_setup.py:21-29`; letture dirette con
`grep -rnE 'IsInputKeyDown|WasInputKeyJust|EKeys::|BindKey|GetInputAnalog' Source Plugins/GSTN*` (solo editor/test).

SUBAGENTI: 1, scansione meccanica di script Python, ini, doc e comandi console, Sonnet (Explore).
COSTO: ~3 $ | 0 minuti desktop | ~100k token subagente

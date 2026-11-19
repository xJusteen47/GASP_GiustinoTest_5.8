# Handover: critico estetico nelle review (anim + VFX) — 2026-11-19

Thread cloud, Opus, budget ~5 $, costo stimato ~3 $. Nessun asset né _ue59 toccati. Owner: scelta 2A (cmsg_01CRvUdrWm1dJrM4q54YDBLNPPt5yd2fN1cZr24Fzzg8j3).

Branch `claude/project-thread-fxzhai` da `feature/gas-npp-5.8` (e79e401a), con merge di
`claude/project-thread-qksijd` (skill revisore anim + contract) e `claude/project-thread-mn2nvi` (teoria VFX + revisore VFX),
così le due skill stanno insieme. Solo documenti.

Fatto:
- `docs/notes/review/critico-estetico.md`: principio del critico (separato, non ancorato, successo = difetti veri,
  almeno 3 per peso grave/medio/lieve, ogni voce con frame e riferimento, il giudice risponde a ogni voce, all'owner
  solo PASS senza `grave` aperti).
- `.claude/skills/gstn-anim-reviewer/SKILL.md`: §3b critico; §3.7 controllo curve GASP (lista dalla verifica del
  fisso, mai nomi a memoria; mancanti = `dati montage`); etichette posa / dati montage / gioco; template e PASS aggiornati.
- `docs/notes/vfx-theory/gstn-vfx-reviewer.SKILL.md`: §3b critico, template e PASS aggiornati.
- Copie aggiornate in project files `anim-skills/`, `vfx-theory/`, `handoffs/critico-estetico.md`.

Scelte applicate (regola 6): critico = subagente Fable high lanciato dal giudice, stesse griglie, senza il verdetto
del giudice; non conta come giro; `medio`/`lieve` aperti riferiti all'owner in una riga.

Aperto:
- La lista delle curve GASP lette dall'ABP non esiste ancora: la produce il controllo chiesto al fisso. Finché manca,
  la review scrive "curve GASP non verificate" (limite, non NO). Il brief della mossa deve citarne il percorso.
- Mai provato in una review vera: il primo uso è il test (prossima review VFX fumo o anim).
- Le skill restano da installare come skill dell'account (proposta del coordinatore).

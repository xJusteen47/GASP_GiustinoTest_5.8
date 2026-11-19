# Handover: skill anim/VFX divise in nucleo generico + strato di progetto (2026-11-19)

Thread cloud, solo documenti. Branch `claude/project-thread-hdoy7s` da `claude/project-thread-5gguxu` (5e2c998f).
Piano eseguito: `docs/notes/review/skills-portabilita.md` passi 1-4. **Nulla pubblicato**; nessun repo creato.

## Fatto
- `skills-public/` (inglese, senza nomi del progetto, percorsi, asset o owner): `README.md`; `anim-reviewer/SKILL.md`
  (+ rubrica di default §9) e `scripts/rv_frames.py` (spostato qui, era in `.claude/skills/gstn-anim-reviewer/scripts/`);
  `anim-executor/SKILL.md`; `vfx-reviewer/SKILL.md` + `vfx-principles.md` (traduzione di `principi-vfx.md`, stessa
  numerazione dei §, fatta da subagente Sonnet); `shared/` contract, aesthetic-critic, reference-dossier, dossier-template.
- Strati di progetto: `.claude/skills/gstn-anim-reviewer|gstn-anim-executor|gstn-vfx-reviewer/` = `SKILL.md` sottile
  (stessa description, "leggi project.md poi il nucleo") + `project.md` (regole GSTN; vince sui conflitti).
- Il revisore VFX ora è skill installata in `.claude/skills/gstn-vfx-reviewer/`; il vecchio
  `docs/notes/vfx-theory/gstn-vfx-reviewer.SKILL.md` è un rimando.
- `principi-vfx.md` ridotto a strato di progetto (lezioni owner, §0, §15, §16-bis, 28 note per sezione) + rimandi.
- `critico-estetico.md`, `dossier-riferimento.md`, `dossier-template.md` ridotti a strato di progetto + rimando al nucleo.
- Controllo: `grep -rniE 'gstn|gasp|giustino|overlord|ganon|owner|_ue59|L_Test|handoffs|docs/notes|Tools/|animpipe|agy|fable|62\.5' skills-public/`
  → 0 righe (escluso `.py`, che non ne contiene).

## Scelte applicate (regola 6)
- `Tools/AnimPipeline/contract/` resta il contratto del progetto (nomi italiani) e sostituisce `shared/contract.md`
  (nomi inglesi); la rubrica del progetto sostituisce la §9 di default. Divergenza voluta, scritta nei `project.md`.
- Strati di progetto anim in inglese (come prima), VFX in italiano (come prima). Gemini resta nominato nelle fonti.

## Aperto
- **Owner**: pubblicazione = OK su repo (nome, pubblico), licenza (es. MIT o CC BY 4.0 per i testi). Non fatto nulla.
- Account skill `gstn-move-animation` non divisa (fuori ambito). Copie in project files `anim-skills/` e `vfx-theory/`
  ora vecchie: il coordinatore le aggiorna dal branch.
- Mai provato in una review reale: la prima review anim o VFX verifica che i due strati si leggano bene insieme.
- Costo stimato: ~3 $ (thread + subagente Sonnet).

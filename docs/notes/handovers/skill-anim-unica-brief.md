# Handover skill-anim-unica-brief (cloud, 2026-11-24, Fable high per scelta dell'owner, branch claude/project-thread-mdss5a da 7zhryd af9ccde2)
OBIETTIVO: conversazione diretta con l'owner su principi e skill; brief della skill di animazione unica (opzione A).
Solo documenti: nessun codice, nessuna skill, nessuna clip toccata.

## Fatto
- `docs/notes/review/skill-anim-unica-brief-2026-11-24.md` (copia in project files `handoffs/`): cause verificate, disegno,
  thread T0-T4 con macchina, modello e budget, ordine di unificazione dei branch.
- `docs/notes/review/proposta-regole-10-17-2026-11-24.md`: testo per le regole 10 e 17 (lo incolla l'owner).
- Decisioni dell'owner 2026-11-24 01:22 Roma, "usiamo le A": 1A tenuta viva normale (difetto = tenuta morta e
  stop-and-go; tetti numerici solo guida), 2A un controllo blocca solo dopo la prova di taratura, 3A prova sul dropkick
  K6 con strati di polish a confronto con K7.

## Prove (ricontrollabili in sola lettura)
- w3: 8 passi in 35 minuti, `git log --format='%h %ad %s' origin/claude/runstart-generic-desktop -- docs/notes/reviews/runstart-w3-c1 docs/notes/reviews/runstart-w3-c2`.
- Check di fase assente: `git show origin/claude/runstart-generic-desktop:Tools/AnimPipeline/cards/runstart/runstart_pivot_gates.py | grep -c -i -E "oppos|phase|fase"` = 0.
- Conflitti di unificazione: `git merge-tree --write-tree --name-only origin/claude/project-thread-7zhryd origin/claude/<ramo>`
  = card.py, checks.py, anim-pipeline.md (sgky92); capture_move.py (bazqb1); i due SKILL.md cheap e executor/project.md
  (runstart-generic-desktop); solo costi.csv per jlb6i3, 3ufz10, 6blrlz.

## Aperto
- Il coordinatore apre T0, T1, T2 (poi T3, T4). L'owner incolla il testo delle regole 10 e 17.
- Non verificato da me: che lo "step" visto dall'owner sia lo stop-and-go da tangenti piatte (mia lettura della diagnosi
  dropkick-k8-3); dove vive la copia di ogni clip approvata (lo fa T0); luce semplice contro non illuminata (lo mostra T3).
- Stima del piano salita da 15-18 $ a ~22-23 $ (MCP delle pose da zero): detta all'owner.
SUBAGENTI: 0
COSTO: ~10 $ (stima) | 0 minuti desktop | 0 token review

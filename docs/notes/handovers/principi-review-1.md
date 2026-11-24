# Handover principi-review-1 (2026-11-24, cloud, Opus medium, branch claude/project-thread-7zhryd da y073op c84edef4)

OBIETTIVO: principi di animazione, note picchiaduro e di Sakurai della OG controllati DAVVERO nella review finale.
Owner 2026-11-24: "sono principi, il reviewer dovrebbe considerarli, non applicarli come un automa".

FATTO
- `Tools/AnimPipeline/contract/indice-ricerca.md`: ricognizione di tutti i documenti di ricerca (repo, storia git, project
  files, skill OG dell'account, KB su m6s9y2): tema, uso prima, principi verificabili, voce PR; feel/camera/VFX separati.
- `Tools/AnimPipeline/contract/principi-review.md`: 20 principi PR-01..20 (fasi, anticipazione, snap, peso, posa d'impatto,
  hitstop Sakurai, liberazione, tenuta con vita, follow-through, archi, settle, recovery/FAF, silhouette, esagerazione,
  articolazioni, colpito/a vuoto, idle, aerei, reazioni, root), ognuno con domande guida e documenti (sezione).
- Metodo §13 per voce: intento, previsione, osservato (frame + vista), scostamento, voluto, giudizio; scostamento voluto e
  motivato = accettato; `non pertinente: motivo` ammesso. Nessuna soglia: le misure restano ai controlli deterministici.
- `gstn_anim_mcp/principi_review.py` (template | check, max 5 righe); `final_review_pack` scrive `final/principi.md`,
  aggiunge picchiaduro + principi ai criteri del critico cieco e gli chiede di riempire il file.
- INV-13 aggiunto a contratto e skill (reviewer, executor, cheap): niente PASS senza `PRINCIPI: OK`.
- Test `tests/test_principi_review.py` (16): fallisce se manca un principio obbligatorio, un documento citato non esiste,
  INV-13 sparisce, o il check accetta un principio non considerato / rifiuta uno scostamento voluto e motivato.

PROVE: `cd Tools/AnimPipeline/tests && python -m pytest -q test_principi_review.py test_review_invariants.py` = 38 passed.
Suite intera (senza test_casc_roundtrip, che manca di file): 11 failed / 206 passed, gli 11 falliscono uguali senza queste
modifiche (cascadeur, verbs, vframes, locks, gstn_anim_mcp: ambiente cloud).

DA FAR CONTROLLARE ALL'OWNER: le 4 voci in fondo a indice-ricerca.md (KB P-* solo su m6s9y2; 5 ricerche solo nei project
files; stile a pose tenute da decidere; hitstop Sakurai forse non compilato).
NON FATTO: aggiornare la skill dell'account e le copie nei project files (al merge); critico-estetico.md non toccato.

SUBAGENTI: 0
COSTO: ~3,5 $ (stima) | 0 minuti desktop | 0 token review

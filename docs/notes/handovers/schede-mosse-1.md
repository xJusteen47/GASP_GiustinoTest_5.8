# Handover: schede mossa + blocco "buona" + skill alla regola 13 (cloud, docs, ~$6 stimato, 2026-11-15)
Branch `claude/project-thread-coq8k5` da `claude/project-thread-rmz1zr` 9d956124. Solo documenti e skill; niente build, niente tag (403).
- `docs/notes/mosse/_modello.md` (modello), `README.md` (ciclo blocco), schede: `overlord-jab1`, `overlord-jab2`, `overlord-dropkick`, `overlord-dair-v9`, `ganondorf-ftilt`.
- `docs/notes/anim-approvate.md` (vuoto, regola 13b). Skill `.claude/skills/gstn-anim-{executor,reviewer}` (da qksijd 730b83c) con sezione 0 "scheda prima".
- Contract copiato da qksijd (README/rubric/instruction/glossary) perché le skill lo citano: unire con qksijd senza conflitti attesi.
## Blocco "buona" (risposta all'owner: sì)
`good/<mossa>-<n>` su commit con card + video di quel giro; la variante successiva è `<mossa>_v<m>.json`; se peggio, `git checkout good/.. -- <card>` (nuovo commit, nulla si cancella). Livello sopra: `approved/` (owner).
Comandi da eseguire dal fisso (tag cloud = 403):
- `git tag -a good/dropkick-1 26733f24^ -m "base Antigravity gambe unite braccia conserte"`
- `git tag -a good/dair-v9-1 <commit con card v9> -m "pose v9 buone"` (proposta, serve OK di revisore/owner)
- `git push origin good/dropkick-1 good/dair-v9-1`
## DA CONFERMARE (raccolti anche dal coordinatore)
1. Jab1: tratto 2 (mano sinistra sul fianco) e pestone 17 cm; chiavi owner bloccate esatte (la card non ha `approved`).
2. Jab2: passo 15-20 cm (owner) contro 30 cm in card; tremore sì/no; "mano all'anca ≥30 cm sotto il jab 1" come regola.
3. Dropkick: terzo tratto d'identità; rialzata veloce o animata; commit della base (`26733f24^` non nel clone corto); valori curve della base r0; FAF.
4. Dair v9: tratto 3 (un rimbalzo e affonda); commit della card v9 per il tag; sezione di atterraggio per gli aerei.
5. Ftilt: l'owner ha approvato la v3.1 (4b)? finestra attiva e FAF (CSV UFD non nel repo); i tre tratti d'identità.
6. Tutte: hitbox/hitstop reali sono nei dati del kit (uasset, illeggibili dal cloud): riportati come "da confermare" in ogni scheda §4.
7. Convenzione CSV (frame da 1) contro card (da 0) dedotta da `anim_active` contro CSV.
Costo: Sonnet, nessun subagente, ~$6 stimati (nessun log).

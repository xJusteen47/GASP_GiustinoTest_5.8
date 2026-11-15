# COMBAT-PLUGIN-DOCS handover (cloud, solo documentazione, 2026-11-14)
Branch `claude/project-thread-0tmywc-docs` da `3556cd0` (tip di 0tmywc). Nessun codice, nessuna build, desktop non usato.
**Fatto**
- `Artifacts/Combattimento/architettura-plugin.md` (+ `.html`): moduli, classi/data asset, flusso colpo -> inbox -> pipeline -> hitstop -> `FGSTN_LayeredMove_Knockback` -> impatto -> Defence F8 (GroundBounce, Knockdown, Getup/Roll/Tech), stati come tag proiettati, reazioni visive (`UGSTN_ReactionAnimSet/Component`), passi per aggiungere mossa/reazione, sezione "ancora solo C++ / specifico del progetto" per renderlo pubblico e solo-BP.
- Artifact pubblicato: https://claude.ai/artifact/CLq2srEECQnd6bKDbBmm72.
- **Convenzione nuova (owner 16:34Z)**: cartella di root `Artifacts/<Area>/` con le copie degli artifact (`Artifacts/README.md` = indice). Copiati: guida mosse di esempio, cose da fare a mano. Aggiornare la copia a ogni cambio artifact. Nome `Artifacts` (non `Docs`) per non scontrarsi con `docs/` su Windows.
**Non verificato**: scritto leggendo header, README e handover; nessun asset aperto in editor. I nomi di asset/tag vanno bene se i README sono aggiornati.
**Per l'owner**: nessun passo manuale; `git fetch origin claude/project-thread-0tmywc-docs` se vuoi la cartella. Non è sul ramo `wkds3r`.
**Costo**: cloud Sonnet, ~100k contesto, nessun subagente; stima ~$3.

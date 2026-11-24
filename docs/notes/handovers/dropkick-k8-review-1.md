# dropkick-k8-review-1 (cloud, 2026-11-23, Opus high) - review regola 10 di K8 su claude/project-thread-6blrlz d36f3a7b
Branch claude/project-thread-690lv0 (= 6blrlz + questa review). Sola lettura, nessun codice toccato.
## Verdetto: FAIL posa (giro 1 di 3). Non va all'owner.
Review completa: docs/notes/reviews/dropkick-kick-k6/k8/review.md.
- Le proposte Doom FUNZIONANO: porta da calcio a1-a10 leggibile da dietro e di lato, chiusura in 2 frame, gomiti fuori dal busto
  a11-a12 (forma a goccia, non piu' palla), testa bassa tra le spalle. Conserte e snap/settle/FAF come K7.
- FAIL posa (grave): a11-a12 gli stinchi cadono in verticale sotto il bacino (az90, run hit e whiff), poi lo snap li porta avanti:
  arco delle gambe invertito, pop prima dello strike, tratto "gambe unite e raccolte" rotto per 2 frame. K7 resta raccolto.
  Causa probabile: palla +16 gradi indietro con chiavi gambe K7 invariate. Fix: chiavi gambe a11-a12 sul bacino ruotato
  (stinchi come K7 +-10 gradi) o togliere il +16 su a11-a13.
- Cattura: la whiff di K8 NON e' una whiff, colpisce il manichino rimasto dalla run hit (H e anello a v158-163). Rifare con
  reset della scena. Da dietro il manichino copre il personaggio da a13 (entrambe le run): per lo strike da dietro serve la whiff.
- Lieve: braccia a0 -> a1 in un frame; porta asimmetrica a7-a9 da dietro.
## Regola 11
Ricontrollati in silhouette.json: palla informe az180 1/16 vs 8/16, area +54.4 %/+56.1 %, gomiti f11 14.5/20.7 cm. Confermati.
## Prossimo
Giro 2 sul portatile: fix gambe a11-a12 + whiff pulita, poi nuova review cloud. Critico separato non usato (budget, regola 3).
SUBAGENTI: 0
COSTO: ~2.5 $ (stima) | 0 minuti desktop | ~70k token review

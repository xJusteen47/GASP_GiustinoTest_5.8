# Cr.L Aerial (dair stomp): Hit Bounce, codice 1 (2026-11-23)

Decisioni owner: 1A (rimbalzo ~150 cm su e ~40 cm indietro in 18 f, libero dopo 24 f, tutto regolabile), 2B (ogni colpo, mai a vuoto o su scudo), 3A (max 2 di fila, a mano; reset a terra o se colpito; al limite la mossa e' normale: fall hold + V10).

## Cosa c'e'
- Core: `Timeline/GSTN_HitBounce.h/.cpp`: traccia timeline "Hit Bounce" (dati: MaxConsecutiveBounces=2, ApexHeightCm=150, RiseFrames=18, BackDistanceCm=40, ActionableFrames=24, bBounceOnBlock=false, BounceMove opzionale) + regole pure (Resolve, CanBounce, IsBouncingHit, ComputeLaunch). Helper `UGSTN_TimelineUtils::SetHitBounceTrack/RemoveHitBounceTracks`.
- NP: `Simulation/GSTN_HitBounceSim.h/.cpp`: fragment di sync (feature id 22, 0 byte quando vuoto) + tick extension (priorita' 140). Arma su colpo del attacker, lancia dopo la fine dell'hitstop con `UGSTN_GE_Knockback` (rule 2), chiude la mossa (nuovo servizio `CancelRunningTimeline`) o avvia BounceMove (futuro backflip), blocca le azioni per ActionableFrames.
- FGSTN_AttackerHit ora porta bBlocked/bParried/bClank.
- Cvar (GSTN Debug, -1 = usa i dati): `gstn.Combat.Debug.HitBounceMax`, `...HitBounceApexCm`, `...HitBounceRiseFrames`, `...HitBounceBackCm`, `...HitBounceActionableFrames`, `...HitBounceOff`. Devono essere uguali su host e client.
- Test: `GSTNCombat.Core.HitBounce.{Resolve,Limit,Launch,FindTrack}`, `GSTNCombat.NP.HitBounce.{Wire,Rules,Run}` (Run: 3 rimbalzi, limite, rollback e resim 104..109, reset a terra, scudo).

## Non verificato
- NIENTE compilato (cloud senza UE). Build + 7 test sul PC.
- Dato sul dair: manca la traccia sull'asset Overlord (finestra attiva ~15..43): da aggiungere con `set_hit_bounce_track` (script editor da scrivere) o a mano nel timeline editor.
- PIE e rete (gasnpp_test.py): arco, hitstop, lock, rollback reale.
- BounceMove: registrato nel table set, ma forse va messo nei TriggeredAbilities del kit per essere concesso.
- Salti aerei non ricaricati dal rimbalzo; drift aereo libero durante il rimbalzo (da confermare con l'owner).

## Indice
- INDICE-PROGETTO: aggiungere voce "Hit Bounce (dair stomp)" sotto Combat -> file sopra, cvar, test.

Branch: `claude/project-thread-3u6rt4`.
SUBAGENTI: 0
COSTO: ~6 $ | 0 | 0

# Atterraggio in auto cancel: proposta senza codice runtime (dropkick-tb-4, portatile, 2026-11-21)

Base: brief `run-start-stop-skid-brief.md` §5 (branch zkycen, owner 2026-11-18 12:56Z): ogni finestra della traccia Landing sceglie da dove
viene la clip (clip della mossa accorciata / landing del salto / clip propria). Stima di allora: ~4 $ di codice (campo + taglio per marker a
runtime). Letto il codice oggi (`GSTN_Landing.h`, `UGSTN_GA_BaseCombat::BeginRecoil`): due delle tre sorgenti esistono gia' come dati.

| Sorgente | Come si fa OGGI | Codice? |
|---|---|---|
| Clip propria | finestra LandingLag con `Lag.RecoilMontage` = il montage scelto | no (esiste) |
| Landing del salto | finestra LandingLag con `RecoilFrames` = N e `RecoilMontage` VUOTO: la mossa finisce, i frame restano bloccati, nessun montage prende lo slot, quindi l'ABP di GASP mostra il suo atterraggio stock | no (da verificare in cattura) |
| Clip della mossa accorciata | NUOVO: un montage generato da script, stessa AnimSequence del landing pesante, DUE segmenti nello slot: [0, fine Urto] e [inizio Rialzo - resto, fine]. La Tenuta viene tagliata nel dato, non a runtime | no: solo uno script editor |

Perche' meglio del campo runtime: nessuno stato nuovo in simulazione (regola 1 di CLAUDE.md), nessun salto di montage a meta' recoil da
rendere rollback-safe, il montage si vede e si scorre nell'editor come gli altri, e lo stock resta stock (regola 14). L'identita' resta: Urto e
Rialzo suonano alla loro velocita', si taglia solo la Tenuta, come chiesto dall'owner. Se il lag e' piu' corto di Urto + Rialzo: frame 1
dell'Urto e Rialzo accorciato dalla testa (stessa regola del brief).

## Lavoro (un thread, Sonnet medium, ~3 $, editor sul fisso o sul portatile)
1. Marker: tre notify `Urto`, `Tenuta`, `Rialzo` sulla clip di landing pesante dello stomp (frame scelti dalla review, non a occhio).
2. `Tools/GAS/landing_lag_cut.py <AM pieno> <lag frames>` -> `AM_<nome>_Lag<N>` con due segmenti; stampa i frame tenuti; rifiuta se mancano i marker.
3. Stomp: finestre 1-3 e 32-42 (lag 4) = `AM_..._Lag4`; 4-31 (lag 16) = clip intera. Cattura dair_land_early / tardo, review Fable.
4. Prova "landing del salto" (RecoilMontage vuoto) su una finestra corta: se l'ABP non rifa' l'atterraggio (base-layer crouch, coda codice
   del 11-17), quella voce torna al codice.

## Decisione per l'owner
A) questa via (script, niente C++) - consigliata. B) il campo runtime del brief (~4 $ + build + test). Resta fermo finche' manca la clip
pesante dello stomp approvata.

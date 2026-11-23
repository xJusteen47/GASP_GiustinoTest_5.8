# Precisione sui weakpoint (piano, 2026-11-09)

Solo progetto, niente codice. Decisioni dell'owner del 2026-11-09, riportate dal coordinatore. Base: `claude/merge-feature-2026-11-09a`
a `642f5165`.

**Principio (owner): niente gioco automatico.** Il gioco non sceglie il weakpoint al posto del giocatore: gli dà **strumenti di
precisione** (scivolata della lama, affondi "profondi", reticolo, sterzata in avvio) e lascia a lui la mira. Ogni strumento è un
dato della mossa o della modalità di gioco, mai un comportamento fisso.

## 0. Cosa esiste già (da riusare, non rifare)

| Pezzo | Dove | Uso qui |
|---|---|---|
| Durezza intera per parte, override per hurtbox (`Hardness`, `HurtboxHardnessOverride`) | `GSTNPartsCore/Data/GSTN_PartDefinition.h` | definisce "superficie dura" (§1) |
| Fasce di penetrazione (`UGSTN_PenetrationTable`, `FGSTN_PenetrationBand`) | `GSTNPartsCore/Data/GSTN_PenetrationTable.h` | soglia "troppo duro per questo attacco" |
| Esito lato attaccante `EGSTN_AttackerHitOutcome::Ricochet` + hook `UGSTN_AttackerOutcome` | `GSTNCombatCore/Hits/GSTN_HitExtensions.h`, `Rules/GSTN_CombatRules.h` | punto d'ingresso della deviazione |
| Rinculo della timeline `FGSTN_RicochetResponse` (`RecoilFrames`, montaggio, movimento, cue) | `GSTNCombatCore/Timeline/GSTN_TimelineTypes.h` | diventa lo stato RECOIL di §1-§2 |
| Politica di mira in avvio `EGSTN_AimPolicy` + `MaxTurnDegrees`, `TurnUntilFrame` | stesso file | base della sterzata (§5) |
| Telecamera e modi camera | `GSTNCamera` | ancoraggio del reticolo (§4) |

Regole di sempre: tutto ciò che decide un colpo gira nella simulazione (deterministico, rollback-safe, niente rami server/risim);
gli scostamenti della lama sono **predetti** dall'attaccante come gli altri colpi; i parametri sono in USTRUCT piccoli sulla timeline.

## 1. Deviazione dello sweep su superficie dura

Quando un fendente colpisce una hurtbox con durezza oltre la fascia dell'attacco (oggi = `Ricochet`), invece di fermarsi subito:
per i **tick attivi rimanenti** la lama **scivola lungo la superficie** cercando una hurtbox morbida o un weakpoint.

- Parametri per mossa (`FGSTN_SweepDeflection`): `MaxDeflectAngleDeg` (quanto la traiettoria può piegarsi), `MaxDeflectFrames`
  (durata massima della scivolata), `bEnabled`.
- Il primo contatto morbido/weakpoint durante la scivolata produce un colpo normale (stessa hit inbox: parata, armatura, parti).
- Due casi, scelti dall'angolo tra la **normale della faccia colpita** e il **piatto della lama**; soglia `gstn.Combat.DeflectFlatAngleDeg`
  (cvar, default 35°, intervallo utile 30-40°):

| Caso | Condizione | Cosa succede |
|---|---|---|
| **A. Faccia contro** | normale opposta al personaggio (angolo col piatto > soglia) | piccolo **impulso di respinta predetto** (queued move, mai stato diretto); lo swing finisce con la scivolata; poi stato **RECOIL** che **sostituisce** il recovery normale |
| **B. Faccia di taglio** | normale quasi parallela al piatto (angolo ≤ soglia) | niente rinculo: il piatto **si riallinea** alla nuova normale (blend della rotazione della mano) e scivola; lo swing finisce normalmente |

- RECOIL = `FGSTN_RicochetResponse` esteso: durata **per mossa** (`RecoilFrames`), animazione fatta dall'owner. La differenza con oggi
  è *quando* parte (dopo lo swing, non subito dopo l'hitstop) e che sostituisce il recovery invece di aggiungersi.
- Il riallineamento del caso B è solo presentazione (layer anim della mano); la geometria della hitbox segue la traiettoria deviata
  già calcolata in simulazione, quindi è identica su tutte le macchine.

## 2. Comportamento per mossa: `HardSurfaceBehaviour`

Enum sulla timeline, `EGSTN_HardSurfaceBehaviour`:

- **FinishThenRecoil**: lo swing completa i tick attivi (con la deviazione di §1), poi RECOIL. **Default dei fendenti.**
- **ImmediateRecoil**: rinculo subito al contatto duro (comportamento attuale del ricochet). **Default degli affondi.**
- Eccezione d'autore: gli affondi con attivi lunghi (attacchi aerei) si impostano **FinishThenRecoil**.

Il default viene dal tipo di mossa (tag/categoria della timeline), l'autore può sovrascriverlo. Il validatore dell'editor segnala
un affondo con attivi > N frame lasciato su ImmediateRecoil (avviso, non errore).

## 3. Hurtbox "profonde" solo per affondi

Per weakpoint interni (nucleo dentro un guscio):

- Flag `bStabOnly` sulla hurtbox (`FGSTN_Hurtbox` / `FGSTN_ExtraHurtbox`) + test d'angolo: il colpo conta solo se la mossa è un
  affondo **e** la direzione della lama è entro `StabMaxAngleDeg` dalla normale d'ingresso (default 30°).
- Regola d'autore: le hurtbox weakpoint stanno **leggermente fuori** dal guscio duro (qualche cm), così uno sweep preciso le tocca
  prima del guscio e la priorità non dipende dall'ordine dei test. Il validatore controlla la sporgenza minima.

## 4. Reticolo su click dello stick destro

- **Niente tasto ADS.** Click dello stick destro = attiva/disattiva il reticolo (comando nuovo nel set `GSTNInput`, tag in
  `Config/Tags/GSTNCommands.ini`).
- Il reticolo dà il **punto di mira** alle mosse con flag `bPrecisionAim` (es. nuovo valore `EGSTN_AimPolicy::Reticle`); le altre mosse
  lo ignorano.
- Inoltre una **freccia di direzione di mira** sul marker del personaggio / a terra (`GSTNDepthMarker`), visibile anche senza reticolo.
- **Per modalità di gioco** (`bAllowPrecisionReticle` nella definizione della modalità): PvE sì, PvP a squadre (TDM) sì, duello no.
  L'owner decide modalità per modalità; il default di una modalità nuova è "no".
- Il punto di mira entra nell'**input predetto** (quantizzato), non viene letto dalla camera dentro la simulazione.

## 5. Sterzata in avvio con lo stick destro

- Durante l'avvio (startup) lo stick destro ruota la direzione della mossa entro `MaxSteerDegrees` per mossa (default ~20°),
  sopra la politica di mira esistente (`MaxTurnDegrees`, `TurnUntilFrame`).
- Va nell'input predetto (stesso pacchetto di §4): deterministico, risimulabile.
- **Giroscopio**: solo a livello input (`GSTNInput`, un dispositivo che produce lo stesso asse dello stick destro), opzionale e spento
  di default. La simulazione non sa da dove arriva l'asse.

## 6. Leve ancora aperte (non decise)

- Scala delle hurtbox weakpoint **1.3-1.5×** (più facili da prendere) contro precisione "pura".
- **Priorità weakpoint sul corpo** quando lo stesso sweep tocca entrambi nello stesso tick.
- **Magnetismo leggero** solo in PvE (piccola correzione della direzione verso un weakpoint visibile). Va contro il principio
  "niente gioco automatico" se forte: se si fa, solo debole e solo PvE.

Proposta: decidere dopo il primo test giocato di §1 + §3, con le stesse mappe di prova.

## 7. Stime e ordine di realizzazione

| # | Lavoro | Stima | Dipende da |
|---|---|---|---|
| 1 | `HardSurfaceBehaviour` + RECOIL che sostituisce il recovery dopo lo swing (riuso `FGSTN_RicochetResponse`) | $2-3 | - |
| 2 | Hurtbox solo-affondo (flag + test d'angolo + validatore sporgenza) | $2 | - |
| 3 | Deviazione dello sweep (scivolata, casi A/B, cvar soglia, impulso predetto) | $6-10 | 1 |
| 4 | Riallineamento del piatto (blend mano, caso B) | $1 | 3, animazione owner |
| 5 | Sterzata in avvio con stick destro (input predetto) | $3-5 | - |
| 6 | Reticolo + freccia di mira + flag per modalità | $4-6 | 5 (stesso campo input) |
| 7 | Giroscopio a livello input | $1-2 | 5 |
| | **Totale** | **~$19-29** | |

Ordine: **1 → 2 → 3 → 4** (la catena del colpo, testabile headless con i test pipeline esistenti), poi **5 → 6 → 7** (input e
presentazione, verifica con l'harness PIE a 2 client). Le leve di §6 dopo il primo test giocato.

Rinviati, si fanno **solo quando servono**: fisica degli oggetti (entita-dinamiche §9) e il ponte con le modalità di movimento
condivise (entita-dinamiche §8). Nessuno dei punti sopra li richiede.

Costo di questo piano: ~$1-2 (solo doc).

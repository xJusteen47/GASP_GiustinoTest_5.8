# Handover grab-link (2026-11-23, cloud, Opus medium)
Brief: project files handoffs/grab-link-brief.md. Branch `claude/grab-link` da main a5b2095. NON COMPILATO (regola 15h).
OBIETTIVO: presa al collo con camminata/salto e lanci N./F./Cr./Cr.F. per l'Overlord su manichino, solo codice, dati e hook.

## Fatto
- Riusato senza riscrivere: GSTNLink F12/M11 K2 (link, mash, watchdog, Release, lanci, LinkEffect, AnchorOverride) e AimPolicy di GSTNCombat (girata istantanea = SnapIfAimOrTilt + MaxTurnDegrees 180 sui lanci F. e Cr.F.).
- Presa al collo: `FGSTN_GrabHoldPose` (settings + override kit), `UGSTN_LinkMath::NeckHoldAnchor`, `FGSTN_LinkTickExtension::StepNeckHold`, neck_01 aggiunto al bake.
- Camminata/salto in presa: `UGSTN_GE_GrabHold` (LinkEffect del Grabber, scritto da setup_defence_link_content.py).
- Modalità `Notation` (nuovo default): stick libero, X = N./F., LT+X = Cr./Cr.F., pummel TOLTO (owner 17:37); tag `Throw.Neutral`, `Throw.CrouchForward`; classi fallback N. e Cr.F.
- Ergonomia `GrabTurnErgonomics` Instant/BackThrow. Put-down su RB (`Command.Link.Release.PutDown`).
- Danno alla liberazione: `FGSTN_GrabEscapeHit` sul kit del BERSAGLIO (spento di default), postato dal watchdog.
- cvar `gstn.Grab.*` nel menu GSTN Debug; test `GSTNLink.Core.KitThrows.NotationPicksThrow`, `GSTNLink.Core.GrabHold.*`; README GSTNLink.

## Input: solo azioni logiche, mai tasti
Gli slot usano i pulsanti logici `Combat.Button.*` (mappati a tastiera/pad dalla config di input) con funzioni di layout rimappabili: N./F. = `Combat.Button.Attack` (`Link.ThrowAttack`, default X), Cr./Cr.F. + `Combat.Button.Crouch` (`Link.Crouch`, default LT), put-down = `Combat.Button.Dodge` (`Link.PutDown`, default RB), presa = `Combat.Button.Interact` (`Link.Grab`, default B). Nessun tasto fisico nel codice; ogni slot si sostituisce anche da `ThrowSlotOverrides`.

## Scelte applicate (regola 6), da portare all'owner
1. Tasto put-down = RB schivata (owner: "credo"). 2. Pummel: DECISO dall'owner 17:37, tolto (righe nascoste in Notation). 3. Default progetto passato a `Notation` (lo Stick lanciava con lo stick e A, incompatibile con camminare e saltare). 4. Punto mano e scale (0.75 camminata, 0.7 salto) = VALORI DI PROVA. 5. Frame dei lanci N./Cr.F. = placeholder copiati da Up/Down.

## Prossimo
Fisso: build + test + setup script + PIE secondo docs/notes/grab-link-verifica-editor.md (subagente Haiku). Harness `kit_throw_stick_*` da adeguare a Notation. Pose: aspettano reference F. throw (elenco animazioni nello stesso doc).

## Prove
Nessun numero misurato: niente build né test eseguiti (cloud senza UE). Conteggio tag 44 = conteggio della lista in GSTN_LinkTags.cpp (script python di conteggio sul file).

SUBAGENTI: 0
COSTO: ~7 $ | 0 min desktop | 0 token review

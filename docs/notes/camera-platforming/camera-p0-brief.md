# Brief Camera P0: spike del plugin GSTNCamera su un personaggio vero
Approvato dall'owner 2026-11-19 17:55Z (1A 2A 3A, cmsg_01CRvUdrWm1dJrM4q54YDBLNLvLd2CFm1X4Ws27ij5eFac). Parte DOPO il ciclo di locomozione in corso (claude/locomotion-finish-3). Sostituisce C0 del vecchio piano.
Macchina: FISSO (editor). Modello: Sonnet medium (giudizio su video a un thread cloud Fable). Budget: 5 $. NON aggiorna _ue59 (lavora in un worktree, PIE locale). Nessun Blueprint condiviso toccato.
Fonte: docs/notes/camera-platforming/design.md (branch claude/project-thread-mu9mh4) §2, §3.1, §5. Non rileggere altri thread.
## Cosa fare
1. Branch nuovo da claude/ue59-sync-1119 (o la testa del ciclo di locomozione). Figlio di prova dell'Overlord `BP_GSTN_CameraP0Test` (nuovo asset, non modificare il BP originale) con `UGSTN_CameraComponent` come camera di vista; modalità di default `Camera.Mode.Free`.
2. Adapter minimo (C++, ≤ 1 file): il soggetto della camera (`IGSTN_CameraSubject::GetPresentedPivot/GetUp`) legge il pivot smorzato di `UGSTN_CameraAnchorComponent` invece della capsula. Air hold dell'anchor spento su questo personaggio (lo fa la modalità Free), tilt-shift resta.
3. Spike C1: chi aggiorna prima, anchor (TG_PostUpdateWork) o `UGSTN_CameraComponent`? Il pivot letto deve essere quello del frame corrente; se no, spostare la lettura nell'update del camera manager. Verifica: `gstn.Camera.Debug 1` + `gstn.Camera.Anchor.Debug 1`, corsa e stop, nessun tremolio.
4. Spike C4: con `ControlRotationPolicy = WriteBack`, input del movimento e attacchi orientati usano la direzione della camera? Prova: corsa in 4 direzioni con camera ruotata di 90°, jab verso lo stick.
5. Chiedi all'assistente AI dell'editor lo stato del plugin Gameplay Cameras in 5.9 (una riga nell'handover).
6. Cattura (script a tempo, 15 min): L_Test_Hub, corsa, salto sul posto, salto su gradino, crouch dentro/fuori, stomp; camera GASP attuale vs P0 (stessi input). Commit mp4 + griglie in docs/notes/reviews/camera-p0/.
## Fine
Handover docs/notes/handovers/camera-p0.md (max 30 righe, regola 11: numeri con log e comando): esito C1/C4, bug trovati, cosa serve per P1 (cloud, Opus high, 12-16 $). Fable guarda i video prima che arrivino all'owner. Max 2 tentativi per problema (regola 5).

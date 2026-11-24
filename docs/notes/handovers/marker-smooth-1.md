# Marker e capsule fluidi (marker-smooth-1, 2026-11-24)
Branch `claude/project-thread-7kdwbe`. Built and tested on the laptop (worktree `GASP_Giustino2_cp1verify`, the desk machine was in use).

## Causes found (code reading, then the captures)
- Depth marker: the Mover adapter read the smoothed root back from the visual mesh (`Base^-1 * Visual`). Other code offsets that mesh after
  Mover's smoothing (hitstop shake and entry drop `UGSTN_ReactionAnimComponent`, body capsule correction `FGSTN_BodyCapsuleModifier::ApplyPresentation`),
  so the marker shook in hitstop and, in the dropkick, followed the drawn body instead of the movement capsule.
- Greybox capsule in hit/launch: shape springs fed by the SIMULATED velocity (16 ms steps, spiky per-frame acceleration) and a hit read as
  locomotion (freeze then knockback in one tick: lean to the cap, run stop kick on the slide, air stretch on an underdamped spring).
- Predicted victim reaction (`UGSTN_HitPresentationComponent`, networked proxies only) added per-frame deltas to a mesh Mover re-places every frame.

## Change
- Mover override: `GetPresentedTransform()`, `GetPresentedVelocity()`, `HasSmoothedPresentation()` (BlueprintPure), kept in
  `FinalizeSmoothingFrame` before any cosmetic offset. The single read for presentation (Mover README table, INDICE 5.12).
- Readers: depth marker Mover adapter, combat debug shapes, GSTN camera rig velocity, greybox capsule velocity.
- Greybox: `bCalmInHitReactions` + `HitReactionSpringDamping` (hitstop/hitstun/tumble: no motion lean, run stop, sway, start squash, air stretch).
- Hit presentation reaction offset survives re-placement. Capture sequence `hit_launch` added.

## Proof
- Build OK (271 s, `-SkipRegress` because of the runstart-v4r5 gate bug, fixed in PR 48). Tests 41/41 PASS (GSTNDepthMarker.Mover/Runtime,
  GSTNCombat.NP.Greybox; new: IgnoresCosmeticMeshOffsets, HitCalm). Logs: laptop `Saved\Marker7kdwbe\`.
- Videos `docs/notes/reviews/marker-smooth-1/`: dropkick v80/v140 az90 shows the dashed line under the torso (movement capsule), not the feet.
  hit_launch (KIT_Example Strong, 150%): dummy upright and calm through hitstop v36-45, but it leaves the frame at v51 and only hops 44 cm:
  the launch itself is NOT seen. Marker stepping not measured frame by frame.

## Open
- Owner check in play: marker while running/jumping, a real launch (tumble) with `gstn.Greybox 1`.
- Nausea in PIE: measurement A/B/C (thread "Capire la nausea") not done, owner was away; needs the desk machine with the owner at the pad.
- Option B of the decision card (one writer for all cosmetic mesh offsets) not done; camera anchor still reads the mesh.

ESITO: PARZIALE compilato e testato, marker nel dropkick verificato a video; lancio greybox e scatto del marker da confermare in gioco
SUBAGENTI: 2 sessioni Remote Control sul laptop (build, test, catture), Opus
COSTO: ~9 $ | 0 min desktop, ~25 min laptop | 3 immagini review

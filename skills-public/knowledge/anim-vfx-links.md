# Where animation and VFX meet (`X-*`)

Animation and VFX are separate disciplines with separate knowledge bases (`animation/`, `vfx/`). They share only a few
points; this file lists them so neither base has to repeat the other. Each item cites both sides.

| ID | Shared point | Animation side | VFX side |
|---|---|---|---|
| X-01 | **The impact frame is one frame.** The pose's contact/extension frame, the effect's peak and the start of hit-pause coincide. | P-SPACE-03, P-GAME-07 | V-IMPACT-01, V-TIME-02 |
| X-02 | **Hit-pause is designed for both.** The body holds the impact pose; each effect layer has a stated behaviour during the pause. | P-GAME-07 | V-IMPACT-02, V-LAYER-04 |
| X-03 | **Anticipation is usually the pose's job.** A charge effect only for slow or charged moves. | P-ANTIC-01..03 | V-TIME-01 |
| X-04 | **Overlap applies to both.** Body parts and effect layers do not start and stop together. | P-OVERLAP-01 | V-TIME-05 |
| X-05 | **Contacts drive ground effects.** Dust and marks are born on the contact frame of the foot/body, at the contact point. | P-CONTACT-01, P-WEIGHT-04 | V-MATTER-02, V-READ-03 |
| X-06 | **The character stays readable.** The effect never hides the silhouette that carries the action. | P-POSE-01, P-GAME-08 | V-INFO-03 |
| X-07 | **Same review captures.** Game camera + side, in-game video; VFX adds a light and a dark background. | P-MEASURE-05 | V-PROC-02, V-READ-07 |
| X-08 | **Strength is timing, not only size.** Heavier = longer hold and decay on both sides. | P-TIME-02 | V-IMPACT-03, V-TIME-04 |
| X-09 | **The impact effect peaks briefly, then moves.** Its peak scale holds only a couple of frames; scale and position keep animating after the hit, following the force. | P-GAME-07 | V-TIME-02, V-SHAPE-07 |
| X-10 | **Presentation shake is its own layer.** Camera or screen shake starts on the contact frame and decays over a few swings; it is neither animation nor VFX and must not hide the impact pose. | P-GAME-07, P-POSE-01 | V-INFO-03, V-LAYER-04 |

Anything not in this table belongs to one side only.

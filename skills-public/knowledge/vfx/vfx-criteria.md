# L1 · VFX criteria (`V-*`)

Level 1: general game-VFX craft, no game or palette. Each criterion is the testable form of a section of
[vfx-theory.md](vfx-theory.md) (the "why" and the sources live there; § numbers below point to it).
Frames at 60 fps counted from 1. Reviews cite `ID + frame + view + background`, e.g. `V-TIME-02 FAIL f3-9 behind, light floor`.

Format: **Rule** · **Measure** (what to look at; indicative numbers) · **Severity** (FAIL / WARN).

## Information (`V-INFO`) — theory §1, §5
| ID | Rule | Measure | Sev. |
|---|---|---|---|
| V-INFO-01 | The effect says what happened, where and how strong before it is pretty. | Freeze the peak frame: can you say the event, its point and its strength? | FAIL |
| V-INFO-02 | Attention is proportional to gameplay importance. | Rank of the effect's on-screen weight vs the event's importance among the effects of the scene. | FAIL if a minor event out-shouts a major one |
| V-INFO-03 | The character stays readable under the effect. | On active frames, the body and striking limb silhouette is still visible from the game camera. | FAIL |
| V-INFO-04 | Honest reach: the effect never shows a different size or reach than the gameplay volume. | Overlay the hit volume on the peak frame. | FAIL |
| V-INFO-05 | Frame 1 after the event already shows a change. | Diff event frame vs next. | FAIL for hits |

## Shape (`V-SHAPE`) — theory §2
| ID | Rule | Measure | Sev. |
|---|---|---|---|
| V-SHAPE-01 | Shape follows energy: sudden = pointed/radiating, gentle = round/soft. | Shape family at the peak vs the cause. | WARN |
| V-SHAPE-02 | One primary (crisp, high value), secondaries subordinate, tertiary atmosphere. | Count elements at full value/opacity: one focal point. | FAIL if two compete |
| V-SHAPE-03 | Big/medium/small scale contrast; no uniform noise. | Size histogram of elements at the peak. | WARN |
| V-SHAPE-04 | No perfect symmetry, twins or regular sawtooth unless a deliberate style. | Look for mirrored or evenly spaced pieces. | WARN |
| V-SHAPE-05 | Directional: elongated along motion, crisp leading edge, soft tail; oriented to the hit axis or the impact plane. | Orientation of the main shape vs the action. | WARN |
| V-SHAPE-06 | Few and large beats many and small. | Element count vs readability at game distance. | WARN |
| V-SHAPE-07 | Effects squash and stretch; they do not only scale uniformly. | Aspect ratio over the first frames. | WARN |

## Timing (`V-TIME`) — theory §3
| ID | Rule | Measure | Sev. |
|---|---|---|---|
| V-TIME-01 | Three phases: anticipation (often given by the pose), climax, dissipation. | Size/opacity/brightness envelopes over frames (`figures/D-vfx-envelope.png`). | FAIL if no clear peak |
| V-TIME-02 | Climax in the first quarter of the life; hits open at maximum on frame 1-2. | Frame of peak brightness/size ÷ life length ≤ 0.25. | FAIL for hits |
| V-TIME-03 | No linear curves: ease-out for what is pushed, ease-in only for what is sucked away. | Spacing of size/position over frames. | FAIL if constant |
| V-TIME-04 | "If it feels long, it is too long." Indicative: flash 1-2 f, hit 12-20 f, dust 15-27 f, landing ring 15-21 f, debris < 1 s. | Life length in frames. | WARN |
| V-TIME-05 | Elements overlap and offset; they do not start and end together. | Birth/death frames of each layer. | WARN (FAIL if all synchronous) |
| V-TIME-06 | Timing is locked with grey shapes before polish. | Process: a grey-box version exists. | process |
| V-TIME-07 | Death according to matter: energy/solids erode or contract; gases disperse, growing while fading. | Last third of life: size trend and opacity trend. | FAIL for gas that shrinks while opaque (`figures/D-dust-growth.png`) |

## Layers (`V-LAYER`) — theory §4
| ID | Rule | Measure | Sev. |
|---|---|---|---|
| V-LAYER-01 | Composite of layers with distinct roles (flash, primary, debris, smoke, ground mark, feel). | List layers and their role. | WARN |
| V-LAYER-02 | Only one layer is light and crisp; others subordinate. | Value per layer at the peak. | FAIL if equal |
| V-LAYER-03 | Additive alone is not enough; needs its own contrast. | Check on a light background. | FAIL if it vanishes |
| V-LAYER-04 | Phases are separable so pauses (hit-pause) can stretch one without breaking others. | Can the dissipation be delayed independently? | WARN |

## Readability and value (`V-READ`) — theory §5, §6
| ID | Rule | Measure | Sev. |
|---|---|---|---|
| V-READ-01 | Minimum on-screen size at the game camera. | Pixel size of the primary at gameplay distance. | FAIL if unreadable |
| V-READ-02 | Reads from every view the game shows; flat cards face the camera or have volume. | Captures from game camera and side. | FAIL |
| V-READ-03 | Depth cue on the ground for airborne events (shadow, ring, light). | Is the event's ground position readable? | WARN |
| V-READ-04 | Grey-scale test: the event's core is the highest value contrast of the frame. | Desaturated peak frame. | FAIL |
| V-READ-05 | Body at mid values; maximum only in the core. | Value histogram of the effect. | WARN |
| V-READ-06 | Saturation above the world's, below the UI's. | Compare with the scene. | WARN |
| V-READ-07 | Verified on a light and on a dark/saturated background. | Two captures. | FAIL if missing |
| V-READ-08 | Matter takes the scene's light; energy emits it. | Is smoke lit? does the flash light the floor? | WARN |

## Matter (`V-MATTER`) — theory §7, §8
| ID | Rule | Measure | Sev. |
|---|---|---|---|
| V-MATTER-01 | Three families, three treatments: energy, gas, solids. | Classify each layer; check its treatment. | WARN |
| V-MATTER-02 | Gas behaves like gas: fast burst, slowing expansion, rises or settles by weight, dissipates by thinning. | Size and opacity curves; motion direction. | FAIL if it behaves like a solid or a sticker |
| V-MATTER-03 | Gas is coloured by what produced it and lit by the scene. | Colour vs surface; shading. | WARN |
| V-MATTER-04 | Solids bounce, roll and stop with physical plausibility. | Trajectories of debris. | WARN |

## Impact (`V-IMPACT`) — theory §9
| ID | Rule | Measure | Sev. |
|---|---|---|---|
| V-IMPACT-01 | The effect's peak frame is the hit frame of the move. | Frame of peak vs frame of contact. | FAIL |
| V-IMPACT-02 | Behaviour during hit-pause is a decision, not an accident: energy may keep going, matter may freeze or slow. | What the effect does during the pause. | WARN |
| V-IMPACT-03 | Strength scales with size, duration and hit-pause together, not size alone. | Compare light vs strong hit. | WARN |

## Technique and cost (`V-TECH`) — theory §10, §11
| ID | Rule | Measure | Sev. |
|---|---|---|---|
| V-TECH-01 | The technique fits the shape (flipbook, sprite, mesh, ribbon, light, decal). | Edge-on views, stretching artefacts. | WARN |
| V-TECH-02 | Overdraw and particle count fit the lowest target hardware. | Profiler capture on the lowest target. | FAIL over budget |

## Process (`V-PROC`) — theory §12-14
| ID | Rule | Sev. |
|---|---|---|
| V-PROC-01 | Effects are designed with the move, hooked to its key frames, not added after. | process |
| V-PROC-02 | Review from in-game video, game camera and side, light and dark background. | FAIL if missing |
| V-PROC-03 | Max three gaps per review round, gravest first, each with the smallest fix. | process |

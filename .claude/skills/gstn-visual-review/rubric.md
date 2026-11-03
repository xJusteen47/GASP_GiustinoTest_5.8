# Rubric (one item per owner rule in Tools/AnimPipeline/cards/README.md, plus what only the eye sees)

`regola` in the verdict = the id in the first column. "Blocca" says when the item is `gravita: blocca`; otherwise `nota`.
Numbers already checked by the gate (bone lengths, clearance, hitstop_snap, co_motion, contact/FAF, locks,
source_fidelity, strike, rules) are not judged again: the eye looks for what the numbers miss.

| id | Where to look | What is wrong | Blocca |
|---|---|---|---|
| frame_0 | side grid f0 vs idle | f0 looks like the idle, or is already the charge | f0 indistinguishable from idle |
| troppo_giusto | key poses, game camera | owner's extreme softened, pose timid compared to his ghost | the pose is visibly smaller than the owner ghost |
| sinergia | side grid, startup | a limb stays glued while trunk/legs travel (arms parked on the chest) | a limb still for 3+ frames while the body moves |
| ginocchia_avanti | side grid charge | knees swing back behind the pelvis during the charge | always |
| carica_viva | side grid, charge hold | a long charge frozen like a statue | no: nota |
| rilascio | hitstop strip + trails | release not slow, or the largest step is not ON the first active frame | biggest step not on the first active frame |
| niente_scatto | hitstop strip, trails after active | head or a limb pops after an active frame (gap in the dots) | a visible jump in a trail after an active frame |
| tenuta | side grid, strike | strike pose not held long (stomp/hold poses) | held fewer frames than the card's active + hold |
| mano_villain | key poses, overlord kit | hands not open-flat, fingers apart, wrist not bent outward, thumb not along the index | on overlord cards: always |
| pugni | key poses, other kits | fists not closed (default) | always, unless the card says open |
| gomito_dentro | back view | back elbow sticking out of the torso silhouette | no: nota |
| compenetrazione | all panels | forearm in the head, arm through the chest, foot through the thigh | always (clearance may have an owner exemption: then nota) |
| piedi | back view | feet not side by side when the pose does not ask it; crossed ankles or frog knees not in the owner pose | when it differs from the owner ghost |
| silhouette | key poses, game camera | the move does not read as itself (a kick read as a punch, an arm hidden behind the body) | the striking part is not recognizable from the game camera |
| parte_che_colpisce | game camera, active | the hitting part is covered or not leading | always |
| intermedio_manca | side grid + trails | a jump between two frames: dots far apart with no in-between, shape changes in one frame | over 10 cm in a frame outside the active window (segment `manca`) |
| intermedio_brutto | side grid between keys | in-between inside the body, knee bending backwards, limbs collapsing to mush | always (segment `brutto`) |
| jitter | trails | dots zig-zag or wobble on non-key frames | a visible back-and-forth on 2+ consecutive frames |
| fantasma_owner | grey ghost on owner keys | the coloured pose does not cover the owner ghost | never correggi: `chiedi_owner` if over ~1 cm visible |

Rules for corrections
- Correct only Claude in-betweens (`origin: claude` in the manifest) and parts not in `approved`. On an owner key or a
  locked part write the problem and set `esito: chiedi_owner`.
- One `correzione` per problem, the smallest that fixes it; never propose new keys on the owner's frames.

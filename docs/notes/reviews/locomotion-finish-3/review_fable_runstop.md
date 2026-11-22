FAIL (close) - run stop V3 + small dust (capture rs3/mv_run_stop, v172-259; reviewer Fable, rule 10)
Sources: 3 grids, 8 single frames (az90 v191/206/217/223/230/240, az45 v206, game v206), frames.jsonl. Clip f0 = v186, 1:1.

Identity traits
- Trait 1 (front foot planted, facing forward, no turn): OK. Heel plant v190-191, no yaw, front foot down until v226.
- Trait 2 (torso rises only after the feet stop): OK. Feet stop v217; head +1.7 cm, pelvis +1 cm in v210-217 (not visible),
  the real rise is v218-238 (head +15.5 cm, 29 cm back).
- Trait 3 (arms/head later): WEAK. Arms hold wide to v217 (good), but then close together with the trunk and everything
  ends on the same frame (hand L v237, head/pelvis v238, hand R v239). No delay, no overshoot.

Fixed since V2 (verified)
- Wide low stance for the whole slide: feet 88 cm apart at v195, 75 cm at v216; pelvis -11/-12 cm under idle v197-217.
- Back foot drags on its toes (heel up, z constant) to v217, steps in only v218-226. Front foot steps back v227-235.
- Rise and exit are now in the capture. No pop into the idle (largest step after v238 is 0.3 cm per frame).
- Dust: ankle/shin high, about 60 cm long, no stars. Feet and shins readable in side, az45 and game view (v206).

posa
1. v196-216: the brake is a frozen pose sliding (hands, head, pelvis change under 1.5 cm in 20 frames). Good silhouette,
   but no life: no sink into the front knee as he slows, no forward push of the chest when the feet stop at v217.
   Reads as a statue on ice, not Ganondorf braking his weight. Cartoony needs the hold to breathe.
2. v218-238: the whole body (two steps of 8 frames each, trunk, arms, head) lands together at v238 and then v239-248 are
   10 identical frames. Hard mechanical stop, no settle. This is the trait 3 defect above.
3. v223 / v230: narrow, slightly hunched, knees bent while two quick 50 cm steps shuffle the feet under him. Readable,
   but light and busy for a medium-heavy character; the front foot step back (51 cm) is longer than it needs to be.

dati montage
4. Clip ends with the left foot 33 cm ahead of the right; the game idle wants about 10 cm. So v249-279 both planted feet
   creep on the floor (L -9 cm, R +9 cm, still moving at v279). Slow, but visible in the side view.
5. v200: actor +5 cm and mesh -5 cm on the same frame (world pose continuous, not visible). Same as V2 item 5.

gioco
6. v185-186: two repeated frames (actor step 0, pose frozen) right before the stop starts at v187. Was one frame in V2.
   Looks like a hitch on the stop/dust start. Check if it is the capture or the first spawn of the dust system.
7. Input delay not verifiable: frames.jsonl has no stick field. The run is at full speed (9.8 cm/frame) to v184 and the
   clip starts at v186. If the stick was really released at v160-165, that is 20+ frames late and a FAIL on its own;
   if it was v180-181, it is fine. Log the stick value per frame.
8. Dust: thin wisps still around the feet at v246-258 (40 frames after the stop). Small, but should be gone by about v240.
   Still a flat pale blob with little light shaping (v3 sheet wants lit smoke). Minor.

Next fix (one): in run_stop_gen.py do an overlap pass on the stop and rise. At STOP the chest and head push 4-6 cm
forward/down for 3-4 frames and recoil; in the rise the arms and head run 5-6 frames after the trunk with a small
overshoot and settle; end the clip on the game idle foot stance (shorter front foot step back) and cut the frozen tail.

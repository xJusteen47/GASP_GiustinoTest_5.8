# character-kits-1: desktop run (2026-11-25/26)

Run in the owner's GASP_Giustino2 checkout (where his local DA_Locomotion_Default tuning lives; backup in its Saved/Backup).

- `setup_run_stage_steps_dump_before.txt`: STEPS_MODE=dump of the owner's Default before anything (stage 1 600/2000, 180 f; stage 2 610/1400, 94 f; stage 3 850/1200; no bursts).
- `setup_character_kits.txt`: the kits script report (3 kits copied from KIT_Test_Playtest, 3 profiles copied from the Default, Voyager values written).
- The readable-steps write (STEPS_ANIM_SET=0) has NOT run yet: the agent's permission check refused to write the owner's local Default without his OK typed in the session. The profiles committed here therefore have no entry bursts except the Voyager's own (stage 2 3 f x6, stage 3 2 f x8).

## PIE check (kit_stage3: 20 idle frames, stick forward, jump at 350, release at 400; greybox, gstn.Loco.Base 2, gstn.Skid.Sim 0)

Videos `kit_stage3_<Kit>.mp4` = side (az90) | game camera, 60 fps. `collars_f150.png` = Playtest, Voyager, BirdKnight, FlameFury.

- Kits applied: collars Playtest pink, Voyager pink (close to Playtest's hue), BirdKnight orange, FlameFury cyan; no greatsword on any. All four jump (~305 cm).
- Speed from frames.jsonl, in simulation frames since the stick (62.5 Hz):

| kit | 600 cm/s at | stage 2 at | stage 3 at |
|---|---|---|---|
| Playtest | 49 f (0.78 s) | 224 f | not reached before the jump (see stalls) |
| Voyager | 39 f (0.62 s) | 55 f | 151 f |
| BirdKnight | 46 f (0.74 s) | 224 f | 318 f |
| FlameFury | 49 f (0.78 s) | 224 f | 318 f |

Voyager reaches 600 cm/s ~0.16 s before the others and stage 3 ~2.7 s sooner. Every run has 1-3 stalls of ~10-28 rendered frames where the simulation does not advance (same counters, same position): a capture hitch, not input (yaw and stick constant). The owner touched the pad around 00:03 UTC during this capture; nothing in the logs shows stray input.

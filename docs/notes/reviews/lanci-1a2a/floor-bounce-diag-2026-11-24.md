# Floor bounces with 0 horizontal speed: raw diagnosis (desktop 2026-11-24, ~17:08-17:20 UTC)

Branch `claude/project-thread-kyph4p`, build f2cad3d9 (binaries 16:57 UTC). No fix. Capture `lanci_measure_boot.py`, KIT_Test_Medium strong,
player victim (`LANCI_VICTIM=player`), `log LogGSTNKnockbackMove VeryVerbose`, `log LogGSTNMoveMix VeryVerbose`, `LANCI_ROWS=1` (new: one
row per rendered frame in `lanci_wall.jsonl` also without a wall: the victim's sim frame (combat historyFrame), mode, velocity, z,
GetImpactInfo, GetImpactDebug, GetSurfaceLaunchInfo, read from the server snapshot after the frame). Server victim = `C_1` (logged per
world in the wall run; same join order in the no-wall run). Captures `Saved/Lanci/floor_nowall`, `Saved/Lanci/floor_wall`; UE logs
`Saved/Logs/ci/cap_floor_nowall.log.ue.log`, `cap_floor_wall.log.ue.log` (not in git).

GetImpactDebug's last field lists each resolved contact as `r<resolve frame>>c<contact frame>:k<kind>:s<speed>:t<type>` (k1 = the wall
contacts, k2 = the floor ones). GetImpactInfo = `frame,outcome,type,speed,damage` of the latest impact the victim step resolved.

## Answers

Three floor bounces were measured. Only the first one keeps horizontal speed:

| run | contact (c) | decision (r) | horizontal on contact frame / decision frame (`have`) | impact speed | bounce want | GetImpactInfo updated? |
|---|---|---|---|---|---|---|
| no wall 100 %, 1st floor | 1245 | 1246 | 2806 (Falling) -> 2803 (Walking substep) / have (2803 0 0) | 1555 | **(1682 0 933)** | yes: `1246,2,4,1555` |
| no wall 100 %, 2nd floor | 1276 | 1277 | 1538 (Falling) -> 1538 (Walking substep) / have (1538 0 0) | 949 | **(0 0 435)** | no (stays `1246,...`) |
| wall 700 at 100 %, floor after the 2nd wall rebound | 1259 | 1260 | 2925 (Falling, -1500 cap) -> 2925 (Walking) / have (2925 0 0) | 1500 | **(0 0 485)** | no (stays `1230,...`, the wall) |

- **(a)** contact = the frame the victim goes Falling -> Walking (a second, partial substep of that frame logs `mode Walking`, z 0);
  decision = the next frame (the new knockback generates with `elapsed 0 mode Walking`).
- **(b)** On both frames the victim still has its full horizontal speed and vertical 0 (the Walking substep zeroed z):
  2803 / 1538 / 2925 cm/s. The impact step's Tangent is not exposed by any log or getter; the velocity it can read on the decision
  frame is `have` = (H 0 0).
- **(c)** The bounce move's want: (1682 0 933) for the first, (0 0 435) and (0 0 485) for the other two: **the horizontal 0 is already in
  the bounce's want**, it is not lost in the mix.
- **(d)** Horizontal drops to 0 on the decision frame itself (1277, 1260), in mode Walking: `[MoveMix] mix 1 momentum 0 out (0 0 435)` /
  `out (0 0 485)`, the velocity override of the bounce move. Next frame Falling, `mix 0 momentum 1`, horizontal stays 0.

Pattern (not verified in code): the bounce that keeps horizontal is the one GetImpactInfo records (the victim impact step's outcome
bounce, vz 933 = 0.6 x 1555, horizontal 0.6 x 2803). The two vertical-only ones do not update GetImpactInfo although GetImpactDebug lists
them as resolved (`r1277>c1276:k2:s949`, `r1260>c1259:k2:s1500`), and their vz is not 0.6 x the speed (435 / 949, 485 / 1500): they
look like a different path. Both happen while the active knockback move is itself a surface relaunch (the first floor bounce's move,
the wall rebound's move), the kept one while the hit's own launch is active. Speed is not the split (1500 vs 1555).

Also seen (logging): the partial Walking substep on the contact frame logs nonsense `elapsed` values (54, 207, 176, 8, 23: e.g. frame
1276 `base 20413.6 start 19920.0 elapsed 207`), where the full-frame lines read 29-30. Probably elapsed computed from the substep's
fractional time; harmless in these runs, worth knowing if anything reads elapsed on a mode-change substep.

120 % into the wall 700: three wall contacts (13253, 7822, then 4463 on frame 1903 while already on the floor) and a floor contact at
750 cm/s (`r1905>c1904:k2:s750`) that does not bounce: the victim slides to a stop (-140 -> 0 in 6 frames). No floor bounce after the
wall rebound to compare. 120 % without wall: same pattern as 100 % (1st floor 1867->1868 flies (1937 0 901) one frame after; 2nd floor 1899->1900 flies
(0 0 253)).

## Raw: no wall, 100 %, 1st floor bounce (contact 1245, decision 1246; frames 1241-1252)
```
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1241 base 19840.0 start 19456.0 elapsed 24 mode Falling balloon 66 want (2868 0 1656) have (2830 0 -1290)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1241 mode Falling mix 0 momentum 1 out (2825 0 -1371)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1242 base 19856.0 start 19456.0 elapsed 25 mode Falling balloon 66 want (2868 0 1656) have (2825 0 -1371)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1242 mode Falling mix 0 momentum 1 out (2820 0 -1452)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1243 base 19872.0 start 19456.0 elapsed 26 mode Falling balloon 66 want (2868 0 1656) have (2820 0 -1452)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1243 mode Falling mix 0 momentum 1 out (2815 0 -1518)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1244 base 19888.0 start 19456.0 elapsed 27 mode Falling balloon 66 want (2868 0 1656) have (2815 0 -1518)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1244 mode Falling mix 0 momentum 1 out (2810 0 -1537)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1245 base 19904.0 start 19456.0 elapsed 28 mode Falling balloon 66 want (2868 0 1656) have (2810 0 -1537)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1245 mode Falling mix 0 momentum 1 out (2806 0 -1555)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1245 base 19911.5 start 19456.0 elapsed 54 mode Walking balloon 66 want (2868 0 1656) have (2806 0 0)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1245 mode Walking mix 0 momentum 0 out (2803 0 0)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1246 base 19920.0 start 19456.0 elapsed 29 mode Walking balloon 66 want (2868 0 1656) have (2803 0 0)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1246 base 19920.0 start 19920.0 elapsed 0 mode Walking balloon 0 want (1682 0 933) have (2803 0 0)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1246 mode Walking mix 1 momentum 0 out (1682 0 933)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1247 base 19936.0 start 19920.0 elapsed 1 mode Falling balloon 0 want (1682 0 933) have (1682 0 933)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1247 mode Falling mix 0 momentum 1 out (1677 0 870)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1248 base 19952.0 start 19920.0 elapsed 2 mode Falling balloon 0 want (1682 0 933) have (1677 0 870)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1248 mode Falling mix 0 momentum 1 out (1672 0 808)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1249 base 19968.0 start 19920.0 elapsed 3 mode Falling balloon 0 want (1682 0 933) have (1672 0 808)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1249 mode Falling mix 0 momentum 1 out (1668 0 745)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1250 base 19984.0 start 19920.0 elapsed 4 mode Falling balloon 0 want (1682 0 933) have (1668 0 745)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1250 mode Falling mix 0 momentum 1 out (1663 0 682)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1251 base 20000.0 start 19920.0 elapsed 5 mode Falling balloon 0 want (1682 0 933) have (1663 0 682)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1251 mode Falling mix 0 momentum 1 out (1658 0 619)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1252 base 20016.0 start 19920.0 elapsed 6 mode Falling balloon 0 want (1682 0 933) have (1658 0 619)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1252 mode Falling mix 0 momentum 1 out (1653 0 557)

frame 1241 mode Falling vel [2825, 0, -1371] z 170.3 impact -1,0,0,0,0
frame 1242 mode Falling vel [2820, 0, -1452] z 147.1 impact -1,0,0,0,0
frame 1243 mode Falling vel [2815, 0, -1518] z 122.8 impact -1,0,0,0,0
frame 1244 mode Falling vel [2810, 0, -1537] z 98.2 impact -1,0,0,0,0
frame 1245 mode Walking vel [2803, 0, 0] z 88.1 impact -1,0,0,0,0
frame 1246 mode Falling vel [1682, 0, 933] z 103.1 impact 1246,2,4,1555,0
frame 1247 mode Falling vel [1677, 0, 870] z 117.0 impact 1246,2,4,1555,0
frame 1248 mode Falling vel [1672, 0, 808] z 129.9 impact 1246,2,4,1555,0
frame 1250 mode Falling vel [1663, 0, 682] z 152.8 impact 1246,2,4,1555,0
frame 1251 mode Falling vel [1658, 0, 619] z 162.7 impact 1246,2,4,1555,0
frame 1252 mode Falling vel [1653, 0, 557] z 171.6 impact 1246,2,4,1555,0
```
## Raw: no wall, 100 %, 2nd floor bounce (contact 1276, decision 1277; frames 1272-1283)
```
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1272 base 20336.0 start 19920.0 elapsed 26 mode Falling balloon 0 want (1682 0 933) have (1562 0 -635)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1272 mode Falling mix 0 momentum 1 out (1557 0 -698)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1273 base 20352.0 start 19920.0 elapsed 27 mode Falling balloon 0 want (1682 0 933) have (1557 0 -698)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1273 mode Falling mix 0 momentum 1 out (1552 0 -760)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1274 base 20368.0 start 19920.0 elapsed 28 mode Falling balloon 0 want (1682 0 933) have (1552 0 -760)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1274 mode Falling mix 0 momentum 1 out (1548 0 -823)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1275 base 20384.0 start 19920.0 elapsed 29 mode Falling balloon 0 want (1682 0 933) have (1548 0 -823)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1275 mode Falling mix 0 momentum 1 out (1543 0 -886)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1276 base 20400.0 start 19920.0 elapsed 30 mode Falling balloon 0 want (1682 0 933) have (1543 0 -886)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1276 mode Falling mix 0 momentum 1 out (1538 0 -949)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1276 base 20413.6 start 19920.0 elapsed 207 mode Walking balloon 0 want (1682 0 933) have (1538 0 0)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1276 mode Walking mix 0 momentum 0 out (1538 0 0)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1277 base 20416.0 start 19920.0 elapsed 31 mode Walking balloon 0 want (1682 0 933) have (1538 0 0)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1277 base 20416.0 start 20416.0 elapsed 0 mode Walking balloon 0 want (0 0 435) have (1538 0 0)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1277 mode Walking mix 1 momentum 0 out (0 0 435)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1278 base 20432.0 start 20416.0 elapsed 1 mode Falling balloon 0 want (0 0 435) have (0 0 435)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1278 mode Falling mix 0 momentum 1 out (0 0 247)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1279 base 20448.0 start 20416.0 elapsed 2 mode Falling balloon 0 want (0 0 435) have (0 0 247)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1279 mode Falling mix 0 momentum 1 out (0 0 59)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1280 base 20464.0 start 20416.0 elapsed 3 mode Falling balloon 0 want (0 0 435) have (0 0 59)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1280 mode Falling mix 0 momentum 1 out (0 0 -129)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1281 base 20480.0 start 20416.0 elapsed 4 mode Falling balloon 0 want (0 0 435) have (0 0 -129)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1281 mode Falling mix 0 momentum 1 out (0 0 -318)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1282 base 20496.0 start 20416.0 elapsed 5 mode Falling balloon 0 want (0 0 435) have (0 0 -318)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1282 mode Falling mix 0 momentum 1 out (0 0 -506)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1282 base 20507.9 start 20416.0 elapsed 23 mode Walking balloon 0 want (0 0 435) have (0 0 0)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1282 mode Walking mix 0 momentum 0 out (0 0 -32)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1283 base 20512.0 start 20416.0 elapsed 6 mode Walking balloon 0 want (0 0 435) have (0 0 -32)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1283 mode Walking mix 0 momentum 0 out (0 0 -62)

frame 1272 mode Falling vel [1557, 0, -698] z 139.0 impact 1246,2,4,1555,0
frame 1273 mode Falling vel [1552, 0, -760] z 126.8 impact 1246,2,4,1555,0
frame 1275 mode Falling vel [1543, 0, -886] z 99.5 impact 1246,2,4,1555,0
frame 1276 mode Walking vel [1538, 0, 0] z 88.1 impact 1246,2,4,1555,0
frame 1277 mode Falling vel [0, 0, 435] z 95.1 impact 1246,2,4,1555,0
frame 1278 mode Falling vel [0, 0, 247] z 99.1 impact 1246,2,4,1555,0
frame 1279 mode Falling vel [0, 0, 59] z 100.0 impact 1246,2,4,1555,0
frame 1280 mode Falling vel [0, 0, -129] z 97.9 impact 1246,2,4,1555,0
frame 1281 mode Falling vel [0, 0, -318] z 92.8 impact 1246,2,4,1555,0
frame 1282 mode Walking vel [0, 0, -32] z 88.0 impact 1246,2,4,1555,0
frame 1283 mode Walking vel [0, 0, -62] z 88.1 impact 1246,2,4,1555,0
```
## Raw: wall 700, 100 %, floor bounce after the wall rebounds (contact 1259, decision 1260; frames 1255-1266)
```
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1255 base 20064.0 start 19664.0 elapsed 25 mode Falling balloon 0 want (3064 0 -68) have (2949 0 -1500)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1255 mode Falling mix 0 momentum 1 out (2944 0 -1500)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1256 base 20080.0 start 19664.0 elapsed 26 mode Falling balloon 0 want (3064 0 -68) have (2944 0 -1500)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1256 mode Falling mix 0 momentum 1 out (2939 0 -1500)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1257 base 20096.0 start 19664.0 elapsed 27 mode Falling balloon 0 want (3064 0 -68) have (2939 0 -1500)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1257 mode Falling mix 0 momentum 1 out (2935 0 -1500)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1258 base 20112.0 start 19664.0 elapsed 28 mode Falling balloon 0 want (3064 0 -68) have (2935 0 -1500)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1258 mode Falling mix 0 momentum 1 out (2930 0 -1500)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1259 base 20128.0 start 19664.0 elapsed 29 mode Falling balloon 0 want (3064 0 -68) have (2930 0 -1500)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1259 mode Falling mix 0 momentum 1 out (2925 0 -1500)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1259 base 20141.3 start 19664.0 elapsed 176 mode Walking balloon 0 want (3064 0 -68) have (2925 0 0)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1259 mode Walking mix 0 momentum 0 out (2925 0 0)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1260 base 20144.0 start 19664.0 elapsed 30 mode Walking balloon 0 want (3064 0 -68) have (2925 0 0)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1260 base 20144.0 start 20144.0 elapsed 0 mode Walking balloon 0 want (0 0 485) have (2925 0 0)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1260 mode Walking mix 1 momentum 0 out (0 0 485)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1261 base 20160.0 start 20144.0 elapsed 1 mode Falling balloon 0 want (0 0 485) have (0 0 485)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1261 mode Falling mix 0 momentum 1 out (0 0 297)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1262 base 20176.0 start 20144.0 elapsed 2 mode Falling balloon 0 want (0 0 485) have (0 0 297)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1262 mode Falling mix 0 momentum 1 out (0 0 109)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1263 base 20192.0 start 20144.0 elapsed 3 mode Falling balloon 0 want (0 0 485) have (0 0 109)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1263 mode Falling mix 0 momentum 1 out (0 0 -80)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1264 base 20208.0 start 20144.0 elapsed 4 mode Falling balloon 0 want (0 0 485) have (0 0 -80)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1264 mode Falling mix 0 momentum 1 out (0 0 -268)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1265 base 20224.0 start 20144.0 elapsed 5 mode Falling balloon 0 want (0 0 485) have (0 0 -268)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1265 mode Falling mix 0 momentum 1 out (0 0 -456)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1266 base 20240.0 start 20144.0 elapsed 6 mode Falling balloon 0 want (0 0 485) have (0 0 -456)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1266 mode Falling mix 0 momentum 1 out (0 0 -644)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1266 base 20243.9 start 20144.0 elapsed 8 mode Walking balloon 0 want (0 0 485) have (0 0 0)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1266 mode Walking mix 0 momentum 0 out (0 0 -95)

frame 1255 mode Falling vel [2944, 0, -1500] z 178.4 impact 1230,2,4,5107,0
frame 1256 mode Falling vel [2939, 0, -1500] z 154.4 impact 1230,2,4,5107,0
frame 1257 mode Falling vel [2935, 0, -1500] z 130.4 impact 1230,2,4,5107,0
frame 1258 mode Falling vel [2930, 0, -1500] z 106.4 impact 1230,2,4,5107,0
frame 1259 mode Walking vel [2925, 0, 0] z 88.1 impact 1230,2,4,5107,0
frame 1260 mode Falling vel [0, 0, 485] z 95.9 impact 1230,2,4,5107,0
frame 1261 mode Falling vel [0, 0, 297] z 100.7 impact 1230,2,4,5107,0
frame 1262 mode Falling vel [0, 0, 109] z 102.4 impact 1230,2,4,5107,0
frame 1263 mode Falling vel [0, 0, -80] z 101.1 impact 1230,2,4,5107,0
frame 1264 mode Falling vel [0, 0, -268] z 96.8 impact 1230,2,4,5107,0
frame 1265 mode Falling vel [0, 0, -456] z 89.5 impact 1230,2,4,5107,0
frame 1266 mode Walking vel [0, 0, -95] z 88.1 impact 1230,2,4,5107,0
```

---

# Check of 077b9c83 (missed-tech bounce keeps GroundBounceMomentumKeep 0.4 + GroundBounceStickSpeed 400), ~18:05-18:25 UTC

**Build** OK (189 s). **Tests** GSTNDefence + GSTNCombat.NP.Hits + Core.Reaction: first run 203/204, the new
`GSTNDefence.Core.Landing.GroundBounceMomentum` failed 6 of its 8 checks: `0.4f` is 0.4000000059..., so 2800 x keep = 1120.0000167, outside
the test's `Equals(..., 1e-6)`. Fixed in the test only (tolerance 1e-3, e578f9e5, no behaviour change): **204/204 PASS**
(GSTNDefence 84, NP.Hits 89, Core.Reaction 31).

**PIE, no wall, player victim, strong at 100 %** (`Saved/Lanci/floor_mom`, `floor_stick_fwd`, `floor_stick_back`; new `LANCI_STICK=x,y,from,to`:
the victim's own client holds its move stick, camera relative, from rendered frame 88 = after the first floor bounce, to 200):

| run | 1st floor bounce want | have before the 2nd floor bounce | **2nd floor bounce want** | slide after the 2nd bounce |
|---|---|---|---|---|
| neutral stick | (1682 0 933) | (1538 0 0) | **(615 0 435)** = 0.4 x 1538, vz unchanged | 1.05 m (x 3008 -> 3113), then still |
| stick (0, 1) = toward the launch | (1682 0 933) | (1886 0 0) | **(1372 0 377)** | 2.51 m |
| stick (0, -1) = against it | (1682 0 933) | (994 0 0) | **(-220 0 377)** | 0.32 m back |

- Neutral: exactly what was asked (was (0 0 435)).
- The stick already acts before the second landing: held from the first bounce's flight it changes the landing speed (1886 / 994 against
  1538: the air steer of the falling mode). So the back run lands with less to keep, the forward one with more.
- With the stick the numbers do not match keep x have + 400 along the stick (forward: 0.4 x 1886 + 400 = 1154, measured 1372; back:
  0.4 x 994 - 400 = -2, measured -220), and the vertical is 377 instead of 435 in both. Not looked into (out of time): possibly the
  landing velocity the step reads is not the logged `have`, or the stick's existing angle steer (GroundBounceSteerDeg) also tilts it.
  Direction and sign are right: forward pushes on (2.5 m), back holds back and even reverses (-220).

Raw, neutral, 2nd floor bounce:
```
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1264 base 20208.0 start 19744.0 elapsed 29 mode Falling balloon 0 want (1682 0 933) have (1548 0 -823)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1264 mode Falling mix 0 momentum 1 out (1543 0 -886)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1265 base 20224.0 start 19744.0 elapsed 30 mode Falling balloon 0 want (1682 0 933) have (1543 0 -886)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1265 mode Falling mix 0 momentum 1 out (1538 0 -949)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1265 base 20237.6 start 19744.0 elapsed 207 mode Walking balloon 0 want (1682 0 933) have (1538 0 0)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1265 mode Walking mix 0 momentum 0 out (1538 0 0)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1266 base 20240.0 start 19744.0 elapsed 31 mode Walking balloon 0 want (1682 0 933) have (1538 0 0)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1266 base 20240.0 start 20240.0 elapsed 0 mode Walking balloon 0 want (615 0 435) have (1538 0 0)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1266 mode Walking mix 1 momentum 0 out (615 0 435)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1267 base 20256.0 start 20240.0 elapsed 1 mode Falling balloon 0 want (615 0 435) have (615 0 435)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1267 mode Falling mix 0 momentum 1 out (610 0 247)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1268 base 20272.0 start 20240.0 elapsed 2 mode Falling balloon 0 want (615 0 435) have (610 0 247)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1268 mode Falling mix 0 momentum 1 out (606 0 59)
```
Raw, stick forward and back, the bounce frames:
```
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1278 base 20432.0 start 20432.0 elapsed 0 mode Walking balloon 0 want (1372 0 377) have (1886 0 0)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1276 base 20400.0 start 20400.0 elapsed 0 mode Walking balloon 0 want (-220 0 377) have (994 0 0)
```

ESITO: dal 2° rimbalzo a terra (0 0 435) si passa a (615 0 435) = 0.4 x 1538, scivola 1 m; lo stick avanti spinge (1372, 2.5 m), indietro trattiene (-220); i valori con lo stick non tornano con la formula semplice (da guardare). Test: corretta la tolleranza di GroundBounceMomentum, 204/204.
SUBAGENTI: nessuno.
COSTO: ~50 min desktop in tutto per le due diagnosi a terra (4 catture PIE, 2 build, 2 run di test), niente _ue59.

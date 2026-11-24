# Wall rebound lost at 100 %: raw diagnosis logs (desktop 2026-11-24, ~16:42 UTC)

Branch `claude/project-thread-kyph4p` at a032a027 (VeryVerbose logs only), built OK. Capture: `lanci_measure_boot.py`, KIT_Test_Medium strong,
`LANCI_VICTIM=player LANCI_WALL=700`, sequences lk_strong_p40 and lk_strong_p100, `log LogGSTNKnockbackMove VeryVerbose`,
`log LogGSTNMoveMix VeryVerbose`. UE log: `Saved/Logs/ci/cap_diag_wall.log.ue.log` (not in git); captures `Saved/Lanci/diag_wall`.
Victim actor per world (logged by the capture): server `BP_GASNPP_TestCharacter_C_1`, its owning client `C_0`, the other client `C_2`.
Only `[KnockbackGen]` C_0 and C_1 lines exist; below are the server's (C_1) lines, unedited apart from the timestamp prefix.
Impacts the victim resolved on the server (GetImpactInfo frame,outcome,type,speed): 40 % `1226,2,4,1466` (bounce); 100 % `1827,2,4,8672`
(bounce), `1835,2,4,3748` (bounce), `1851,2,4,1456` (floor).

## Answers
- **Does the rebound move ever generate with elapsed 0? Yes.** 100 %: frame 1827, `start 29216 elapsed 0 balloon 0 want (-5203 0 1203)`.
- **Combined move on that frame:** `mix 1 momentum 0 out (7809 0 1289)`: an OverrideVelocity whose value is the ORIGINAL launch's
  (balloon still running: `elapsed 5 balloon 66`, its outputs 11879 -> 10696 -> 9631 -> 8672 -> **7809** -> 7031 ... are the `out` series),
  not the rebound's (-5203 0 1203).
- **'have' on the next frame (1828): (0 0 1289)**: the launch's +X push is stopped by the wall, the z is the launch's 1289. The `out`
  stays the launch's balloon decay (7031, 6331, 5701, 5133, 4622, 4162, 3748, 3374, 3038, 2868) through frame 1837 with have X = 0-1;
  the second rebound (frame 1835, elapsed 0, want (-2249 0 -328)) is lost the same way. From frame 1838 the mix is 0 (momentum 1,
  out (0 0 -703)): no override any more, the rebound moves are still alive (elapsed 11 and 3) but no longer override.
- **40 % for comparison:** rebound at frame 1226 elapsed 0 want (-880 0 -529); that frame `mix 1 out (-880 0 -529)` and next frame have
  (-880 0 -529): it works. There the original launch (elapsed 23, balloon 36) was no longer overriding (frames 1224-1225 already `mix 0
  momentum 1`), so the rebound was the only override.
- **First launch frames at 100 %:** frame 1822 the move generates `elapsed 0 mode Walking`, combined `mix 1 out (0 0 0)`; frame 1823
  `elapsed 1 mode Falling`, `out (11879 0 5664)`, and the next have is that value. (`have (0 0 0)` on 1822 and 1823 is the start velocity
  of those frames.)

Reading (not verified in code): with two velocity-override layered moves alive, the mix keeps the old launch's velocity while its
balloon override lasts; the newer rebound does not replace it. At 40 % the old move had already stopped overriding.

## Raw: 100 %, first launch (frames 1820-1825)
```
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1820 mode Walking mix 2 momentum 0 out (0 0 0)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1821 mode Walking mix 2 momentum 0 out (0 0 0)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1822 base 29136.0 start 29136.0 elapsed 0 mode Walking balloon 66 want (2868 0 1656) have (0 0 0)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1822 mode Walking mix 1 momentum 0 out (0 0 0)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1823 base 29152.0 start 29136.0 elapsed 1 mode Falling balloon 66 want (2868 0 1656) have (0 0 0)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1823 mode Falling mix 1 momentum 0 out (11879 0 5664)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1824 base 29168.0 start 29136.0 elapsed 2 mode Falling balloon 66 want (2868 0 1656) have (11879 0 5664)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1824 mode Falling mix 1 momentum 0 out (10696 0 4131)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1825 base 29184.0 start 29136.0 elapsed 3 mode Falling balloon 66 want (2868 0 1656) have (10696 0 4131)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1825 mode Falling mix 1 momentum 0 out (9631 0 2934)
```
## Raw: 100 %, wall impact (bounce decided on 1827; frames 1825-1832)
```
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1825 base 29184.0 start 29136.0 elapsed 3 mode Falling balloon 66 want (2868 0 1656) have (10696 0 4131)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1825 mode Falling mix 1 momentum 0 out (9631 0 2934)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1826 base 29200.0 start 29136.0 elapsed 4 mode Falling balloon 66 want (2868 0 1656) have (9631 0 2934)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1826 mode Falling mix 1 momentum 0 out (8672 0 2005)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1827 base 29216.0 start 29136.0 elapsed 5 mode Falling balloon 66 want (2868 0 1656) have (3661 0 2005)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1827 base 29216.0 start 29216.0 elapsed 0 mode Falling balloon 0 want (-5203 0 1203) have (3661 0 2005)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1827 mode Falling mix 1 momentum 0 out (7809 0 1289)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1828 base 29232.0 start 29136.0 elapsed 6 mode Falling balloon 66 want (2868 0 1656) have (0 0 1289)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1828 base 29232.0 start 29216.0 elapsed 1 mode Falling balloon 0 want (-5203 0 1203) have (0 0 1289)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1828 mode Falling mix 1 momentum 0 out (7031 0 742)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1829 base 29248.0 start 29136.0 elapsed 7 mode Falling balloon 66 want (2868 0 1656) have (0 0 742)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1829 base 29248.0 start 29216.0 elapsed 2 mode Falling balloon 0 want (-5203 0 1203) have (0 0 742)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1829 mode Falling mix 1 momentum 0 out (6331 0 329)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1830 base 29264.0 start 29136.0 elapsed 8 mode Falling balloon 66 want (2868 0 1656) have (0 0 329)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1830 base 29264.0 start 29216.0 elapsed 3 mode Falling balloon 0 want (-5203 0 1203) have (0 0 329)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1830 mode Falling mix 1 momentum 0 out (5701 0 21)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1831 base 29280.0 start 29136.0 elapsed 9 mode Falling balloon 66 want (2868 0 1656) have (1 0 21)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1831 base 29280.0 start 29216.0 elapsed 4 mode Falling balloon 0 want (-5203 0 1203) have (1 0 21)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1831 mode Falling mix 1 momentum 0 out (5133 0 -204)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1832 base 29296.0 start 29136.0 elapsed 10 mode Falling balloon 66 want (2868 0 1656) have (1 0 -204)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1832 base 29296.0 start 29216.0 elapsed 5 mode Falling balloon 0 want (-5203 0 1203) have (1 0 -204)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1832 mode Falling mix 1 momentum 0 out (4622 0 -365)
```
## Raw: 100 %, second wall bounce (decided on 1835; frames 1833-1840)
```
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1833 base 29312.0 start 29136.0 elapsed 11 mode Falling balloon 66 want (2868 0 1656) have (1 0 -365)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1833 base 29312.0 start 29216.0 elapsed 6 mode Falling balloon 0 want (-5203 0 1203) have (1 0 -365)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1833 mode Falling mix 1 momentum 0 out (4162 0 -475)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1834 base 29328.0 start 29136.0 elapsed 12 mode Falling balloon 66 want (2868 0 1656) have (1 0 -475)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1834 base 29328.0 start 29216.0 elapsed 7 mode Falling balloon 0 want (-5203 0 1203) have (1 0 -475)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1834 mode Falling mix 1 momentum 0 out (3748 0 -547)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1835 base 29344.0 start 29136.0 elapsed 13 mode Falling balloon 66 want (2868 0 1656) have (1 0 -547)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1835 base 29344.0 start 29216.0 elapsed 8 mode Falling balloon 0 want (-5203 0 1203) have (1 0 -547)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1835 base 29344.0 start 29344.0 elapsed 0 mode Falling balloon 0 want (-2249 0 -328) have (1 0 -547)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1835 mode Falling mix 1 momentum 0 out (3374 0 -589)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1836 base 29360.0 start 29136.0 elapsed 14 mode Falling balloon 66 want (2868 0 1656) have (1 0 -589)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1836 base 29360.0 start 29216.0 elapsed 9 mode Falling balloon 0 want (-5203 0 1203) have (1 0 -589)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1836 base 29360.0 start 29344.0 elapsed 1 mode Falling balloon 0 want (-2249 0 -328) have (1 0 -589)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1836 mode Falling mix 1 momentum 0 out (3038 0 -608)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1837 base 29376.0 start 29136.0 elapsed 15 mode Falling balloon 66 want (2868 0 1656) have (1 0 -608)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1837 base 29376.0 start 29216.0 elapsed 10 mode Falling balloon 0 want (-5203 0 1203) have (1 0 -608)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1837 base 29376.0 start 29344.0 elapsed 2 mode Falling balloon 0 want (-2249 0 -328) have (1 0 -608)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1837 mode Falling mix 1 momentum 0 out (2868 0 -641)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1838 base 29392.0 start 29136.0 elapsed 16 mode Falling balloon 66 want (2868 0 1656) have (0 0 -641)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1838 base 29392.0 start 29216.0 elapsed 11 mode Falling balloon 0 want (-5203 0 1203) have (0 0 -641)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1838 base 29392.0 start 29344.0 elapsed 3 mode Falling balloon 0 want (-2249 0 -328) have (0 0 -641)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1838 mode Falling mix 0 momentum 1 out (0 0 -703)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1839 base 29408.0 start 29136.0 elapsed 17 mode Falling balloon 66 want (2868 0 1656) have (0 0 -703)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1839 base 29408.0 start 29216.0 elapsed 12 mode Falling balloon 0 want (-5203 0 1203) have (0 0 -703)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1839 base 29408.0 start 29344.0 elapsed 4 mode Falling balloon 0 want (-2249 0 -328) have (0 0 -703)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1839 mode Falling mix 0 momentum 1 out (0 0 -766)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1840 base 29424.0 start 29136.0 elapsed 18 mode Falling balloon 66 want (2868 0 1656) have (0 0 -766)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1840 base 29424.0 start 29216.0 elapsed 13 mode Falling balloon 0 want (-5203 0 1203) have (0 0 -766)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1840 base 29424.0 start 29344.0 elapsed 5 mode Falling balloon 0 want (-2249 0 -328) have (0 0 -766)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1840 mode Falling mix 0 momentum 1 out (0 0 -829)
```
## Raw: 40 %, wall impact (bounce decided on 1226; frames 1224-1231)
```
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1224 base 19568.0 start 19232.0 elapsed 21 mode Falling balloon 36 want (1560 0 900) have (1502 0 -719)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1224 mode Falling mix 0 momentum 1 out (1498 0 -800)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1225 base 19584.0 start 19232.0 elapsed 22 mode Falling balloon 36 want (1560 0 900) have (1498 0 -800)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1225 mode Falling mix 0 momentum 1 out (1493 0 -881)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1226 base 19600.0 start 19232.0 elapsed 23 mode Falling balloon 36 want (1560 0 900) have (923 0 -881)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1226 base 19600.0 start 19600.0 elapsed 0 mode Falling balloon 0 want (-880 0 -529) have (923 0 -881)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1226 mode Falling mix 1 momentum 0 out (-880 0 -529)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1227 base 19616.0 start 19232.0 elapsed 24 mode Falling balloon 36 want (1560 0 900) have (-880 0 -529)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1227 base 19616.0 start 19600.0 elapsed 1 mode Falling balloon 0 want (-880 0 -529) have (-880 0 -529)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1227 mode Falling mix 0 momentum 1 out (-875 0 -591)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1228 base 19632.0 start 19232.0 elapsed 25 mode Falling balloon 36 want (1560 0 900) have (-875 0 -591)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1228 base 19632.0 start 19600.0 elapsed 2 mode Falling balloon 0 want (-880 0 -529) have (-875 0 -591)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1228 mode Falling mix 0 momentum 1 out (-870 0 -654)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1228 base 19633.7 start 19232.0 elapsed 28 mode Walking balloon 36 want (1560 0 900) have (-870 0 0)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1228 base 19633.7 start 19600.0 elapsed 2 mode Walking balloon 0 want (-880 0 -529) have (-870 0 0)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1228 mode Walking mix 0 momentum 0 out (-860 0 0)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1229 base 19648.0 start 19232.0 elapsed 26 mode Walking balloon 36 want (1560 0 900) have (-860 0 0)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1229 base 19648.0 start 19600.0 elapsed 3 mode Walking balloon 0 want (-880 0 -529) have (-860 0 0)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1229 base 19648.0 start 19648.0 elapsed 0 mode Walking balloon 0 want (0 0 368) have (-860 0 0)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1229 mode Walking mix 1 momentum 0 out (0 0 368)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1230 base 19664.0 start 19232.0 elapsed 27 mode Falling balloon 36 want (1560 0 900) have (0 0 368)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1230 base 19664.0 start 19600.0 elapsed 4 mode Falling balloon 0 want (-880 0 -529) have (0 0 368)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1230 base 19664.0 start 19648.0 elapsed 1 mode Falling balloon 0 want (0 0 368) have (0 0 368)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1230 mode Falling mix 0 momentum 1 out (0 0 161)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1231 base 19680.0 start 19232.0 elapsed 28 mode Falling balloon 36 want (1560 0 900) have (0 0 161)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1231 base 19680.0 start 19600.0 elapsed 5 mode Falling balloon 0 want (-880 0 -529) have (0 0 161)
[KnockbackGen] BP_GASNPP_TestCharacter_C_1 frame 1231 base 19680.0 start 19648.0 elapsed 2 mode Falling balloon 0 want (0 0 368) have (0 0 161)
[MoveMix] BP_GASNPP_TestCharacter_C_1 frame 1231 mode Falling mix 0 momentum 1 out (0 0 -45)
```

ESITO: il rimbalzo genera con elapsed 0 ma il mix tiene la velocita del lancio originale (balloon ancora attivo); a 40 % funziona perché il lancio non sovrascrive più.
SUBAGENTI: nessuno.
COSTO: ~10 min desktop (build 42 s, 1 cattura PIE).

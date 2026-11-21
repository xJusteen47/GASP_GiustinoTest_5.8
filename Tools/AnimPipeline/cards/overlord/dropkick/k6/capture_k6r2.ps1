# Dropkick K6 ROUND 2 captures (2026-11-21): views az90 (side), az180 (behind, owner rule 11-21 19:03), az315, game; high press
# with the cameras at 350 cm so head and feet stay in frame (review k6a, capture note). Asserts reuse round 1's specs (same montage).
# Round 1 header:
# Dropkick K6 captures (weight + strike settle) (copy of capture_r2.ps1; high press with the side cameras at 160 cm for the head-and-arms close-up).
# Round 2 header follows.
# Dropkick K5 pose round 2 captures (2026-11-21, laptop, checkout GASP_Giustino2_n216qs). Three Play sessions, each under Run-Timed
# (Capture, 15 min). Side cameras at 250 cm (was 380: the reviewer wants the character large), dummy 190 cm ahead (was 150: at f15 the
# shins went through it; capture setup only, the hitbox is the owner's).
#   k5r2w / k5r2hit  KIT_Test_PlaytestTBK6, press falling at 480 cm: whiff (ASSERT with assert_k5.json) and hit in SEPARATE Play sessions
#                    (in one session the hit's dummy stays where it was put and stands in front of the side camera in the whiff)
#   k5r2high  KIT_Test_PlaytestTBK6, press at 700 cm: whiff only, so the smug fall loop plays before the landing
#   baser2    KIT_Test_PlaytestTBR4 (the base dropkick), press at 480 cm, same cameras: whiff, for the frame-by-frame comparison
# Output: Saved/DropkickK6/<name>/ (frames, frames.jsonl, assert.txt); Run-Timed logs in Saved/Logs/ci/k5r2_<name>.log
param([string[]]$Only = @('k6r2w', 'k6r2hit', 'k6r2high'))
$ErrorActionPreference = 'Continue'
$P = 'C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_n216qs'
Set-Location $P
$env:PIE_CAPTURE_AIR_GIVE_UP = '200'
$env:PIE_CAPTURE_DROPKICK_LOW_DIST = '190'
$env:PIE_CAPTURE_DIST = '250'
$env:PIE_CAPTURE_VIEWS = 'az90,az180,az315,game'
$runs = @(
    @{ n = 'k6r2w'; d = '250'; kit = 'KIT_Test_PlaytestTBK6'; h = '480'; seq = 'dropkick_sweet_whiff'; assert = "$P\Saved\DropkickK6\assert_k6_w.json" },
    @{ n = 'k6r2hit'; d = '250'; kit = 'KIT_Test_PlaytestTBK6'; h = '480'; seq = 'dropkick_sweet_hit'; assert = "$P\Saved\DropkickK6\assert_k6_hit.json" },
    @{ n = 'k6r2high'; d = '350'; kit = 'KIT_Test_PlaytestTBK6'; h = '700'; seq = 'dropkick_sweet_whiff'; assert = "$P\Saved\DropkickK6\assert_k6_high.json" }
)
foreach ($r in ($runs | Where-Object { $Only -contains $_.n })) {
    $out = "$P\Saved\DropkickK6\$($r.n)"
    $env:PIE_CAPTURE_KIT = $r.kit; $env:PIE_CAPTURE_OUT = $out; $env:PIE_CAPTURE_SEQUENCES = $r.seq
    $env:PIE_CAPTURE_DIST = $r.d
    $env:PIE_CAPTURE_DROPKICK_SWEET_H = $r.h; $env:PIE_CAPTURE_ASSERT = $r.assert
    & "$P\Tools\CI\Run-Timed.ps1" -Preset Capture -Script Tools\GASNPPTest\pie_capture_move.py -Log "Saved\Logs\ci\$($r.n).log" -AssertFile "$out\assert.txt"
    "$($r.n): exit $LASTEXITCODE"
}

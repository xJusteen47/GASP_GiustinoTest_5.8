# capture-speed (owner 2026-11-21): several run start variants in ONE Play session (pie_capture_move.py PIE_CAPTURE_RUNS), instead of one
# editor launch per variant (runstart_generic_captures.ps1, ~139 s each of which ~100 s is boot/Play/quit). Needs the _ue59 editor closed.
# usage: runstart_batch_captures.ps1 -SavedDir CaptureSpeed -Runs 'v24_h1|24|AS_GSTN_RunStart_Generic_w2fs|mv_run_start_v5|97,140|gstn.Loco.TorsoLean 0', ...
#   run = name | gstn.RunStart.Variant | clip of the ASSERT | sequence | ASSERT window | extra cvars (';' separated, optional)
#   Every run sets Variant, Handoff and the extra cvars; give every extra cvar a value in EVERY run (nothing is undone between runs).
# Output: Saved\<SavedDir>\<batch>\<run>\<sequence>\<view>\f*.png + assert.txt per run, timing.txt (seconds per step), 1 line per run.
param([string[]]$Runs, [string]$SavedDir = 'CaptureSpeed', [string]$Batch = 'batch', [int]$Handoff = 1,
      [string]$Views = 'az180,az90,az0,game', [string]$Shoot = '70,210', [int]$Size = 480)   # frames shot (A/B and evidence use v77-v197)
$proj = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
$cap = Join-Path $proj 'Tools\GASNPPTest\pie_capture_timed.ps1'
$dir = Join-Path $proj "Saved\$SavedDir"
$out = Join-Path $dir $Batch
New-Item -ItemType Directory -Force $out | Out-Null
$list = @()
foreach ($r in $Runs) {
    $p = $r.Split('|')
    $cv = "gstn.RunStart.Variant $($p[1]);gstn.RunStart.Handoff $Handoff" + $(if ($p.Count -gt 5 -and $p[5]) { ";$($p[5])" } else { '' })
    $list += [ordered]@{ name = $p[0]; cvars = $cv; sequences = @($p[3])
                         assert = @{ $p[3] = @(@{ clip = $p[2]; window = @($p[4].Split(',') | ForEach-Object { [int]$_ }); min_weight = 0.8; coverage = 0.9; additive = @('BS_Relaxed_*Lean*') }) } }
}
$runsJson = Join-Path $out 'runs.json'
[IO.File]::WriteAllText($runsJson, (ConvertTo-Json -InputObject $list -Depth 8))
$env:PIE_CAPTURE_RUNS = $runsJson
$env:PIE_CAPTURE_CVARS = 'gstn.Loco.Base 1'
$env:PIE_CAPTURE_VIEWS = $Views
$env:PIE_CAPTURE_CLIENTS = '0'   # one character: 1 PIE world (speed-up.md 4)
$env:PIE_CAPTURE_ASSERT_REPLACE = '1'
$env:PIE_CAPTURE_SHOOT = $Shoot
$env:PIE_CAPTURE_SIZE = "$Size"
Remove-Item Env:PIE_CAPTURE_ASSERT -ErrorAction SilentlyContinue
$t0 = Get-Date
& $cap -Kit KIT_Test_Overlord -Out $out -MaxMinutes (15 + 2 * $Runs.Count) | Out-File (Join-Path $out 'console.txt')
$rc = $LASTEXITCODE
$t1 = Get-Date
Remove-Item Env:PIE_CAPTURE_RUNS, Env:PIE_CAPTURE_VIEWS, Env:PIE_CAPTURE_CLIENTS, Env:PIE_CAPTURE_SHOOT, Env:PIE_CAPTURE_SIZE -ErrorAction SilentlyContinue
$a = if (Test-Path (Join-Path $out 'assert.txt')) { Get-Content (Join-Path $out 'assert.txt') -TotalCount 1 } else { 'ASSERT: MANCANTE' }
"batch $Batch rc=$rc $a editor=$([int]($t1 - $t0).TotalSeconds)s runs=$($Runs.Count)"
foreach ($r in $list) {
    $ra = Join-Path $out "$($r.name)\assert.txt"
    "$($r.name) $(if (Test-Path $ra) { Get-Content $ra -TotalCount 1 } else { 'ASSERT: MANCANTE' })"
}
# per-step times from the driver log (driver-relative seconds) + the editor wall time
$log = Join-Path $out 'capture.log'
$lines = @("editor_wall_s $([int]($t1 - $t0).TotalSeconds)")
if (Test-Path $log) { $lines += Get-Content $log | Select-String -Pattern 'setup_editor|timing |^\s*[\d.]+s (run |sequence .* (pawn at|done)|finish|ASSERT: )' | ForEach-Object { $_.Line.Substring(0, [Math]::Min(140, $_.Line.Length)) } }
$lines | Out-File (Join-Path $out 'timing.txt')

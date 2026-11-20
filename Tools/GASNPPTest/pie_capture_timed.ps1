# One Play capture of a kit under Run-Timed (rule 5, capture limit 15 min). Output in -Out (frames, frames.jsonl, capture.log, done.txt, run.log).
# usage: pie_capture_timed.ps1 -Kit KIT_Test_OverlordDAirV9 -Out Saved\DairV10\base -Sequences dair_whiff,dair_hit [-Button Attack] [-Assert asserts.json]
# -Assert (project rule 10): {sequence: [{"clip", "window": [first, last]}]} checked at the end (Tools/AnimPipeline/capture_assert.py);
# Run-Timed prints the ASSERT line and exits 3 when the capture ran but the expected clip did not own the window. Always on: the
# sequences' own 'expect' entries; with neither, assert.txt says ASSERT: FAIL (no expectation = no review).
param([string]$Kit, [string]$Out, [string[]]$Sequences = @(), [string]$Button = 'Attack', [double]$MaxMinutes = 15, [string]$Assert)
$proj = Split-Path (Split-Path $PSScriptRoot)
$outAbs = if ([IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $proj $Out }
New-Item -ItemType Directory -Force $outAbs | Out-Null
Remove-Item (Join-Path $outAbs 'done.txt') -ErrorAction SilentlyContinue
$env:PIE_CAPTURE_KIT = $Kit
$env:PIE_CAPTURE_OUT = $outAbs -replace '\\', '/'
$env:PIE_CAPTURE_BUTTON = $Button
if ($Assert) { $env:PIE_CAPTURE_ASSERT = (Resolve-Path $Assert).Path } else { Remove-Item Env:PIE_CAPTURE_ASSERT -ErrorAction SilentlyContinue }
if ($Sequences) { $env:PIE_CAPTURE_SEQUENCES = ($Sequences -join ',') } else { Remove-Item Env:PIE_CAPTURE_SEQUENCES -ErrorAction SilentlyContinue }
# -ExecCmds "py <path>" breaks on the space in "Unreal Projects": a one-line boot in %TEMP% execs the real one
$boot = Join-Path $env:TEMP 'gstn_pie_capture_boot.py'
$real = (Join-Path $PSScriptRoot 'pie_capture_boot_vd2.py') -replace '\\', '/'
[IO.File]::WriteAllText($boot, "__file__ = r'$real'`nexec(open(__file__).read())`n", (New-Object Text.UTF8Encoding $false))
& (Join-Path $PSScriptRoot '..\CI\Run-Timed.ps1') -Preset Capture -Log (Join-Path $outAbs 'run.log') -Script $boot -MaxMinutes $MaxMinutes -AssertFile (Join-Path $outAbs 'assert.txt')
$done = Join-Path $outAbs 'done.txt'
if (Test-Path $done) { "done: " + (Get-Content $done -TotalCount 1) } else { 'done.txt missing' }

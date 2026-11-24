# getup-momentum-1 (2026-11-21): the knockdown get-up variants in ONE Play session under Run-Timed (rule 5), behind (az180) and side (az90).
# usage: getup_capture_timed.ps1 -Runs Saved\Getup\runs_new.json -Out Saved\Getup\new [-MaxMinutes 25]
# The runs file: pie_capture_react_ingame.py PIE_CAPTURE_GETUP_RUNS. Output in -Out (<run>/<view>/f###.png, frames.jsonl, capture.log,
# done.txt, assert.txt, run.log). Prints at most 5 lines; exit 3 = the capture ran but ASSERT is not OK.
# -MemoryData: the new get-up data (FAF, tail, exit speeds) set in memory before Play, nothing saved (PIE_CAPTURE_GETUP_SETUP)
param([string]$Runs, [string]$Out, [double]$MaxMinutes = 25, [switch]$MemoryData)
if ($MemoryData) { $env:PIE_CAPTURE_GETUP_SETUP = '1' } else { Remove-Item Env:PIE_CAPTURE_GETUP_SETUP -ErrorAction SilentlyContinue }
$proj = Split-Path (Split-Path $PSScriptRoot)
$outAbs = if ([IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $proj $Out }
$runsAbs = if ([IO.Path]::IsPathRooted($Runs)) { $Runs } else { Join-Path $proj $Runs }
New-Item -ItemType Directory -Force $outAbs | Out-Null
Remove-Item (Join-Path $outAbs 'done.txt'), (Join-Path $outAbs 'assert.txt') -ErrorAction SilentlyContinue
$env:PIE_CAPTURE_OUT = $outAbs -replace '\\', '/'
$env:PIE_CAPTURE_GETUP_RUNS = $runsAbs
$env:PIE_CAPTURE_VIEWS = 'az180:180,az90:90'
$env:PIE_CAPTURE_SHOOT_FROM_INPUT = '1'
# -ExecCmds "py <path>" breaks on the space in "Unreal Projects": a boot in %TEMP% moves the editor to desktop 2 and execs the driver
$boot = Join-Path $env:TEMP 'gstn_getup_capture_boot.py'
$here = $PSScriptRoot -replace '\\', '/'
$text = "import runpy`ntry:`n    runpy.run_path(r'$here/vd2_mover.py', run_name='vd2_mover')`nexcept Exception as e:`n    print('vd2_mover failed:', e)`n" +
        "__file__ = r'$here/pie_capture_react_ingame.py'`nexec(open(__file__).read())`n"
[IO.File]::WriteAllText($boot, $text, (New-Object Text.UTF8Encoding $false))
& (Join-Path $PSScriptRoot '..\CI\Run-Timed.ps1') -Preset Capture -Log (Join-Path $outAbs 'run.log') -Script $boot -MaxMinutes $MaxMinutes -AssertFile (Join-Path $outAbs 'assert.txt')
$rc = $LASTEXITCODE
Remove-Item Env:PIE_CAPTURE_GETUP_RUNS, Env:PIE_CAPTURE_VIEWS, Env:PIE_CAPTURE_SHOOT_FROM_INPUT -ErrorAction SilentlyContinue
$done = Join-Path $outAbs 'done.txt'
if (Test-Path $done) { "done: " + (Get-Content $done -TotalCount 1) } else { 'done.txt missing' }
exit $rc

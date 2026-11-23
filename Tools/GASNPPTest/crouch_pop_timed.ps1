# Crouch mesh pop capture (crouch_pop_run.py) under Run-Timed (capture limit 15 min), then the checker. usage:
#   powershell -File Tools\GASNPPTest\crouch_pop_timed.ps1 -Out Saved\CrouchPop\r1
param([string]$Out = 'Saved\CrouchPop\r1', [int]$Fps = 144, [double]$MaxMinutes = 15)
$proj = Split-Path (Split-Path $PSScriptRoot)
$outAbs = if ([IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $proj $Out }
New-Item -ItemType Directory -Force $outAbs | Out-Null
Remove-Item (Join-Path $outAbs 'done.txt') -ErrorAction SilentlyContinue
$env:CROUCHPOP_OUT = $outAbs -replace '\\', '/'
$env:CROUCHPOP_FPS = "$Fps"
$env:CROUCHPOP_QUIT = '1'
$boot = Join-Path $env:TEMP 'gstn_crouchpop_boot.py'
$real = (Join-Path $PSScriptRoot 'crouch_pop_boot_vd2.py') -replace '\\', '/'
[IO.File]::WriteAllText($boot, "__file__ = r'$real'`nexec(open(__file__).read())`n", (New-Object Text.UTF8Encoding $false))
& (Join-Path $PSScriptRoot '..\CI\Run-Timed.ps1') -Preset Capture -Log (Join-Path $outAbs 'editor.log') -Script $boot -MaxMinutes $MaxMinutes
$rc = $LASTEXITCODE
$done = Join-Path $outAbs 'done.txt'
if (Test-Path $done) { "done: " + (Get-Content $done -TotalCount 1) } else { 'done.txt missing' }
python (Join-Path $PSScriptRoot 'crouch_pop_check.py') (Join-Path $outAbs 'rows.csv')
exit $rc

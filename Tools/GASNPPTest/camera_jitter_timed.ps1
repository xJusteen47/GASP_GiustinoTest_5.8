# One traced Play run (camera_jitter_run.py) under Run-Timed (capture limit 15 min). usage:
#   camera_jitter_timed.ps1 -Out Saved\CamJitter\p1_early1 -Clients 0 -CVars "gstn.Camera.Anchor.Early 1"
param([string]$Out, [int]$Clients = 0, [string]$CVars = '', [double]$MaxMinutes = 15)
$proj = Split-Path (Split-Path $PSScriptRoot)
$outAbs = if ([IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $proj $Out }
New-Item -ItemType Directory -Force $outAbs | Out-Null
Remove-Item (Join-Path $outAbs 'done.txt') -ErrorAction SilentlyContinue
$env:CAMTRACE_OUT = $outAbs -replace '\\', '/'
$env:CAMTRACE_CLIENTS = "$Clients"
$env:CAMTRACE_CVARS = $CVars
$env:CAMTRACE_QUIT = '1'
$boot = Join-Path $env:TEMP 'gstn_camjitter_boot.py'
$real = (Join-Path $PSScriptRoot 'camera_jitter_boot_vd2.py') -replace '\\', '/'
[IO.File]::WriteAllText($boot, "__file__ = r'$real'`nexec(open(__file__).read())`n", (New-Object Text.UTF8Encoding $false))
& (Join-Path $PSScriptRoot '..\CI\Run-Timed.ps1') -Preset Capture -Log (Join-Path $outAbs 'editor.log') -Script $boot -MaxMinutes $MaxMinutes
$rc = $LASTEXITCODE
$done = Join-Path $outAbs 'done.txt'
if (Test-Path $done) { "done: " + (Get-Content $done -TotalCount 1) } else { 'done.txt missing' }
exit $rc

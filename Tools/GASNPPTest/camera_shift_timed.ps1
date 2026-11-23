# One camera_shift_capture.py Play run under Run-Timed (capture limit 15 min). usage:
#   camera_shift_timed.ps1 -Out Saved\CamShift\on -CVars "gstn.Camera.Shift 1"
param([string]$Out, [string]$CVars = '', [double]$MaxMinutes = 15)
$proj = Split-Path (Split-Path $PSScriptRoot)
$outAbs = if ([IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $proj $Out }
New-Item -ItemType Directory -Force $outAbs | Out-Null
Remove-Item (Join-Path $outAbs 'done.txt') -ErrorAction SilentlyContinue
$env:SHIFTCAP_OUT = $outAbs -replace '\\', '/'
$env:SHIFTCAP_CVARS = $CVars
$env:SHIFTCAP_QUIT = '1'
$boot = Join-Path $env:TEMP 'gstn_camshift_boot.py'
$vd2 = (Join-Path $PSScriptRoot 'vd2_mover.py') -replace '\\', '/'
$real = (Join-Path $PSScriptRoot 'camera_shift_capture.py') -replace '\\', '/'
$code = "import runpy`ntry:`n    runpy.run_path(r'$vd2', run_name='vd2_mover')`nexcept Exception as e:`n    print('vd2_mover failed:', e)`n__file__ = r'$real'`nexec(open(__file__).read())`n"
[IO.File]::WriteAllText($boot, $code, (New-Object Text.UTF8Encoding $false))
& (Join-Path $PSScriptRoot '..\CI\Run-Timed.ps1') -Preset Capture -Log (Join-Path $outAbs 'editor.log') -Script $boot -MaxMinutes $MaxMinutes
$rc = $LASTEXITCODE
$done = Join-Path $outAbs 'done.txt'
if (Test-Path $done) { "done: " + (Get-Content $done -TotalCount 1) } else { 'done.txt missing' }
exit $rc

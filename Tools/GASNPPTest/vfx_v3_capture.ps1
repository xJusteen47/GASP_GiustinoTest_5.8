# VFX v3 capture (pie_capture_vfx.py, NOT pie_capture_move.py) on a light or dark backdrop, under Run-Timed (rule 5).
# usage: vfx_v3_capture.ps1 -Out Saved\VfxV3\r3\light -Backdrop light -Surface "0.88,0.91,0.97" -Seq "hit_debris,hit_medium,ground_bounce,jump,run_skid,sprint"
# -Pre <module>: a Tools/GAS setup module whose run() is called before the capture (e.g. setup_presentation_table after a row change).
param([string]$Out, [string]$Backdrop, [string]$Surface, [string]$Seq, [double]$Max = 14, [string]$Pre = '', [string]$Wide = '', [string]$Close = '0.5', [string]$Views = 'side,game,close', [string]$Vfx = '1')
$proj = Split-Path (Split-Path $PSScriptRoot)
$outAbs = Join-Path $proj $Out
New-Item -ItemType Directory -Force $outAbs | Out-Null
Remove-Item (Join-Path $outAbs 'done.txt') -ErrorAction SilentlyContinue
$env:PIE_CAPTURE_OUT = $outAbs -replace '\\', '/'
$env:PIE_BACKDROP = $Backdrop; $env:PIE_SURFACE_COLOR = $Surface; $env:PIE_CAPTURE_SIZE = '720'; $env:PIE_CLOSE = $Close; $env:PIE_EXPORT_VIEWS = $Views
# -Wide 1920x1080: owner video frames (wide render targets)
if ($Wide) { $env:PIE_CAPTURE_W, $env:PIE_CAPTURE_H = $Wide -split 'x' } else { $env:PIE_CAPTURE_W = ''; $env:PIE_CAPTURE_H = '' }
# -Vfx 0: the VFX-off pass (same session flow, GSTN.Presentation.Effects off)
$env:PIE_VFX = $Vfx
$env:PIE_CAPTURE_KIT = 'KIT_Test_Hits'; $env:PIE_CAPTURE_SEQUENCES = $Seq
# -ExecCmds "py <path>" breaks on the space in "Unreal Projects": a boot in %TEMP%
$here = $PSScriptRoot -replace '\\', '/'
$boot = Join-Path $env:TEMP 'gstn_vfx_v3_boot.py'
$gas = (Join-Path (Split-Path $PSScriptRoot) 'GAS') -replace '\\', '/'
# -Pre a,b,c: several modules in order (v3 materials, then systems, then table)
$preLine = ($Pre -split ',' | ? { $_ } | % { "try:`n    print('pre $_', runpy.run_path(r'$gas/$_.py', run_name='pre')['run']())`nexcept Exception as e:`n    print('pre $_ failed:', e)`n" }) -join ''
[IO.File]::WriteAllText($boot, "import runpy`n$preLine" + "try:`n    runpy.run_path(r'$here/vd2_mover.py', run_name='vd2_mover')`nexcept Exception as e:`n    print('vd2_mover failed:', e)`n__file__ = r'$here/pie_capture_vfx.py'`nexec(open(__file__).read())`n", (New-Object Text.UTF8Encoding $false))
& (Join-Path $PSScriptRoot '..\CI\Run-Timed.ps1') -Preset Capture -Log (Join-Path $outAbs 'run.log') -Script $boot -MaxMinutes $Max
$done = Join-Path $outAbs 'done.txt'
if (Test-Path $done) { 'done: ' + (Get-Content $done -TotalCount 1) } else { 'done.txt missing' }

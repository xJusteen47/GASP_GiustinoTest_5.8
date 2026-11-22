# Headless crouch clip build under Run-Timed (rule 5). Editor must be closed on this checkout, _ue59 lock held.
# usage: run_crouch_build.ps1 -Plan Tools\GAS\crouch\crouch_plan_v4.json [-MaxMinutes 15]
# Result: Saved/Crouch/crouch_fk_build.log (pose report lines "<clip> f<N>: bone=(fwd,right,up)") + Saved/Crouch/build_run.log.esito
param([string]$Plan, [double]$MaxMinutes = 15)
$proj = (Resolve-Path "$PSScriptRoot\..\..\..").Path
$planAbs = if ([IO.Path]::IsPathRooted($Plan)) { $Plan } else { Join-Path $proj $Plan }
$env:CROUCH_PLAN = $planAbs
$eng = if ($env:UE_ENGINE_DIR) { $env:UE_ENGINE_DIR } else { 'C:\Program Files\Epic Games\UE_5.9\Engine' }
$script = (Join-Path $PSScriptRoot 'crouch_fk_build.py') -replace '\\', '/'
$log = Join-Path $proj 'Saved\Crouch\build_run.log'
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null
$uproject = Join-Path $proj 'GASP_Giustino2.uproject'
$argLine = "`"$uproject`" -run=pythonscript -script=`"$script`" -unattended -nullrhi -nosplash -nosound -abslog=`"$log.ue.log`""
& (Join-Path $proj 'Tools\CI\Run-Timed.ps1') -Kind Capture -Log $log -Exe "$eng\Binaries\Win64\UnrealEditor-Cmd.exe" -ArgLine $argLine -WatchFile "$log.ue.log" -MaxMinutes $MaxMinutes
Get-Content (Join-Path $proj 'Saved\Crouch\crouch_fk_build.log') -ErrorAction SilentlyContinue | Select-Object -Last 1

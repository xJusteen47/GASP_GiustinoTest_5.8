<#
Movement QA batch (qa-movement-1117): runs movement_scenarios.py in a RENDERING editor (the anim graph must tick for the animation checks),
each scenario in a fresh listen PIE (host + 2 clients), under Run-Timed (Capture limit 15 min unless -MaxMinutes).
  Tools\GASNPPTest\run_movement_qa.ps1 [-Names move_skid,move_jump_full] [-Map /Game/Levels/DefaultLevel] [-Tag run1] [-MaxMinutes 25]
Output: Saved\Logs\qam\batch-<Tag>.log (one line per scenario), Saved\Logs\qam\<scenario>.json (numbers), Saved\Logs\ci\qam-<Tag>.log.ue.log.
#>
param(
    [string[]]$Names = @(),
    [string]$Map = '/Game/Levels/DefaultLevel',
    [string]$Tag = 'run',
    [double]$MaxMinutes = 25,
    [string]$Engine = $(if ($env:UE_ENGINE_DIR) { $env:UE_ENGINE_DIR } else { 'C:\Program Files\Epic Games\UE_5.9\Engine' })
)
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$uproject = Join-Path $root 'GASP_Giustino2.uproject'
$driver = (Join-Path $root 'Tools\GASNPPTest\batch_driver.py') -replace '\\', '/'
$log = Join-Path $root "Saved\Logs\ci\qam-$Tag.log"
$env:BATCH_MODULE = 'movement_scenarios'
$env:BATCH_NAMES = if ($Names.Count) { ($Names -join ',') } else { '' }
if (-not $env:BATCH_NAMES) { $env:BATCH_NAMES = 'move_run_stages,move_skid,move_analog_walk,move_jump_full,move_jump_tap,move_air_control,move_fast_fall' }
$env:BATCH_MODE = 'listen'
$env:BATCH_LOSS = '0'
$env:BATCH_REPORT = "qam/batch-$Tag.log"
$env:BATCH_MAP = $Map
$exe = Join-Path $Engine 'Binaries\Win64\UnrealEditor.exe'
$argLine = "`"$uproject`" -windowed -ResX=1280 -ResY=720 -nosplash -nosound -unattended -ExecCmds=`"py $driver`" -abslog=`"$log.ue.log`""
& (Join-Path $root 'Tools\CI\Run-Timed.ps1') -Kind Capture -Log $log -Exe $exe -ArgLine $argLine -WatchFile "$log.ue.log" -MaxMinutes $MaxMinutes

# Skid fix capture: mv_skid, KIT_Test_Playtest, 1 player, no VFX, views az90/az0/game, through Run-Timed on virtual desktop 2.
#   pwsh Tools/AnimPipeline/cards/skid/skid_fix_capture.ps1 -Out Saved/SkidFix/before [-Bake]
param([Parameter(Mandatory)][string]$Out, [switch]$Bake, [string]$Sequences = 'mv_skid')
$proj = Resolve-Path "$PSScriptRoot/../../../.."
$outAbs = Join-Path $proj $Out
New-Item -ItemType Directory -Force $outAbs | Out-Null
$boot = Join-Path $env:TEMP 'skid_fix_boot_vd2.py'
Copy-Item "$PSScriptRoot/skid_fix_boot_vd2.py" $boot -Force
$env:PIE_CAPTURE_KIT = 'KIT_Test_Playtest'; $env:PIE_CAPTURE_CLIENTS = '0'; $env:PIE_CAPTURE_NOVFX = '1'
$env:PIE_CAPTURE_SEQUENCES = $Sequences; $env:PIE_CAPTURE_VIEWS = 'az90,az0,game'; $env:PIE_CAPTURE_OUT = $outAbs
$env:SKIDFIX_BAKE = $(if ($Bake) { '1' } else { '0' })
& "$proj/Tools/CI/Run-Timed.ps1" -Preset Capture -Log (Join-Path $outAbs 'run.log') -Script $boot -AssertFile (Join-Path $outAbs 'assert.txt')
exit $LASTEXITCODE

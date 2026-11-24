<#
Invoke-RunStartPipeline.ps1 - deterministic post-authoring pipeline (0 LLM tokens).
Runs:
  1. Authoring UE script (via Run-SparseAnimStep.ps1 if -WorktreeDir is set, else Save-Assets.ps1 in _ue59)
  2. Joint dump + pivot gates (GATES: OK required before GPU capture)
  3. Batch GPU Play A/B capture (runstart_batch_captures.ps1 in _ue59)
  4. Side-by-side .mp4 + .jpg grid generation (runstart_ab_video.py), copied to -WorktreeDir if set.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Script,
    [Parameter(Mandatory = $true)][string]$Asset,
    [Parameter(Mandatory = $true)][string[]]$Runs,
    [string]$WorktreeDir = '',
    [string]$Ue59Dir = 'C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_ue59',
    [string]$SavedDir = 'CaptureSpeed',
    [string]$Batch = 'batch',
    [string]$BaselineRun = '',
    [string]$CandidateRun = '',
    [string]$BaselineLabel = 'A',
    [string]$CandidateLabel = 'B',
    [string]$Seq = 'mv_run_start_v5',
    [int]$From = 77,
    [int]$To = 197,
    [string]$Views = 'az180,az90,game',
    [int]$PivotFrom = 84,
    [int]$PivotTo = 116,
    [int]$PivotStep = 4,
    [int]$GateWin = 18,
    [string]$BeforeFbx = '',
    [string]$AfterFbx = '',
    [string]$Tag = 'runstart_pipe'
)

$ErrorActionPreference = 'Stop'
$ue59 = (Resolve-Path -LiteralPath $Ue59Dir).Path
$ciDir = Join-Path $ue59 'Tools\CI'
$rsDir = Join-Path $ue59 'Tools\AnimPipeline\cards\runstart'
$assetList = @($Asset -split ',' | ForEach-Object { ($_.Trim().TrimEnd('/') -split '/')[-1] } | Where-Object { $_ })
if ($assetList.Count -eq 0) { throw "Asset list is empty" }
$assetsCsv = $assetList -join ','

if ($WorktreeDir) {
    $wt = (Resolve-Path -LiteralPath $WorktreeDir).Path
    $sparseHelper = Join-Path $wt 'Tools\CI\Run-SparseAnimStep.ps1'
    if (-not (Test-Path -LiteralPath $sparseHelper)) { $sparseHelper = Join-Path $ciDir 'Run-SparseAnimStep.ps1' }
    & $sparseHelper -WorktreeDir $wt -Script $Script -Ue59Dir $ue59 -Assets $assetsCsv -GateWin $GateWin -Tag $Tag
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
    & (Join-Path $ciDir 'Save-Assets.ps1') -Script $Script -Tag $Tag
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $engine = if ($env:UE_ENGINE_DIR) { $env:UE_ENGINE_DIR } else { 'C:\Program Files\Epic Games\UE_5.9\Engine' }
    $exe = Join-Path $engine 'Binaries\Win64\UnrealEditor-Cmd.exe'
    $uproj = Join-Path $ue59 'GASP_Giustino2.uproject'
    $rcj = Join-Path $rsDir 'runstart_clip_joints_ue.py'
    $env:RCJ_ASSETS = $assetsCsv
    try {
        & (Join-Path $ciDir 'Run-Timed.ps1') -Kind Capture -Log (Join-Path $ue59 "Saved\Logs\ci\${Tag}_rcj.log") -Exe $exe `
            -ArgLine "`"$uproj`" -run=pythonscript -script=`"$($rcj -replace '\\', '/')`" -unattended -nullrhi -nosplash -nopause" -MaxMinutes 10
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    } finally {
        Remove-Item Env:RCJ_ASSETS -ErrorAction SilentlyContinue
    }
    foreach ($aItem in $assetList) {
        $jFile = Join-Path $ue59 "Saved\AnimPipeline\runstart\joints_$aItem.json"
        $prevNativePref = $PSNativeCommandUseErrorActionPreference
        $PSNativeCommandUseErrorActionPreference = $false
        try {
            $gOut = & python (Join-Path $rsDir 'runstart_pivot_gates.py') --json $jFile $GateWin 2>&1
            $gExit = $LASTEXITCODE
        } finally {
            $PSNativeCommandUseErrorActionPreference = $prevNativePref
        }
        $gOut | ForEach-Object { Write-Output "$_" }
        $gTxt = ($gOut | Out-String)
        if ($gExit -ne 0) { exit $gExit }
        if ($gTxt -match 'GATES:\s*FAIL') {
            Write-Output "PIPELINE: FAIL - gates falliti per $aItem"
            exit 1
        }
    }
}

if ($BeforeFbx -and $AfterFbx) {
    $ccPy = Join-Path $ue59 'Tools\AnimPipeline\clipcheck\clip_check.py'
    $conserveOut = Join-Path $ue59 "Saved\AnimPipeline\runstart\conserve_$($assetList[0]).json"
    & python $ccPy --conserve $BeforeFbx $AfterFbx $conserveOut
    if ($LASTEXITCODE -ne 0) { Write-Error "CONSERVATION GUARD FAIL (>15% loss)"; exit $LASTEXITCODE }
}

# GPU Capture in _ue59 (needs editor closed)
& (Join-Path $rsDir 'runstart_batch_captures.ps1') -Runs $Runs -SavedDir $SavedDir -Batch $Batch -Views $Views
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$batchRoot = Join-Path $ue59 "Saved\$SavedDir\$Batch"
$runNames = @($Runs | ForEach-Object { ($_ -split '\|')[0] })
$aName = if ($BaselineRun) { $BaselineRun } else { $runNames[0] }
$bName = if ($CandidateRun) { $CandidateRun } else { $runNames[-1] }

$outVideoDir = Join-Path $batchRoot 'ab_out'
& python (Join-Path $rsDir 'runstart_ab_video.py') `
    (Join-Path $batchRoot $aName) (Join-Path $batchRoot $bName) $outVideoDir `
    "--a=$BaselineLabel" "--b=$CandidateLabel" "--seq=$Seq" "--from=$From" "--to=$To" "--views=$Views" `
    "--pivot-from=$PivotFrom" "--pivot-to=$PivotTo" "--pivot-step=$PivotStep"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($WorktreeDir) {
    $wtOut = Join-Path (Resolve-Path -LiteralPath $WorktreeDir).Path "Saved\$SavedDir\$Batch\ab_out"
    New-Item -ItemType Directory -Force $wtOut | Out-Null
    Copy-Item -Path (Join-Path $outVideoDir '*.jpg') -Destination $wtOut -Force -ErrorAction SilentlyContinue
    Copy-Item -Path (Join-Path $outVideoDir '*.mp4') -Destination $wtOut -Force -ErrorAction SilentlyContinue
    Write-Output "PIPELINE: OK - griglie e video copiati in $wtOut"
} else {
    Write-Output "PIPELINE: OK - griglie e video in $outVideoDir"
}
exit 0

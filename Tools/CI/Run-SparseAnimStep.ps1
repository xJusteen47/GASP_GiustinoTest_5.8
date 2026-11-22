<#
Run-SparseAnimStep.ps1 - bridges a sparse DeepSeek worktree (without Plugins/GASNPP) and GASP_Giustino2_ue59.
Copies the target .py script temporarily into GASP_Giustino2_ue59, runs UnrealEditor-Cmd headless via Run-Timed.ps1,
optionally dumps joints & runs pivot gates, copies produced .uasset / joints_*.json / gates_*.json back into
the sparse worktree, and restores GASP_Giustino2_ue59 cleanly without touching pre-existing owner dirty files.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$WorktreeDir,
    [Parameter(Mandatory = $true)][string]$Script,
    [string]$Ue59Dir = 'C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_ue59',
    [string]$Assets = '',
    [int]$GateWin = 40,
    [string]$Tag = 'sparse_step'
)

$ErrorActionPreference = 'Stop'
function Refuse($msg) { Write-Output "ESITO: RIFIUTATO - $msg"; exit 2 }

$wt = (Resolve-Path -LiteralPath $WorktreeDir).Path
$ue59 = (Resolve-Path -LiteralPath $Ue59Dir).Path
$rel = ($Script -replace '/', '\') -replace '^\.\\', ''
if ($rel -notmatch '^Tools\\[^:]+\.py$' -or $rel -match '\.\.') { Refuse "-Script deve essere un .py sotto Tools\" }

$srcScript = Join-Path $wt $rel
if (-not (Test-Path -LiteralPath $srcScript)) { Refuse "script non trovato nella worktree: $srcScript" }
if ($Tag -notmatch '^[A-Za-z0-9_-]{1,40}$') { Refuse "-Tag: solo lettere, cifre, _ e -" }

$open = Get-CimInstance Win32_Process | Where-Object { $_.Name -like 'UnrealEditor*' -and $_.CommandLine -like '*GASP_Giustino2_ue59*' }
if ($open) { Refuse "un editor di _ue59 e' aperto (pid $($open[0].ProcessId))" }

# Snapshot pre-existing dirty/untracked files in _ue59 so we NEVER touch or revert owner work
$preDirty = @{}
$statusBefore = & git -C $ue59 status --porcelain 2>$null
foreach ($line in $statusBefore) {
    if ($line.Length -ge 4) {
        $p = ($line.Substring(3) -split ' -> ')[-1].Trim('"') -replace '/', '\'
        $preDirty[$p] = $true
    }
}

$dstScript = Join-Path $ue59 $rel
$hadScript = Test-Path -LiteralPath $dstScript
$backupScript = $null
if ($hadScript) {
    $backupScript = Join-Path $env:TEMP ("ue59_bak_" + [guid]::NewGuid().ToString('N') + ".py")
    Copy-Item -LiteralPath $dstScript -Destination $backupScript -Force
}

$copiedAssets = @()
try {
    New-Item -ItemType Directory -Force (Split-Path $dstScript -Parent) | Out-Null
    Copy-Item -LiteralPath $srcScript -Destination $dstScript -Force

    $engine = if ($env:UE_ENGINE_DIR) { $env:UE_ENGINE_DIR } else { 'C:\Program Files\Epic Games\UE_5.9\Engine' }
    $exe = Join-Path $engine 'Binaries\Win64\UnrealEditor-Cmd.exe'
    $uproj = Join-Path $ue59 'GASP_Giustino2.uproject'
    $log = Join-Path $ue59 "Saved\Logs\ci\sparse_${Tag}.log"
    $ueLog = Join-Path $ue59 "Saved\Logs\ci\sparse_${Tag}_ue.log"
    $runTimed = Join-Path $PSScriptRoot 'Run-Timed.ps1'

    $env:CLIPFIX_WRITE = '1'
    $env:GSTN_SAVE = '1'
    $argLine = "`"$uproj`" -run=pythonscript -script=`"$($dstScript -replace '\\', '/')`" -unattended -nullrhi -nosplash -nopause -abslog=`"$ueLog`""
    & $runTimed -Kind Capture -Log $log -Exe $exe -ArgLine $argLine -WatchFile $ueLog -MaxMinutes 15
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    if ($Assets) {
        $rcj = Join-Path $ue59 'Tools\AnimPipeline\cards\runstart\runstart_clip_joints_ue.py'
        $env:RCJ_ASSETS = $Assets
        $rcjLog = Join-Path $ue59 "Saved\Logs\ci\sparse_${Tag}_rcj.log"
        $rcjUe = Join-Path $ue59 "Saved\Logs\ci\sparse_${Tag}_rcj_ue.log"
        $argRcj = "`"$uproj`" -run=pythonscript -script=`"$($rcj -replace '\\', '/')`" -unattended -nullrhi -nosplash -nopause -abslog=`"$rcjUe`""
        & $runTimed -Kind Capture -Log $rcjLog -Exe $exe -ArgLine $argRcj -WatchFile $rcjUe -MaxMinutes 10
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

        $gatesPy = Join-Path $wt 'Tools\AnimPipeline\cards\runstart\runstart_pivot_gates.py'
        $wtRs = Join-Path $wt 'Saved\AnimPipeline\runstart'
        New-Item -ItemType Directory -Force $wtRs | Out-Null
        foreach ($assetName in ($Assets.Split(',') | Where-Object { $_ })) {
            $jSrc = Join-Path $ue59 "Saved\AnimPipeline\runstart\joints_$assetName.json"
            $jDst = Join-Path $wtRs "joints_$assetName.json"
            if (Test-Path -LiteralPath $jSrc) {
                Copy-Item -LiteralPath $jSrc -Destination $jDst -Force
                if (Test-Path -LiteralPath $gatesPy) {
                    & python $gatesPy --json $jDst $GateWin
                    if ($LASTEXITCODE -ne 0) { Write-Output "ESITO: FAIL - gates falliti per $assetName"; exit $LASTEXITCODE }
                }
            }
        }
    }

    # Copy newly touched/created .uasset/.uexp files from _ue59 back into sparse worktree
    $statusAfter = & git -C $ue59 status --porcelain 2>$null
    foreach ($line in $statusAfter) {
        if ($line.Length -lt 4) { continue }
        $p = ($line.Substring(3) -split ' -> ')[-1].Trim('"') -replace '/', '\'
        if ($preDirty.ContainsKey($p)) { continue }
        if ($p -match '^Content\\.*\.(uasset|uexp)$') {
            $s = Join-Path $ue59 $p
            $d = Join-Path $wt $p
            if (Test-Path -LiteralPath $s) {
                New-Item -ItemType Directory -Force (Split-Path $d -Parent) | Out-Null
                Copy-Item -LiteralPath $s -Destination $d -Force
                $copiedAssets += $p
            }
        }
    }
}
finally {
    Remove-Item Env:RCJ_ASSETS -ErrorAction SilentlyContinue
    # Restore _ue59 script & any new/modified Content assets we created during this step
    if ($hadScript -and $backupScript -and (Test-Path -LiteralPath $backupScript)) {
        Copy-Item -LiteralPath $backupScript -Destination $dstScript -Force
        Remove-Item -LiteralPath $backupScript -Force -ErrorAction SilentlyContinue
    } elseif (-not $hadScript -and (Test-Path -LiteralPath $dstScript)) {
        Remove-Item -LiteralPath $dstScript -Force -ErrorAction SilentlyContinue
    }
    $statusCleanup = & git -C $ue59 status --porcelain 2>$null
    foreach ($line in $statusCleanup) {
        if ($line.Length -lt 4) { continue }
        $code = $line.Substring(0, 2)
        $p = ($line.Substring(3) -split ' -> ')[-1].Trim('"') -replace '/', '\'
        if ($preDirty.ContainsKey($p)) { continue }
        if ($p -match '^Content\\.*\.(uasset|uexp)$') {
            if ($code -eq '??') {
                Remove-Item -LiteralPath (Join-Path $ue59 $p) -Force -ErrorAction SilentlyContinue
            } else {
                & git -C $ue59 checkout -- ($p -replace '\\', '/') 2>$null
            }
        }
    }
}

Write-Output "ESITO: OK - sparse step completato ($($copiedAssets.Count) uasset copiati in $wt)"
exit 0

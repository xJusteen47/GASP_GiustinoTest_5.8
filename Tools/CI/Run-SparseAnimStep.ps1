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
    [int]$GateWin = 18,
    [string]$Tag = 'sparse_step',
    [switch]$KeepUe59Content   # anim_register_variant: leave the edited chooser + copied clips in _ue59 (the tool restores them)
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

$assetList = @($Assets -split ',' | ForEach-Object { ($_.Trim().TrimEnd('/') -split '/')[-1] } | Where-Object { $_ })
if ($PSBoundParameters.ContainsKey('Assets') -and $Assets.Trim() -ne '' -and $assetList.Count -eq 0) {
    Refuse "-Assets non contiene alcun nome asset valido: '$Assets'"
}
foreach ($a in $assetList) {
    if ($a -notmatch '^[A-Za-z0-9_]+$') { Refuse "-Assets contiene nome non valido: $a" }
}
$assetsCsv = $assetList -join ','

$ue59Leaf = Split-Path $ue59 -Leaf
$open = Get-CimInstance Win32_Process | Where-Object { $_.Name -like 'UnrealEditor*' -and $_.CommandLine -like "*$ue59Leaf*" }
if ($open) { Refuse "un editor di _ue59 e' aperto (pid $($open[0].ProcessId))" }

# Snapshot pre-existing dirty/untracked files in _ue59 so we NEVER touch or revert owner work
$preDirty = @{}
$preTrackedDirty = @{}
$statusBefore = & git -C $ue59 status --porcelain -uall 2>$null
foreach ($line in $statusBefore) {
    if ($line.Length -ge 4) {
        $code = $line.Substring(0, 2)
        $p = ($line.Substring(3) -split ' -> ')[-1].Trim('"') -replace '/', '\'
        $preDirty[$p] = $true
        if ($code -ne '??') { $preTrackedDirty[$p] = $true }
    }
}

foreach ($a in $assetList) {
    foreach ($k in $preTrackedDirty.Keys) {
        if ($k -match ("\\$([regex]::Escape($a))\.(uasset|uexp)$")) {
            Refuse "asset target gia' modificato (tracked) in _ue59: $k"
        }
    }
}

$dstScript = Join-Path $ue59 $rel
$hadScript = Test-Path -LiteralPath $dstScript
$backupScript = $null
if ($hadScript) {
    $backupScript = Join-Path $env:TEMP ("ue59_bak_" + [guid]::NewGuid().ToString('N') + ".py")
    Copy-Item -LiteralPath $dstScript -Destination $backupScript -Force
}

$prevClipfix = $env:CLIPFIX_WRITE
$prevGstnSave = $env:GSTN_SAVE
$prevDumpMirror = $env:GSTN_DUMP_MIRROR
$prevRcjAssets = $env:RCJ_ASSETS
$copiedAssets = @()
$gateFailed = $false
$gateFailName = ''
try {
    New-Item -ItemType Directory -Force (Split-Path $dstScript -Parent) | Out-Null
    Copy-Item -LiteralPath $srcScript -Destination $dstScript -Force

    $engine = if ($env:UE_ENGINE_DIR) { $env:UE_ENGINE_DIR } else { 'C:\Program Files\Epic Games\UE_5.9\Engine' }
    $exe = Join-Path $engine 'Binaries\Win64\UnrealEditor-Cmd.exe'
    $uproj = Join-Path $ue59 'GASP_Giustino2.uproject'
    $log = Join-Path $ue59 "Saved\Logs\ci\sparse_${Tag}.log"
    $ueLog = Join-Path $ue59 "Saved\Logs\ci\sparse_${Tag}_ue.log"
    $runTimed = Join-Path $PSScriptRoot 'Run-Timed.ps1'

    $stepStart = (Get-Date).AddSeconds(-2)
    if (-not $env:CLIPFIX_WRITE) { $env:CLIPFIX_WRITE = '1' }
    if (-not $env:GSTN_SAVE) { $env:GSTN_SAVE = '1' }
    $env:GSTN_DUMP_MIRROR = Join-Path $wt 'Saved\AnimPipeline\runloop'
    $argLine = "`"$uproj`" -run=pythonscript -script=`"$($dstScript -replace '\\', '/')`" -unattended -nullrhi -nosplash -nopause -abslog=`"$ueLog`""
    & $runTimed -Kind Capture -Log $log -Exe $exe -ArgLine $argLine -WatchFile $ueLog -MaxMinutes 15
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    if ($assetsCsv) {
        $rcj = Join-Path $ue59 'Tools\AnimPipeline\cards\runstart\runstart_clip_joints_ue.py'
        $rcjWt = Join-Path $wt 'Tools\AnimPipeline\cards\runstart\runstart_clip_joints_ue.py'   # the worktree dump script wins (head/neck keys)
        if (Test-Path -LiteralPath $rcjWt) { $rcj = $rcjWt }
        $env:RCJ_ASSETS = $assetsCsv
        $rcjLog = Join-Path $ue59 "Saved\Logs\ci\sparse_${Tag}_rcj.log"
        $rcjUe = Join-Path $ue59 "Saved\Logs\ci\sparse_${Tag}_rcj_ue.log"
        $argRcj = "`"$uproj`" -run=pythonscript -script=`"$($rcj -replace '\\', '/')`" -unattended -nullrhi -nosplash -nopause -abslog=`"$rcjUe`""
        & $runTimed -Kind Capture -Log $rcjLog -Exe $exe -ArgLine $argRcj -WatchFile $rcjUe -MaxMinutes 10
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

        $wtLoop = Join-Path $wt 'Saved\AnimPipeline\runloop\loop_relaxed_dump.npz'
        $ueLoop = Join-Path $ue59 'Saved\AnimPipeline\runloop\loop_relaxed_dump.npz'
        if (-not (Test-Path -LiteralPath $wtLoop) -and (Test-Path -LiteralPath $ueLoop)) {
            New-Item -ItemType Directory -Force (Split-Path $wtLoop -Parent) | Out-Null
            Copy-Item -LiteralPath $ueLoop -Destination $wtLoop -Force
        }

        $gatesPy = Join-Path $wt 'Tools\AnimPipeline\cards\runstart\runstart_pivot_gates.py'
        $wtRs = Join-Path $wt 'Saved\AnimPipeline\runstart'
        New-Item -ItemType Directory -Force $wtRs | Out-Null
        foreach ($assetName in $assetList) {
            $jSrc = Join-Path $ue59 "Saved\AnimPipeline\runstart\joints_$assetName.json"
            $jDst = Join-Path $wtRs "joints_$assetName.json"
            if (Test-Path -LiteralPath $jSrc) {
                Copy-Item -LiteralPath $jSrc -Destination $jDst -Force
                if (Test-Path -LiteralPath $gatesPy) {
                    $prevNativePref = $PSNativeCommandUseErrorActionPreference
                    $PSNativeCommandUseErrorActionPreference = $false
                    try {
                        $gateOut = & python $gatesPy --json $jDst $(if ($GateWin -gt 0) { $GateWin }) 2>&1
                        $gateExit = $LASTEXITCODE
                    } finally {
                        $PSNativeCommandUseErrorActionPreference = $prevNativePref
                    }
                    $gateOut | ForEach-Object { Write-Output "$_" }
                    $gateTxt = ($gateOut | Out-String)
                    if ($gateExit -ne 0 -or $gateTxt -match 'GATES:\s*FAIL') {
                        $gateFailed = $true
                        $gateFailName = $assetName
                    }
                }
            }
        }
    }

    # Copy newly touched/created .uasset/.uexp files from _ue59 back into sparse worktree
    $copiedSet = @{}
    $copiedByAsset = @{}
    foreach ($assetName in $assetList) {
        $copiedByAsset[$assetName] = $false
        Get-ChildItem -LiteralPath (Join-Path $ue59 'Content') -Recurse -File -Filter "$assetName.*" -ErrorAction SilentlyContinue |
            Where-Object { $_.Extension -in '.uasset', '.uexp' -and $_.LastWriteTime -ge $stepStart } |
            ForEach-Object {
                $f = $_
                $p = $f.FullName.Substring($ue59.Length).TrimStart('\')
                $d = Join-Path $wt $p
                New-Item -ItemType Directory -Force (Split-Path $d -Parent) | Out-Null
                Copy-Item -LiteralPath $f.FullName -Destination $d -Force
                $copiedSet[$p] = $true
                $copiedAssets += $p
                if ($f.Extension -eq '.uasset') { $copiedByAsset[$assetName] = $true }
            }
    }

    $statusAfter = if ($KeepUe59Content) { @() } else { & git -C $ue59 status --porcelain -uall 2>$null }
    foreach ($line in $statusAfter) {
        if ($line.Length -lt 4) { continue }
        $p = ($line.Substring(3) -split ' -> ')[-1].Trim('"') -replace '/', '\'
        if ($preDirty.ContainsKey($p)) { continue }
        if ($copiedSet.ContainsKey($p)) { continue }
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

    if ($assetsCsv) {
        $missing = @($assetList | Where-Object { -not $copiedByAsset[$_] })
        if ($missing.Count -gt 0) {
            Write-Output "ESITO: FAIL - asset richiesti non prodotti/copiati: $($missing -join ', ')"
            exit 1
        }
    }
    if ($gateFailed) {
        Write-Output "ESITO: FAIL - gates falliti per $gateFailName"
        exit 1
    }
}
finally {
    if ($null -eq $prevRcjAssets) { Remove-Item Env:RCJ_ASSETS -ErrorAction SilentlyContinue } else { $env:RCJ_ASSETS = $prevRcjAssets }
    if ($null -eq $prevClipfix) { Remove-Item Env:CLIPFIX_WRITE -ErrorAction SilentlyContinue } else { $env:CLIPFIX_WRITE = $prevClipfix }
    if ($null -eq $prevGstnSave) { Remove-Item Env:GSTN_SAVE -ErrorAction SilentlyContinue } else { $env:GSTN_SAVE = $prevGstnSave }
    if ($null -eq $prevDumpMirror) { Remove-Item Env:GSTN_DUMP_MIRROR -ErrorAction SilentlyContinue } else { $env:GSTN_DUMP_MIRROR = $prevDumpMirror }
    # Restore _ue59 script & any new/modified Content assets we created during this step
    if ($hadScript -and $backupScript -and (Test-Path -LiteralPath $backupScript)) {
        Copy-Item -LiteralPath $backupScript -Destination $dstScript -Force
        Remove-Item -LiteralPath $backupScript -Force -ErrorAction SilentlyContinue
    } elseif (-not $hadScript -and (Test-Path -LiteralPath $dstScript)) {
        Remove-Item -LiteralPath $dstScript -Force -ErrorAction SilentlyContinue
    }
    $statusCleanup = if ($KeepUe59Content) { @() } else { & git -C $ue59 status --porcelain -uall 2>$null }
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

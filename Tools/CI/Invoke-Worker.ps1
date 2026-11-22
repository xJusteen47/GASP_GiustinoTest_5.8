<#
Invoke-Worker.ps1 - deterministic multi-model worker router (Regola 3 / Tier M0-M3).
Routes tasks to DeepSeek / Antigravity / Claude Pro / local Inkling via agent-bridge cli.py.
Enforces: filesystem+text EULA Epic guard, local VRAM guard, atomic .lock + stale PID recovery
on a single absolute _ue59/IN-USO.txt path, own hard timeout + taskkill /T /F, GSTN_OWNER_UNLOCK strip,
and 11-column costi.csv logging. Prints at most 5 lines.
#>
[CmdletBinding()]
param(
    [ValidateSet('M0', 'M1_CODE', 'M1_ANIM', 'M2', 'M3')][string]$Tier = 'M1_CODE',
    [string]$Obiettivo = 'task',
    [string]$Prompt = '',
    [string]$PromptFile = '',
    [string]$Brief = '',
    [string]$Cwd,
    [ValidateSet('fisso', 'portatile', 'cloud')][string]$Machine = 'fisso',
    [switch]$UsesUe59,
    [string]$Ue59LockPath,
    [switch]$UseLocal,
    [switch]$AllowAgyFallback,
    [double]$MaxUsd = 0,
    [int]$TimeoutSec = 300,
    [string]$CostCsv,
    [string]$BridgeCli = 'C:\Users\jacop\Tools\agent-bridge\cli.py',
    [switch]$DryRun,
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$GuardConfigPath = Join-Path $PSScriptRoot 'protected-paths.json'

function Load-GuardSettings {
    $res = @{
        EulaPatterns = @('plugins/engineoverrides', 'plugins/gasnpp', 'program files/epic games', 'epic games/ue_', 'ue_5.9', 'ue_5.8')
        EulaDirs     = @('Plugins/EngineOverrides', 'Plugins/GASNPP')
        Ue59LockPath = 'C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_ue59\_ue59\IN-USO.txt'
    }
    if (Test-Path -LiteralPath $GuardConfigPath) {
        $cfg = Get-Content -Raw -LiteralPath $GuardConfigPath -Encoding UTF8 | ConvertFrom-Json
        if ($cfg.epic_eula_patterns) { $res.EulaPatterns = @($cfg.epic_eula_patterns) }
        if ($cfg.epic_eula_dirs) { $res.EulaDirs = @($cfg.epic_eula_dirs) }
        if ($cfg.ue59_lock_path) { $res.Ue59LockPath = "$($cfg.ue59_lock_path)" }
    }
    if ($env:GSTN_UE59_LOCK_PATH) { $res.Ue59LockPath = $env:GSTN_UE59_LOCK_PATH }
    return $res
}

function Get-WorkerRoute {
    param([string]$WorkerTier, [bool]$LocalFlag, [bool]$AgyFallback)
    switch ($WorkerTier) {
        'M0' {
            if ($LocalFlag) {
                return @{ Command = 'local'; Provider = 'local'; Model = 'inkling-2-mini'; Args = @('--permission-mode', 'acceptEdits') }
            }
            if ($AgyFallback) {
                return @{ Command = 'agy'; Provider = 'agy_student'; Model = 'gemini-4-hydrogen-low'; Args = @('--model', 'hydrogen-low', '--mode', 'accept-edits') }
            }
            return @{ Command = 'deepseek'; Provider = 'deepseek'; Model = 'deepseek-flash'; Args = @('--tier', 'flash', '--permission-mode', 'acceptEdits') }
        }
        'M1_CODE' {
            return @{ Command = 'deepseek'; Provider = 'deepseek'; Model = 'deepseek-v4-pro'; Args = @('--tier', 'pro', '--permission-mode', 'acceptEdits') }
        }
        'M1_ANIM' {
            return @{ Command = 'agy'; Provider = 'agy_student'; Model = 'gemini-4-hydrogen-high'; Args = @('--model', 'hydrogen-high', '--mode', 'accept-edits') }
        }
        'M2' {
            return @{ Command = 'claude'; Provider = 'claude_pro'; Model = 'sonnet'; Args = @('--model', 'sonnet', '--effort', 'high', '--permission-mode', 'plan') }
        }
        'M3' {
            return @{ Command = 'agy'; Provider = 'agy_student'; Model = 'gemini-4-neon-high'; Args = @('--model', 'neon', '--mode', 'plan') }
        }
    }
}

function Test-EpicDirsOnDisk {
    param([string]$WorkDir, [string[]]$EulaDirs)
    if (-not $WorkDir -or -not (Test-Path -LiteralPath $WorkDir)) { return $null }
    $cur = (Resolve-Path -LiteralPath $WorkDir).Path
    for ($depth = 0; $depth -lt 8 -and $cur; $depth++) {
        foreach ($rel in $EulaDirs) {
            $candidate = Join-Path $cur ($rel -replace '/', '\')
            if (Test-Path -LiteralPath $candidate) {
                return "EULA Epic guard: '$rel' esiste su disco in '$cur' (usa Tools/CI/New-DeepSeekWorktree.ps1 per un worktree sparse senza cartelle Epic, oppure usa M2)"
            }
        }
        if (Test-Path -LiteralPath (Join-Path $cur '.git')) { break }
        $parent = Split-Path $cur -Parent
        if (-not $parent -or $parent -eq $cur) { break }
        $cur = $parent
    }
    return $null
}

function Test-WorkerGuards {
    param(
        $Route,
        [string]$WorkDir,
        [string]$PromptText,
        [string]$BriefText,
        [bool]$IsEditorOpen,
        [string[]]$EulaPatterns,
        [string[]]$EulaDirs
    )
    if ($Route.Command -eq 'deepseek') {
        $fsErr = Test-EpicDirsOnDisk $WorkDir $EulaDirs
        if ($fsErr) { return $fsErr }
        $hay = (($WorkDir + "`n" + $PromptText + "`n" + $BriefText) -replace '\\', '/').ToLowerInvariant()
        foreach ($pat in $EulaPatterns) {
            if ($hay.Contains($pat.ToLowerInvariant())) {
                return "EULA Epic guard: riferimento bloccato ('$pat') su DeepSeek; usa M2 (Claude Pro) o locale offline"
            }
        }
    }
    if ($Route.Command -eq 'local' -and $IsEditorOpen) {
        return "VRAM guard: UnrealEditor* aperto, modello locale rifiutato"
    }
    return $null
}

function Acquire-Ue59Lock {
    param([string]$LockPath, [string]$OwnerTag)
    $dir = Split-Path $LockPath -Parent
    if ($dir) { New-Item -ItemType Directory -Force $dir | Out-Null }
    $metaPath = "$LockPath.lock"
    $utc = (Get-Date).ToUniversalTime().ToString('yyyy-MM-dd HH:mm')
    $myMeta = @{ host = $env:COMPUTERNAME; pid = $PID; owner = $OwnerTag; utc = $utc } | ConvertTo-Json -Compress

    $acquiredMeta = $false
    for ($try = 0; $try -lt 2; $try++) {
        try {
            $fs = [System.IO.File]::Open($metaPath, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
            try {
                $bytes = [Text.Encoding]::UTF8.GetBytes($myMeta + "`n")
                $fs.Write($bytes, 0, $bytes.Length)
            } finally {
                $fs.Dispose()
            }
            $acquiredMeta = $true
            break
        } catch [System.IO.IOException] {
            $staleRecovered = $false
            if (Test-Path -LiteralPath $metaPath) {
                try {
                    $old = Get-Content -Raw -LiteralPath $metaPath -Encoding UTF8 | ConvertFrom-Json
                    if ($old.host -eq $env:COMPUTERNAME -and $old.pid) {
                        $alive = Get-Process -Id ([int]$old.pid) -ErrorAction SilentlyContinue
                        if ($null -eq $alive) {
                            $staleTmp = "$metaPath.stale.$PID"
                            Move-Item -LiteralPath $metaPath -Destination $staleTmp -Force -ErrorAction Stop
                            Remove-Item -LiteralPath $staleTmp -Force -ErrorAction SilentlyContinue
                            $staleRecovered = $true
                        }
                    }
                } catch {
                    $staleRecovered = $false
                }
            }
            if ($staleRecovered -and $try -eq 0) { continue }
            $info = if (Test-Path -LiteralPath $metaPath) { (Get-Content -Raw -LiteralPath $metaPath -Encoding UTF8).Trim() } else { 'contesa' }
            return "Lock _ue59 gia' occupato: $info"
        }
    }

    if (-not $acquiredMeta) { return "Impossibile acquisire lock atomico: $metaPath" }

    # Check human-readable IN-USO.txt (e.g. manual lock held by owner or Claude session without .lock)
    if (Test-Path -LiteralPath $LockPath) {
        foreach ($line in [IO.File]::ReadAllLines($LockPath, [Text.Encoding]::UTF8)) {
            $t = $line.Trim()
            if (-not $t) { continue }
            if ($t -match '^IN-USO:') {
                Remove-Item -LiteralPath $metaPath -Force -ErrorAction SilentlyContinue
                return "Lock manuale _ue59 gia' occupato: $t"
            }
            break
        }
    }
    [IO.File]::WriteAllText($LockPath, "IN-USO: $OwnerTag (host=$env:COMPUTERNAME pid=$PID) dal $utc UTC`n", [Text.Encoding]::UTF8)
    return $null
}

function Release-Ue59Lock {
    param([string]$LockPath, [string]$OwnerTag)
    if (-not $LockPath) { return }
    $utc = (Get-Date).ToUniversalTime().ToString('yyyy-MM-dd HH:mm')
    $dir = Split-Path $LockPath -Parent
    if ($dir) { New-Item -ItemType Directory -Force $dir | Out-Null }
    [IO.File]::WriteAllText($LockPath, "LIBERO (ultimo: $OwnerTag $utc UTC)`n", [Text.Encoding]::UTF8)
    Remove-Item -LiteralPath "$LockPath.lock" -Force -ErrorAction SilentlyContinue
}

function Invoke-ChildCommandWithTimeout {
    param([string]$CmdLine, [string]$WorkDir, [int]$HardTimeoutSec, [string]$OutLog)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = 'cmd.exe'
    $psi.Arguments = "/d /s /c $CmdLine"
    $psi.WorkingDirectory = $WorkDir
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    # Mandatory #6: worker children must never inherit owner unlock bypass.
    if ($psi.EnvironmentVariables.ContainsKey('GSTN_OWNER_UNLOCK')) {
        $psi.EnvironmentVariables.Remove('GSTN_OWNER_UNLOCK')
    }
    $proc = [System.Diagnostics.Process]::Start($psi)
    if (-not $proc.WaitForExit($HardTimeoutSec * 1000)) {
        Start-Process -FilePath 'taskkill.exe' -ArgumentList "/F /T /PID $($proc.Id)" -WindowStyle Hidden -Wait | Out-Null
        try { $proc.WaitForExit(3000) | Out-Null } catch {}
        [IO.File]::AppendAllText($OutLog, "`nESITO: TIMEOUT (${HardTimeoutSec}s, process tree terminato)`n", [Text.Encoding]::UTF8)
        return @{ ExitCode = 124; TimedOut = $true }
    }
    return @{ ExitCode = $proc.ExitCode; TimedOut = $false }
}

function Format-CsvField([string]$Value) {
    if ($null -eq $Value) { return '' }
    if ($Value.Contains('"') -or $Value.Contains(',') -or $Value.Contains("`n") -or $Value.Contains("`r")) {
        return '"' + ($Value -replace '"', '""') + '"'
    }
    return $Value
}

function Append-CostCsvRow {
    param(
        [string]$CsvPath, [string]$ThreadName, [string]$Obj, [string]$Mach,
        [string]$Prov, [string]$Mod, [string]$MinDesktop, [string]$TokReview,
        [string]$UsdEst, [string]$QuotaPct, [string]$Outcome
    )
    $header = 'data,thread,obiettivo,macchina,provider,modello,minuti_desktop,token_review,usd_stimati,quota_pro_pct,esito'
    $dir = Split-Path $CsvPath -Parent
    if ($dir) { New-Item -ItemType Directory -Force $dir | Out-Null }
    if (-not (Test-Path -LiteralPath $CsvPath)) {
        [IO.File]::WriteAllText($CsvPath, $header + "`n", [Text.Encoding]::UTF8)
    }
    $today = (Get-Date).ToString('yyyy-MM-dd')
    $fields = @(
        $today, $ThreadName, $Obj, $Mach, $Prov, $Mod,
        $MinDesktop, $TokReview, $UsdEst, $QuotaPct, $Outcome
    ) | ForEach-Object { Format-CsvField "$_" }
    [IO.File]::AppendAllText($CsvPath, ($fields -join ',') + "`n", [Text.Encoding]::UTF8)
}

if ($SelfTest) {
    $gs = Load-GuardSettings
    $tmp = Join-Path $env:TEMP 'invokeworker-selftest'
    if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force }
    New-Item -ItemType Directory -Force $tmp | Out-Null

    # 1. Routing table
    $rM0 = Get-WorkerRoute 'M0' $false $false
    $rM0L = Get-WorkerRoute 'M0' $true $false
    $rM1C = Get-WorkerRoute 'M1_CODE' $false $false
    $rM1A = Get-WorkerRoute 'M1_ANIM' $false $false
    $rM2 = Get-WorkerRoute 'M2' $false $false
    $rM3 = Get-WorkerRoute 'M3' $false $false
    $ok1 = ($rM0.Command -eq 'deepseek') -and ($rM0L.Command -eq 'local') -and ($rM1C.Command -eq 'deepseek') -and ($rM1A.Command -eq 'agy') -and ($rM2.Command -eq 'claude') -and ($rM3.Model -eq 'gemini-4-neon-high')

    # 2. Filesystem + text EULA guard + VRAM guard
    $epicDir = Join-Path $tmp 'repo_with_epic'
    $cleanDir = Join-Path $tmp 'repo_clean'
    New-Item -ItemType Directory -Force (Join-Path $epicDir 'Plugins\GASNPP') | Out-Null
    New-Item -ItemType Directory -Force (Join-Path $epicDir 'Source\GSTN') | Out-Null
    New-Item -ItemType Directory -Force (Join-Path $cleanDir 'Source\GSTN') | Out-Null
    $gFsBad  = Test-WorkerGuards $rM1C (Join-Path $epicDir 'Source\GSTN') 'say hi' '' $false $gs.EulaPatterns $gs.EulaDirs
    $gTxtBad = Test-WorkerGuards $rM1C $cleanDir 'Inspect Plugins/EngineOverrides/Foo.h' '' $false $gs.EulaPatterns $gs.EulaDirs
    $gOk     = Test-WorkerGuards $rM1C $cleanDir 'Fix Source/GSTN/Ability.cpp' '' $false $gs.EulaPatterns $gs.EulaDirs
    $gVram   = Test-WorkerGuards $rM0L $cleanDir 'Small grep' '' $true $gs.EulaPatterns $gs.EulaDirs
    $ok2 = ($null -ne $gFsBad) -and ($null -ne $gTxtBad) -and ($null -eq $gOk) -and ($null -ne $gVram)

    # 3. Atomic CreateNew lock + stale dead-PID recovery + manual IN-USO refusal + release
    $lk = Join-Path $tmp 'IN-USO.txt'
    $a1 = Acquire-Ue59Lock $lk 'test-obj (M1_CODE)'
    $a2 = Acquire-Ue59Lock $lk 'other-obj'
    try { throw 'simulated-crash' } catch { $null = $_ } finally { Release-Ue59Lock $lk 'test-obj' }
    $after = (Get-Content -LiteralPath $lk -Raw).Trim()
    # Stale dead-PID lock recovery on same host
    $deadPid = 9999999
    while (Get-Process -Id $deadPid -ErrorAction SilentlyContinue) { $deadPid++ }
    $staleJson = @{ host = $env:COMPUTERNAME; pid = $deadPid; owner = 'crashed'; utc = '2026-01-01' } | ConvertTo-Json -Compress
    [IO.File]::WriteAllText("$lk.lock", $staleJson, [Text.Encoding]::UTF8)
    $aStale = Acquire-Ue59Lock $lk 'recovered-obj'
    Release-Ue59Lock $lk 'recovered-obj'
    # Manual lock without .lock file must still block
    [IO.File]::WriteAllText($lk, "IN-USO: manual owner lock`n", [Text.Encoding]::UTF8)
    $aManual = Acquire-Ue59Lock $lk 'worker-x'
    $ok3 = ($null -eq $a1) -and ($null -ne $a2) -and ($after -like 'LIBERO*') -and ($null -eq $aStale) -and (-not (Test-Path "$lk.lock")) -and ($null -ne $aManual)

    # 4. Child helper strips GSTN_OWNER_UNLOCK and kills hanging tree on hard timeout (exit 124)
    $envCheckLog = Join-Path $tmp 'env_check.log'
    $toLog = Join-Path $tmp 'timeout_check.log'
    $oldUnlock = $env:GSTN_OWNER_UNLOCK
    $env:GSTN_OWNER_UNLOCK = '1'
    try {
        $psExe = (Get-Command powershell.exe).Source
        Set-Content (Join-Path $tmp 'chk_env.ps1') "Write-Output `"UNLOCK=`$env:GSTN_OWNER_UNLOCK`""
        $resEnv = Invoke-ChildCommandWithTimeout "`"`"$psExe`" -NoProfile -File `"$tmp\chk_env.ps1`" > `"$envCheckLog`" 2>&1`"" $tmp 10 $envCheckLog
        $envOut = (Get-Content -Raw -LiteralPath $envCheckLog).Trim()
        Set-Content (Join-Path $tmp 'hang.ps1') 'Start-Sleep -Seconds 60'
        $resTo = Invoke-ChildCommandWithTimeout "`"`"$psExe`" -NoProfile -File `"$tmp\hang.ps1`" > `"$toLog`" 2>&1`"" $tmp 2 $toLog
        $toOut = Get-Content -Raw -LiteralPath $toLog
        $ok4 = ($resEnv.ExitCode -eq 0) -and ($envOut -eq 'UNLOCK=') -and ($resTo.ExitCode -eq 124) -and $resTo.TimedOut -and ($toOut -match 'ESITO: TIMEOUT')
    } finally {
        if ($null -eq $oldUnlock) { Remove-Item Env:GSTN_OWNER_UNLOCK -ErrorAction SilentlyContinue }
        else { $env:GSTN_OWNER_UNLOCK = $oldUnlock }
    }

    # 5. 11-column costi.csv append
    $csv = Join-Path $tmp 'costi.csv'
    Append-CostCsvRow $csv 'worker-test' 'obj-1, ok' 'fisso' 'deepseek' 'deepseek-v4-pro' '0.25' '0' '0.02' '' 'PASS'
    $rows = @(Import-Csv -LiteralPath $csv)
    $cols = @($rows[0].PSObject.Properties.Name)
    $ok5 = ($rows.Count -eq 1) -and ($cols.Count -eq 11) -and ($rows[0].provider -eq 'deepseek') -and ($rows[0].minuti_desktop -eq '0.25')

    Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
    $allOk = $ok1 -and $ok2 -and $ok3 -and $ok4 -and $ok5
    Write-Output "ESITO SELFTEST: $(if ($allOk) {'PASS'} else {'FAIL'})"
    Write-Output "1 Route:$ok1 | 2 Guards:$ok2 | 3 Lock:$ok3 | 4 Env+Timeout:$ok4 | 5 CostCsv:$ok5"
    exit $(if ($allOk) { 0 } else { 1 })
}

$gs = Load-GuardSettings
if (-not $Cwd) { $Cwd = $Root }
if (-not $Ue59LockPath) { $Ue59LockPath = $gs.Ue59LockPath }
if (-not $CostCsv) { $CostCsv = Join-Path $Root 'handoffs\costi.csv' }

$promptBody = $Prompt
if ($PromptFile) {
    if (-not (Test-Path -LiteralPath $PromptFile)) { Write-Output "ESITO: RIFIUTATO - PromptFile mancante: $PromptFile"; exit 2 }
    $promptBody = Get-Content -Raw -LiteralPath $PromptFile -Encoding UTF8
}
if (-not $promptBody -and $Brief) {
    if (Test-Path -LiteralPath $Brief) {
        $promptBody = "Esegui esattamente il brief in ${Brief}:`n`n" + (Get-Content -Raw -LiteralPath $Brief -Encoding UTF8)
    } else {
        $promptBody = $Brief
    }
}
if (-not $promptBody) { Write-Output 'ESITO: RIFIUTATO - serve -Prompt, -PromptFile o -Brief'; exit 2 }

$route = Get-WorkerRoute $Tier ([bool]$UseLocal) ([bool]$AllowAgyFallback)
$edOpen = @(Get-Process -Name 'UnrealEditor*' -ErrorAction SilentlyContinue).Count -gt 0
$guardErr = Test-WorkerGuards $route $Cwd $promptBody $Brief $edOpen $gs.EulaPatterns $gs.EulaDirs
if ($guardErr) {
    Write-Output "ESITO: RIFIUTATO - $guardErr"
    exit 2
}

$stamp = (Get-Date).ToString('yyyyMMdd-HHmmss')
$safeObj = ($Obiettivo -replace '[^A-Za-z0-9_.-]', '_')
$logDir = Join-Path $Root 'Saved\Logs\ci'
New-Item -ItemType Directory -Force $logDir | Out-Null
$logPath = Join-Path $logDir "worker_${safeObj}_${stamp}.log"

if ($DryRun) {
    Write-Output "ESITO: DRYRUN tier=$Tier provider=$($route.Provider) model=$($route.Model) cwd=$Cwd"
    Write-Output "log=$logPath (nessuna chiamata esterna eseguita)"
    exit 0
}

$lockTag = "$Obiettivo ($Tier pid=$PID)"
$lockHeld = $false
if ($UsesUe59) {
    $lkErr = Acquire-Ue59Lock $Ue59LockPath $lockTag
    if ($lkErr) {
        Write-Output "ESITO: RIFIUTATO - $lkErr"
        exit 2
    }
    $lockHeld = $true
}

$tmpPrompt = Join-Path $logDir "prompt_${safeObj}_${stamp}.txt"
[IO.File]::WriteAllText($tmpPrompt, $promptBody, [Text.Encoding]::UTF8)

$exitCode = 1
$timedOut = $false
$usd = ''
$t0 = Get-Date
try {
    $cliArgs = @('-u', $BridgeCli, $route.Command, '--cwd', $Cwd, '--timeout', "$TimeoutSec", '--prompt-file', $tmpPrompt) + $route.Args
    if ($MaxUsd -gt 0 -and ($route.Command -eq 'deepseek' -or $route.Command -eq 'claude')) {
        $cliArgs += @('--max-budget-usd', "$MaxUsd")
    }
    $quotedArgs = ($cliArgs | ForEach-Object { "`"$_`"" }) -join ' '
    $cmdLine = "`"python $quotedArgs > `"$logPath`" 2>&1`""
    $runRes = Invoke-ChildCommandWithTimeout $cmdLine $Cwd ($TimeoutSec + 60) $logPath
    $exitCode = $runRes.ExitCode
    $timedOut = $runRes.TimedOut
    if (Test-Path -LiteralPath $logPath) {
        $txt = Get-Content -Raw -LiteralPath $logPath -Encoding UTF8
        if ($txt -match 'cost:\s*\$?([0-9]+(?:\.[0-9]+)?)') { $usd = $Matches[1] }
    }
} finally {
    Remove-Item -LiteralPath $tmpPrompt -Force -ErrorAction SilentlyContinue
    if ($lockHeld) { Release-Ue59Lock $Ue59LockPath $lockTag }
}

$secs = [int]((Get-Date) - $t0).TotalSeconds
$minDesk = if ($Machine -eq 'fisso') { [string][math]::Round($secs / 60.0, 2) } else { '0' }
$outcome = if ($timedOut) { 'ESITO: TIMEOUT' } elseif ($exitCode -eq 0) { 'PASS' } else { "FAIL (exit=$exitCode)" }
Append-CostCsvRow $CostCsv "worker-$Tier" $Obiettivo $Machine $route.Provider $route.Model $minDesk '0' $usd '' $outcome

Write-Output "ESITO WORKER: $outcome tier=$Tier provider=$($route.Provider) dur=${secs}s"
Write-Output "log=$logPath"
exit $exitCode

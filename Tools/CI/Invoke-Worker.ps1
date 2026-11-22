<#
Invoke-Worker.ps1 - deterministic multi-model worker router (Regola 3 / Tier M0-M3).
Routes tasks to DeepSeek / Antigravity / Claude Pro / local Inkling via agent-bridge cli.py,
enforces EULA Epic guard, local VRAM guard, atomic _ue59/IN-USO.txt lock, and logs to costi.csv.
Prints at most 5 lines.
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

function Load-EulaPatterns {
    if (Test-Path -LiteralPath $GuardConfigPath) {
        $cfg = Get-Content -Raw -LiteralPath $GuardConfigPath -Encoding UTF8 | ConvertFrom-Json
        if ($cfg.epic_eula_patterns) { return @($cfg.epic_eula_patterns) }
    }
    return @('plugins/engineoverrides', 'plugins/gasnpp', 'program files/epic games', 'epic games/ue_', 'ue_5.9', 'ue_5.8')
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

function Test-WorkerGuards {
    param(
        $Route,
        [string]$WorkDir,
        [string]$PromptText,
        [string]$BriefText,
        [bool]$IsEditorOpen,
        [string[]]$EulaPatterns
    )
    if ($Route.Command -eq 'deepseek') {
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
    if (Test-Path -LiteralPath $LockPath) {
        foreach ($line in [IO.File]::ReadAllLines($LockPath, [Text.Encoding]::UTF8)) {
            $t = $line.Trim()
            if (-not $t) { continue }
            if ($t -match '^IN-USO:') {
                return "Lock _ue59 gia' occupato: $t"
            }
            break
        }
    }
    $dir = Split-Path $LockPath -Parent
    if ($dir) { New-Item -ItemType Directory -Force $dir | Out-Null }
    $utc = (Get-Date).ToUniversalTime().ToString('yyyy-MM-dd HH:mm')
    [IO.File]::WriteAllText($LockPath, "IN-USO: $OwnerTag dal $utc UTC`n", [Text.Encoding]::UTF8)
    return $null
}

function Release-Ue59Lock {
    param([string]$LockPath, [string]$OwnerTag)
    if (-not $LockPath) { return }
    $utc = (Get-Date).ToUniversalTime().ToString('yyyy-MM-dd HH:mm')
    $dir = Split-Path $LockPath -Parent
    if ($dir) { New-Item -ItemType Directory -Force $dir | Out-Null }
    [IO.File]::WriteAllText($LockPath, "LIBERO (ultimo: $OwnerTag $utc UTC)`n", [Text.Encoding]::UTF8)
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
        [string]$CsvPath,
        [string]$ThreadName,
        [string]$Obj,
        [string]$Mach,
        [string]$Prov,
        [string]$Mod,
        [string]$MinDesktop,
        [string]$TokReview,
        [string]$UsdEst,
        [string]$QuotaPct,
        [string]$Outcome
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
    $eulaPats = Load-EulaPatterns
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

    # 2. EULA guard + local VRAM guard
    $gBad = Test-WorkerGuards $rM1C $tmp 'Check Plugins/GASNPP/Foo.cpp' '' $false $eulaPats
    $gOk  = Test-WorkerGuards $rM1C $tmp 'Fix Source/GASP_Giustino2/Private/GAS/Ability.cpp' '' $false $eulaPats
    $gVram = Test-WorkerGuards $rM0L $tmp 'Small grep' '' $true $eulaPats
    $ok2 = ($null -ne $gBad) -and ($null -eq $gOk) -and ($null -ne $gVram)

    # 3. Atomic _ue59 lock acquire / refuse / release in finally
    $lk = Join-Path $tmp 'IN-USO.txt'
    $a1 = Acquire-Ue59Lock $lk 'test-obj (M1_CODE)'
    $a2 = Acquire-Ue59Lock $lk 'other-obj'
    try { throw 'simulated-crash' } catch { $null = $_ } finally { Release-Ue59Lock $lk 'test-obj' }
    $after = (Get-Content -LiteralPath $lk -Raw).Trim()
    $ok3 = ($null -eq $a1) -and ($null -ne $a2) -and ($after -like 'LIBERO*')

    # 4. 11-column costi.csv append
    $csv = Join-Path $tmp 'costi.csv'
    Append-CostCsvRow $csv 'worker-test' 'obj-1, ok' 'fisso' 'deepseek' 'deepseek-v4-pro' '0' '0' '0.02' '' 'PASS'
    $rows = @(Import-Csv -LiteralPath $csv)
    $cols = @($rows[0].PSObject.Properties.Name)
    $ok4 = ($rows.Count -eq 1) -and ($cols.Count -eq 11) -and ($rows[0].provider -eq 'deepseek') -and ($rows[0].obiettivo -eq 'obj-1, ok')

    Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
    $allOk = $ok1 -and $ok2 -and $ok3 -and $ok4
    Write-Output "ESITO SELFTEST: $(if ($allOk) {'PASS'} else {'FAIL'})"
    Write-Output "1 Route:$ok1 | 2 Guards:$ok2 | 3 Lock:$ok3 | 4 CostCsv:$ok4"
    exit $(if ($allOk) { 0 } else { 1 })
}

if (-not $Cwd) { $Cwd = $Root }
if (-not $Ue59LockPath) { $Ue59LockPath = Join-Path $Root '_ue59\IN-USO.txt' }
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
$eulaPats = Load-EulaPatterns
$edOpen = @(Get-Process -Name 'UnrealEditor*' -ErrorAction SilentlyContinue).Count -gt 0
$guardErr = Test-WorkerGuards $route $Cwd $promptBody $Brief $edOpen $eulaPats
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
    Write-Output "ESITO: DRYRUN tier=$Tier provider=$($route.Provider) model=$($route.Model) obj=$Obiettivo"
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
$usd = ''
$t0 = Get-Date
try {
    $cliArgs = @('-u', $BridgeCli, $route.Command, '--cwd', $Cwd, '--timeout', "$TimeoutSec", '--prompt-file', $tmpPrompt) + $route.Args
    if ($MaxUsd -gt 0 -and ($route.Command -eq 'deepseek' -or $route.Command -eq 'claude')) {
        $cliArgs += @('--max-budget-usd', "$MaxUsd")
    }
    $quotedArgs = ($cliArgs | ForEach-Object { "`"$_`"" }) -join ' '
    $cmdLine = "`"python $quotedArgs > `"$logPath`" 2>&1`""
    $p = Start-Process -FilePath 'cmd.exe' -ArgumentList '/d', '/s', '/c', $cmdLine -WorkingDirectory $Cwd -WindowStyle Hidden -Wait -PassThru
    $exitCode = $p.ExitCode
    if (Test-Path -LiteralPath $logPath) {
        $txt = Get-Content -Raw -LiteralPath $logPath -Encoding UTF8
        if ($txt -match 'cost:\s*\$?([0-9]+(?:\.[0-9]+)?)') { $usd = $Matches[1] }
    }
} finally {
    Remove-Item -LiteralPath $tmpPrompt -Force -ErrorAction SilentlyContinue
    if ($lockHeld) { Release-Ue59Lock $Ue59LockPath $lockTag }
}

$secs = [int]((Get-Date) - $t0).TotalSeconds
$outcome = if ($exitCode -eq 0) { 'PASS' } else { "FAIL (exit=$exitCode)" }
Append-CostCsvRow $CostCsv "worker-$Tier" $Obiettivo $Machine $route.Provider $route.Model '0' '0' $usd '' $outcome

Write-Output "ESITO WORKER: $outcome tier=$Tier provider=$($route.Provider) dur=${secs}s"
Write-Output "log=$logPath"
exit $exitCode

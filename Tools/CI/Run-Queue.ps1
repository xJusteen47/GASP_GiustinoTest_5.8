<#
Run-Queue.ps1 - deterministic queue dispatcher (Regola 15f, 0 LLM tokens for scheduling).
Reads handoffs/queue.json, skips blocked/unmet items, requires explicit isolated -Cwd per item,
does NOT count exit 2 (RIFIUTATO) as a failed attempt, enforces R5-bis (needs_diagnosis after max_attempts),
launches a fresh Invoke-Worker.ps1 per runnable item, and persists queue.json atomically.
Prints at most 5 lines.
#>
[CmdletBinding()]
param(
    [string]$QueueFile,
    [ValidateSet('fisso', 'portatile', 'cloud')][string]$Machine = 'fisso',
    [int]$MaxItems = 1,
    [string]$WorktreeRoot = 'C:\Users\jacop\.gemini\antigravity\worktrees\GASP_Giustino2',
    [switch]$DryRun,
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$WorkerScript = Join-Path $PSScriptRoot 'Invoke-Worker.ps1'

function Save-QueueAtomic {
    param([string]$Path, $QueueObj)
    $tmp = "$Path.tmp.$PID"
    $json = $QueueObj | ConvertTo-Json -Depth 10
    [IO.File]::WriteAllText($tmp, $json + "`n", [Text.Encoding]::UTF8)
    Move-Item -LiteralPath $tmp -Destination $Path -Force
}

function Resolve-ItemCwd {
    param($Item)
    $wtVal = ''
    if ($Item.PSObject.Properties['worktree'] -and $Item.worktree) {
        $wtVal = "$($Item.worktree)".Trim()
    }
    if (-not $wtVal) {
        return @{ Ok = $false; Cwd = ''; Reason = 'worktree esplicito mancante' }
    }
    $leaf = Split-Path $wtVal -Leaf
    if (-not $Item.uses_ue59 -and $leaf -eq 'GASP_Giustino2_ue59') {
        return @{ Ok = $false; Cwd = ''; Reason = 'voce senza uses_ue59 non puo usare GASP_Giustino2_ue59' }
    }
    return @{ Ok = $true; Cwd = $wtVal; Reason = '' }
}

function Invoke-QueuePass {
    param(
        [string]$Path,
        [string]$TargetMachine,
        [int]$Limit,
        [bool]$IsDryRun,
        [string]$WorkerPath
    )
    if (-not (Test-Path -LiteralPath $Path)) {
        return @{ Exit = 1; Lines = @("ESITO QUEUE: FAIL - file coda mancante: $Path") }
    }
    $q = Get-Content -Raw -LiteralPath $Path -Encoding UTF8 | ConvertFrom-Json
    $items = @($q.items)
    $byId = @{}
    foreach ($it in $items) { $byId[$it.id] = $it }

    $ran = 0; $diag = 0; $skipped = 0; $deferred = 0; $summary = @()
    foreach ($it in $items) {
        if ($it.machine -ne $TargetMachine) { continue }
        if ($it.status -ne 'pending') { $skipped++; continue }

        $maxAtt = if ($it.PSObject.Properties['max_attempts'] -and $it.max_attempts) { [int]$it.max_attempts } else { 2 }
        $curAtt = if ($it.PSObject.Properties['attempts'] -and $null -ne $it.attempts) { [int]$it.attempts } else { 0 }
        if ($curAtt -ge $maxAtt) {
            $it.status = 'needs_diagnosis'
            $it.blocked_reason = "R5-bis: raggiunti $curAtt/$maxAtt tentativi su $($it.obiettivo); serve Diagnosi di Sistema (M2)"
            $diag++
            $summary += "R5-bis->$($it.id)"
            continue
        }

        $depsOk = $true
        foreach ($dep in @($it.depends_on)) {
            if (-not $byId.ContainsKey($dep) -or $byId[$dep].status -ne 'done') {
                $depsOk = $false; break
            }
        }
        if (-not $depsOk) { $skipped++; continue }

        $cwdRes = Resolve-ItemCwd $it
        if (-not $cwdRes.Ok) {
            $deferred++
            $summary += "deferred($($it.id):$($cwdRes.Reason))"
            continue
        }

        if ($ran -ge $Limit) { break }

        $tier = if ($it.tier) { "$($it.tier)" } else { 'M1_CODE' }
        $prompt = "Voce coda '$($it.id)' (obiettivo: $($it.obiettivo), branch: $($it.branch)). Note: $($it.notes)"
        $wParams = @{
            Tier      = $tier
            Obiettivo = "$($it.obiettivo)"
            Prompt    = $prompt
            Cwd       = $cwdRes.Cwd
            Machine   = $TargetMachine
        }
        if ($it.uses_ue59) { $wParams['UsesUe59'] = $true }
        if ($IsDryRun) { $wParams['DryRun'] = $true }

        $null = & $WorkerPath @wParams
        $wRc = $LASTEXITCODE
        if (-not $IsDryRun) {
            $utc = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
            if ($wRc -eq 2) {
                # Guard/lock refusal is NOT a failed attempt (do not burn R5-bis budget).
                if (-not $it.PSObject.Properties['last_refusal']) { $it | Add-Member last_refusal '' }
                $it.last_refusal = "$utc rc=2"
                $deferred++
                $summary += "deferred($($it.id):rc=2)"
                continue
            }
            $it.attempts = $curAtt + 1
            if (-not $it.PSObject.Properties['last_run']) { $it | Add-Member last_run '' }
            $it.last_run = $utc
            if ($wRc -eq 0) {
                $it.status = 'done'
            } elseif ($it.attempts -ge $maxAtt) {
                $it.status = 'needs_diagnosis'
                $it.blocked_reason = "R5-bis: falliti $($it.attempts)/$maxAtt tentativi (exit=$wRc)"
                $diag++
            }
        }
        $ran++
        $summary += "$($it.id):rc=$wRc"
    }

    $q.updated = (Get-Date).ToString('yyyy-MM-dd')
    Save-QueueAtomic $Path $q

    $sTxt = if ($summary.Count -gt 0) { $summary -join ', ' } else { 'nessuna voce eseguibile' }
    return @{
        Exit  = 0
        Lines = @(
            "ESITO QUEUE: OK machine=$TargetMachine ran=$ran deferred=$deferred needs_diagnosis=$diag skipped=$skipped",
            "dettaglio: $sTxt"
        )
    }
}

if ($SelfTest) {
    $tmp = Join-Path $env:TEMP 'runqueue-selftest'
    if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force }
    New-Item -ItemType Directory -Force $tmp | Out-Null

    $mockWorker = Join-Path $tmp 'mock_worker.ps1'
    $cwdLog = Join-Path $tmp 'worker_cwds.log'
    Set-Content -Path $mockWorker -Encoding UTF8 -Value @"
param([string]`$Tier, [string]`$Obiettivo, [string]`$Prompt, [string]`$Cwd, [string]`$Machine, [switch]`$UsesUe59, [switch]`$DryRun)
Add-Content -LiteralPath '$cwdLog' -Value "`$Obiettivo|`$Cwd"
if (`$Obiettivo -eq 'obj-refuse') { exit 2 }
if (`$Obiettivo -eq 'obj-timeout') { exit 124 }
exit 0
"@

    $safeWt = Join-Path $tmp 'wt-safe'
    New-Item -ItemType Directory -Force $safeWt | Out-Null
    $qPath = Join-Path $tmp 'queue.json'
    $qData = @{
        schema_version = 1
        updated        = '2026-11-22'
        mode           = 'prova_b'
        items          = @(
            @{ id = 'item-a'; obiettivo = 'a'; machine = 'fisso'; tier = 'M0'; status = 'done'; uses_ue59 = $false; worktree = $safeWt; attempts = 1; max_attempts = 2; depends_on = @(); blocked_reason = $null; branch = 'claude/a'; notes = 'done' },
            @{ id = 'item-b'; obiettivo = 'b'; machine = 'fisso'; tier = 'M0'; status = 'blocked'; uses_ue59 = $false; worktree = $safeWt; attempts = 0; max_attempts = 2; depends_on = @(); blocked_reason = 'owner'; branch = 'claude/b'; notes = 'blocked' },
            @{ id = 'item-c'; obiettivo = 'c'; machine = 'fisso'; tier = 'M0'; status = 'pending'; uses_ue59 = $false; worktree = $safeWt; attempts = 0; max_attempts = 2; depends_on = @('missing-dep'); blocked_reason = $null; branch = 'claude/c'; notes = 'unmet' },
            @{ id = 'item-badcwd'; obiettivo = 'badcwd'; machine = 'fisso'; tier = 'M0'; status = 'pending'; uses_ue59 = $false; worktree = 'C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_ue59'; attempts = 0; max_attempts = 2; depends_on = @(); blocked_reason = $null; branch = 'claude/x'; notes = 'bad cwd' },
            @{ id = 'item-refuse'; obiettivo = 'obj-refuse'; machine = 'fisso'; tier = 'M0'; status = 'pending'; uses_ue59 = $false; worktree = $safeWt; attempts = 0; max_attempts = 2; depends_on = @(); blocked_reason = $null; branch = 'claude/r'; notes = 'refuse' },
            @{ id = 'item-to'; obiettivo = 'obj-timeout'; machine = 'fisso'; tier = 'M1_CODE'; status = 'pending'; uses_ue59 = $false; worktree = $safeWt; attempts = 1; max_attempts = 2; depends_on = @(); blocked_reason = $null; branch = 'claude/to'; notes = 'timeout->r5bis' },
            @{ id = 'item-ok'; obiettivo = 'obj-ok'; machine = 'fisso'; tier = 'M1_CODE'; status = 'pending'; uses_ue59 = $false; worktree = $safeWt; attempts = 0; max_attempts = 2; depends_on = @('item-a'); blocked_reason = $null; branch = 'claude/ok'; notes = 'runnable' },
            @{ id = 'item-f'; obiettivo = 'f'; machine = 'portatile'; tier = 'M0'; status = 'pending'; uses_ue59 = $false; worktree = $safeWt; attempts = 0; max_attempts = 2; depends_on = @(); blocked_reason = $null; branch = 'claude/f'; notes = 'laptop' }
        )
    }
    Save-QueueAtomic $qPath $qData

    # Run with Limit=2 (not dryrun): item-badcwd deferred without running, item-refuse rc=2 (attempts stays 0!),
    # item-to rc=124 (attempts 1->2 -> needs_diagnosis), item-ok rc=0 (attempts 0->1 -> done).
    $res = Invoke-QueuePass -Path $qPath -TargetMachine 'fisso' -Limit 2 -IsDryRun $false -WorkerPath $mockWorker
    $after = Get-Content -Raw -LiteralPath $qPath -Encoding UTF8 | ConvertFrom-Json
    $map = @{}; foreach ($x in $after.items) { $map[$x.id] = $x }
    $cwds = @(Get-Content -LiteralPath $cwdLog)

    $ok1 = ($res.Exit -eq 0)
    $ok2 = ($map['item-b'].status -eq 'blocked') -and ($map['item-c'].status -eq 'pending') -and ($map['item-badcwd'].attempts -eq 0) -and (-not ($cwds -match 'badcwd'))
    $ok3 = ($map['item-refuse'].attempts -eq 0) -and ($map['item-refuse'].status -eq 'pending')
    $ok4 = ($map['item-to'].attempts -eq 2) -and ($map['item-to'].status -eq 'needs_diagnosis')
    $ok5 = ($map['item-ok'].attempts -eq 1) -and ($map['item-ok'].status -eq 'done') -and ($cwds -contains "obj-ok|$safeWt")

    Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
    $allOk = $ok1 -and $ok2 -and $ok3 -and $ok4 -and $ok5
    Write-Output "ESITO SELFTEST: $(if ($allOk) {'PASS'} else {'FAIL'})"
    Write-Output "1 Ok:$ok1 | 2 BadCwd+Skip:$ok2 | 3 Rc2NoCount:$ok3 | 4 TimeoutR5bis:$ok4 | 5 ExplicitCwd:$ok5"
    exit $(if ($allOk) { 0 } else { 1 })
}

if (-not $QueueFile) { $QueueFile = Join-Path $Root 'handoffs\queue.json' }
$res = Invoke-QueuePass -Path $QueueFile -TargetMachine $Machine -Limit $MaxItems -IsDryRun ([bool]$DryRun) -WorkerPath $WorkerScript
$res.Lines | Select-Object -First 5 | ForEach-Object { Write-Output $_ }
exit $res.Exit

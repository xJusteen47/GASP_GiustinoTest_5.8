<#
Run-Queue.ps1 - deterministic queue dispatcher (Regola 15f, 0 LLM tokens for scheduling).
Reads handoffs/queue.json, skips blocked/unmet items, enforces R5-bis (needs_diagnosis after max_attempts),
launches a fresh Invoke-Worker.ps1 per runnable item, and persists queue.json atomically.
Prints at most 5 lines.
#>
[CmdletBinding()]
param(
    [string]$QueueFile,
    [ValidateSet('fisso', 'portatile', 'cloud')][string]$Machine = 'fisso',
    [int]$MaxItems = 1,
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

    $ran = 0; $diag = 0; $skipped = 0; $summary = @()
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

        if ($ran -ge $Limit) { break }

        $tier = if ($it.tier) { "$($it.tier)" } else { 'M1_CODE' }
        $prompt = "Voce coda '$($it.id)' (obiettivo: $($it.obiettivo), branch: $($it.branch)). Note: $($it.notes)"
        $wParams = @{
            Tier      = $tier
            Obiettivo = "$($it.obiettivo)"
            Prompt    = $prompt
            Machine   = $TargetMachine
        }
        if ($it.uses_ue59) { $wParams['UsesUe59'] = $true }
        if ($IsDryRun) { $wParams['DryRun'] = $true }

        $wOut = & $WorkerPath @wParams
        $wRc = $LASTEXITCODE
        if (-not $IsDryRun) {
            $it.attempts = $curAtt + 1
            if (-not $it.PSObject.Properties['last_run']) { $it | Add-Member last_run '' }
            $it.last_run = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
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
            "ESITO QUEUE: OK machine=$TargetMachine ran=$ran needs_diagnosis=$diag skipped=$skipped",
            "dettaglio: $sTxt"
        )
    }
}

if ($SelfTest) {
    $tmp = Join-Path $env:TEMP 'runqueue-selftest'
    if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force }
    New-Item -ItemType Directory -Force $tmp | Out-Null

    $qPath = Join-Path $tmp 'queue.json'
    $qData = @{
        schema_version = 1
        updated        = '2026-11-22'
        mode           = 'prova_b'
        items          = @(
            @{ id = 'item-a'; obiettivo = 'a'; machine = 'fisso'; tier = 'M0'; status = 'done'; uses_ue59 = $false; attempts = 1; max_attempts = 2; depends_on = @(); blocked_reason = $null; branch = 'claude/a'; notes = 'done' },
            @{ id = 'item-b'; obiettivo = 'b'; machine = 'fisso'; tier = 'M0'; status = 'blocked'; uses_ue59 = $false; attempts = 0; max_attempts = 2; depends_on = @(); blocked_reason = 'owner'; branch = 'claude/b'; notes = 'blocked' },
            @{ id = 'item-c'; obiettivo = 'c'; machine = 'fisso'; tier = 'M0'; status = 'pending'; uses_ue59 = $false; attempts = 0; max_attempts = 2; depends_on = @('missing-dep'); blocked_reason = $null; branch = 'claude/c'; notes = 'unmet' },
            @{ id = 'item-d'; obiettivo = 'd'; machine = 'fisso'; tier = 'M1_CODE'; status = 'pending'; uses_ue59 = $false; attempts = 2; max_attempts = 2; depends_on = @(); blocked_reason = $null; branch = 'claude/d'; notes = 'r5bis' },
            @{ id = 'item-e'; obiettivo = 'e'; machine = 'fisso'; tier = 'M1_CODE'; status = 'pending'; uses_ue59 = $false; attempts = 0; max_attempts = 2; depends_on = @('item-a'); blocked_reason = $null; branch = 'claude/e'; notes = 'runnable' },
            @{ id = 'item-f'; obiettivo = 'f'; machine = 'portatile'; tier = 'M0'; status = 'pending'; uses_ue59 = $false; attempts = 0; max_attempts = 2; depends_on = @(); blocked_reason = $null; branch = 'claude/f'; notes = 'laptop' }
        )
    }
    Save-QueueAtomic $qPath $qData

    $res = Invoke-QueuePass -Path $qPath -TargetMachine 'fisso' -Limit 1 -IsDryRun $true -WorkerPath $WorkerScript
    $after = Get-Content -Raw -LiteralPath $qPath -Encoding UTF8 | ConvertFrom-Json
    $map = @{}; foreach ($x in $after.items) { $map[$x.id] = $x }

    $ok1 = ($res.Exit -eq 0)
    $ok2 = ($map['item-b'].status -eq 'blocked') -and ($map['item-c'].status -eq 'pending')
    $ok3 = ($map['item-d'].status -eq 'needs_diagnosis')
    $ok4 = ($res.Lines[1] -match 'item-e:rc=0') -and ($map['item-f'].status -eq 'pending')

    Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
    $allOk = $ok1 -and $ok2 -and $ok3 -and $ok4
    Write-Output "ESITO SELFTEST: $(if ($allOk) {'PASS'} else {'FAIL'})"
    Write-Output "1 Run:$ok1 | 2 Skip:$ok2 | 3 R5bis:$ok3 | 4 Select:$ok4"
    exit $(if ($allOk) { 0 } else { 1 })
}

if (-not $QueueFile) { $QueueFile = Join-Path $Root 'handoffs\queue.json' }
$res = Invoke-QueuePass -Path $QueueFile -TargetMachine $Machine -Limit $MaxItems -IsDryRun ([bool]$DryRun) -WorkerPath $WorkerScript
$res.Lines | Select-Object -First 5 | ForEach-Object { Write-Output $_ }
exit $res.Exit

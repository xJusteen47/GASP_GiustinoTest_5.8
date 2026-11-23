<#
Test-GitGuard.ps1 - deterministic git pre-commit / pre-push guard (Rules 13b, 15g, EULA/secrets).
Uses --no-renames -z with UTF-8 output for staged paths, and merge-base --is-ancestor against force-push.
Reads patterns from Tools/CI/protected-paths.json and locked assets from docs/notes/anim-approvate.md.
Bypass (owner only): set env GSTN_OWNER_UNLOCK=1 (secrets are NEVER bypassed).
Prints at most 5 lines. Exit 0 PASS, 2 BLOCKED.
#>
[CmdletBinding()]
param(
    [ValidateSet('Commit', 'Push')][string]$Mode = 'Commit',
    [string]$RepoRoot,
    [string]$TargetRef = '',
    [string]$LocalSha = '',
    [string]$RemoteSha = '',
    [string[]]$StagedFiles,
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'
$GuardConfigPath = Join-Path $PSScriptRoot 'protected-paths.json'

function Load-GuardConfig([string]$ConfigPath) {
    if (-not (Test-Path -LiteralPath $ConfigPath)) {
        throw "Config mancante: $ConfigPath"
    }
    return (Get-Content -Raw -LiteralPath $ConfigPath -Encoding UTF8 | ConvertFrom-Json)
}

function Get-StagedFilesNullDelimited([string]$Root) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = 'git'
    $psi.Arguments = "-C `"$Root`" -c core.quotepath=false diff --cached --name-only --no-renames -z"
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $psi.StandardOutputEncoding = [Text.Encoding]::UTF8
    $psi.StandardErrorEncoding = [Text.Encoding]::UTF8
    $proc = [System.Diagnostics.Process]::Start($psi)
    $stdout = $proc.StandardOutput.ReadToEnd()
    $stderr = $proc.StandardError.ReadToEnd()
    $proc.WaitForExit()
    if ($proc.ExitCode -ne 0) {
        throw "git diff --cached failed ($($proc.ExitCode)): $stderr"
    }
    $out = @()
    foreach ($part in $stdout.Split([char]0)) {
        if ($part -ne '') { $out += $part }
    }
    return $out
}

function Get-LockedAssets([string]$Root) {
    $locked = @()
    $doc = Join-Path $Root 'docs\notes\anim-approvate.md'
    if (-not (Test-Path -LiteralPath $doc)) { return $locked }
    foreach ($line in [IO.File]::ReadAllLines($doc, [Text.Encoding]::UTF8)) {
        $t = $line.Trim()
        if ($t -notmatch '^\|') { continue }
        if ($t -match '^\|\s*-+') { continue }
        foreach ($m in [regex]::Matches($t, '\b(AS_GSTN_[A-Za-z0-9_]+)\b')) {
            $locked += [pscustomobject]@{ Kind = 'asset'; Name = $m.Groups[1].Value }
        }
        foreach ($m in [regex]::Matches($t, '([A-Za-z0-9_]+\.casc)\b')) {
            $locked += [pscustomobject]@{ Kind = 'casc'; Name = $m.Groups[1].Value }
        }
    }
    return $locked
}

function Test-BranchProtected([string]$Branch, $Cfg) {
    foreach ($pat in @($Cfg.protected_branches)) {
        if ($Branch -like $pat) { return $true }
    }
    return $false
}

function Invoke-CommitGuard {
    param(
        [string]$Root,
        [string]$Branch,
        [string[]]$Files,
        [string]$DiffText,
        $Cfg,
        [bool]$OwnerUnlock
    )
    $violations = @()

    if (-not $OwnerUnlock -and $Branch -and (Test-BranchProtected $Branch $Cfg)) {
        $violations += "branch protetto '$Branch' (usa branch claude/* o GSTN_OWNER_UNLOCK=1)"
    }

    $lockedAssets = Get-LockedAssets $Root
    foreach ($rawPath in @($Files)) {
        $p = ($rawPath -replace '\\', '/').Trim()
        if (-not $p) { continue }

        foreach ($pat in @($Cfg.secret_file_patterns)) {
            if ($p -imatch $pat) {
                $violations += "file segreto/credenziale bloccato: $p"
                break
            }
        }
        if ($OwnerUnlock) { continue }

        foreach ($pat in @($Cfg.owner_local_patterns)) {
            if ($p -imatch $pat) {
                $violations += "file locale owner bloccato: $p"
                break
            }
        }
        foreach ($lk in $lockedAssets) {
            if ($lk.Kind -eq 'asset') {
                $esc = [regex]::Escape($lk.Name)
                if ($p -imatch "(^|/)${esc}(\.uasset)?$") {
                    $violations += "asset approvato bloccato ($($lk.Name)): $p"
                    break
                }
            } elseif ($lk.Kind -eq 'casc') {
                $esc = [regex]::Escape($lk.Name)
                if ($p -imatch "(^|/)${esc}$") {
                    $violations += "sorgente Cascadeur bloccato ($($lk.Name)): $p"
                    break
                }
            }
        }
    }

    # Scan only added lines in staged text diffs for credential leaks (never bypassed).
    $skPat = 'sk-' + '[A-Za-z0-9]{20,}'
    $aiPat = 'AIza' + '[A-Za-z0-9_-]{30,}'
    $envKeyPat = '(DEEPSEEK_API_KEY|ANTHROPIC_API_KEY)\s*=\s*["'']?(?!<|YOUR_|EXAMPLE|PLACEHOLDER)[A-Za-z0-9_-]{16,}'
    if ($DiffText) {
        foreach ($line in ($DiffText -split "`r?`n")) {
            if ($line -notmatch '^\+[^+]') { continue }
            $added = $line.Substring(1)
            if ($added -match $skPat -or $added -match $aiPat -or $added -match $envKeyPat) {
                $violations += "possibile API key/token rilevato nel diff staged"
                break
            }
        }
    }

    return $violations
}

function Invoke-PushGuard {
    param(
        [string]$Root = '',
        [string]$Ref,
        [string]$Sha,
        [string]$RemoteSha = '',
        $Cfg,
        [bool]$OwnerUnlock
    )
    $violations = @()
    $shortRef = ($Ref -replace '^refs/heads/', '').Trim()
    if ($Sha -match '^0+$' -or -not $shortRef) {
        $violations += "cancellazione branch remoto bloccata: $Ref"
        return $violations
    }
    if (-not $OwnerUnlock) {
        if (Test-BranchProtected $shortRef $Cfg) {
            $violations += "push su branch protetto bloccato: $shortRef"
        } elseif (@($Cfg.allowed_push_prefixes).Count -gt 0) {
            $okPrefix = $false
            foreach ($pfx in @($Cfg.allowed_push_prefixes)) {
                if ($shortRef.StartsWith($pfx)) { $okPrefix = $true; break }
            }
            if (-not $okPrefix) {
                $violations += "push consentito solo su $($Cfg.allowed_push_prefixes -join ', ') (trovato: $shortRef)"
            }
        }
        if ($RemoteSha -and ($RemoteSha -notmatch '^0+$') -and $Root) {
            & git -C $Root merge-base --is-ancestor $RemoteSha $Sha 2>$null
            if ($LASTEXITCODE -ne 0) {
                $violations += "force-push / non-fast-forward bloccato ($RemoteSha non e' antenato di $Sha)"
            }
        }
    }
    return $violations
}

if ($SelfTest) {
    $cfg = Load-GuardConfig $GuardConfigPath
    $tmp = Join-Path $env:TEMP 'gitguard-selftest'
    if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force }
    New-Item -ItemType Directory -Force (Join-Path $tmp 'docs\notes') | Out-Null
    New-Item -ItemType Directory -Force (Join-Path $tmp 'Tools') | Out-Null
    New-Item -ItemType Directory -Force (Join-Path $tmp 'Content\GASNPPTest\Combat') | Out-Null

    Set-Content -Path (Join-Path $tmp 'docs\notes\anim-approvate.md') -Encoding UTF8 -Value @(
        '| run start | generic w2 | AS_GSTN_RunStart_Generic_w2 (Saved/AnimPipeline/runstart/RunStart_Generic_w2.casc) | ok | approved/runstart-w2 |'
    )

    & git -C $tmp init -b claude/test 2>&1 | Out-Null
    & git -C $tmp config user.name 'selftest'
    & git -C $tmp config user.email 'selftest@local'
    Set-Content -Path (Join-Path $tmp 'README.md') -Value 'init' -Encoding UTF8
    Set-Content -Path (Join-Path $tmp 'Content\GASNPPTest\Combat\DA_Locomotion_Default.uasset') -Value 'dummy' -Encoding UTF8
    & git -C $tmp add README.md docs/notes/anim-approvate.md Content/GASNPPTest/Combat/DA_Locomotion_Default.uasset
    & git -C $tmp commit -m 'init' 2>&1 | Out-Null
    $initSha = (& git -C $tmp rev-parse HEAD).Trim()

    $results = @()

    # 1. Owner local DA_Locomotion_Default.uasset + real git mv (--no-renames -z)
    $v1a = Invoke-CommitGuard -Root $tmp -Branch 'claude/test' -Files @('Content/GASNPPTest/Combat/DA_Locomotion_Default.uasset') -DiffText '' -Cfg $cfg -OwnerUnlock:$false
    & git -C $tmp mv Content/GASNPPTest/Combat/DA_Locomotion_Default.uasset Content/GASNPPTest/Combat/Renamed.uasset 2>&1 | Out-Null
    $mvStaged = Get-StagedFilesNullDelimited $tmp
    $v1mv = Invoke-CommitGuard -Root $tmp -Branch 'claude/test' -Files $mvStaged -DiffText '' -Cfg $cfg -OwnerUnlock:$false
    & git -C $tmp reset --hard HEAD 2>&1 | Out-Null
    $results += "1 DA+Mv: $(if ($v1a.Count -gt 0 -and $v1mv.Count -gt 0) {'PASS'} else {'FAIL'})"

    # 2. CHT_*_PoseMatch.uasset + non-ASCII unquoted path
    $v2 = Invoke-CommitGuard -Root $tmp -Branch 'claude/test' -Files @('Content/GSTN/Locomotion/StockBase/CHT_GSTN_StockMover_PoseMatch.uasset') -DiffText '' -Cfg $cfg -OwnerUnlock:$false
    $nonAsciiRel = 'Tools/nota_perch' + [char]0x00E8 + '.txt'
    [IO.File]::WriteAllText((Join-Path $tmp ($nonAsciiRel -replace '/', '\')), 'ciao', [Text.Encoding]::UTF8)
    & git -C $tmp add -- $nonAsciiRel
    $naStaged = Get-StagedFilesNullDelimited $tmp
    $okNa = ($naStaged -contains $nonAsciiRel)
    & git -C $tmp reset HEAD -- $nonAsciiRel 2>&1 | Out-Null
    $results += "2 Pose+Utf8: $(if ($v2.Count -gt 0 -and $okNa) {'PASS'} else {'FAIL'})"

    # 3. Locked approved asset parsed from anim-approvate.md (and allow sibling variant w2t180Lf)
    $v3a = Invoke-CommitGuard -Root $tmp -Branch 'claude/test' -Files @('Content/GSTN/Animation/AS_GSTN_RunStart_Generic_w2.uasset') -DiffText '' -Cfg $cfg -OwnerUnlock:$false
    $v3b = Invoke-CommitGuard -Root $tmp -Branch 'claude/test' -Files @('Content/GSTN/Animation/AS_GSTN_RunStart_Generic_w2t180Lf.uasset') -DiffText '' -Cfg $cfg -OwnerUnlock:$false
    $results += "3 ApprovedAnim: $(if ($v3a.Count -gt 0 -and $v3b.Count -eq 0) {'PASS'} else {'FAIL'})"

    # 4. Real git staged secret leak (constructed dynamically)
    $fakeKey = 'sk-' + ('A' * 24)
    Set-Content -Path (Join-Path $tmp 'Tools\leak.txt') -Value "token=$fakeKey" -Encoding UTF8
    & git -C $tmp add Tools/leak.txt
    $staged4 = Get-StagedFilesNullDelimited $tmp
    $diff4 = (& git -C $tmp -c core.quotepath=false diff --cached --no-renames --unified=0) -join "`n"
    $v4 = Invoke-CommitGuard -Root $tmp -Branch 'claude/test' -Files $staged4 -DiffText $diff4 -Cfg $cfg -OwnerUnlock:$false
    & git -C $tmp reset HEAD -- Tools/leak.txt 2>&1 | Out-Null
    $results += "4 SecretScan: $(if ($v4.Count -gt 0) {'PASS'} else {'FAIL'})"

    # 5. Clean edit on claude/test
    Set-Content -Path (Join-Path $tmp 'Tools\ok.txt') -Value 'hello' -Encoding UTF8
    & git -C $tmp add Tools/ok.txt
    $staged5 = Get-StagedFilesNullDelimited $tmp
    $diff5 = (& git -C $tmp -c core.quotepath=false diff --cached --no-renames --unified=0) -join "`n"
    $v5 = Invoke-CommitGuard -Root $tmp -Branch 'claude/test' -Files $staged5 -DiffText $diff5 -Cfg $cfg -OwnerUnlock:$false
    & git -C $tmp commit -m 'branchA' 2>&1 | Out-Null
    $shaA = (& git -C $tmp rev-parse HEAD).Trim()
    & git -C $tmp checkout -q -b claude/div $initSha
    Set-Content -Path (Join-Path $tmp 'Tools\div.txt') -Value 'diverged' -Encoding UTF8
    & git -C $tmp add Tools/div.txt
    & git -C $tmp commit -q -m 'branchB'
    $shaB = (& git -C $tmp rev-parse HEAD).Trim()
    $results += "5 CleanAllow: $(if ($v5.Count -eq 0) {'PASS'} else {'FAIL'})"

    # 6. Push guard (main/feature/delete/force-push blocked; fast-forward claude/* allowed)
    $pMain  = Invoke-PushGuard -Root $tmp -Ref 'refs/heads/main' -Sha $shaA -RemoteSha '' -Cfg $cfg -OwnerUnlock:$false
    $pFeat  = Invoke-PushGuard -Root $tmp -Ref 'refs/heads/feature/gas-npp-5.8' -Sha $shaA -RemoteSha '' -Cfg $cfg -OwnerUnlock:$false
    $pDel   = Invoke-PushGuard -Root $tmp -Ref 'refs/heads/claude/tmp' -Sha ('0' * 40) -RemoteSha $shaA -Cfg $cfg -OwnerUnlock:$false
    $pForce = Invoke-PushGuard -Root $tmp -Ref 'refs/heads/claude/harness-v2' -Sha $shaB -RemoteSha $shaA -Cfg $cfg -OwnerUnlock:$false
    $pOk    = Invoke-PushGuard -Root $tmp -Ref 'refs/heads/claude/harness-v2' -Sha $shaA -RemoteSha $initSha -Cfg $cfg -OwnerUnlock:$false
    $ok6 = ($pMain.Count -gt 0) -and ($pFeat.Count -gt 0) -and ($pDel.Count -gt 0) -and ($pForce.Count -gt 0) -and ($pOk.Count -eq 0)
    $results += "6 Push+Force: $(if ($ok6) {'PASS'} else {'FAIL'})"

    Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
    $summary = $results -join ' | '
    $allOk = -not ($results -match ': FAIL')
    Write-Output "ESITO SELFTEST: $(if ($allOk) {'PASS'} else {'FAIL'})"
    Write-Output $summary
    exit $(if ($allOk) { 0 } else { 1 })
}

$cfg = Load-GuardConfig $GuardConfigPath
$ownerUnlock = ($env:GSTN_OWNER_UNLOCK -eq '1')
if (-not $RepoRoot) { $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path }

if ($Mode -eq 'Push') {
    $v = Invoke-PushGuard -Root $RepoRoot -Ref $TargetRef -Sha $LocalSha -RemoteSha $RemoteSha -Cfg $cfg -OwnerUnlock $ownerUnlock
    if ($v.Count -gt 0) {
        Write-Output "ESITO GUARD: BLOCCATO (push)"
        $v | Select-Object -First 3 | ForEach-Object { Write-Output "  $_" }
        exit 2
    }
    Write-Output "ESITO GUARD: OK (push $TargetRef)"
    exit 0
}

$branch = (& git -C $RepoRoot rev-parse --abbrev-ref HEAD 2>$null)
if ($LASTEXITCODE -ne 0) { $branch = '' } else { $branch = "$branch".Trim() }

if ($StagedFiles -and $StagedFiles.Count -gt 0) {
    $files = $StagedFiles
    $diffText = ''
} else {
    $files = Get-StagedFilesNullDelimited $RepoRoot
    $diffText = (& git -C $RepoRoot -c core.quotepath=false diff --cached --no-renames --unified=0 -- . ':(exclude)*.uasset' ':(exclude)*.umap') -join "`n"
}

$v = Invoke-CommitGuard -Root $RepoRoot -Branch $branch -Files $files -DiffText $diffText -Cfg $cfg -OwnerUnlock $ownerUnlock
if ($v.Count -gt 0) {
    Write-Output "ESITO GUARD: BLOCCATO (commit)"
    $v | Select-Object -First 4 | ForEach-Object { Write-Output "  $_" }
    exit 2
}
Write-Output "ESITO GUARD: OK (commit su $branch, files=$($files.Count))"
exit 0

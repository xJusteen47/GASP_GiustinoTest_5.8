<#
Cleanup-Branches.ps1 - prune the claude/* branches of origin (owner decision 1A, 2026-11-24: `main` is the only trunk).
Replaces the old script that used feature/gas-npp-5.8 as reference and the second script Remove-MergedRemoteBranches.ps1.

What it does (DRY RUN by default: it only rewrites the list, nothing on origin changes):
  1. fetches origin (claude/*, main, tags);
  2. runs Tools\CI\branch_elenco.py, which classifies every claude/* branch against origin/main (and the integration branches) and
     writes docs\notes\pulizia-2026-11-24\branch-elenco.csv: A already in the trunk, B subset of another branch, C old history,
     D docs only, E code not in the trunk yet, TIENI kept (artifacts, integration, approved-* without tag, open PRs, -Keep);
  3. with -Apply: for each A/B/C/D branch it first creates and pushes the tag archivio/<name> (so every deleted branch can be
     restored: git push origin archivio/<name>:refs/heads/claude/<name>), then deletes the branch on origin in blocks of 50.
     E branches (code not merged) are NEVER deleted unless -IncludeCode is given. Also tags archivio/feature-gas-npp-5.8 (the old
     trunk is archived, never deleted here).
The pushes use --no-verify on purpose: the pre-push guard refuses deletions and non-approved tags to agents, and this script is run
BY THE OWNER, by hand. An agent may run it only without -Apply.
  powershell -NoProfile -ExecutionPolicy Bypass -File Tools\CI\Cleanup-Branches.ps1            (prova, riscrive il CSV)
  powershell -NoProfile -ExecutionPolicy Bypass -File Tools\CI\Cleanup-Branches.ps1 -Apply     (tag + cancella A/B/C/D)
Console: max 5 lines. Detail: the CSV and Saved\Logs\ci\cleanup-<date>.log.
#>
param(
    [switch]$Apply,
    [switch]$IncludeCode,                         # also delete E (code not in main): only after the code was merged or on purpose
    [string]$Trunk = 'main',
    [string[]]$Integration = @('claude/anim-skill-t3'),   # extra "already integrated" references (used when they exist on origin)
    [string[]]$Keep = @(),                        # extra branches to keep (e.g. the branch of the running thread)
    [string]$Csv = 'docs\notes\pulizia-2026-11-24\branch-elenco.csv',
    [string]$Root
)
$ErrorActionPreference = 'Stop'
$root = if ($Root) { $Root } else { (Resolve-Path "$PSScriptRoot\..\..").Path }
Set-Location $root
$csvPath = if ([IO.Path]::IsPathRooted($Csv)) { $Csv } else { Join-Path $root $Csv }
$log = Join-Path $root "Saved\Logs\ci\cleanup-$(Get-Date -Format yyyyMMdd-HHmm).log"
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null
New-Item -ItemType Directory -Force (Split-Path $csvPath) | Out-Null

$ErrorActionPreference = 'Continue'   # git writes progress on stderr
git fetch --quiet --prune --tags origin "+refs/heads/claude/*:refs/remotes/origin/claude/*" "+refs/heads/${Trunk}:refs/remotes/origin/$Trunk" 2>>$log
if ($LASTEXITCODE -ne 0) { Write-Output "ERRORE: fetch fallito (vedi $log)"; exit 1 }
git fetch --quiet origin "+refs/heads/feature/gas-npp-5.8:refs/remotes/origin/feature/gas-npp-5.8" 2>>$log

$open = @()
if (Get-Command gh -ErrorAction SilentlyContinue) { $open = @(gh pr list --state open --limit 500 --json headRefName --jq '.[].headRefName' 2>$null | Where-Object { $_ -like 'claude/*' }) }
$integ = @()
foreach ($i in @($Integration | Where-Object { $_ })) {
    git rev-parse --verify --quiet "origin/$i^{commit}" 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) { $integ += "origin/$i" }
}

$pyArgs = @('-I', (Join-Path $PSScriptRoot 'branch_elenco.py'), '--out', $csvPath, '--trunk', "origin/$Trunk")
if ($integ.Count) { $pyArgs += @('--integration') + $integ }
if ($Keep.Count) { $pyArgs += @('--keep') + $Keep }
if ($open.Count) { $pyArgs += @('--open-pr') + $open }
$summary = & python @pyArgs 2>>$log
if ($LASTEXITCODE -ne 0) { Write-Output "ERRORE: classificazione fallita (vedi $log)"; exit 1 }

$rows = @(Import-Csv -LiteralPath $csvPath)
$delCats = @('A_GIA_IN_INTEGRAZIONE', 'B_SOTTOINSIEME', 'C_STORIA_VECCHIA', 'D_SOLO_DOCS')
if ($IncludeCode) { $delCats += 'E_CODICE' }
$todo = @($rows | Where-Object { $delCats -contains $_.categoria })

if ($Apply -and $todo.Count) {
    $tags = @()
    foreach ($r in $todo) {
        $tag = "archivio/$($r.branch -replace '^claude/', '')"
        git tag -f $tag $r.sha 2>>$log | Out-Null
        $tags += "refs/tags/$tag"
    }
    $ft = git rev-parse --verify --quiet 'origin/feature/gas-npp-5.8'
    if ($ft) {
        $has = git tag --list 'archivio/feature-gas-npp-5.8'
        if (-not $has) { git tag 'archivio/feature-gas-npp-5.8' $ft 2>>$log | Out-Null; $tags += 'refs/tags/archivio/feature-gas-npp-5.8' }
    }
    for ($i = 0; $i -lt $tags.Count; $i += 50) {
        $chunk = @($tags[$i..([Math]::Min($i + 49, $tags.Count - 1))])
        git push --no-verify --quiet origin @chunk 2>>$log
        if ($LASTEXITCODE -ne 0) { Write-Output "ERRORE: push dei tag fallito, nessun branch cancellato (vedi $log)"; exit 1 }
    }
    $names = @($todo | ForEach-Object { $_.branch })
    for ($i = 0; $i -lt $names.Count; $i += 50) {
        $chunk = @($names[$i..([Math]::Min($i + 49, $names.Count - 1))])
        git push --no-verify --quiet origin --delete @chunk 2>>$log
        if ($LASTEXITCODE -ne 0) { Write-Output "ERRORE: cancellazione parziale (vedi $log); i tag archivio/* esistono gia" ; exit 1 }
    }
}
$mode = if ($Apply) { "ESEGUITO: $($todo.Count) branch cancellati dopo il tag archivio/<nome>" } else { 'PROVA (niente cambiato su origin; -Apply per eseguire)' }
Write-Output "PULIZIA $mode"
Write-Output $summary
Write-Output "da cancellare=$($todo.Count) (E_CODICE tenuti: $(@($rows | Where-Object { $_.categoria -eq 'E_CODICE' }).Count) senza -IncludeCode)"
Write-Output "elenco: $csvPath"

<#
Cleanup-Branches.ps1 - weekend cleanup of claude/* branches on origin (decision 3A, approved by the owner 2026-11-16).
  - claude/* whose tip is already in feature/gas-npp-5.8 and untouched for >= 2 days      -> deleted on origin
  - claude/* NOT in feature and untouched for > 14 days -> tag archivio/<name> pushed first, then branch deleted
  - never touched: feature/*, main, claude/artifacts, branches with an open PR, anything newer than the thresholds
Default is DRY RUN (lists, changes nothing). The owner runs -Apply by hand: Claude's rules forbid it branch deletion.
  powershell -NoProfile -ExecutionPolicy Bypass -File Tools\CI\Cleanup-Branches.ps1            (prova)
  powershell -NoProfile -ExecutionPolicy Bypass -File Tools\CI\Cleanup-Branches.ps1 -Apply     (esegue)
Full list in Saved\Logs\ci\cleanup-<date>.tsv; console max 5 lines. A deleted branch can be restored from the tag
(archivio/*) or, for merged ones, from the SHA in the tsv: git push origin <sha>:refs/heads/<name>.
#>
param([switch]$Apply, [int]$StaleDays = 14, [int]$MergedMinDays = 2, [string]$Feature = 'feature/gas-npp-5.8', [string]$Root)
$ErrorActionPreference = 'Stop'
$root = if ($Root) { $Root } else { (Resolve-Path "$PSScriptRoot\..\..").Path }
Set-Location $root
$out = "Saved\Logs\ci\cleanup-$(Get-Date -Format yyyyMMdd-HHmm).tsv"
New-Item -ItemType Directory -Force (Split-Path $out) | Out-Null

$ErrorActionPreference = 'Continue'   # git writes progress on stderr
git fetch --quiet --prune origin "+refs/heads/claude/*:refs/remotes/origin/claude/*" "+refs/heads/${Feature}:refs/remotes/origin/$Feature" 2>$null
$open = @()
if (Get-Command gh -ErrorAction SilentlyContinue) { $open = @(gh pr list --state open --limit 500 --json headRefName --jq '.[].headRefName' 2>$null) }
$now = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
$rows = @(); $del = @(); $arch = @()
foreach ($line in (git for-each-ref --format='%(refname:strip=3)|%(objectname)|%(committerdate:unix)' 'refs/remotes/origin/claude/')) {
    $b, $sha, $ts = $line -split '\|'          # $b = claude/<name>
    $age = [int](($now - [long]$ts) / 86400)
    git merge-base --is-ancestor $sha "origin/$Feature" 2>$null; $merged = ($LASTEXITCODE -eq 0)
    $action = 'tieni'
    if ($b -eq 'claude/artifacts') { $action = 'tieni (artifacts)' }
    elseif ($open -contains $b) { $action = 'tieni (PR aperta)' }
    elseif ($merged -and $age -ge $MergedMinDays) { $action = 'cancella (in feature)'; $del += $b }
    elseif (-not $merged -and $age -gt $StaleDays) { $action = 'archivia+cancella'; $arch += [pscustomobject]@{ b = $b; sha = $sha } }
    $rows += "$b`t$sha`t$age`t$merged`t$action"
}
@("branch`tsha`tgiorni`tin_feature`tazione") + $rows | Set-Content $out -Encoding UTF8

if ($Apply) {
    $tags = @()
    foreach ($a in $arch) {
        $tag = "archivio/$($a.b -replace '^claude/', '')"
        git tag -f $tag $a.sha 2>$null | Out-Null
        $tags += "refs/tags/$tag"
    }
    if ($tags.Count) {
        git push --quiet origin $tags 2>>"$out.err"
        if ($LASTEXITCODE -ne 0) { Write-Output "ERRORE: push dei tag fallito, nessun branch cancellato (vedi $out.err)"; exit 1 }
    }
    $all = @($del) + @($arch | ForEach-Object { $_.b })
    for ($i = 0; $i -lt $all.Count; $i += 50) {   # a blocchi di 50 branch per riga di comando
        $chunk = $all[$i..([Math]::Min($i + 49, $all.Count - 1))]
        git push --quiet origin --delete $chunk 2>>"$out.err"
    }
}
$mode = if ($Apply) { 'ESEGUITO' } else { 'PROVA (niente cambiato; -Apply per eseguire)' }
Write-Output "PULIZIA $mode"
Write-Output "claude/* totali=$($rows.Count) cancella_fusi=$($del.Count) archivia+cancella=$($arch.Count) tenuti=$($rows.Count - $del.Count - $arch.Count)"
Write-Output "elenco: $out"

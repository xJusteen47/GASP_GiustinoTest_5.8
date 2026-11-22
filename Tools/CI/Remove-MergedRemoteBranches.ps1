<#
Remove-MergedRemoteBranches.ps1 - Lists (and optionally deletes) origin branches already merged
into TargetRef (default HEAD). Dry-run by default; requires -Execute to push deletions.
#>
[CmdletBinding()]
param(
    [string]$RepoPath = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path,
    [string]$TargetRef = 'HEAD',
    [string]$Remote = 'origin',
    [string]$ArchiveTag = '',
    [switch]$Execute
)

$ErrorActionPreference = 'Stop'

$protected = @(
    "$Remote/main",
    "$Remote/HEAD",
    "$Remote/claude/harness-v2",
    "$Remote/claude/runstart-generic-desktop",
    "$Remote/claude/integ-playtest-2026-11-22"
)

$merged = & git -C $RepoPath branch -r --merged $TargetRef |
    ForEach-Object { $_.Trim() } |
    Where-Object {
        $_ -and
        $_ -notmatch '->' -and
        $_.StartsWith("$Remote/") -and
        ($_ -notin $protected) -and
        ($_ -notmatch "^$Remote/(feature|experiment)/")
    }

Write-Output "TargetRef=$TargetRef | Merged removable branches on $Remote : $($merged.Count)"
if (-not $Execute) {
    $merged | Select-Object -First 25 | ForEach-Object { Write-Output "  [DRY-RUN] $_" }
    if ($merged.Count -gt 25) { Write-Output "  ... (+$($merged.Count - 25) more)" }
    Write-Output "Run again with -Execute (and optionally -ArchiveTag archive-2026-11-22) to delete on $Remote."
    exit 0
}

if ($ArchiveTag) {
    & git -C $RepoPath tag -f $ArchiveTag $TargetRef
    & git -C $RepoPath push $Remote "refs/tags/$ArchiveTag"
    if ($LASTEXITCODE -ne 0) { throw "Archive tag push failed ($LASTEXITCODE)" }
}

$shortBranches = $merged | ForEach-Object { $_.Substring($Remote.Length + 1) }
$batchSize = 25
for ($i = 0; $i -lt $shortBranches.Count; $i += $batchSize) {
    $end = [Math]::Min($i + $batchSize - 1, $shortBranches.Count - 1)
    $batch = $shortBranches[$i..$end]
    & git -C $RepoPath push $Remote --delete @batch
    if ($LASTEXITCODE -ne 0) { throw "git push --delete batch failed at index $i" }
}
Write-Output "Deleted $($shortBranches.Count) merged branches from $Remote."

<#
Install-GitHooks.ps1 - configures git core.hooksPath to .githooks for this repository.
Idempotent; prints at most 5 lines.
  powershell -NoProfile -ExecutionPolicy Bypass -File Tools\CI\Install-GitHooks.ps1
#>
[CmdletBinding()]
param([string]$RepoRoot)
$ErrorActionPreference = 'Stop'
if (-not $RepoRoot) { $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path }
if (-not (Test-Path (Join-Path $RepoRoot '.githooks\pre-commit'))) {
    Write-Output "ESITO: FAIL - .githooks/pre-commit mancante in $RepoRoot"
    exit 1
}
& git -C $RepoRoot config core.hooksPath .githooks
if ($LASTEXITCODE -ne 0) {
    Write-Output "ESITO: FAIL - git config core.hooksPath fallito ($LASTEXITCODE)"
    exit 1
}
$cur = (& git -C $RepoRoot config --get core.hooksPath).Trim()
Write-Output "ESITO: OK - hooksPath=$cur in $RepoRoot"
exit 0

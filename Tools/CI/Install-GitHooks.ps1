<#
Install-GitHooks.ps1 - configures git core.hooksPath for this repository and all shared worktrees.
By default installs hooks + guard scripts to an absolute path (C:\Users\jacop\Tools\gstn-gitguard)
so worktrees on older branches (like _ue59) remain protected. Idempotent; prints at most 5 lines.
  powershell -NoProfile -ExecutionPolicy Bypass -File Tools\CI\Install-GitHooks.ps1
#>
[CmdletBinding()]
param(
    [string]$RepoRoot,
    [string]$InstallDir = 'C:\Users\jacop\Tools\gstn-gitguard',
    [switch]$Relative
)
$ErrorActionPreference = 'Stop'
$SourceRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $RepoRoot) { $RepoRoot = $SourceRoot }

$hooksSrcDir = Join-Path $SourceRoot '.githooks'
if (-not (Test-Path -LiteralPath (Join-Path $hooksSrcDir 'pre-commit'))) {
    Write-Output "ESITO: FAIL - .githooks/pre-commit mancante in $SourceRoot"
    exit 1
}

if ($Relative) {
    if (-not (Test-Path -LiteralPath (Join-Path $RepoRoot '.githooks\pre-commit'))) {
        Write-Output "ESITO: FAIL - .githooks/pre-commit mancante in $RepoRoot"
        exit 1
    }
    & git -C $RepoRoot config core.hooksPath .githooks
} else {
    $dstHooksDir = Join-Path $InstallDir '.githooks'
    $dstCiDir = Join-Path $InstallDir 'Tools\CI'
    New-Item -ItemType Directory -Force $dstHooksDir | Out-Null
    New-Item -ItemType Directory -Force $dstCiDir | Out-Null

    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    foreach ($hookName in @('pre-commit', 'pre-push')) {
        $srcHook = Join-Path $hooksSrcDir $hookName
        if (-not (Test-Path -LiteralPath $srcHook)) {
            Write-Output "ESITO: FAIL - hook sorgente mancante: $srcHook"
            exit 1
        }
        $raw = [IO.File]::ReadAllText($srcHook, [Text.Encoding]::UTF8)
        $lf = ($raw -replace "`r`n", "`n") -replace "`r", "`n"
        [IO.File]::WriteAllText((Join-Path $dstHooksDir $hookName), $lf, $utf8NoBom)
    }

    foreach ($ciFile in @('Test-GitGuard.ps1', 'protected-paths.json')) {
        $srcCi = Join-Path $SourceRoot "Tools\CI\$ciFile"
        if (-not (Test-Path -LiteralPath $srcCi)) {
            Write-Output "ESITO: FAIL - file CI sorgente mancante: $srcCi"
            exit 1
        }
        Copy-Item -LiteralPath $srcCi -Destination (Join-Path $dstCiDir $ciFile) -Force
    }

    $hooksPosix = (($InstallDir -replace '\\', '/').TrimEnd('/')) + '/.githooks'
    & git -C $RepoRoot config core.hooksPath "$hooksPosix"
}

if ($LASTEXITCODE -ne 0) {
    Write-Output "ESITO: FAIL - git config core.hooksPath fallito ($LASTEXITCODE)"
    exit 1
}
$cur = (& git -C $RepoRoot config --get core.hooksPath).Trim()
Write-Output "ESITO: OK - hooksPath=$cur in $RepoRoot"
exit 0

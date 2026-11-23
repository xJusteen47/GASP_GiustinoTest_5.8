<#
New-DeepSeekWorktree.ps1 - creates/removes an isolated git worktree WITHOUT Epic EULA folders
(Plugins/EngineOverrides/, Plugins/GASNPP/) and with GIT_LFS_SKIP_SMUDGE=1.
Use this as the Cwd for M0/M1 DeepSeek workers. Prints at most 5 lines.
#>
[CmdletBinding()]
param(
    [string]$SourceRepo = 'C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_ue59',
    [string]$Branch,
    [string]$BaseRef = 'HEAD',
    [string]$TargetDir,
    [switch]$Remove,
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'

function Invoke-GitQuiet {
    param([string[]]$GitArgs)
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & git @GitArgs 2>&1 | Out-Null
        return $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
    }
}

function New-SparseDeepSeekWorktree {
    param([string]$Repo, [string]$BranchName, [string]$Base, [string]$Dest)
    if (Test-Path -LiteralPath $Dest) {
        throw "TargetDir gia' esistente: $Dest"
    }
    $prevSmudge = $env:GIT_LFS_SKIP_SMUDGE
    $env:GIT_LFS_SKIP_SMUDGE = '1'
    try {
        $hasBranch = (Invoke-GitQuiet @('-C', $Repo, 'show-ref', '--verify', '--quiet', "refs/heads/$BranchName")) -eq 0
        if ($hasBranch) {
            $rc = Invoke-GitQuiet @('-C', $Repo, 'worktree', 'add', '--no-checkout', $Dest, $BranchName)
        } else {
            $rc = Invoke-GitQuiet @('-C', $Repo, 'worktree', 'add', '--no-checkout', '-b', $BranchName, $Dest, $Base)
        }
        if ($rc -ne 0) { throw "git worktree add fallito ($rc)" }

        $rc = Invoke-GitQuiet @('-C', $Dest, 'sparse-checkout', 'init', '--no-cone')
        if ($rc -ne 0) { throw "sparse-checkout init fallito ($rc)" }
        $rc = Invoke-GitQuiet @('-C', $Dest, 'sparse-checkout', 'set', '--no-cone', '/*', '!/Plugins/EngineOverrides/', '!/Plugins/GASNPP/')
        if ($rc -ne 0) { throw "sparse-checkout set fallito ($rc)" }
        $rc = Invoke-GitQuiet @('-C', $Dest, 'checkout')
        if ($rc -ne 0) { throw "git checkout sparse fallito ($rc)" }
    } finally {
        if ($null -eq $prevSmudge) { Remove-Item Env:GIT_LFS_SKIP_SMUDGE -ErrorAction SilentlyContinue }
        else { $env:GIT_LFS_SKIP_SMUDGE = $prevSmudge }
    }

    foreach ($bad in @('Plugins\EngineOverrides', 'Plugins\GASNPP')) {
        if (Test-Path -LiteralPath (Join-Path $Dest $bad)) {
            throw "EULA FAIL: $bad presente dopo sparse-checkout in $Dest"
        }
    }
}

if ($SelfTest) {
    $tmp = Join-Path $env:TEMP 'dsworktree-selftest'
    if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force }
    $src = Join-Path $tmp 'src'
    $dst = Join-Path $tmp 'wt-ds'
    New-Item -ItemType Directory -Force (Join-Path $src 'Plugins\GASNPP') | Out-Null
    New-Item -ItemType Directory -Force (Join-Path $src 'Plugins\EngineOverrides') | Out-Null
    New-Item -ItemType Directory -Force (Join-Path $src 'Source\GASP_Giustino2') | Out-Null

    Set-Content -Path (Join-Path $src 'Plugins\GASNPP\Secret.h') -Value '// epic' -Encoding UTF8
    Set-Content -Path (Join-Path $src 'Plugins\EngineOverrides\Ov.h') -Value '// epic' -Encoding UTF8
    Set-Content -Path (Join-Path $src 'Source\GASP_Giustino2\Ok.h') -Value '// gstn' -Encoding UTF8

    $null = Invoke-GitQuiet @('-C', $src, 'init', '-b', 'claude/base')
    $null = Invoke-GitQuiet @('-C', $src, 'config', 'user.name', 'selftest')
    $null = Invoke-GitQuiet @('-C', $src, 'config', 'user.email', 'selftest@local')
    $null = Invoke-GitQuiet @('-C', $src, 'add', '.')
    $null = Invoke-GitQuiet @('-C', $src, 'commit', '-m', 'init')

    New-SparseDeepSeekWorktree -Repo $src -BranchName 'claude/ds-test' -Base 'HEAD' -Dest $dst
    $okSrc = Test-Path -LiteralPath (Join-Path $dst 'Source\GASP_Giustino2\Ok.h')
    $noGasnpp = -not (Test-Path -LiteralPath (Join-Path $dst 'Plugins\GASNPP'))
    $noOv = -not (Test-Path -LiteralPath (Join-Path $dst 'Plugins\EngineOverrides'))
    $null = Invoke-GitQuiet @('-C', $src, 'worktree', 'remove', $dst)
    $rmOk = -not (Test-Path -LiteralPath $dst)
    Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue

    $allOk = $okSrc -and $noGasnpp -and $noOv -and $rmOk
    Write-Output "ESITO SELFTEST: $(if ($allOk) {'PASS'} else {'FAIL'})"
    Write-Output "1 SourcePresent:$okSrc | 2 NoGASNPP:$noGasnpp | 3 NoEngineOv:$noOv | 4 Remove:$rmOk"
    exit $(if ($allOk) { 0 } else { 1 })
}

if (-not $TargetDir) { Write-Output 'ESITO: FAIL - -TargetDir obbligatorio'; exit 2 }
if ($Remove) {
    $rc = Invoke-GitQuiet @('-C', $SourceRepo, 'worktree', 'remove', $TargetDir)
    if ($rc -ne 0) { Write-Output "ESITO: FAIL - worktree remove ($rc)"; exit 1 }
    Write-Output "ESITO: OK - rimosso worktree $TargetDir"
    exit 0
}
if (-not $Branch) { Write-Output 'ESITO: FAIL - -Branch obbligatorio'; exit 2 }
New-SparseDeepSeekWorktree -Repo $SourceRepo -BranchName $Branch -Base $BaseRef -Dest $TargetDir
Write-Output "ESITO: OK - worktree sparse DeepSeek pronto: $TargetDir (branch=$Branch, senza EngineOverrides/GASNPP)"
exit 0

<#
Setup-Runner.ps1 - installs the self-hosted GitHub Actions runner on the LAPTOP (decision 1A, 2026-11-16).
To be run BY THE OWNER, once, in a normal PowerShell (no admin needed), from the repo root:
  powershell -NoProfile -ExecutionPolicy Bypass -File Tools\CI\Setup-Runner.ps1

What it does (idempotent; -Remove undoes it):
 1. downloads the latest actions/runner for win-x64 into C:\actions-runner (outside the repo);
 2. registers it on xJusteen47/GASP_GiustinoTest_5.8 with name and label "gstn-laptop" (token via `gh`, or pasted);
 3. writes UE_ENGINE_DIR in the runner's .env (engine taken from the EngineAssociation of the .uproject);
 4. creates a Task Scheduler task "GSTN CI runner" that starts the runner at logon, hidden, as the current user
    (no service and no password: builds use the owner's own profile, DDC and engine).
Work folder C:\gr\_work (short path: UE paths are long). Prints at most 5 lines; details in C:\actions-runner\setup.log.
#>
param(
    [string]$Repo = 'xJusteen47/GASP_GiustinoTest_5.8',
    [string]$RunnerDir = 'C:\actions-runner',
    [string]$WorkDir = 'C:\gr\_work',
    [string]$Name = 'gstn-laptop',
    [string]$Engine,
    [switch]$Remove
)
$ErrorActionPreference = 'Stop'
$task = 'GSTN CI runner'
New-Item -ItemType Directory -Force $RunnerDir | Out-Null
$log = Join-Path $RunnerDir 'setup.log'
function Say($m) { Add-Content $log "$(Get-Date -Format s) $m" }

function Get-Token([string]$kind) {
    $gh = Get-Command gh -ErrorAction SilentlyContinue
    if ($gh) {
        $t = & gh api -X POST "repos/$Repo/actions/runners/$kind-token" --jq .token 2>>$log
        if ($LASTEXITCODE -eq 0 -and $t) { return $t.Trim() }
    }
    Write-Output "gh non disponibile: apri https://github.com/$Repo/settings/actions/runners/new e copia il token dopo --token"
    return (Read-Host 'Incolla il token').Trim()
}

if ($Remove) {
    schtasks /Delete /TN $task /F 2>>$log | Out-Null
    Get-Process Runner.Listener -ErrorAction SilentlyContinue | Stop-Process -Force
    Push-Location $RunnerDir; & .\config.cmd remove --token (Get-Token 'remove') *>> $log; Pop-Location
    Write-Output "Runner rimosso (cartella $RunnerDir lasciata: cancellala a mano se vuoi)."; exit 0
}

# 1. runner
if (-not (Test-Path "$RunnerDir\config.cmd")) {
    $rel = Invoke-RestMethod 'https://api.github.com/repos/actions/runner/releases/latest'
    $asset = $rel.assets | Where-Object { $_.name -match '^actions-runner-win-x64-[\d.]+\.zip$' } | Select-Object -First 1
    $zip = Join-Path $env:TEMP $asset.name
    Say "download $($asset.browser_download_url)"
    Invoke-WebRequest $asset.browser_download_url -OutFile $zip
    Expand-Archive $zip -DestinationPath $RunnerDir -Force
    Remove-Item $zip
}

# 2. engine for .env
if (-not $Engine) {
    $up = Get-Content -Raw (Join-Path $PSScriptRoot '..\..\GASP_Giustino2.uproject') | ConvertFrom-Json
    $Engine = "C:\Program Files\Epic Games\UE_$($up.EngineAssociation)\Engine"
}
if (-not (Test-Path "$Engine\Build\BatchFiles\Build.bat")) { Write-Output "ERRORE: engine non trovato in $Engine (usa -Engine <cartella Engine>)"; exit 1 }
Set-Content "$RunnerDir\.env" "UE_ENGINE_DIR=$Engine`nGIT_LFS_SKIP_SMUDGE=0" -Encoding ASCII
git config --global core.longpaths true

# 3. register
if (-not (Test-Path "$RunnerDir\.runner")) {
    New-Item -ItemType Directory -Force $WorkDir | Out-Null
    Push-Location $RunnerDir
    & .\config.cmd --unattended --replace --url "https://github.com/$Repo" --token (Get-Token 'registration') `
        --name $Name --labels $Name --work $WorkDir *>> $log
    $rc = $LASTEXITCODE; Pop-Location
    if ($rc -ne 0) { Write-Output "ERRORE: registrazione fallita (vedi $log)"; exit 1 }
}

# 4. start at logon, hidden, as the current user
$cmd = "powershell -NoProfile -WindowStyle Hidden -Command `"Set-Location '$RunnerDir'; .\run.cmd *>> '$RunnerDir\runner.log'`""
schtasks /Create /TN $task /TR $cmd /SC ONLOGON /RL LIMITED /F 2>>$log | Out-Null
schtasks /Run /TN $task 2>>$log | Out-Null

Write-Output "OK: runner '$Name' registrato su $Repo, engine $Engine"
Write-Output "Avvio automatico al login (attivita' '$task'). Controlla: https://github.com/$Repo/settings/actions/runners"
Write-Output "Dettagli: $log"

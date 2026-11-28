<#
Aggiorna-Progetto.ps1 - bring the owner's project to the latest origin/main and build the editor, in one go.

Why it exists (owner 2026-11-28): after a pull the editor asks to rebuild modules, or opens with old DLLs and odd behaviour,
and a pull can be blocked by local changes to kits. This script does the safe sequence every time, so the owner double-clicks
"Aggiorna e compila.bat" (project root) instead of typing git and Build.bat commands.

Steps (each prints one line; any failure stops with ESITO: FAIL and says what to do, nothing is ever deleted):
  1. Unreal Editor for THIS project must be closed (the build cannot overwrite loaded DLLs). The script never closes it.
     In the _ue59 worktree, _ue59\IN-USO.txt must say "libero" (project rule 5).
  2. Local changes are put aside with `git stash push -u -m "aggiorna-<date>"` (tracked and untracked; ignored files stay).
     They stay in `git stash list`: the script never applies, drops or commits them. Owner-local files (patterns
     owner_local_patterns in protected-paths.json, e.g. DA_Locomotion_Default) are copied to Saved\Backups\Aggiorna\<date>\
     before the stash and copied back after the pull, so the owner's own local versions are still in place afterwards.
  3. `git fetch origin main`, switch to main if on another branch, `git merge --ff-only origin/main` (a local main that has
     its own commits is NOT rewritten: the script stops and says so), then `git lfs pull`.
  4. Engine: -Engine, env UE_ENGINE_DIR, the EngineAssociation of the .uproject in the registry
     (HKLM\SOFTWARE\EpicGames\Unreal Engine\<ver>) or the Epic launcher's LauncherInstalled.dat, else the default path.
  5. Build through Run-Timed.ps1 -Preset Build (the project's only build path: 45 min limit, log on file, approved-pose gate),
     log Saved\Logs\ci\aggiorna-build.log. Without python the gates run by Run-Timed are skipped and the output says so.

  Aggiorna-Progetto.ps1                     update + build
  Aggiorna-Progetto.ps1 -SenzaBuild         update only
  Aggiorna-Progetto.ps1 -SoloBuild          build only (no git)
  Aggiorna-Progetto.ps1 -Progetto <dir>     another checkout (default: the repo this script is in)

Exit: 0 OK, 1 FAIL (git or build), 2 editor open / lock taken, 4 build refused by Run-Timed's gates.
#>
[CmdletBinding()]
param(
    [string]$Progetto,
    [string]$Engine,
    [switch]$SenzaBuild,
    [switch]$SoloBuild
)

$ErrorActionPreference = 'Stop'
if (-not $Progetto) { $Progetto = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path }
$Progetto = (Resolve-Path $Progetto).Path.TrimEnd('\')
$uproject = Get-ChildItem -LiteralPath $Progetto -Filter *.uproject | Select-Object -First 1
if (-not $uproject) { Write-Host "ESITO: FAIL nessun .uproject in $Progetto" -ForegroundColor Red; exit 1 }
$stamp = Get-Date -Format 'yyyy-MM-dd_HHmm'

function Say([string]$t, [string]$c = 'Gray') { Write-Host $t -ForegroundColor $c }
function Stop-With([int]$code, [string]$msg, [string[]]$hint) {
    Say "ESITO: FAIL $msg" Red
    foreach ($h in @($hint)) { if ($h) { Say "  $h" Yellow } }
    exit $code
}
# git writes progress on stderr: under 'Stop' Windows PowerShell 5.1 would turn that into an exception, so run it under 'Continue'
# and judge by the exit code only.
function Git {
    $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    try { $out = & git -C $Progetto @args 2>&1 | ForEach-Object { "$_" } } finally { $ErrorActionPreference = $old }
    return [pscustomobject]@{ Code = $LASTEXITCODE; Out = @($out) }
}

Say "== Aggiorna e compila: $Progetto ==" Cyan

# --- 1. editor closed, lock free -------------------------------------------------------------------------------------------
$projNorm = $Progetto.ToLowerInvariant()
$editors = @(Get-CimInstance Win32_Process -Filter "Name LIKE 'UnrealEditor%'" -ErrorAction SilentlyContinue)
$mine = @(); $unknown = @()
foreach ($p in $editors) {
    $cl = "$($p.CommandLine)".ToLowerInvariant().Replace('/', '\')
    if ($cl -match '\.uproject') { if ($cl.Contains($projNorm + '\')) { $mine += $p } }
    else { $unknown += $p }   # started from the launcher without a project on the command line: cannot tell which one it is
}
if ($mine.Count -or $unknown.Count) {
    $ids = (@($mine) + @($unknown) | ForEach-Object { "$($_.Name) pid $($_.ProcessId)" }) -join ', '
    Stop-With 2 "Unreal Editor aperto ($ids)" @('Salva e chiudi l''editor, poi rilancia lo script.')
}
Say 'OK editor chiuso'
if ((Split-Path $Progetto -Leaf) -match '_ue59$') {
    $lock = Join-Path $Progetto '_ue59\IN-USO.txt'
    if ((Test-Path -LiteralPath $lock) -and ((Get-Content -LiteralPath $lock -TotalCount 1) -notmatch '^IN-USO:\s*libero')) {
        Stop-With 2 "_ue59 occupato: $(Get-Content -LiteralPath $lock -TotalCount 1)" @('Aspetta che il thread lo liberi.')
    }
    Say 'OK _ue59 libero'
}

if (-not $SoloBuild) {
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Stop-With 1 'git non trovato nel PATH' }
    $r = Git lfs version
    if ($r.Code -ne 0) { Stop-With 1 'git lfs non installato' @('Installa Git LFS (git lfs install), poi rilancia.') }
    $r = Git rev-parse --abbrev-ref HEAD
    $branch = $r.Out[-1]
    $before = (Git rev-parse --short HEAD).Out[-1]
    Say "OK branch $branch @ $before"

    # --- 2. put local changes aside --------------------------------------------------------------------------------------------
    $r = Git status --porcelain=v1 --untracked-files=all
    if ($r.Code -ne 0) { Stop-With 1 'git status fallito' $r.Out }
    $changed = @($r.Out | Where-Object { $_.Length -gt 3 } | ForEach-Object { $_.Substring(3).Trim('"') -replace '^.* -> ', '' })
    $ownerFiles = @()
    $guard = Join-Path $Progetto 'Tools\CI\protected-paths.json'
    if ((Test-Path -LiteralPath $guard) -and $changed.Count) {
        $pats = @((Get-Content -LiteralPath $guard -Raw | ConvertFrom-Json).owner_local_patterns)
        $ownerFiles = @($changed | Where-Object { $f = $_; @($pats | Where-Object { $f -match $_ }).Count } |
            Where-Object { Test-Path -LiteralPath (Join-Path $Progetto $_) -PathType Leaf })
    }
    $backup = Join-Path $Progetto "Saved\Backups\Aggiorna\$stamp"
    foreach ($f in $ownerFiles) {
        $dst = Join-Path $backup $f
        New-Item -ItemType Directory -Force (Split-Path $dst -Parent) | Out-Null
        Copy-Item -LiteralPath (Join-Path $Progetto $f) -Destination $dst -Force
    }
    $stashName = "aggiorna-$stamp"
    if ($changed.Count) {
        Say "ATTENZIONE: $($changed.Count) file modificati o nuovi, li metto da parte (stash '$stashName'):" Yellow
        $changed | Select-Object -First 15 | ForEach-Object { Say "  $_" Yellow }
        if ($changed.Count -gt 15) { Say "  ... e altri $($changed.Count - 15)" Yellow }
        $r = Git stash push -u -m $stashName
        if ($r.Code -ne 0) { Stop-With 1 'git stash fallito, nulla e'' stato cambiato' $r.Out }
        Say "OK stash '$stashName' (git stash list per vederlo; git stash apply per riprenderlo)"
    } else { Say 'OK nessuna modifica locale' }

    # --- 3. latest main + LFS --------------------------------------------------------------------------------------------------
    $r = Git fetch origin main
    if ($r.Code -ne 0) { Stop-With 1 'git fetch fallito (rete?)' (@($r.Out | Select-Object -Last 3) + "Le modifiche locali sono nello stash '$stashName'.") }
    if ($branch -ne 'main') {
        $r = Git switch main
        if ($r.Code -ne 0) { Stop-With 1 "non riesco a passare a main da $branch" @($r.Out | Select-Object -Last 3) }
        Say "OK passato da $branch a main" Yellow
    }
    $r = Git merge --ff-only origin/main
    if ($r.Code -ne 0) {
        Stop-With 1 'main locale ha commit suoi, non lo riscrivo' (@($r.Out | Select-Object -Last 2) +
            'Chiedi a un thread di controllare (git log origin/main..main).')
    }
    $r = Git lfs pull
    if ($r.Code -ne 0) { Stop-With 1 'git lfs pull fallito' @($r.Out | Select-Object -Last 3) }
    $after = (Git rev-parse --short HEAD).Out[-1]
    $subject = (Git log -1 --format=%s).Out[-1]
    if ($before -eq $after) { Say "OK gia' aggiornato: main @ $after" } else { Say "OK main $before -> $after ($subject)" Green }

    foreach ($f in $ownerFiles) {
        $dst = Join-Path $Progetto $f
        New-Item -ItemType Directory -Force (Split-Path $dst -Parent) | Out-Null
        Copy-Item -LiteralPath (Join-Path $backup $f) -Destination $dst -Force
        Say "OK rimesso il tuo file locale: $f" Green
    }
    if ($ownerFiles.Count) { Say "   (copia anche in $backup)" }
}

if ($SenzaBuild) { Say 'ESITO: OK aggiornato (build saltata con -SenzaBuild)' Green; exit 0 }

# --- 4. engine -------------------------------------------------------------------------------------------------------------
function Find-Engine {
    if ($Engine) { return $Engine }
    if ($env:UE_ENGINE_DIR) { return $env:UE_ENGINE_DIR }
    $assoc = (Get-Content -LiteralPath $uproject.FullName -Raw | ConvertFrom-Json).EngineAssociation
    if ($assoc) {
        foreach ($k in @("HKLM:\SOFTWARE\EpicGames\Unreal Engine\$assoc", "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$assoc")) {
            $d = (Get-ItemProperty -Path $k -Name InstalledDirectory -ErrorAction SilentlyContinue).InstalledDirectory
            if ($d -and (Test-Path -LiteralPath "$d\Engine")) { return "$d\Engine" }
        }
        $dat = 'C:\ProgramData\Epic\UnrealEngineLauncher\LauncherInstalled.dat'
        if (Test-Path -LiteralPath $dat) {
            $inst = (Get-Content -LiteralPath $dat -Raw | ConvertFrom-Json).InstallationList | Where-Object { $_.AppName -eq "UE_$assoc" } | Select-Object -First 1
            if ($inst -and (Test-Path -LiteralPath "$($inst.InstallLocation)\Engine")) { return "$($inst.InstallLocation)\Engine" }
        }
        $d = "C:\Program Files\Epic Games\UE_$assoc\Engine"
        if (Test-Path -LiteralPath $d) { return $d }
    }
    return 'C:\Program Files\Epic Games\UE_5.9\Engine'
}
$eng = Find-Engine
if (-not (Test-Path -LiteralPath "$eng\Build\BatchFiles\Build.bat")) {
    Stop-With 1 "motore non trovato ($eng)" @('Rilancia con -Engine "<cartella UE_x.y>\Engine".')
}
Say "OK motore $eng"

# --- 5. build --------------------------------------------------------------------------------------------------------------
$runTimed = Join-Path $Progetto 'Tools\CI\Run-Timed.ps1'
$log = Join-Path $Progetto 'Saved\Logs\ci\aggiorna-build.log'
New-Item -ItemType Directory -Force (Split-Path $log -Parent) | Out-Null
$rtArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $runTimed, '-Preset', 'Build', '-Log', $log, '-Engine', $eng)
if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    $rtArgs += @('-NoPreCheck', '-SkipRegress')
    Say 'ATTENZIONE: python non trovato, build senza i controlli di Run-Timed (precheck, pose approvate)' Yellow
}
Say "Build in corso (fino a 45 min), log: $log"
$t0 = Get-Date
$old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
& powershell @rtArgs 2>&1 | ForEach-Object { Say "  $_" }
$code = $LASTEXITCODE
$ErrorActionPreference = $old
$min = [math]::Round(((Get-Date) - $t0).TotalMinutes, 1)
if ($code -eq 0) { Say "ESITO: OK aggiornato e compilato in $min min. Puoi aprire l'editor." Green; exit 0 }
$tail = @()
if (Test-Path -LiteralPath $log) { $tail = @(Get-Content -LiteralPath $log | Where-Object { $_ -match 'error|errore' } | Select-Object -First 5) }
Stop-With $(if ($code -eq 4) { 4 } else { 1 }) "build non riuscita (codice $code, $min min)" (@($tail) + "Log completo: $log" + 'Non aprire l''editor: manda il log a un thread.')

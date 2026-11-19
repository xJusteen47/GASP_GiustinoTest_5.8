<#
Save-Assets.ps1 - the ONLY route an agent uses to save versioned assets headless in GASP_Giustino2_ue59.
The owner allows exactly this file in his Claude Code permissions (rule 2026-11-19, decision 3A of anim-setup-3).

  & .\Tools\CI\Save-Assets.ps1 -Script Tools\AnimPipeline\clipcheck\fix_clips_ue.py [-Only "RowA,RowB"] [-Tag name]

Guards (each one refuses with exit 2, nothing runs):
  - runs only from the GASP_Giustino2_ue59 checkout;
  - -Script must be a .py under Tools\ (no "..") that git tracks AND that has no uncommitted change, so what saves assets
    is always a reviewable commit;
  - no Unreal editor of this checkout may be open (PIE/capture in progress, or the _ue59 lock taken by a windowed editor).
Runs UnrealEditor-Cmd -run=pythonscript headless (-nullrhi) through Run-Timed.ps1 (15 min, job object, ESITO line).
Sets CLIPFIX_WRITE=1 and GSTN_SAVE=1: the scripts save only when they see one of them (their default is a dry run).
Log: Saved\Logs\ci\save_<tag>.log. Prints at most 5 lines.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Script,
    [string]$Only = '',
    [string]$Tag = 'assets'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
function Refuse($msg) { Write-Output "ESITO: RIFIUTATO - $msg"; exit 2 }

if ((Split-Path $root -Leaf) -ne 'GASP_Giustino2_ue59') { Refuse "solo nel checkout GASP_Giustino2_ue59 (qui: $root)" }
$rel = $Script -replace '/', '\'
if ($rel -notmatch '^(\.\\)?Tools\\[^:]+\.py$' -or $rel -match '\.\.') { Refuse "-Script deve essere un .py sotto Tools\" }
$rel = $rel -replace '^\.\\', ''
$full = Join-Path $root $rel
if (-not (Test-Path $full)) { Refuse "script non trovato: $rel" }
Push-Location $root
try {
    git ls-files --error-unmatch -- $rel 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) { Refuse "script non tracciato da git: $rel" }
    git diff --quiet HEAD -- $rel
    if ($LASTEXITCODE -ne 0) { Refuse "script con modifiche non committate: $rel" }
} finally { Pop-Location }
if ($Tag -notmatch '^[A-Za-z0-9_-]{1,40}$') { Refuse "-Tag: solo lettere, cifre, _ e -" }
$open = Get-CimInstance Win32_Process | Where-Object { $_.Name -like 'UnrealEditor*' -and $_.CommandLine -like '*GASP_Giustino2_ue59*' }
if ($open) { Refuse "un editor di questo checkout e' aperto (pid $($open[0].ProcessId))" }

$env:CLIPFIX_WRITE = '1'
$env:GSTN_SAVE = '1'
$env:CLIPFIX_ONLY = $Only
$engine = if ($env:UE_ENGINE_DIR) { $env:UE_ENGINE_DIR } else { 'C:\Program Files\Epic Games\UE_5.9\Engine' }
$exe = Join-Path $engine 'Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $root "Saved\Logs\ci\save_$Tag.log"
$ue = Join-Path $root "Saved\Logs\ci\save_${Tag}_ue.log"
$uproj = Join-Path $root 'GASP_Giustino2.uproject'
$argLine = "`"$uproj`" -run=pythonscript -script=`"$full`" -unattended -nullrhi -nosplash -nopause -abslog=`"$ue`""
& (Join-Path $PSScriptRoot 'Run-Timed.ps1') -Kind Capture -Log $log -Exe $exe -ArgLine $argLine -WatchFile $ue -MaxMinutes 15
exit $LASTEXITCODE

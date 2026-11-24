<#
Sync-AgentSkills.ps1 - make Antigravity read the project skills from the repo, not from copies (cleanup plan 2026-11-24, 5).
For every folder in <repo>\.claude\skills\<name> that has a SKILL.md it makes sure
  ~\.gemini\config\skills\<name>   is a junction to   <repo>\.claude\skills\<name>
and it removes BROKEN junctions in that folder (e.g. gstn-anim-cycle-cheap, renamed by the unified skill). A real folder (not a
junction) with the same name is never touched: it is reported and left for the owner (it may hold local edits).
DRY RUN by default: it lists what it would do and changes nothing. -Apply does it. Run it on each machine that runs Antigravity.
  powershell -NoProfile -ExecutionPolicy Bypass -File Tools\CI\Sync-AgentSkills.ps1            (prova)
  powershell -NoProfile -ExecutionPolicy Bypass -File Tools\CI\Sync-AgentSkills.ps1 -Apply
-Target overrides the Antigravity skills folder; -Root the repo. Console: max 5 lines, the full list is in <Target>\..\sync-skills.log.
#>
param(
    [switch]$Apply,
    [string]$Target = (Join-Path $env:USERPROFILE '.gemini\config\skills'),
    [string]$Root
)
$ErrorActionPreference = 'Stop'
$root = if ($Root) { $Root } else { (Resolve-Path "$PSScriptRoot\..\..").Path }
$src = Join-Path $root '.claude\skills'
if (-not (Test-Path -LiteralPath $src)) { Write-Output "ERRORE: manca $src"; exit 1 }

function Test-Junction([string]$Path) {
    $i = Get-Item -LiteralPath $Path -Force -ErrorAction SilentlyContinue
    return ($i -and ($i.Attributes -band [IO.FileAttributes]::ReparsePoint))
}

$plan = @()
# 1. broken junctions go first (a junction whose target does not exist any more)
if (Test-Path -LiteralPath $Target) {
    foreach ($d in Get-ChildItem -LiteralPath $Target -Force -Directory -ErrorAction SilentlyContinue) {
        if (Test-Junction $d.FullName) {
            $t = $d.Target; if ($t -is [array]) { $t = $t[0] }
            if (-not $t -or -not (Test-Path -LiteralPath $t)) { $plan += [pscustomobject]@{ Act = 'remove-broken'; Name = $d.Name; Path = $d.FullName; To = '' } }
        }
    }
}
# 2. one junction per project skill
foreach ($s in Get-ChildItem -LiteralPath $src -Directory) {
    if (-not (Test-Path -LiteralPath (Join-Path $s.FullName 'SKILL.md'))) { continue }
    $link = Join-Path $Target $s.Name
    if (-not (Test-Path -LiteralPath $link)) { $plan += [pscustomobject]@{ Act = 'link'; Name = $s.Name; Path = $link; To = $s.FullName }; continue }
    if (Test-Junction $link) {
        $t = (Get-Item -LiteralPath $link -Force).Target; if ($t -is [array]) { $t = $t[0] }
        if ($t -and ($t.TrimEnd('\') -ieq $s.FullName.TrimEnd('\'))) { continue }          # already right
        $plan += [pscustomobject]@{ Act = 'relink'; Name = $s.Name; Path = $link; To = $s.FullName }
    } else {
        $plan += [pscustomobject]@{ Act = 'SKIP-real-folder'; Name = $s.Name; Path = $link; To = '' }   # never overwrite a real folder
    }
}

$log = Join-Path (Split-Path $Target -Parent) 'sync-skills.log'
if ($Apply) {
    New-Item -ItemType Directory -Force $Target | Out-Null
    foreach ($p in $plan) {
        switch ($p.Act) {
            'remove-broken' { (Get-Item -LiteralPath $p.Path -Force).Delete() }      # removes the junction only, never the target
            'relink'        { (Get-Item -LiteralPath $p.Path -Force).Delete(); New-Item -ItemType Junction -Path $p.Path -Target $p.To | Out-Null }
            'link'          { New-Item -ItemType Junction -Path $p.Path -Target $p.To | Out-Null }
        }
    }
    $plan | ForEach-Object { "$($_.Act)`t$($_.Name)`t$($_.To)" } | Set-Content -Path $log -Encoding UTF8
}
$n = @{}; foreach ($p in $plan) { $n[$p.Act] = 1 + [int]$n[$p.Act] }
Write-Output "SKILL SYNC $(if ($Apply) { 'ESEGUITO' } else { 'PROVA (niente cambiato; -Apply per eseguire)' })"
Write-Output "da fare: $(if ($plan.Count) { ($n.Keys | Sort-Object | ForEach-Object { "$_=$($n[$_])" }) -join ' ' } else { 'niente, tutto allineato' })"
foreach ($p in ($plan | Where-Object { $_.Act -in 'remove-broken', 'SKIP-real-folder' } | Select-Object -First 3)) { Write-Output "  $($p.Act): $($p.Name)" }

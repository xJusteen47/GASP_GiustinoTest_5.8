# Generic run start captures (runloop-desktop, 2026-11-21): the STOCK relaxed run start (gstn.RunStart.Variant 4, rows of
# runstart_generic_rows_ue.py) with and without the hand-off tuning (Handoff 1 = rows at 14), same kit, sequence and frames as
# runstart-entry-desktop. Evidence against v4r7h of that round (Saved\RunStartEntry\v4r7h). Needs the _ue59 editor closed.
# usage: runstart_generic_captures.ps1 [-Handoff 1,0] [-SavedDir RunStartGeneric]   Output 1 line per capture.
# runstart-generic-desktop: -Variant 5 -Clip AS_GSTN_RunStart_Generic_g1 -Name g1 -RefName generic_h1 (base = stock start capture)
param([int[]]$Handoff = @(1), [string]$SavedDir = 'RunStartGeneric', [string]$Ref = 'Saved\RunStartEntry\v4r7h',
      [int]$Variant = 4, [string]$Clip = 'M_Relaxed_Run_Start_F_*', [string]$Name = 'generic', [string]$RefName = 'v4r7h',
      [string]$Sequence = 'mv_run_start_v5', [string]$Window = '97, 140', [string]$ExtraCvars = '')
# runstart-turnaround-desktop: -Sequence mv_turn_180a|b / mv_turn_090a|b (stick reversed / sideways), -Window of the ASSERT
$proj = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
$cap = Join-Path $proj 'Tools\GASNPPTest\pie_capture_timed.ps1'
New-Item -ItemType Directory -Force (Join-Path $proj "Saved\$SavedDir") | Out-Null
foreach ($h in $Handoff) {
    $n = "${Name}_h$h"
    $spec = Join-Path $proj "Saved\$SavedDir\assert_$n.json"
    [IO.File]::WriteAllText($spec, "{`"$Sequence`": [{`"clip`": `"$Clip`", `"window`": [$Window], `"min_weight`": 0.8, `"coverage`": 0.9, `"additive`": [`"BS_Relaxed_*Lean*`"]}]}")
    $env:PIE_CAPTURE_CVARS = "gstn.Loco.Base 1;gstn.RunStart.Variant $Variant;gstn.RunStart.Handoff $h" + $(if ($ExtraCvars) { ";$ExtraCvars" } else { "" })   # -ExtraCvars "gstn.Loco.TorsoLean -5" (runstart-v23)
    $env:PIE_CAPTURE_ASSERT_REPLACE = '1'
    $out = Join-Path $proj "Saved\$SavedDir\$n"
    & $cap -Kit KIT_Test_Overlord -Out $out -Sequences $Sequence -Assert $spec | Out-File (Join-Path $proj "Saved\$SavedDir\$n.console.txt")
    $a = if (Test-Path (Join-Path $out 'assert.txt')) { Get-Content (Join-Path $out 'assert.txt') -TotalCount 1 } else { 'ASSERT: MANCANTE' }
    "$n rc=$LASTEXITCODE $a"
    $env:EV_CAPS = "${RefName}:$Sequence,${n}:$Sequence"
    python (Join-Path $PSScriptRoot 'runstart_v4_evidence.py') (Join-Path $proj $Ref) $out (Join-Path $proj "docs\notes\reviews\runstart-$n") --from=84 --to=180 --step=4 | Out-File (Join-Path $proj "Saved\$SavedDir\$n.evidence.txt")
}

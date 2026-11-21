# Run start hand-off fix captures (runstart-phase-desktop, 2026-11-21): v4r7, c, a, b with gstn.RunStart.Handoff 1 (rows at
# variant+10, clip copies <v>h), same kit, sequence and frames as runstart-v8c. Evidence of each <v>h against its own pre-fix
# capture of runstart-v8c (Saved\RunStartV8c\<v>). Needs the _ue59 editor closed (pie_capture_timed starts its own).
# usage: runstart_handoff_captures.ps1 [-Variants v4r7,v4r8c,v4r8a,v4r8b]
# Output: Saved\$SavedDir\<v>h\ (capture + assert.txt), docs\notes\reviews\runstart-<v>h\ (grids, mp4, metrics). 1 line per variant.
# -SavedDir / -ReviewPrefix (runstart-entry-desktop): a new round keeps the previous round's evidence.
param([string[]]$Variants = @('v4r7', 'v4r8c', 'v4r8a', 'v4r8b'), [string]$SavedDir = 'RunStartPhase', [string]$ReviewPrefix = 'runstart-')
$proj = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
$idx = @{ v4r7 = 0; v4r8c = 1; v4r8a = 2; v4r8b = 3 }
$cap = Join-Path $proj 'Tools\GASNPPTest\pie_capture_timed.ps1'
New-Item -ItemType Directory -Force (Join-Path $proj "Saved\$SavedDir") | Out-Null
foreach ($v in $Variants) {
    $spec = Join-Path $proj "Saved\$SavedDir\assert_${v}h.json"
    [IO.File]::WriteAllText($spec, "{`"mv_run_start_v5`": [{`"clip`": `"AS_GSTN_RunStart_${v}h`", `"window`": [97, 115], `"min_weight`": 0.8, `"coverage`": 0.9, `"additive`": [`"BS_Relaxed_*Lean*`"]}]}")
    $env:PIE_CAPTURE_CVARS = "gstn.Loco.Base 1;gstn.RunStart.Variant $($idx[$v]);gstn.RunStart.Handoff 1"
    $env:PIE_CAPTURE_ASSERT_REPLACE = '1'
    $out = Join-Path $proj "Saved\$SavedDir\${v}h"
    & $cap -Kit KIT_Test_Overlord -Out $out -Sequences mv_run_start_v5 -Assert $spec | Out-File (Join-Path $proj "Saved\$SavedDir\${v}h.console.txt")
    $a = if (Test-Path (Join-Path $out 'assert.txt')) { Get-Content (Join-Path $out 'assert.txt') -TotalCount 1 } else { 'ASSERT: MANCANTE' }
    "${v}h rc=$LASTEXITCODE $a"
}
foreach ($v in $Variants) {
    $env:EV_CAPS = "${v}:mv_run_start_v5,${v}h:mv_run_start_v5"
    python (Join-Path $PSScriptRoot 'runstart_v4_evidence.py') (Join-Path $proj "Saved\RunStartV8c\$v") (Join-Path $proj "Saved\$SavedDir\${v}h") (Join-Path $proj "docs\notes\reviews\${ReviewPrefix}${v}h") --from=84 --to=144 --step=4 | Out-File (Join-Path $proj "Saved\$SavedDir\${v}h.evidence.txt")
}

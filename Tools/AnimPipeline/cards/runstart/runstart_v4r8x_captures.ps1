# Run start variants (runstart-v8c-desktop, 2026-11-21): one capture per variant on the clean base, same sequence and frames, then
# evidence per variant against the v4r7 base of the SAME run. Needs the _ue59 editor closed (pie_capture_timed starts its own).
#   gstn.RunStart.Variant: 0 = v4r7 (base), 1 = v4r8c, 2 = v4r8a, 3 = v4r8b
# usage: runstart_v4r8x_captures.ps1 [-Variants v4r7,v4r8c,v4r8a,v4r8b]
# Output: Saved\RunStartV8c\<tag>\ (capture + assert.txt), docs\notes\reviews\runstart-<tag>\ (grids, mp4, metrics). Prints 1 line per variant.
param([string[]]$Variants = @('v4r7', 'v4r8c', 'v4r8a', 'v4r8b'))
$proj = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
$idx = @{ v4r7 = 0; v4r8c = 1; v4r8a = 2; v4r8b = 3 }
$cap = Join-Path $proj 'Tools\GASNPPTest\pie_capture_timed.ps1'
foreach ($v in $Variants) {
    $spec = Join-Path $proj "Saved\RunStartV8c\assert_$v.json"
    if (-not (Test-Path $spec)) {
        [IO.File]::WriteAllText($spec, "{`"mv_run_start_v5`": [{`"clip`": `"AS_GSTN_RunStart_$v`", `"window`": [97, 115], `"min_weight`": 0.8, `"coverage`": 0.9, `"additive`": [`"BS_Relaxed_*Lean*`"]}]}")
    }
    $env:PIE_CAPTURE_CVARS = "gstn.Loco.Base 1;gstn.RunStart.Variant $($idx[$v])"
    $env:PIE_CAPTURE_ASSERT_REPLACE = '1'
    $out = Join-Path $proj "Saved\RunStartV8c\$v"
    & $cap -Kit KIT_Test_Overlord -Out $out -Sequences mv_run_start_v5 -Assert $spec | Out-File (Join-Path $proj "Saved\RunStartV8c\$v.console.txt")
    $a = if (Test-Path (Join-Path $out 'assert.txt')) { Get-Content (Join-Path $out 'assert.txt') -TotalCount 1 } else { 'ASSERT: MANCANTE' }
    "$v rc=$LASTEXITCODE $a"
}
$base = Join-Path $proj 'Saved\RunStartV8c\v4r7'
foreach ($v in $Variants | Where-Object { $_ -ne 'v4r7' }) {
    $env:EV_CAPS = "v4r7:mv_run_start_v5,${v}:mv_run_start_v5"
    python (Join-Path $PSScriptRoot 'runstart_v4_evidence.py') $base (Join-Path $proj "Saved\RunStartV8c\$v") (Join-Path $proj "docs\notes\reviews\runstart-$v") --from=84 --to=144 --step=4 | Out-File (Join-Path $proj "Saved\RunStartV8c\$v.evidence.txt")
}

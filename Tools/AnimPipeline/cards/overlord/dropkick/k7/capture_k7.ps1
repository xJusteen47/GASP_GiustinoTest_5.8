# Dropkick K7 skill test captures: base K6 and K7 kits, one Play session each (PIE_CAPTURE_RUNS: whiff, hit, hit + hitbox debug)
$P = 'C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_cp1verify'
$C = "$P\Saved\AnimPipeline\captures\dropkick-kick-k6\k7"
Set-Location $P
$env:PIE_CAPTURE_AIR_GIVE_UP = '200'; $env:PIE_CAPTURE_DROPKICK_LOW_DIST = '190'; $env:PIE_CAPTURE_DIST = '350'
$env:PIE_CAPTURE_VIEWS = 'az90,az180,game'; $env:PIE_CAPTURE_DROPKICK_SWEET_H = '480'
foreach ($r in @(@{n='base'; kit='KIT_Test_PlaytestTBK6'}, @{n='k7'; kit='KIT_Test_PlaytestTBK7'})) {
    $t0 = Get-Date
    $env:PIE_CAPTURE_RUNS = "$C\runs_$($r.n).json"
    & "$P\Tools\GASNPPTest\pie_capture_timed.ps1" -Kit $r.kit -Out "$C\$($r.n)" -Assert "$C\assert_$($r.n).json"
    "$($r.n): exit $LASTEXITCODE min $([math]::Round(((Get-Date) - $t0).TotalMinutes, 1))"
    Get-Content "$C\$($r.n)\assert.txt" -TotalCount 1 -ErrorAction SilentlyContinue
}

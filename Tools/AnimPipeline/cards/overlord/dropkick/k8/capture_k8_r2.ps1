# Dropkick K8 round 2 capture (review 1 point 2): ALL whiff runs before any hit run, so no hit dummy is left in a whiff scene.
# One Play session, per-run kit (runs_r2.json). Env as K7/K8 r1. Output Saved\AnimPipeline\captures\dropkick-kick-k6\k8r2.
$P = 'C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_cp1verify'
$R = "$P\Saved\AnimPipeline\captures\dropkick-kick-k6\k8r2"
New-Item -ItemType Directory -Force $R | Out-Null
Set-Location $P
$env:PATH = "$env:TEMP\ffbin;$env:PATH"
$env:PIE_CAPTURE_AIR_GIVE_UP = '200'; $env:PIE_CAPTURE_DROPKICK_LOW_DIST = '190'; $env:PIE_CAPTURE_DIST = '350'
$env:PIE_CAPTURE_VIEWS = 'az90,az180,game'; $env:PIE_CAPTURE_DROPKICK_SWEET_H = '480'
Copy-Item "$P\Tools\AnimPipeline\cards\overlord\dropkick\k8\runs_r2.json" "$R\runs.json" -Force
Copy-Item "$P\Tools\AnimPipeline\cards\overlord\dropkick\k8\assert_r2.json" "$R\assert_spec.json" -Force   # default; each run has its own
$env:PIE_CAPTURE_RUNS = "$R\runs.json"
& "$P\Tools\GASNPPTest\pie_capture_timed.ps1" -Kit KIT_Test_PlaytestTBK8 -Out $R -Assert "$R\assert_spec.json"
"exit $LASTEXITCODE"

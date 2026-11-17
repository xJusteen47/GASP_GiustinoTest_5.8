# M1 step 3: one standalone off-screen capture (animpipe standalone_capture.py) of one clip at one azimuth, under Run-Timed.
# usage: react_cap_m1.ps1 -Anim /Game/... -Name asis -Az 315
param([string]$Anim, [string]$Name, [int]$Az = 315, [int]$Dist = 450, [int]$Az2 = 0, [string]$Center = "")   # Center as "x;y;z" (a comma list is read as one number in this locale)
$proj = Split-Path (Split-Path $PSScriptRoot)
$out = Join-Path $proj "Saved\Reaction\m1cap\$Name"
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Force $out | Out-Null
$job = @{ anim = $Anim; move = $Name; azs = $(if ($Az2) { @($Az, $Az2) } else { @($Az) }); size = 384; el = 12; dist = $Dist; fov = 55
          center = $(if ($Center) { [double[]]($Center -split ';' | ForEach-Object { [double]::Parse($_, [Globalization.CultureInfo]::InvariantCulture) }) } else { $null }); out_dir = ($out -replace '\\', '/'); tools_dir = (Join-Path $proj "Tools\AnimPipeline") -replace '\\', '/' }
$jp = Join-Path $out "job.json"
[IO.File]::WriteAllText($jp, ($job | ConvertTo-Json), (New-Object Text.UTF8Encoding $false))   # no BOM: Python json.load rejects it
$env:ANIMPIPE_JOB = $jp -replace '\\', '/'
$env:ANIMPIPE_BOOT = (Join-Path $proj "Tools\AnimPipeline\standalone_capture.py") -replace '\\', '/'
$boot = Join-Path $PSScriptRoot "react_cap_boot_m1.py"
& (Join-Path $PSScriptRoot "..\CI\Run-Timed.ps1") -Preset Capture -Log (Join-Path $out "run.log") -Script $boot -MaxMinutes 12

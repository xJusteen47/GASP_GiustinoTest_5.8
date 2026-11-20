<#
Opens the UE 5.9 editor on this project with FORWARD SHADING, without touching Config/DefaultEngine.ini.
The overrides live only on this command line (-ini:Engine:...), so closing the editor and opening it the normal way
("Apri editor 5.9.bat") brings back deferred + Lumen. Nothing is written to the project config.

FIRST LAUNCH: forward shading is a different shader permutation set -> expect a long shader compile (tens of minutes,
like the first 5.9 open). Later launches reuse the local DDC; the deferred shaders stay cached too.
One editor per project copy: close the normal editor first.

  -Msaa      use MSAA x8 (r.MSAACount=8 is already in the project) instead of TSR. Sharper, no temporal smear/ghosting,
             but no temporal effects (hair/dither fades look noisy) and costs more with many polygons.
  -Engine    path to UnrealEditor.exe (default: UE_5.9 in Program Files)
  -Map       map to open (default L_Test_Hub)

Doc: docs/notes/rendering-profiles.md
#>
param(
    [switch]$Msaa,
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.9\Engine\Binaries\Win64\UnrealEditor.exe',
    [string]$Map = '/Game/Levels/Test/L_Test_Hub'
)
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$uproject = Join-Path $root 'GASP_Giustino2.uproject'
if (-not (Test-Path $Engine)) { throw "UnrealEditor.exe not found: $Engine (pass -Engine)" }

$rs = '[/Script/Engine.RendererSettings]'
$ov = @(
    "${rs}:r.ForwardShading=1",
    # Lumen needs the deferred GBuffer: switch GI/reflections to non-Lumen so nothing asks for it
    "${rs}:r.DynamicGlobalIlluminationMethod=0",
    "${rs}:r.ReflectionMethod=0",
    # Keep sky occlusion from the existing mesh distance fields
    "${rs}:r.GenerateMeshDistanceFields=True"
)
if ($Msaa) { $ov += "${rs}:r.AntiAliasingMethod=3" }
$iniArg = '-ini:Engine:' + ($ov -join ',')

Write-Host "Forward editor: $uproject"
Write-Host "Overrides: $($ov -join ' | ')"
Start-Process -FilePath $Engine -ArgumentList @("`"$uproject`"", $Map, $iniArg, '-nosplash')
Write-Host 'Started. In the console: gstn.Render.Info  (r.ForwardShading must print 1)'

# M1 depth marker: timed PIE capture in L_Test_Hub (15 min cap). usage: dm_cap_m1.ps1 -Out <dir> -Seqs dm_circle_jump,dm_projection
param([string]$Out, [string]$Seqs = "dm_circle_jump,dm_projection", [string]$Views = "game,az90")
$proj = Split-Path (Split-Path $PSScriptRoot)
New-Item -ItemType Directory -Force $Out | Out-Null
$env:PIE_CAPTURE_OUT = $Out.Replace([string][char]92, '/')
$env:PIE_CAPTURE_SEQUENCES = $Seqs
$env:PIE_CAPTURE_VIEWS = $Views
$boot = (Join-Path $PSScriptRoot "dm_cap_boot_m1.py").Replace([string][char]92, '/')
$exe = "C:\Program Files\Epic Games\UE_5.9\Engine\Binaries\Win64\UnrealEditor.exe"
$al = "`"$proj\GASP_Giustino2.uproject`" /Game/GSTN/Maps/L_Test_Hub -windowed -nosplash -nosound -unattended -ExecCmds=`"py $boot`" -abslog=`"$Out\editor.log`""
& (Join-Path $PSScriptRoot "..\CI\Run-Timed.ps1") -Kind Capture -Log (Join-Path $Out "run.log") -Exe $exe -ArgLine $al -WatchFile "$Out\editor.log" -MaxMinutes 15

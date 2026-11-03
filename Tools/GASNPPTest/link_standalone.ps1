param(
    [string]$Scenarios = 'warm:link_spike_client_grabs_client,link_spike_host_grabs_client,link_spike_client_grabs_host,link_spike_client_grabs_client',
    [string]$Loss = '2',
    [string]$Report = 'link-standalone.log',
    [string]$Tag = 'sa',
    [switch]$NullRhi,
    [switch]$PrintReconciles,
    [switch]$NoLink,
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe')
# One batch of link_mp scenarios with a standalone listen host and two client game processes (see link_standalone.py). Fresh processes per
# batch: host first (it runs link_standalone.py), client 1 once the host listens, client 2 once client 1 has loaded the map (the join order is
# what link_mp's keys rely on). -NullRhi on all three: rendering three game processes and an editor saturates a laptop (a client hardly ticked).
# -notimeouts: an uncooked client's map load takes over a minute without servicing its connection. Logs: Saved/Logs/<report dir>/<Tag>-{host,c1,c2}.log;
# the report under Saved/Logs/<Report>. The editor may stay open (throttled), but nothing else on the machine should run the game.
#   powershell -File Tools/GASNPPTest/link_standalone.ps1 -Loss 2 -Report f14-sweep/sa-spike-2pct-1.log -Tag spike2-1 -NullRhi
$p = (Resolve-Path "$PSScriptRoot\..\..").Path
$uproject = "$p\GASP_Giustino2.uproject"
$rep = "$p\Saved\Logs\$($Report -replace '/', '\')"
$dir = Split-Path $rep -Parent
New-Item -ItemType Directory -Force $dir | Out-Null
if (Test-Path $rep) { Remove-Item $rep }
# -ExecCmds cannot quote a path with spaces inside its own quotes: run a copy from the temp directory (the script finds the harness by project dir)
$py = Join-Path $env:TEMP 'gstn_link_standalone.py'
Copy-Item "$PSScriptRoot\link_standalone.py" $py -Force
$env:GSTN_SA_SCENARIOS = $Scenarios; $env:GSTN_SA_LOSS = $Loss; $env:GSTN_SA_REPORT = $Report
$env:GSTN_SA_PRINT = if ($PrintReconciles) { '1' } else { '' }
$env:GSTN_SA_NOLINK = if ($NoLink) { '1' } else { '' }
$extra = @(); if ($NullRhi) { $extra = @('-nullrhi') }
$common = @('-game', '-windowed', '-ResX=640', '-ResY=360', '-WinY=40', '-nosplash', '-nosound', '-notimeouts') + $extra
$hostLog = "$dir\$Tag-host.log"
$map = '/Game/Levels/DefaultLevel_Harness?listen?game=/Game/GASNPPTest/GM_GASNPPTest.GM_GASNPPTest_C'
function WaitFor($file, $pattern, $secs) { for ($i = 0; $i -lt $secs; $i++) { if ((Test-Path $file) -and (Select-String -Path $file -Pattern $pattern -Quiet)) { return $true }; [System.Threading.Thread]::Sleep(1000) }; return $false }
$t0 = Get-Date
$h = Start-Process -FilePath $Engine -PassThru -ArgumentList (@("`"$uproject`"", $map, '-WinX=0', "-abslog=`"$hostLog`"", "-ExecCmds=`"py $($py -replace '\\', '/')`"") + $common)
if (-not (WaitFor $hostLog 'GSTN standalone host runner armed' 240)) { "host never armed"; Stop-Process -Id $h.Id -Force -ErrorAction SilentlyContinue; return }
"host armed after $([int]((Get-Date) - $t0).TotalSeconds) s"
$c = @()
foreach ($i in 1, 2) {
    $cl = "$dir\$Tag-c$i.log"
    $c += Start-Process -FilePath $Engine -PassThru -ArgumentList (@("`"$uproject`"", '127.0.0.1', "-WinX=$(660 * $i)", "-abslog=`"$cl`"") + $common)
    if (WaitFor $cl 'LogLoad: Took .* to LoadMap\(/Game/Levels/DefaultLevel_Harness\)' 300) { "client $i in after $([int]((Get-Date) - $t0).TotalSeconds) s" } else { "client $i not in after 300 s" }
    [System.Threading.Thread]::Sleep(5000)
}
for ($i = 0; $i -lt 900; $i++) { if ((Test-Path $rep) -or $h.HasExited) { break }; [System.Threading.Thread]::Sleep(1000) }
"report: $(Test-Path $rep) after $([int]((Get-Date) - $t0).TotalSeconds) s"
for ($i = 0; $i -lt 40; $i++) { if ($h.HasExited -and ($c | Where-Object { -not $_.HasExited }).Count -eq 0) { break }; [System.Threading.Thread]::Sleep(1000) }
foreach ($x in @($h) + $c) { if (-not $x.HasExited) { "force-stopping $($x.Id)"; Stop-Process -Id $x.Id -Force -ErrorAction SilentlyContinue } }

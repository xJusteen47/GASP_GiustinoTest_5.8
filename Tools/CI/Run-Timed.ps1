<#
Run-Timed.ps1 - run a build / test / capture under a hard time limit (project rule 5).

The wrapper STARTS the process itself, suspended, puts it in a Windows Job Object and only then resumes it, so every child
(UnrealEditor, ShaderCompileWorker, cl.exe, python...) belongs to the job. On timeout, on a silent log, or when the wrapper dies,
the whole job is terminated by the wrapper itself: no taskkill of foreign processes, so it works under Claude auto mode.

Output goes to a file; the console gets at most 5 lines. Exit code: 0 OK, 1 FAIL, 3 ASSERT not OK (with -AssertFile), 124 TIMEOUT,
125 BLOCCATO (log silent).

  Run-Timed.ps1 -Preset Build   -Log Saved\Logs\ci\build.log
  Run-Timed.ps1 -Preset Test    -Log Saved\Logs\ci\test.log -Tests GSTN            (Automation RunTests <filter>)
  Run-Timed.ps1 -Preset Capture -Log Saved\Logs\ci\cap.log  -Script Tools\GASNPPTest\hub_startup.py
  Run-Timed.ps1 -Kind Test -Log x.log -Exe "C:\path\tool.exe" -ArgLine "arg1 \"arg 2\""     (generic)
  Run-Timed.ps1 -SelfTest

Limits: Build 45 min, Test 30, Capture 15 (override: -MaxMinutes / -MaxSeconds). Silent log limit 10 min (-SilentMinutes /
-SilentSeconds): no growth of the watched file (default = the log) => ESITO: BLOCCATO. Use -WatchFile for a process that
writes elsewhere (UE: -abslog). The verdict is also written to <Log>.esito (first line "ESITO: ...").
Engine: env UE_ENGINE_DIR, default C:\Program Files\Epic Games\UE_5.9\Engine.

No blocking dialogs (audit 2026-11-20, skid-root-1 lost 2 attempts to them): before an editor starts (Test, Capture, or an -Exe that
is UnrealEditor*), <project>\Saved\Autosaves\PackageRestoreData.json is renamed to PackageRestoreData.json.<date_time>.bak (the
"restore packages?" dialog waits forever on a headless run); -KeepRestoreData skips it. <project> = the folder of the first
.uproject in the command line, else this repo. Every editor run gets -unattended (also with -KeepThrottle, which now only keeps
the background-throttle ini and EcoQoS as they are).
Capture gate (project rule 10): -AssertFile <capture>\assert.txt (written by pie_capture_move.py) adds its first line
("ASSERT: OK" / "ASSERT: FAIL ...", or "ASSERT: MANCANTE") to the output; ESITO OK with an ASSERT not OK exits 3, so no review
starts on a video that does not show the expected clip.
#>
[CmdletBinding()]
param(
    [ValidateSet('Build', 'Test', 'Capture')][string]$Preset,
    [ValidateSet('Build', 'Test', 'Capture')][string]$Kind,
    [string]$Log,
    [string]$Exe,
    [string]$ArgLine = '',
    [string]$Tests = 'GSTN',
    [string]$Script,
    [string]$Target = 'GASP_Giustino2Editor',
    [string]$WatchFile,
    [double]$MaxMinutes,
    [double]$MaxSeconds,
    [double]$SilentMinutes = 10,
    [double]$SilentSeconds,
    [string]$Engine,
    [switch]$SelfTest,
    [switch]$KeepThrottle,      # Capture: keep the editor's background throttle ini and EcoQoS (default: off, see docs/notes/speed-up.md)
    [string]$ExtraArgs = '',    # appended to the preset's command line
    [switch]$KeepRestoreData,   # do not move Saved\Autosaves\PackageRestoreData.json aside before an editor run
    [string]$AssertFile         # capture gate: the assert.txt to read after the run (exit 3 when ESITO OK but ASSERT not OK)
)

$ErrorActionPreference = 'Stop'
$script:Unthrottle = -not $KeepThrottle

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public class GstnJob {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    struct STARTUPINFO { public int cb; public string lpReserved, lpDesktop, lpTitle; public int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags; public short wShowWindow, cbReserved2; public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError; }
    [StructLayout(LayoutKind.Sequential)]
    struct PROCESS_INFORMATION { public IntPtr hProcess, hThread; public int dwProcessId, dwThreadId; }
    [StructLayout(LayoutKind.Sequential)]
    struct BASIC_LIMIT { public long PerProcessUserTimeLimit, PerJobUserTimeLimit; public uint LimitFlags; public UIntPtr MinWs, MaxWs; public uint ActiveProcessLimit; public UIntPtr Affinity; public uint PriorityClass, SchedulingClass; }
    [StructLayout(LayoutKind.Sequential)]
    struct IO_COUNTERS { public ulong a, b, c, d, e, f; }
    [StructLayout(LayoutKind.Sequential)]
    struct EXT_LIMIT { public BASIC_LIMIT Basic; public IO_COUNTERS Io; public UIntPtr ProcessMem, JobMem, PeakProcessMem, PeakJobMem; }
    [StructLayout(LayoutKind.Sequential)]
    struct BASIC_ACCOUNTING { public long TotalUser, TotalKernel, PeriodUser, PeriodKernel; public uint PageFaults, TotalProcesses, ActiveProcesses, TotalTerminated; }

    [DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr CreateJobObject(IntPtr a, string n);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool SetInformationJobObject(IntPtr j, int c, ref EXT_LIMIT i, int l);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool QueryInformationJobObject(IntPtr j, int c, out BASIC_ACCOUNTING i, int l, IntPtr r);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool AssignProcessToJobObject(IntPtr j, IntPtr p);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool TerminateJobObject(IntPtr j, uint code);
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern bool CreateProcessW(string app, StringBuilder cmd, IntPtr pa, IntPtr ta, bool inherit, uint flags, IntPtr env, string cwd, ref STARTUPINFO si, out PROCESS_INFORMATION pi);
    [DllImport("kernel32.dll", SetLastError = true)] static extern uint ResumeThread(IntPtr t);
    [DllImport("kernel32.dll", SetLastError = true)] static extern uint WaitForSingleObject(IntPtr h, uint ms);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool GetExitCodeProcess(IntPtr h, out uint code);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool CloseHandle(IntPtr h);

    IntPtr job, proc;
    public int Pid;

    // Starts commandLine suspended inside a new kill-on-close job, then resumes it. Throws on failure.
    public void Start(string commandLine, string cwd) {
        job = CreateJobObject(IntPtr.Zero, null);
        if (job == IntPtr.Zero) throw new Exception("CreateJobObject failed " + Marshal.GetLastWin32Error());
        EXT_LIMIT lim = new EXT_LIMIT();
        lim.Basic.LimitFlags = 0x2000; // JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        if (!SetInformationJobObject(job, 9, ref lim, Marshal.SizeOf(typeof(EXT_LIMIT)))) throw new Exception("SetInformationJobObject failed " + Marshal.GetLastWin32Error());
        STARTUPINFO si = new STARTUPINFO(); si.cb = Marshal.SizeOf(typeof(STARTUPINFO));
        PROCESS_INFORMATION pi;
        // CREATE_SUSPENDED | CREATE_NO_WINDOW (console only; GUI windows of the editor are unaffected)
        if (!CreateProcessW(null, new StringBuilder(commandLine), IntPtr.Zero, IntPtr.Zero, false, 0x4u | 0x08000000u, IntPtr.Zero, cwd, ref si, out pi))
            throw new Exception("CreateProcess failed " + Marshal.GetLastWin32Error());
        proc = pi.hProcess; Pid = pi.dwProcessId;
        if (!AssignProcessToJobObject(job, proc)) { int e = Marshal.GetLastWin32Error(); TerminateProcessSafe(pi); throw new Exception("AssignProcessToJobObject failed " + e); }
        ResumeThread(pi.hThread); CloseHandle(pi.hThread);
    }
    static void TerminateProcessSafe(PROCESS_INFORMATION pi) { try { System.Diagnostics.Process.GetProcessById(pi.dwProcessId).Kill(); } catch { } }
    public bool WaitMs(uint ms) { return WaitForSingleObject(proc, ms) == 0; }   // true = the main process has exited
    public uint ExitCode() { uint c; GetExitCodeProcess(proc, out c); return c; }
    public uint Active() { BASIC_ACCOUNTING a; return QueryInformationJobObject(job, 1, out a, Marshal.SizeOf(typeof(BASIC_ACCOUNTING)), IntPtr.Zero) ? a.ActiveProcesses : 0; }
    public void KillAll() { if (job != IntPtr.Zero) TerminateJobObject(job, 1); }

    // Windows 11 EcoQoS / timer-resolution throttling of background processes: opt the process out (speed-up.md, item 1)
    [StructLayout(LayoutKind.Sequential)] struct POWER_THROTTLING { public uint Version, ControlMask, StateMask; }
    [DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool SetProcessInformation(IntPtr h, int cls, ref POWER_THROTTLING info, int size);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool SetPriorityClass(IntPtr h, uint cls);
    public static string Unthrottle(int pid) {
        IntPtr h = OpenProcess(0x0200, false, pid);   // PROCESS_SET_INFORMATION
        if (h == IntPtr.Zero) return "open failed " + Marshal.GetLastWin32Error();
        POWER_THROTTLING s = new POWER_THROTTLING(); s.Version = 1; s.ControlMask = 0x1 | 0x4; s.StateMask = 0; // EXECUTION_SPEED | IGNORE_TIMER_RESOLUTION, both off
        bool a = SetProcessInformation(h, 4, ref s, Marshal.SizeOf(typeof(POWER_THROTTLING)));   // ProcessPowerThrottling
        bool b = SetPriorityClass(h, 0x8000);                                                     // ABOVE_NORMAL_PRIORITY_CLASS
        CloseHandle(h);
        return "ecoqos_off=" + a + " above_normal=" + b;
    }
    public void Close() { if (proc != IntPtr.Zero) CloseHandle(proc); if (job != IntPtr.Zero) CloseHandle(job); proc = IntPtr.Zero; job = IntPtr.Zero; }
}
'@

$Root = (Resolve-Path "$PSScriptRoot\..\..").Path

function Invoke-Timed {
    param([string]$Kind, [string]$Log, [string]$Exe, [string]$ArgLine, [string]$Watch,
          [double]$MaxSec, [double]$SilentSec, [string]$Cwd, [switch]$Quiet)
    $logDir = Split-Path $Log -Parent
    if ($logDir) { New-Item -ItemType Directory -Force $logDir | Out-Null }
    if (Test-Path $Log) { Remove-Item $Log -Force }
    if (-not $Watch) { $Watch = $Log }
    $esito = "$Log.esito"
    # cmd /s /c "<line>": the redirect is inside cmd, so the child writes straight to the file and keeps running if we die
    $line = "cmd.exe /d /s /c `"`"$Exe`" $ArgLine > `"$Log`" 2>&1`""
    $j = New-Object GstnJob
    $t0 = Get-Date
    $verdict = $null; $code = $null
    try {
        $j.Start($line, $Cwd)
        $lastSize = -1; $lastChange = Get-Date
        $unthrottled = @{}; $nextScan = Get-Date
        while ($true) {
            if ($j.WaitMs(1000)) { $code = [int][BitConverter]::ToInt32([BitConverter]::GetBytes($j.ExitCode()), 0); break }
            $now = Get-Date
            if ($script:Unthrottle -and $now -ge $nextScan) {
                # the editor is a child of cmd.exe (and PIE clients children of the editor): opt each one out of EcoQoS once
                $nextScan = $now.AddSeconds(5)
                $tree = @($j.Pid); $all = @(Get-CimInstance Win32_Process -Property ProcessId, ParentProcessId, Name)
                for ($k = 0; $k -lt $tree.Count; $k++) { $tree += @($all | Where-Object { $_.ParentProcessId -eq $tree[$k] } | ForEach-Object ProcessId) }
                foreach ($p in $all) {
                    if ($tree -contains $p.ProcessId -and $p.Name -match '^UnrealEditor' -and -not $unthrottled[$p.ProcessId]) {
                        $unthrottled[$p.ProcessId] = [GstnJob]::Unthrottle([int]$p.ProcessId)
                    }
                }
            }
            if (($now - $t0).TotalSeconds -ge $MaxSec) { $verdict = 'TIMEOUT'; break }
            $size = -1
            if (Test-Path $Watch) { $fi = Get-Item $Watch; $size = $fi.Length + $fi.LastWriteTimeUtc.Ticks }
            if ($size -ne $lastSize) { $lastSize = $size; $lastChange = $now }
            elseif (($now - $lastChange).TotalSeconds -ge $SilentSec) { $verdict = 'BLOCCATO'; break }
        }
        $left = $j.Active()
        $j.KillAll()          # always: nothing may outlive the run (editor, workers, python)
    } finally { $j.Close() }
    $secs = [int]((Get-Date) - $t0).TotalSeconds
    if (-not $verdict) { $verdict = if ($code -eq 0) { 'OK' } else { 'FAIL' } }
    $exit = switch ($verdict) { 'OK' { 0 } 'FAIL' { 1 } 'TIMEOUT' { 124 } 'BLOCCATO' { 125 } }
    $tail = ''
    if (Test-Path $Log) {
        $hit = Select-String -Path $Log -Pattern 'error|fail|fatal|crash' -SimpleMatch:$false | Select-Object -Last 1
        if ($hit) { $tail = $hit.Line.Trim(); if ($tail.Length -gt 160) { $tail = $tail.Substring(0, 160) } }
    }
    $out = @("ESITO: $verdict", "tipo=$Kind durata=${secs}s limite=$([int]$MaxSec)s exit=$code figli_rimasti_uccisi=$left", "log=$Log")
    if ($tail) { $out += "ultima riga errore: $tail" }
    $out | Set-Content -Path $esito -Encoding UTF8
    if (-not $Quiet) { $out | Select-Object -First 5 | ForEach-Object { Write-Host $_ } }
    return $exit
}

# Moves the autosave restore list aside so the editor never opens the modal "restore packages?" dialog on a headless run.
# Returns a short note for the output ('' when there was nothing to move).
function Clear-RestoreData {
    param([string]$ProjectDir)
    $f = Join-Path $ProjectDir 'Saved\Autosaves\PackageRestoreData.json'
    if (-not (Test-Path -LiteralPath $f)) { return '' }
    $bak = "$f.$(Get-Date -Format 'yyyyMMdd_HHmmss').bak"
    try { Rename-Item -LiteralPath $f -NewName (Split-Path $bak -Leaf) -ErrorAction Stop; return "prestart: PackageRestoreData.json -> $(Split-Path $bak -Leaf)" }
    catch { return "prestart: PackageRestoreData.json NOT moved ($($_.Exception.Message))" }
}

# The project folder of a command line: the first quoted (or bare) path ending in .uproject, else $Fallback.
function Get-ProjectDir {
    param([string]$Line, [string]$Fallback)
    $m = [regex]::Match($Line, '"([^"]+\.uproject)"|(\S+\.uproject)')
    if ($m.Success) { $p = if ($m.Groups[1].Success) { $m.Groups[1].Value } else { $m.Groups[2].Value }; return (Split-Path $p -Parent) }
    return $Fallback
}

# First line of the capture's assert file, or the reason it is missing.
function Read-AssertLine {
    param([string]$Path)
    if (-not $Path) { return '' }
    if (-not (Test-Path -LiteralPath $Path)) { return "ASSERT: MANCANTE ($Path)" }
    $l = Get-Content -LiteralPath $Path -TotalCount 1
    if ($l -match '^ASSERT: ') { return $l.Trim() }
    return "ASSERT: MANCANTE (prima riga non valida in $Path)"
}

function Resolve-EngineDir {
    if ($Engine) { return $Engine }
    if ($env:UE_ENGINE_DIR) { return $env:UE_ENGINE_DIR }
    return 'C:\Program Files\Epic Games\UE_5.9\Engine'
}

# ---- self test: needs only Windows PowerShell, no engine ------------------------------------------------------------
if ($SelfTest) {
    $d = Join-Path $env:TEMP 'runtimed-selftest'; New-Item -ItemType Directory -Force $d | Out-Null
    $ps = (Get-Command powershell.exe).Source
    $res = @()
    $r = Invoke-Timed 'Test' "$d\ok.log" $ps '-NoProfile -Command "Write-Output hello"' $null 30 30 $d -Quiet | Select-Object -Last 1
    $res += "1 OK-case: $(if ($r -eq 0) {'PASS'} else {'FAIL'})"
    $r = Invoke-Timed 'Test' "$d\fail.log" $ps '-NoProfile -Command "exit 3"' $null 30 30 $d -Quiet | Select-Object -Last 1
    $res += "2 FAIL-case: $(if ($r -eq 1) {'PASS'} else {'FAIL'})"
    # a child that starts a grandchild and sleeps: timeout must kill the whole tree
    $marker = "runtimed_selftest_$PID"
    $kid = "-NoProfile -Command `"`$host.UI.RawUI.WindowTitle='$marker'; Start-Process powershell.exe -ArgumentList '-NoProfile','-Command','Start-Sleep 300'; Start-Sleep 300`""
    $r = Invoke-Timed 'Test' "$d\to.log" $ps $kid $null 6 600 $d -Quiet | Select-Object -Last 1
    Start-Sleep -Seconds 1
    $alive = @(Get-CimInstance Win32_Process -Filter "Name='powershell.exe'" | Where-Object { $_.CommandLine -match 'Start-Sleep 300' }).Count
    $res += "3 TIMEOUT-case: $(if ($r -eq 124 -and $alive -eq 0) {'PASS'} else {"FAIL (exit=$r, sleepers left=$alive)"})"
    $r = Invoke-Timed 'Test' "$d\silent.log" $ps '-NoProfile -Command "Start-Sleep 300"' $null 600 4 $d -Quiet | Select-Object -Last 1
    $res += "4 BLOCCATO-case: $(if ($r -eq 125) {'PASS'} else {'FAIL'})"
    # 5: the restore list is moved aside, the project folder is found in a quoted command line, a second call finds nothing
    $proj = Join-Path $d 'proj with space'; New-Item -ItemType Directory -Force (Join-Path $proj 'Saved\Autosaves') | Out-Null
    Get-ChildItem (Join-Path $proj 'Saved\Autosaves') | Remove-Item -Force
    Set-Content (Join-Path $proj 'Saved\Autosaves\PackageRestoreData.json') '{}'
    $pd = Get-ProjectDir "`"$proj\X.uproject`" -run=pythonscript" $d
    $n1 = Clear-RestoreData $pd; $n2 = Clear-RestoreData $pd
    $baks = @(Get-ChildItem (Join-Path $proj 'Saved\Autosaves') -Filter 'PackageRestoreData.json.*.bak').Count
    $ok5 = ($pd -eq $proj) -and ($n1 -match '-> PackageRestoreData') -and ($n2 -eq '') -and ($baks -eq 1) -and -not (Test-Path (Join-Path $proj 'Saved\Autosaves\PackageRestoreData.json'))
    # 6: assert file: OK line, missing file, garbage
    Set-Content "$d\a_ok.txt" "ASSERT: OK seq AS_X f1-f9"; Set-Content "$d\a_bad.txt" "hello"
    $ok6 = ((Read-AssertLine "$d\a_ok.txt") -eq 'ASSERT: OK seq AS_X f1-f9') -and ((Read-AssertLine "$d\none.txt") -match '^ASSERT: MANCANTE') -and ((Read-AssertLine "$d\a_bad.txt") -match '^ASSERT: MANCANTE')
    $res += "5 RESTORE-DATA + 6 ASSERT-FILE: $(if ($ok5 -and $ok6) {'PASS'} else {"FAIL (restore=$ok5 [$pd|$n1|$n2|$baks] assert=$ok6)"})"
    $res | Select-Object -First 5 | ForEach-Object { Write-Output $_ }
    exit $(if ($res -match ': FAIL') { 1 } else { 0 })
}

# ---- real run ---------------------------------------------------------------------------------------------------------
if (-not $Kind) { $Kind = if ($Preset) { $Preset } else { 'Test' } }
if (-not $Log) { Write-Output 'ERRORE: -Log obbligatorio'; exit 2 }
$limits = @{ Build = 45; Test = 30; Capture = 15 }
$maxSec = if ($MaxSeconds) { $MaxSeconds } elseif ($MaxMinutes) { $MaxMinutes * 60 } else { $limits[$Kind] * 60 }
$silentSec = if ($SilentSeconds) { $SilentSeconds } else { $SilentMinutes * 60 }
$uproject = "$Root\GASP_Giustino2.uproject"
$eng = Resolve-EngineDir
$Log = if ([IO.Path]::IsPathRooted($Log)) { $Log } else { Join-Path $Root $Log }

# -unattended: UEditorEngine::ShouldThrottleCPUUsage returns false (no 3 fps cap in the background, no modal dialogs).
# The ini override also keeps editor viewports rendering when the window has no focus (EditorEngine.cpp, 'Background Process').
$NoThrottleIni = '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False'
switch ($Preset) {
    'Build' {
        $Exe = "$eng\Build\BatchFiles\Build.bat"
        $ArgLine = "$Target Win64 Development `"-Project=$uproject`" -WaitMutex -NoHotReload"
    }
    'Test' {
        $Exe = "$eng\Binaries\Win64\UnrealEditor-Cmd.exe"
        $ArgLine = "`"$uproject`" -ExecCmds=`"Automation RunTests $Tests; Quit`" -unattended -nopause -nosplash -nullrhi -nosound -stdout -FullStdOutLogOutput -abslog=`"$Log.ue.log`""
        if (-not $WatchFile) { $WatchFile = "$Log.ue.log" }
    }
    'Capture' {
        if (-not $Script) { Write-Output 'ERRORE: -Script obbligatorio per Capture'; exit 2 }
        $py = if ([IO.Path]::IsPathRooted($Script)) { $Script } else { Join-Path $Root $Script }
        $Exe = "$eng\Binaries\Win64\UnrealEditor.exe"
        $ArgLine = "`"$uproject`" -windowed -nosplash -nosound -unattended $(if (-not $KeepThrottle) { $NoThrottleIni }) -ExecCmds=`"py $($py -replace '\\', '/')`" -abslog=`"$Log.ue.log`""
        if (-not $WatchFile) { $WatchFile = "$Log.ue.log" }
    }
}
if (-not $Exe) { Write-Output 'ERRORE: serve -Preset oppure -Exe'; exit 2 }
if ($ExtraArgs) { $ArgLine = "$ArgLine $ExtraArgs" }
$isEditor = (Split-Path $Exe -Leaf) -match '^UnrealEditor'
$notes = @()
if ($isEditor) {
    if ($ArgLine -notmatch '(^|\s)-unattended(\s|$)') { $ArgLine = "$ArgLine -unattended" }
    if (-not $KeepRestoreData) { $n = Clear-RestoreData (Get-ProjectDir $ArgLine $Root); if ($n) { $notes += $n } }
}
if ($AssertFile) {
    $AssertFile = if ([IO.Path]::IsPathRooted($AssertFile)) { $AssertFile } else { Join-Path $Root $AssertFile }
    Remove-Item -LiteralPath $AssertFile -ErrorAction SilentlyContinue   # a stale OK from an earlier run must never pass this one
}
$code = Invoke-Timed $Kind $Log $Exe $ArgLine $WatchFile $maxSec $silentSec $Root -Quiet | Select-Object -Last 1
$esito = "$Log.esito"
$lines = @(Get-Content -LiteralPath $esito)
if ($notes) { $lines = @($lines[0], ((@($lines[1]) + $notes) -join ' | ')) + $lines[2..($lines.Count - 1)] }
if ($AssertFile) {
    $a = Read-AssertLine $AssertFile
    $lines = @($lines[0], $a) + $lines[1..($lines.Count - 1)]
    if ($code -eq 0 -and $a -notmatch '^ASSERT: OK') { $code = 3 }
}
$lines | Set-Content -Path $esito -Encoding UTF8
$lines | Select-Object -First 5 | ForEach-Object { Write-Host $_ }
exit $code

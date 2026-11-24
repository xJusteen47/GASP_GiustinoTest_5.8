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
Approved-pose gate (project rule 13b, "7 deterministic additions" no. 7): -Preset Build runs
Tools\AnimPipeline\capdet\regress.py first and REFUSES to build when a locked version changed (exit 4, nothing is compiled), so a
thread cannot build and hand over work that silently overwrote an approved pose. -SkipRegress bypasses it and the output says so.
Default pre-run gate (cleanup plan 2026-11-24, fixes 5, 6, 8): Tools\CI\precheck.py runs before every command: the .py passed (-Script
and any .py in the command line) must compile, the /Game/... assets named by -RequireAsset, by "# REQUIRES_ASSET: /Game/..." header
lines of the driver or in the command line must exist under the Content of the project the command opens (a worktree without the
skeleton is refused, not run), and a Capture is refused while a local LLM server holds the GPU (-AllowLocalLlm). Failure =
"ESITO: PRECHECK_FAIL", exit 4, no editor. -NoPreCheck skips it and the output says so. The explicit -PreCheck gate still runs after.
Editor close on ESITO (fix 4): a driver finishes with unreal.log("ESITO: OK") (or FAIL) before QUIT_EDITOR, or writes that line to
-DoneFile. When the runner sees it, the editor has -DoneGraceSec (25) seconds to quit; then the whole job is closed and the verdict is
the driver's. A TIMEOUT, BLOCCATO or crash after "ESITO: OK" is reported as OK (shutdown problem), so it does not count as an attempt.
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
    [string]$AssertFile,        # capture gate: the assert.txt to read after the run (exit 3 when ESITO OK but ASSERT not OK)
    [string]$LockFile,          # optional _ue59/IN-USO.txt path: refuse with exit 2 if occupied by someone else
    [string]$LockOwner = '',    # optional owner tag allowed to proceed when present in LockFile
    [string]$PreCheck,          # optional .py or .ps1 gate executed BEFORE launching main command (exit 4 on failure)
    [string]$PreCheckArgs = '', # optional arguments for PreCheck script
    [int]$PreCheckTimeoutSec = 120, # hard timeout for PreCheck (kills tree and exits 124 on timeout)
    [switch]$SkipRegress,       # Build: do not run the approved-pose regression first (stated in the output when used)
    [switch]$NoPreCheck,        # skip the default pre-run gate (precheck.py); the output says so
    [string[]]$RequireAsset,    # /Game/... assets that must exist under the Content of the project the command opens (default gate)
    [switch]$AllowLocalLlm,     # Capture: do not refuse when a local LLM server holds the GPU
    [string]$DoneFile,          # editor runs: extra file the driver writes "ESITO: OK|FAIL" to (the UE log is always read as well)
    [int]$DoneGraceSec = 25     # editor runs: seconds the editor gets to quit by itself after the driver wrote ESITO, then the job is closed
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

# Last "ESITO: OK|FAIL" a driver wrote in a file (plain line, or after "LogPython:" in the UE log); $null when none. Reads only the
# tail and shares the file with the editor that is still writing it.
function Read-DriverEsito {
    param([string]$Path)
    if (-not $Path -or -not (Test-Path -LiteralPath $Path)) { return $null }
    $txt = ''
    try {
        $fs = [IO.File]::Open($Path, 'Open', 'Read', 'ReadWrite')
        try {
            $len = $fs.Length; $take = [int][Math]::Min($len, 65536)
            if ($take -le 0) { return $null }
            $null = $fs.Seek($len - $take, 'Begin')
            $buf = New-Object byte[] $take
            $n = $fs.Read($buf, 0, $take)
            $txt = [Text.Encoding]::UTF8.GetString($buf, 0, $n)
        } finally { $fs.Dispose() }
    } catch { return $null }
    $ms = [regex]::Matches($txt, '(?m)^(?:.*LogPython:\s*)?ESITO:\s*(OK|FAIL)\b')
    if ($ms.Count -eq 0) { return $null }
    return $ms[$ms.Count - 1].Groups[1].Value
}

function Invoke-Timed {
    param([string]$Kind, [string]$Log, [string]$Exe, [string]$ArgLine, [string]$Watch,
          [double]$MaxSec, [double]$SilentSec, [string]$Cwd, [switch]$Quiet,
          [switch]$Done, [string]$DoneFile, [int]$GraceSec = 25)
    $logDir = Split-Path $Log -Parent
    if ($logDir) { New-Item -ItemType Directory -Force $logDir | Out-Null }
    if (Test-Path $Log) { Remove-Item $Log -Force }
    if (-not $Watch) { $Watch = $Log }
    $esito = "$Log.esito"
    # cmd /s /c "<line>": the redirect is inside cmd, so the child writes straight to the file and keeps running if we die
    $line = "cmd.exe /d /s /c `"`"$Exe`" $ArgLine > `"$Log`" 2>&1`""
    $j = New-Object GstnJob
    $t0 = Get-Date
    $verdict = $null; $code = $null; $driver = $null; $driverAt = $null; $closed = $false
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
            if ($Done) {
                # the driver says it is finished ("ESITO: OK|FAIL" in its done file or in the UE log) but QUIT_EDITOR may never take effect:
                # after a grace period the runner closes the job itself instead of waiting for the limit (cleanup plan, fix 4)
                if (-not $driver) {
                    $d = Read-DriverEsito $DoneFile
                    if (-not $d) { $d = Read-DriverEsito $Watch }
                    if ($d) { $driver = $d; $driverAt = $now }
                } elseif (($now - $driverAt).TotalSeconds -ge $GraceSec) {
                    $verdict = if ($driver -eq 'OK') { 'OK' } else { 'FAIL' }; $closed = $true; break
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
    # a TIMEOUT / BLOCCATO / crash AFTER the driver wrote ESITO: OK is a shutdown problem, not a failed attempt (rule 5)
    if ($driver -eq 'OK' -and $verdict -ne 'OK') { $verdict = 'OK'; $closed = $true }
    $exit = switch ($verdict) { 'OK' { 0 } 'FAIL' { 1 } 'TIMEOUT' { 124 } 'BLOCCATO' { 125 } }
    $tail = ''
    if (Test-Path $Log) {
        $hit = Select-String -Path $Log -Pattern 'error|fail|fatal|crash' -SimpleMatch:$false | Select-Object -Last 1
        if ($hit) { $tail = $hit.Line.Trim(); if ($tail.Length -gt 160) { $tail = $tail.Substring(0, 160) } }
    }
    $exitTxt = if ($closed) { 'chiuso_dal_runner' } else { "$code" }
    $out = @("ESITO: $verdict", "tipo=$Kind durata=${secs}s limite=$([int]$MaxSec)s exit=$exitTxt figli_rimasti_uccisi=$left$(if ($driver) { " driver=$driver" })", "log=$Log")
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
# Owner 2026-11-21 ("ci sono ancora i 3 pie"): agent editor runs on the owner's checkout set the PIE multiplayer options in memory
# (pie_capture_move.py: listen host + 2 clients) and the editor SAVES them to Saved\Config\WindowsEditor\EditorPerProjectUserSettings.ini
# when Play starts, so the owner's next editor opened with 3 players. Every editor run now puts these keys back as they were.
$PieKeys = @('PlayNumberOfClients', 'PlayNetMode', 'RunUnderOneProcess', 'bLaunchSeparateServer')
function Get-PieIni([string]$ProjectDir) {
    $f = Join-Path $ProjectDir 'Saved\Config\WindowsEditor\EditorPerProjectUserSettings.ini'
    $snap = @{}
    if (Test-Path -LiteralPath $f) {
        foreach ($l in [IO.File]::ReadAllLines($f)) { foreach ($k in $PieKeys) { if ($l -like "$k=*") { $snap[$k] = $l } } }
    }
    return @{ File = $f; Lines = $snap }
}
function Restore-PieIni($Snap) {
    if (-not $Snap -or -not (Test-Path -LiteralPath $Snap.File) -or $Snap.Lines.Count -eq 0) { return '' }
    $lines = [IO.File]::ReadAllLines($Snap.File); $changed = @()
    for ($i = 0; $i -lt $lines.Count; $i++) {
        foreach ($k in $Snap.Lines.Keys) {
            if ($lines[$i] -like "$k=*" -and $lines[$i] -ne $Snap.Lines[$k]) { $changed += "$($lines[$i]) -> $($Snap.Lines[$k])"; $lines[$i] = $Snap.Lines[$k] }
        }
    }
    if (-not $changed) { return '' }
    [IO.File]::WriteAllLines($Snap.File, $lines)
    return "poststop: PIE ini restored ($($changed -join ', '))"
}

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

# Checks optional lock file (e.g. _ue59/IN-USO.txt). Returns $null if free or exact owner prefix matches, else refusal message.
function Test-LockFree {
    param([string]$Path, [string]$Owner)
    if (-not $Path -or -not (Test-Path -LiteralPath $Path)) { return $null }
    $first = ''
    foreach ($line in [IO.File]::ReadAllLines($Path, [Text.Encoding]::UTF8)) {
        if ($line.Trim()) { $first = $line.Trim(); break }
    }
    if ($first -match '^IN-USO:') {
        if ($Owner) {
            $esc = [regex]::Escape($Owner)
            if ($first -match "^IN-USO:\s+$esc(\s|\(|$)") { return $null }
        }
        return "ESITO: RIFIUTATO - lock occupato: $first"
    }
    return $null
}

# Runs optional PreCheck gate (.py or .ps1) with hard timeout and tree kill before launching main command.
function Invoke-PreCheck {
    param([string]$ScriptPath, [string]$ArgsText, [string]$OutLog, [string]$WorkDir, [int]$TimeoutSec = 120)
    $full = if ([IO.Path]::IsPathRooted($ScriptPath)) { $ScriptPath } else { Join-Path $WorkDir $ScriptPath }
    if (-not (Test-Path -LiteralPath $full)) { return @{ Ok = $false; TimedOut = $false; Detail = "PreCheck mancante: $ScriptPath" } }
    $dir = Split-Path $OutLog -Parent
    if ($dir) { New-Item -ItemType Directory -Force $dir | Out-Null }
    $runner = if ($full -match '\.py$') { 'python' } else { 'powershell.exe' }
    $runnerArgs = if ($full -match '\.py$') { "`"$full`" $ArgsText" } else { "-NoProfile -ExecutionPolicy Bypass -File `"$full`" $ArgsText" }
    $cmdLine = "`"`"$runner`" $runnerArgs > `"$OutLog`" 2>&1`""
    $p = Start-Process -FilePath 'cmd.exe' -ArgumentList '/d', '/s', '/c', $cmdLine -WorkingDirectory $WorkDir -WindowStyle Hidden -PassThru
    if (-not $p.WaitForExit($TimeoutSec * 1000)) {
        Start-Process -FilePath 'taskkill.exe' -ArgumentList "/F /T /PID $($p.Id)" -WindowStyle Hidden -Wait | Out-Null
        try { $p.WaitForExit(3000) | Out-Null } catch {}
        return @{ Ok = $false; TimedOut = $true; Detail = "PreCheck TIMEOUT dopo ${TimeoutSec}s (process tree terminato)" }
    }
    if ($p.ExitCode -eq 0) { return @{ Ok = $true; TimedOut = $false; Detail = '' } }
    $first = ''
    if (Test-Path -LiteralPath $OutLog) {
        $l = Get-Content -LiteralPath $OutLog -TotalCount 1
        if ($l) { $first = $l.Trim() }
    }
    return @{ Ok = $false; TimedOut = $false; Detail = "PreCheck exit=$($p.ExitCode) ($first)" }
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
    # 7: lock check (exact prefix) + precheck fail/timeout/ok
    Set-Content "$d\inuso.txt" "IN-USO: owner-task dal 2026-11-22"
    $lkBusy = Test-LockFree "$d\inuso.txt" ''
    $lkMine = Test-LockFree "$d\inuso.txt" 'owner-task'
    Set-Content "$d\inuso.txt" "IN-USO: owner-task2 dal 2026-11-22"
    $lkPrefix = Test-LockFree "$d\inuso.txt" 'owner-task'
    Set-Content "$d\inuso.txt" "LIBERO"
    $lkFree = Test-LockFree "$d\inuso.txt" ''
    Set-Content "$d\pc_fail.ps1" "Write-Output 'gate15b failed'; exit 1"
    Set-Content "$d\pc_ok.ps1" "Write-Output 'gate15b ok'; exit 0"
    Set-Content "$d\pc_hang.ps1" "Start-Sleep -Seconds 60"
    $pcFailRes = Invoke-PreCheck "$d\pc_fail.ps1" '' "$d\pc_fail.log" $d 30
    $pcOkRes   = Invoke-PreCheck "$d\pc_ok.ps1" '' "$d\pc_ok.log" $d 30
    $ranMarker = "$d\should_not_exist.txt"; Remove-Item $ranMarker -ErrorAction SilentlyContinue
    Set-Content "$d\noop.ps1" "Write-Output ok"
    Set-Content "$d\mark.ps1" "Set-Content -LiteralPath '$ranMarker' -Value 'ran'"
    $e2eProc = Start-Process -FilePath $ps -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Kind Test -Log `"$d\e2e_pc.log`" -Exe `"$ps`" -ArgLine `"-NoProfile -File $d\mark.ps1`" -PreCheck `"$d\pc_fail.ps1`"" -WindowStyle Hidden -Wait -PassThru
    $e2eExit = $e2eProc.ExitCode
    Set-Content "$d\inuso_busy.txt" "IN-USO: other-owner"
    $e2eLockProc = Start-Process -FilePath $ps -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Kind Test -Log `"$d\e2e_lk.log`" -Exe `"$ps`" -ArgLine `"-NoProfile -File $d\noop.ps1`" -LockFile `"$d\inuso_busy.txt`"" -WindowStyle Hidden -Wait -PassThru
    $e2eOkProc = Start-Process -FilePath $ps -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Kind Test -Log `"$d\e2e_ok.log`" -Exe `"$ps`" -ArgLine `"-NoProfile -File $d\noop.ps1`" -PreCheck `"$d\pc_ok.ps1`"" -WindowStyle Hidden -Wait -PassThru
    $e2eToProc = Start-Process -FilePath $ps -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Kind Test -Log `"$d\e2e_to.log`" -Exe `"$ps`" -ArgLine `"-NoProfile -File $d\noop.ps1`" -PreCheck `"$d\pc_hang.ps1`" -PreCheckTimeoutSec 2" -WindowStyle Hidden -Wait -PassThru
    $toEsito = if (Test-Path "$d\e2e_to.log.esito") { Get-Content -Raw "$d\e2e_to.log.esito" } else { '' }
    $ok7 = ($null -ne $lkBusy) -and ($null -eq $lkMine) -and ($null -ne $lkPrefix) -and ($null -eq $lkFree) -and (-not $pcFailRes.Ok) -and ($pcOkRes.Ok) -and ($e2eExit -eq 4) -and ($e2eLockProc.ExitCode -eq 2) -and ($e2eOkProc.ExitCode -eq 0) -and ($e2eToProc.ExitCode -eq 124) -and ($toEsito -match 'ESITO: TIMEOUT') -and (-not (Test-Path $ranMarker))
    $res += "5 RESTORE + 6 ASSERT + 7 LOCK/PRECHECK: $(if ($ok5 -and $ok6 -and $ok7) {'PASS'} else {"FAIL (r=$ok5 a=$ok6 lp=$ok7 e2e=$e2eExit lk=$($e2eLockProc.ExitCode) ok=$($e2eOkProc.ExitCode) to=$($e2eToProc.ExitCode))"})"
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
        # Rule 13b gate: an approved pose that changed must stop the build, not be discovered after the capture.
        if (-not $SkipRegress) {
            $rg = Join-Path $Root 'Tools\AnimPipeline\capdet\regress.py'
            if (Test-Path -LiteralPath $rg) {
                $rgOut = & python -I $rg --out "$Log.regress.json" 2>&1
                if ($LASTEXITCODE -ne 0) {
                    $txt = @("ESITO: FAIL (regressione pose approvate)") + (@($rgOut) | Select-Object -First 4)
                    $txt | Set-Content -LiteralPath "$Log.esito" -Encoding UTF8
                    $txt | Write-Output
                    Write-Output 'BUILD: SALTATA (-SkipRegress per forzare)'
                    exit 4
                }
                $script:RegressNote = @($rgOut) | Where-Object { $_ -match '^REGRESS:' } | Select-Object -First 1
            }
        } else {
            $script:RegressNote = 'REGRESS: SALTATO (-SkipRegress)'
        }
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

if ($LockFile) {
    $lockMsg = Test-LockFree $LockFile $LockOwner
    if ($lockMsg) {
        $ld = Split-Path "$Log.esito" -Parent; if ($ld) { New-Item -ItemType Directory -Force $ld | Out-Null }
        @($lockMsg, "lock=$LockFile") | Set-Content -Path "$Log.esito" -Encoding UTF8
        Write-Output $lockMsg
        exit 2
    }
}
if ($PreCheck) {
    $pcLog = "$Log.precheck.log"
    $pc = Invoke-PreCheck $PreCheck $PreCheckArgs $pcLog $Root $PreCheckTimeoutSec
    if (-not $pc.Ok) {
        $ld = Split-Path "$Log.esito" -Parent; if ($ld) { New-Item -ItemType Directory -Force $ld | Out-Null }
        $hdr = if ($pc.TimedOut) { 'ESITO: TIMEOUT' } else { 'ESITO: PRECHECK_FAIL' }
        $pcExit = if ($pc.TimedOut) { 124 } else { 4 }
        $pcOut = @($hdr, $pc.Detail, "log=$pcLog")
        $pcOut | Set-Content -Path "$Log.esito" -Encoding UTF8
        $pcOut | Select-Object -First 5 | ForEach-Object { Write-Host $_ }
        exit $pcExit
    }
}
$isEditor = (Split-Path $Exe -Leaf) -match '^UnrealEditor'
# Default gate (cleanup plan 2026-11-24, fixes 5, 6, 8): scripts compile, requested assets exist in the project THIS command opens,
# and a Capture is refused while a local LLM holds the GPU. Before the editor, so a mistake costs seconds, not an hour.
$preNote = ''
if ($NoPreCheck) { $preNote = 'precheck: SALTATO (-NoPreCheck)' }
else {
    $pcPy = Join-Path $PSScriptRoot 'precheck.py'
    if (Test-Path -LiteralPath $pcPy) {
        $pcArgs = @('-I', $pcPy, '--project', (Get-ProjectDir $ArgLine $Root), '--cmdline', "$ArgLine $ExtraArgs")
        if ($Script) { $pcArgs += @('--py', $(if ([IO.Path]::IsPathRooted($Script)) { $Script } else { Join-Path $Root $Script })) }
        foreach ($ra in @($RequireAsset)) { if ($ra) { $pcArgs += @('--asset', $ra) } }
        if ($Kind -eq 'Capture' -and -not $AllowLocalLlm) { $pcArgs += '--vram' }
        $dpOut = @(& python @pcArgs 2>&1)
        if ($LASTEXITCODE -ne 0) {
            $ld = Split-Path "$Log.esito" -Parent; if ($ld) { New-Item -ItemType Directory -Force $ld | Out-Null }
            $dpTxt = @('ESITO: PRECHECK_FAIL') + @($dpOut | Where-Object { $_ -match '^FAIL' } | Select-Object -First 3) + @("log=$Log.esito")
            $dpTxt | Set-Content -Path "$Log.esito" -Encoding UTF8
            $dpTxt | Select-Object -First 5 | ForEach-Object { Write-Host $_ }
            exit 4
        }
        $preNote = 'precheck: OK'
    } else { $preNote = 'precheck: SALTATO (precheck.py mancante)' }
}
$notes = @()
if ($preNote) { $notes += $preNote }
if ($script:RegressNote) { $notes += $script:RegressNote }   # the Build preset's rule-13b gate ran before this point
if ($isEditor) {
    if ($ArgLine -notmatch '(^|\s)-unattended(\s|$)') { $ArgLine = "$ArgLine -unattended" }
    if (-not $KeepRestoreData) { $n = Clear-RestoreData (Get-ProjectDir $ArgLine $Root); if ($n) { $notes += $n } }
    $pieSnap = Get-PieIni (Get-ProjectDir $ArgLine $Root)
}
if ($AssertFile) {
    $AssertFile = if ([IO.Path]::IsPathRooted($AssertFile)) { $AssertFile } else { Join-Path $Root $AssertFile }
    Remove-Item -LiteralPath $AssertFile -ErrorAction SilentlyContinue   # a stale OK from an earlier run must never pass this one
}
$code = Invoke-Timed $Kind $Log $Exe $ArgLine $WatchFile $maxSec $silentSec $Root -Quiet -Done:$isEditor -DoneFile $DoneFile -GraceSec $DoneGraceSec | Select-Object -Last 1
if ($isEditor) { $n = Restore-PieIni $pieSnap; if ($n) { $notes += $n } }
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

@echo off
rem Applies the generic clip fixes (clips.json rows with "fix") to the editor assets and SAVES them.
rem Owner-run on purpose: saving versioned binary assets from an agent is blocked by its permission rules.
rem Waits while an Unreal editor of THIS checkout is open (a capture may be running), then runs headless.
:wait
powershell -NoProfile -Command "if (Get-CimInstance Win32_Process | Where-Object { $_.Name -like 'UnrealEditor*' -and $_.CommandLine -like '*GASP_Giustino2_ue59*' }) { exit 1 } else { exit 0 }"
if errorlevel 1 (
  echo Un editor di questo progetto e' aperto ^(cattura in corso^): aspetto 20 secondi...
  timeout /t 20 /nobreak >nul
  goto wait
)
set CLIPFIX_WRITE=1
"C:\Program Files\Epic Games\UE_5.9\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0..\..\..\GASP_Giustino2.uproject" -run=pythonscript -script="%~dp0fix_clips_ue.py" -unattended -nullrhi -nosplash -abslog="%~dp0..\..\..\Saved\Logs\ci\clipfix_write.log"
echo Fatto. Report: Saved\ClipCheck\fix_report.json
pause

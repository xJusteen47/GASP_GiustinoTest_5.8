@echo off
rem Porta il progetto all'ultima main e compila l'editor. Dettagli: Tools\CI\Aggiorna-Progetto.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\CI\Aggiorna-Progetto.ps1" %*
echo.
pause

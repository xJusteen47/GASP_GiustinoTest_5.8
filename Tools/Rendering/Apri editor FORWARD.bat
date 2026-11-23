@echo off
rem Opens the editor with forward shading (temporary, nothing saved in the project config). Add -Msaa for MSAA x8.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Open-Editor-Forward.ps1" %*

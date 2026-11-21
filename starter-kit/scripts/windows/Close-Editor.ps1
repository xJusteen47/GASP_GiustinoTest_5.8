# Close the owner's open UE editor, guarded: only when not in PIE and after saving every dirty asset (needs the owner's standing OK).
# Talks to the running editor through its MCP server (ue_mcp.py), never kills a process:
#   1. no editor/MCP reachable      -> ESITO: NESSUN EDITOR (exit 0, nothing to do)
#   2. PIE / Simulate running        -> ESITO: RIFIUTATO PIE (exit 2), nothing touched
#   3. save every dirty map + asset  -> any package still dirty: ESITO: RIFIUTATO SALVATAGGIO (exit 3), editor left open
#   4. graceful quit (unreal.SystemLibrary.quit_editor), wait up to -WaitSeconds for UnrealEditor.exe of this project to exit
#                                    -> ESITO: CHIUSO (exit 0) or ESITO: TIMEOUT (exit 124, editor may still be closing)
#   0. an agent editor (-unattended: capture or test) is running -> ESITO: RIFIUTATO (exit 4), nothing touched
# usage: powershell -File harness\scripts\windows\Close-Editor.ps1 [-WaitSeconds 120]   Prints at most 5 lines.
param([int]$WaitSeconds = 120)
$proj = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path   # repo root when the kit sits in <repo>\harness
$py = Join-Path $PSScriptRoot '..\python\close_editor.py'
# the MCP port may belong to an agent's capture/test editor (Run-Timed adds -unattended): never close those from here
$agent = Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe'" -ErrorAction SilentlyContinue |
    Where-Object { $_.CommandLine -like '*-unattended*' }
if ($agent) { 'ESITO: RIFIUTATO (editor di un agente in corso: cattura o test; riprova a fine run)'; exit 4 }
$out = & python $py 2>&1 | Select-Object -Last 1
$code = $LASTEXITCODE
if ($code -ne 10) { "$out"; exit $code }
"$out"
$deadline = (Get-Date).AddSeconds($WaitSeconds)
while ((Get-Date) -lt $deadline) {
    $alive = Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandLine -like "*$(Split-Path $proj -Leaf)*" }
    if (-not $alive) { 'ESITO: CHIUSO'; exit 0 }
    Start-Sleep -Seconds 3
}
"ESITO: TIMEOUT dopo $WaitSeconds s (editor ancora aperto: forse un dialogo di salvataggio)"; exit 124

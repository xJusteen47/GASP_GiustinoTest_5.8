<#
Check-TestLog.ps1 - reads the UE log of an Automation run (Run-Timed -Preset Test writes it to <Log>.ue.log)
and decides PASS/FAIL. The editor exit code alone is not trusted: a run with 0 tests or a failed test may still exit 0.
Prints at most 5 lines; exit 0 PASS, 1 FAIL. Writes the failed test names to <UeLog>.fail.txt.
  Check-TestLog.ps1 -UeLog Saved\Logs\ci\pr-test.log.ue.log [-MinTests 50]
#>
param([Parameter(Mandatory)][string]$UeLog, [int]$MinTests = 1)
if (-not (Test-Path $UeLog)) { Write-Output "ESITO TEST: FAIL (log mancante: $UeLog)"; exit 1 }
$ok   = @(Select-String -Path $UeLog -Pattern 'Test Completed\. Result=\{(Success|Passed)\}')
$fail = @(Select-String -Path $UeLog -Pattern 'Test Completed\. Result=\{(Fail|Failed|Error)\}')
$names = $fail | ForEach-Object { if ($_.Line -match 'Name=\{([^}]*)\}') { $Matches[1] } else { $_.Line.Trim() } }
$names | Set-Content -Path "$UeLog.fail.txt" -Encoding UTF8
$total = $ok.Count + $fail.Count
$verdict = if ($fail.Count -eq 0 -and $total -ge $MinTests) { 'PASS' } else { 'FAIL' }
Write-Output "ESITO TEST: $verdict  ok=$($ok.Count) fail=$($fail.Count) totale=$total (minimo $MinTests)"
$names | Select-Object -First 3 | ForEach-Object { Write-Output "  fallito: $_" }
if ($fail.Count -gt 3) { Write-Output "  ... altri $($fail.Count - 3) in $UeLog.fail.txt" }
exit $(if ($verdict -eq 'PASS') { 0 } else { 1 })

<#
Apply-PushRules.ps1 - adds the push rules of Tools/CI/push-rules.json to the USER settings of Claude Code
(%USERPROFILE%\.claude\settings.json), so they hold in every folder (repo, worktrees, "Unreal Projects").
Result: git push to claude/* runs without a prompt; push to feature/main, force push, delete, mirror are refused.

To be run BY THE OWNER, by hand (Claude may not change its own permissions). Makes a backup first; idempotent.
  powershell -NoProfile -ExecutionPolicy Bypass -File Tools\CI\Apply-PushRules.ps1
#>
$ErrorActionPreference = 'Stop'
$rules = Get-Content -Raw "$PSScriptRoot\push-rules.json" | ConvertFrom-Json
$path = Join-Path $env:USERPROFILE '.claude\settings.json'
if (Test-Path $path) {
    Copy-Item $path "$path.bak-$(Get-Date -Format yyyyMMdd-HHmmss)"
    $s = Get-Content -Raw $path | ConvertFrom-Json
} else { $s = [pscustomobject]@{} }
if (-not $s.PSObject.Properties['permissions']) { $s | Add-Member permissions ([pscustomobject]@{}) }
$p = $s.permissions
foreach ($k in 'allow', 'deny') {
    $cur = @(); if ($p.PSObject.Properties[$k]) { $cur = @($p.$k) }
    $new = @($cur + @($rules.$k) | Select-Object -Unique)
    # never keep an allow for pushing to feature or main
    if ($k -eq 'allow') { $new = @($new | Where-Object { $_ -notmatch 'git push.*(feature/|main)' }) }
    $p | Add-Member -Force $k $new
}
$s | ConvertTo-Json -Depth 20 | Set-Content -Path $path -Encoding UTF8
Write-Output "OK: regole push in $path (backup accanto). allow=$(@($p.allow).Count) deny=$(@($p.deny).Count)"
Write-Output 'Riavvia le sessioni Claude Code aperte perche'' le leggano.'

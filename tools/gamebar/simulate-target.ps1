# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
# Diagnostic simulation ONLY: it does not read the Xbox Game Bar SDK.
param(
    [Parameter(Mandatory=$true)][string]$WidgetsJsonPath,
    [Parameter(Mandatory=$true)][string]$DisplayName,
    [int]$Seconds = 60
)
$ErrorActionPreference = "Stop"
if ($Seconds -lt 1 -or $Seconds -gt 300) { throw "Seconds must be between 1 and 300." }
if ([string]::IsNullOrWhiteSpace($DisplayName) -or $DisplayName.Length -gt 120) { throw "Invalid display name." }
$target = "$WidgetsJsonPath.gamebar-target.json"
if (-not (Test-Path (Split-Path $target -Parent))) { throw "Missing widgets.json directory: $target" }
Write-Host "Experimental fake Game Bar source: $target"
Write-Host "Switch to the game window within five seconds; reports refresh for $Seconds seconds."
Start-Sleep -Seconds 5
for ($n = 0; $n -lt $Seconds; $n++) {
    $report = @{
        at = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
        isGame = $true
        displayName = $DisplayName
    } | ConvertTo-Json -Compress
    [System.IO.File]::WriteAllText($target, $report, [System.Text.UTF8Encoding]::new($false))
    Start-Sleep -Seconds 1
}
Write-Host "Stopped. Reports expire automatically after five seconds."

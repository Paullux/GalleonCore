# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory = $true)][string]$Setup)
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or -not $env:RUNNER_TEMP) { throw 'This integration test is for an isolated CI runner.' }
$root = Join-Path $env:RUNNER_TEMP 'GalleonCore setup test'
$env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
$env:QT_PLUGIN_PATH = Join-Path $root 'bin'
$env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $root 'bin/platforms'
$env:QML_IMPORT_PATH = Join-Path $root 'bin/qml'
$env:QML2_IMPORT_PATH = $env:QML_IMPORT_PATH
$helper = Join-Path $PSScriptRoot 'stop-galleon.ps1' 
$windowsPowerShell = Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
$pages = Join-Path $env:RUNNER_TEMP 'galleon-setup-pages.json'
$sentinelRoot = Join-Path $env:LOCALAPPDATA 'galleoncore/wallpaper/personal'
New-Item -ItemType Directory -Force $sentinelRoot | Out-Null
$sentinel = Join-Path $sentinelRoot 'setup-preservation-test.txt'
'Keep this user-owned file.' | Set-Content $sentinel
$settingsKey = 'HKCU:\Software\galleoncore\galleond'
New-Item -Path $settingsKey -Force | Out-Null
New-ItemProperty -Path $settingsKey -Name SetupPreservationTest -Value 'Keep settings' -PropertyType String -Force | Out-Null
$processes = @()
$launchNumber = 0
$env:QT_FORCE_STDERR_LOGGING = '1'

function Start-LoggedApplication([string]$Name, [string]$Arguments) {
    $script:launchNumber++
    $prefix = "setup-app-$($script:launchNumber)-$Name"
    $child = Start-Process (Join-Path $root "bin/$Name.exe") -ArgumentList $Arguments -PassThru `
        -RedirectStandardOutput "$prefix-stdout.log" -RedirectStandardError "$prefix-stderr.log"
    $null = $child.Handle
    return $child
}
# A wall-clock budget accommodates slow first-run initialization without accepting
# a process that has exited or replacing endpoint readiness with a fixed sleep.
function Wait-ApplicationsReady([int]$TimeoutSeconds = 60, [scriptblock]$Probe = {
    & $windowsPowerShell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $helper -InstallDir $root -Mode Probe
    return ($LASTEXITCODE -eq 0)
}) {
    $clock = [System.Diagnostics.Stopwatch]::StartNew()
    do {
        foreach ($child in $script:processes) {
            $child.Refresh()
            if ($child.HasExited) { throw "Setup application $($child.Id) exited before readiness (exit: $($child.ExitCode))." }
        }
        if (& $Probe) { return $true }
        Start-Sleep -Milliseconds 300
    } while ($clock.Elapsed.TotalSeconds -lt $TimeoutSeconds)
    return $false
}
function Write-StartupDiagnostics {
    foreach ($child in $script:processes) {
        try {
            $child.Refresh()
            if ($child.HasExited) { Write-Host "Setup child $($child.Id) exited: $($child.ExitCode)" }
            else { Write-Host "Setup child $($child.Id) is still running." }
        } catch { Write-Host "Could not inspect setup child: $_" }
    }
    foreach ($log in @(Get-ChildItem -Path 'setup-app-*.log' -ErrorAction SilentlyContinue)) {
        Write-Host "Application log: $($log.Name)"
        Get-Content -LiteralPath $log.FullName -Tail 200 -ErrorAction SilentlyContinue
    }
}

function Run-Installer([string]$Program, [string]$Arguments) {
    $child = Start-Process -FilePath $Program -ArgumentList $Arguments -PassThru
    if (-not $child.WaitForExit(90000)) {
        $child.Kill()
        throw 'Installer exceeded its deadline.'
    }
    if ($child.ExitCode -ne 0) { throw "Installer failed: $($child.ExitCode)" }
}
function Start-Applications {
    $daemon = Start-LoggedApplication 'galleond' "--pages `"$pages`""
    $bosun = Start-LoggedApplication 'galleon-bosun' "--minimized --pages `"$pages`""
    $script:processes = @($daemon, $bosun)
    $ready = Wait-ApplicationsReady
    if (-not $ready) { throw 'Installed Bosun and daemon did not expose their private endpoints.' }
}
function Assert-Closed {
    foreach ($child in $script:processes) {
        if (-not $child.WaitForExit(10000)) { throw 'Setup left an application running.' }
        if ($child.ExitCode -ne 0) { throw "Application $($child.Id) did not shut down cleanly (exit: $($child.ExitCode))." }
    }
}
try {
    # First installation, then upgrade while both applications are running.
    Run-Installer (Resolve-Path $Setup).Path "/S /D=$root"
    $startup = [System.Environment]::GetFolderPath('Startup')
    $runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
    $daemonCommand = '"' + (Join-Path $root 'bin/galleond.exe') + '"'
    $bosunCommand = '"' + (Join-Path $root 'bin/galleon-bosun.exe') + '" --minimized'
    if ((Get-ItemPropertyValue $runKey GalleonCore) -ne $daemonCommand) { throw 'Daemon sign-in command missing or incorrectly quoted.' }
    if ((Get-ItemPropertyValue $runKey GalleonBosun) -ne $bosunCommand) { throw 'Bosun tray sign-in command missing or incorrectly quoted.' }
    # Launch Bosun alone, as from the Start menu: it must start its missing daemon.
    $bosun = Start-LoggedApplication 'galleon-bosun' '--minimized'
    $script:processes = @($bosun)
    $ready = Wait-ApplicationsReady
    $daemon = @(Get-Process galleond -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq (Join-Path $root 'bin/galleond.exe') })
    $script:processes += $daemon
    if (-not $ready -or $daemon.Count -ne 1) { throw 'Bosun did not automatically start one installed daemon.' }
    # Get-Process returns a snapshot; retain its native handle before exit so ExitCode remains readable.
    $null = $daemon[0].Handle
    Run-Installer (Resolve-Path $Setup).Path "/S /D=$root"
    Assert-Closed
    if (-not (Test-Path $sentinel)) { throw 'Upgrade removed personal data.' }
    # Uninstall has the same lifecycle guarantee and keeps personal data.
    Start-Applications
    Run-Installer (Join-Path $root 'Uninstall.exe') '/S'
    Assert-Closed
    # NSIS launches a temporary uninstaller copy; its launcher can exit before cleanup ends.
    $markers = @((Join-Path $root 'bin/galleond.exe'), (Join-Path $root 'Uninstall.exe'),
        (Join-Path $startup 'GalleonCore.lnk'), (Join-Path $startup 'Galleon Bosun.lnk'))
    $deadline = [System.DateTime]::UtcNow.AddSeconds(20)
    do {
        $remaining = @($markers | Where-Object { Test-Path -LiteralPath $_ })
        if ($remaining.Count -eq 0) { break }
        Start-Sleep -Milliseconds 100
    } while ([System.DateTime]::UtcNow -lt $deadline)
    if ($remaining.Count -gt 0) { throw "Uninstall left files: $($remaining -join ', ')" }
    foreach ($name in @('GalleonCore', 'GalleonBosun')) {
        if ($null -ne (Get-ItemProperty $runKey -Name $name -ErrorAction SilentlyContinue)) { throw 'Uninstall left a sign-in command.' }
    }
    if (-not (Test-Path $sentinel)) { throw 'Uninstall removed personal data.' }
    if ((Get-ItemPropertyValue $settingsKey SetupPreservationTest) -ne 'Keep settings') { throw 'Uninstall removed settings.' }
} catch {
    Write-StartupDiagnostics
    throw
} finally {
    # Only CI-owned processes created by this script may be terminated on failure.
    foreach ($child in $processes) { if (-not $child.HasExited) { $child.Kill() } }
    Remove-Item $sentinel -ErrorAction SilentlyContinue
    Remove-ItemProperty $settingsKey SetupPreservationTest -ErrorAction SilentlyContinue
}

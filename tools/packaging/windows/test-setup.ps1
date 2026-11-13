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
# Diagnostic only: records every galleond process with its parent and start time, in the
# console and in setup-app-daemons.log (uploaded with the other setup logs). It never
# changes whether the test passes; it tells apart a daemon started twice from a second
# daemon that is still exiting when the processes are counted.
# galleond also runs a short, isolated "--launcher-info" child to read launcher
# metadata before taking its instance lock or opening HID (GameInstallations::scanMetadata,
# see daemon/main.cpp); it shares the same executable path as the real daemon, so a
# path-only filter can count it as a second daemon while it is still exiting.
function Get-RunningDaemons {
    $candidates = @(Get-CimInstance Win32_Process -Filter "Name = 'galleond.exe'" -ErrorAction SilentlyContinue |
        Where-Object { $_.ExecutablePath -eq (Join-Path $root 'bin/galleond.exe') -and $_.CommandLine -notlike '*--launcher-info*' })
    @($candidates | ForEach-Object { Get-Process -Id $_.ProcessId -ErrorAction SilentlyContinue } | Where-Object { $_ })
}
function Write-DaemonReport([string]$When, $Bosun) {
    $lines = @("galleond processes $When (Bosun pid $($Bosun.Id)):")
    $found = @(Get-CimInstance Win32_Process -Filter "Name = 'galleond.exe'" -ErrorAction SilentlyContinue)
    if ($found.Count -eq 0) { $lines += '  none' }
    foreach ($p in $found) {
        $started = if ($p.CreationDate) { $p.CreationDate.ToString('o') } else { 'unknown' }
        $lines += "  pid $($p.ProcessId) parent $($p.ParentProcessId) started $started path $($p.ExecutablePath) command $($p.CommandLine)"
    }
    $lines | Tee-Object -FilePath 'setup-app-daemons.log' -Append | Write-Host
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
# The .gctheme file type is registered for the current user only (HKCU), with an icon shipped in the install folder.
$classesKey = 'HKCU:\Software\Classes'
function Assert-ThemeFileType {
    if ((Get-ItemPropertyValue "$classesKey\.gctheme" '(default)') -ne 'GalleonCore.Theme') { throw '.gctheme is not associated with GalleonCore.Theme.' }
    if ((Get-ItemPropertyValue "$classesKey\GalleonCore.Theme" '(default)') -notin @('GalleonCore theme', "Th$([char]0xE8)me GalleonCore")) { throw 'GalleonCore.Theme has no label.' }
    $icon = Get-ItemPropertyValue "$classesKey\GalleonCore.Theme\DefaultIcon" '(default)'
    if ($icon -ne (Join-Path $root 'gctheme.ico')) { throw "Unexpected .gctheme icon: $icon" }
    if (-not (Test-Path -LiteralPath $icon)) { throw 'The .gctheme icon file is not installed.' }
    if (Test-Path "$classesKey\GalleonCore.Theme\shell") { throw 'The theme file type must not have an open verb.' }
}
# The watchdog of the live lighting is installed beside galleond.exe (that is where galleond looks for it), and runs.
function Assert-Watchdog {
    $exe = Join-Path $root 'bin/galleon-keyboard-watchdog.exe'
    if (-not (Test-Path -LiteralPath $exe)) { throw 'The watchdog is not installed beside galleond.' }
    $usage = Start-Process -FilePath $exe -PassThru -Wait -NoNewWindow
    if ($usage.ExitCode -ne 1) { throw "The installed watchdog exited with $($usage.ExitCode) without arguments, not 1." }
    # A process that has already ended is the case it exists for; --dry-run opens no keyboard.
    $gone = Start-Process -FilePath $exe -ArgumentList '--pid 4194999 --dry-run' -PassThru -Wait -NoNewWindow
    if ($gone.ExitCode -ne 0) { throw "The installed watchdog dry run exited with $($gone.ExitCode)." }
}
function Assert-ThemeFileTypeRemoved {
    foreach ($key in @("$classesKey\.gctheme", "$classesKey\GalleonCore.Theme")) {
        if (Test-Path $key) { throw "Uninstall left $key." }
    }
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
    Assert-ThemeFileType
    Assert-Watchdog
    # Launch Bosun alone, as from the Start menu: it must start its missing daemon.
    $bosun = Start-LoggedApplication 'galleon-bosun' '--minimized'
    $script:processes = @($bosun)
    $ready = Wait-ApplicationsReady
    $daemon = Get-RunningDaemons
    Write-DaemonReport 'when counted' $bosun
    if ($daemon.Count -ne 1) { Start-Sleep -Seconds 3; Write-DaemonReport '3 seconds later' $bosun; $daemon = Get-RunningDaemons }
    $script:processes += $daemon
    if (-not $ready -or $daemon.Count -ne 1) {
        throw "Bosun did not automatically start one installed daemon (ready: $ready, installed daemons: $($daemon.Count))."
    }
    # Get-Process returns a snapshot; retain its native handle before exit so ExitCode remains readable.
    $null = $daemon[0].Handle
    Run-Installer (Resolve-Path $Setup).Path "/S /D=$root"
    Assert-Closed
    if (-not (Test-Path $sentinel)) { throw 'Upgrade removed personal data.' }
    Assert-ThemeFileType
    Assert-Watchdog
    # Uninstall has the same lifecycle guarantee and keeps personal data.
    Start-Applications
    Run-Installer (Join-Path $root 'Uninstall.exe') '/S'
    Assert-Closed
    # NSIS launches a temporary uninstaller copy; its launcher can exit before cleanup ends.
    $markers = @((Join-Path $root 'bin/galleond.exe'), (Join-Path $root 'bin/galleon-keyboard-watchdog.exe'), (Join-Path $root 'Uninstall.exe'),
        (Join-Path $root 'gctheme.ico'), (Join-Path $startup 'GalleonCore.lnk'), (Join-Path $startup 'Galleon Bosun.lnk'))
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
    Assert-ThemeFileTypeRemoved
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

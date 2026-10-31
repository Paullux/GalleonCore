# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
# Exercise the real readiness policy with fake IPC and no installed processes.
$ErrorActionPreference = 'Stop'
$parseErrors = $null
$tokens = $null
$source = Join-Path $PSScriptRoot 'test-setup.ps1'
$ast = [System.Management.Automation.Language.Parser]::ParseFile($source, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw ($parseErrors -join "`n") }
$definition = $ast.Find({ param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Wait-ApplicationsReady'
}, $false)
if (-not $definition) { throw 'Readiness policy not found.' }
Invoke-Expression $definition.Extent.Text
function Start-Sleep { param([int]$Milliseconds) } # No real waiting in these fixtures.
$script:processes = @()
$script:probes = 0
$ready = Wait-ApplicationsReady -Probe { $script:probes++; return $script:probes -ge 45 }
if (-not $ready -or $script:probes -ne 45) { throw 'Slow readiness was not accepted after more than 40 probes.' }
$ready = Wait-ApplicationsReady -TimeoutSeconds 0 -Probe { return $false }
if ($ready) { throw 'An unavailable endpoint was accepted.' }
$child = [pscustomobject]@{ Id = 123; HasExited = $true; ExitCode = 7 }
$child | Add-Member -MemberType ScriptMethod -Name Refresh -Value {}
$script:processes = @($child)
$rejected = $false
try { [void](Wait-ApplicationsReady -Probe { throw 'An exited application must be detected before probing.' }) }
catch { $rejected = $_.Exception.Message -match 'exited before readiness.*7' }
if (-not $rejected) { throw 'Early application exit was not reported.' }
Write-Host 'Setup readiness policy passed: slow startup, unavailable IPC and early process exit.'

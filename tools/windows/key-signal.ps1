# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
<#
.SYNOPSIS
  Lights (or clears) one key of the Galleon 100 SD through galleond's local endpoint. docs/KEY_SIGNALS.md describes the rules.
.DESCRIPTION
  Off unless the person allowed it in Bosun (Keyboard > live lighting > "Other programs may light keys"), and only while the live lighting is on.
  The endpoint gives no reply about the result: a request that is well formed is acknowledged, whether or not a key is lit.
  Not tested on a Windows machine yet (docs/WINDOWS_ACCEPTANCE.md, section 8).
.EXAMPLE
  .\key-signal.ps1 -Client assistant -Key F3 -Red 255 -Green 120 -Blue 0 -Effect pulse -Seconds 600
  .\key-signal.ps1 -Client assistant -Key F3 -Clear
  .\key-signal.ps1 -Client assistant -ClearAll
#>
[CmdletBinding(DefaultParameterSetName = 'Set')]
param(
    [Parameter(Mandatory = $true)][ValidateLength(1, 64)][string]$Client,
    [Parameter(Mandatory = $true, ParameterSetName = 'Set')]
    [Parameter(Mandatory = $true, ParameterSetName = 'Clear')][ValidateLength(1, 32)][string]$Key,
    [Parameter(Mandatory = $true, ParameterSetName = 'Set')][ValidateRange(0, 255)][int]$Red,
    [Parameter(Mandatory = $true, ParameterSetName = 'Set')][ValidateRange(0, 255)][int]$Green,
    [Parameter(Mandatory = $true, ParameterSetName = 'Set')][ValidateRange(0, 255)][int]$Blue,
    [Parameter(ParameterSetName = 'Set')][ValidateSet('steady', 'pulse')][string]$Effect = 'steady',
    [Parameter(Mandatory = $true, ParameterSetName = 'Set')][ValidateRange(1, 86400)][double]$Seconds,
    [Parameter(Mandatory = $true, ParameterSetName = 'Clear')][switch]$Clear,
    [Parameter(Mandatory = $true, ParameterSetName = 'ClearAll')][switch]$ClearAll
)
$ErrorActionPreference = 'Stop'

# The name of the endpoint: the same contract as stop-galleon.ps1 and daemon/LocalControl.cpp (UTF-8 SHA-256 of the user's SID, first 24 hex digits).
$sid = [System.Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$sha = [System.Security.Cryptography.SHA256]::Create()
try {
    $digest = ([System.BitConverter]::ToString($sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($sid)))).Replace('-', '').ToLowerInvariant()
} finally { $sha.Dispose() }
$name = "galleoncore-$($digest.Substring(0, 24))"

if ($ClearAll) {
    $request = [ordered]@{ action = 'key-signal-clear-all'; client = $Client }
} elseif ($Clear) {
    $request = [ordered]@{ action = 'key-signal-clear'; client = $Client; key = $Key }
} else {
    $request = [ordered]@{ action = 'key-signal'; client = $Client; key = $Key; red = $Red; green = $Green; blue = $Blue; effect = $Effect; seconds = $Seconds }
}
$frame = [System.Text.Encoding]::UTF8.GetBytes(($request | ConvertTo-Json -Compress) + "`n")

$pipe = [System.IO.Pipes.NamedPipeClientStream]::new('.', $name, [System.IO.Pipes.PipeDirection]::InOut)
try {
    $pipe.Connect(1500)
    $pipe.Write($frame, 0, $frame.Length)
    $pipe.Flush()
} finally { $pipe.Dispose() }

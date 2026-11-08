# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later

# Windows port of tools/linux/bragi-probe.c: READ-ONLY probe of the keyboard's
# vendor interface (usage page 0xFF42, usage 1, instance id ...&MI_01 of
# 1b1c:2b0c), Corsair "V2"/Bragi protocol. It sends property GET requests only
# by default: nothing that selects a mode, writes a colour or touches the
# lighting is ever sent unless -SoftwareMode or -Hold is passed, and both of
# those only ever write property 3 (the render mode), restored on every exit
# path, including Ctrl-C.
#
# Frame format (see docs/protocol/README.md, "Vendor interface seen from the
# Corsair Web Hub" and tools/linux/bragi-probe.c): 128-byte unnumbered reports.
#   request  00 01 00 02 <property>              (command 0x02 = read a property)
#   answer   00 00 00 02 <status> <value u32 little-endian>
# Windows reads and writes this interface through HidD's ReadFile/WriteFile,
# not HidD_GetFeature/SetFeature: like hidraw, the unnumbered report still
# needs a leading report-ID byte (0x00), so every buffer is 129 bytes (see
# docs/protocol/README.md, "Live per-key lighting (Windows, 2026-10-25)",
# "Report length 129 bytes on Windows").
#
# Properties asked: the same list as the Linux probe, all read by the Web Hub
# with status 0, including 0xea (brightness, see
# docs/protocol/README.md#keyboard-brightness-property-0xea-linux-2026-11-08).
# This script's purpose is to check whether Windows answers the same values on
# the same unit: fill in the result in that section once run.
#
# Usage:
#   .\bragi-probe.ps1                   read-only: GET every property, once
#   .\bragi-probe.ps1 -SoftwareMode      property 3 = 2 around the reads, then back to 1
#   .\bragi-probe.ps1 -Hold 30           property 3 = 2 for 30 s (1..300, Ctrl-C ends it early),
#                                        no reads; meant to run while another tool drives the
#                                        LampArray interface in a second window
#
# Close OpenRGB, Corsair Web Hub and iCUE first: another reader steals replies.
param(
    [switch]$SoftwareMode,
    [ValidateRange(0, 300)][int]$Hold = 0
)

if ($SoftwareMode -and $Hold -gt 0) {
    Write-Error "pass only one of -SoftwareMode or -Hold"
    exit 2
}
if ($PSBoundParameters.ContainsKey('Hold') -and $Hold -eq 0) {
    Write-Error "-Hold needs a value from 1 to 300 seconds"
    exit 2
}

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

public static class BragiProbe
{
    const uint GenericReadWrite = 0xC0000000;
    const uint ShareReadWrite = 3;
    const uint OpenExisting = 3;
    const uint FlagOverlapped = 0x40000000;
    const uint ErrorIoPending = 997;
    const int PacketSize = 128; // the vendor interface's unnumbered report, without the ID byte
    public const byte CommandGet = 0x02;
    public const byte CommandSet = 0x01;

    [StructLayout(LayoutKind.Sequential)]
    struct Overlapped
    {
        public IntPtr Internal, InternalHigh;
        public int Offset, OffsetHigh;
        public IntPtr hEvent;
    }

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern SafeFileHandle CreateFile(string name, uint access, uint share, IntPtr sec,
                                            uint disp, uint flags, IntPtr tmpl);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr CreateEvent(IntPtr sec, bool manualReset, bool initialState, string name);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool WriteFile(SafeFileHandle h, byte[] buf, uint n, IntPtr written, ref Overlapped ov);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool ReadFile(SafeFileHandle h, byte[] buf, uint n, IntPtr read, ref Overlapped ov);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern uint WaitForSingleObject(IntPtr h, uint ms);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool GetOverlappedResult(SafeFileHandle h, ref Overlapped ov, out uint transferred, bool wait);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool CancelIoEx(SafeFileHandle h, ref Overlapped ov);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool CloseHandle(IntPtr h);

    public static SafeFileHandle Open(string path)
    {
        return CreateFile(path, GenericReadWrite, ShareReadWrite, IntPtr.Zero, OpenExisting, FlagOverlapped, IntPtr.Zero);
    }

    // Runs one overlapped WriteFile or ReadFile to completion, waiting at most
    // timeoutMs; cancels and reports failure on timeout. Mirrors the Linux
    // probe's write()/poll()/read() with a 500 ms timeout.
    static bool RunIo(SafeFileHandle h, bool write, byte[] buf, uint timeoutMs, out uint transferred)
    {
        transferred = 0;
        IntPtr ev = CreateEvent(IntPtr.Zero, true, false, null);
        if (ev == IntPtr.Zero) return false;
        try
        {
            var ov = new Overlapped();
            ov.hEvent = ev;
            bool immediate = write
                ? WriteFile(h, buf, (uint)buf.Length, IntPtr.Zero, ref ov)
                : ReadFile(h, buf, (uint)buf.Length, IntPtr.Zero, ref ov);
            if (!immediate && Marshal.GetLastWin32Error() != ErrorIoPending)
                return false;
            if (!immediate)
            {
                if (WaitForSingleObject(ev, timeoutMs) != 0) // WAIT_OBJECT_0
                {
                    CancelIoEx(h, ref ov);
                    GetOverlappedResult(h, ref ov, out transferred, true); // reap the cancelled request
                    return false;
                }
            }
            return GetOverlappedResult(h, ref ov, out transferred, true);
        }
        finally { CloseHandle(ev); }
    }

    // Drains any reply already queued (e.g. from the Web Hub's own polling)
    // before sending a request, so a read never matches a stale answer.
    public static void Drain(SafeFileHandle h)
    {
        var buf = new byte[PacketSize + 1];
        uint transferred;
        while (RunIo(h, false, buf, 0, out transferred) && transferred > 0) { }
    }

    // frame is the 128-byte unnumbered report payload, as in bragi-probe.c
    // (frame[1] = 0x01, frame[3] = command, ...). Returns the 129-byte reply
    // (report-ID byte 0 then the 128-byte frame), or null on failure.
    public static byte[] Exchange(SafeFileHandle h, byte[] frame, uint timeoutMs, out string error)
    {
        error = null;
        var request = new byte[PacketSize + 1]; // [0] = report ID, always 0 (unnumbered report)
        Array.Copy(frame, 0, request, 1, PacketSize);
        Drain(h);
        uint written;
        if (!RunIo(h, true, request, timeoutMs, out written))
        {
            error = "write failed (error " + Marshal.GetLastWin32Error() + ")";
            return null;
        }
        var reply = new byte[PacketSize + 1];
        uint read;
        if (!RunIo(h, false, reply, timeoutMs, out read))
        {
            error = "no reply in " + timeoutMs + " ms";
            return null;
        }
        if (read < 10) { error = "short reply (" + read + " bytes)"; return null; }
        return reply;
    }
}
"@

function Open-VendorInterface {
    # Same instance-ID convention as the other tools/windows scripts.
    $dev = Get-PnpDevice -PresentOnly |
        Where-Object { $_.InstanceId -like 'HID\VID_1B1C&PID_2B0C&MI_01*' } |
        Select-Object -First 1
    if (-not $dev) {
        Write-Error "vendor interface of 1b1c:2b0c (MI_01) not present"
        exit 1
    }
    $guid = '{4d1e55b2-f16f-11cf-88cb-001111000030}'
    $path = '\\?\' + ($dev.InstanceId -replace '\\', '#') + '#' + $guid
    $handle = [BragiProbe]::Open($path)
    if ($handle.IsInvalid) {
        Write-Error "cannot open $path (error $([System.Runtime.InteropServices.Marshal]::GetLastWin32Error()))"
        exit 1
    }
    Write-Host "using $path"
    return $handle
}

function Read-Property([Microsoft.Win32.SafeHandles.SafeFileHandle]$Handle, [byte]$Property) {
    $frame = New-Object byte[] 128
    $frame[1] = 0x01
    $frame[3] = [BragiProbe]::CommandGet
    $frame[4] = $Property

    $err = $null
    $reply = [BragiProbe]::Exchange($Handle, $frame, 500, [ref]$err)
    if (-not $reply) {
        Write-Host ("prop 0x{0:x2}: {1}" -f $Property, $err)
        return $false
    }
    $raw = ($reply[0..11] | ForEach-Object { "{0:x2}" -f $_ }) -join ' '
    if ($reply[4] -ne [BragiProbe]::CommandGet) {
        Write-Host ("prop 0x{0:x2}: raw {1}  (unexpected: byte 3 is not 0x02)" -f $Property, $raw)
        return $false
    }
    $status = $reply[5]
    $value = [uint32]$reply[6] -bor ([uint32]$reply[7] -shl 8) -bor ([uint32]$reply[8] -shl 16) -bor ([uint32]$reply[9] -shl 24)
    $line = "prop 0x{0:x2}: raw {1}  status={2} value=0x{3:x} ({3})" -f $Property, $raw, $status, $value
    if ($Property -eq 0xea -and $status -eq 0) {
        $line += "  brightness {0:n1} %" -f ($value / 10.0)
    }
    if ($Property -eq 0x13 -and $status -eq 0) {
        $line += "  firmware {0}.{1}.{2}" -f $reply[6], $reply[7], ([int]$reply[8] -bor ([int]$reply[9] -shl 8))
    }
    Write-Host $line
    return $status -eq 0
}

# Property 3: 1 = hardware RGB mode, 2 = software RGB mode (OpenRGB's naming).
# Sends 00 01 00 01 03 00 <mode>, as the Web Hub does. Returns $true on status 0.
function Set-Mode([Microsoft.Win32.SafeHandles.SafeFileHandle]$Handle, [byte]$Mode) {
    $frame = New-Object byte[] 128
    $frame[1] = 0x01
    $frame[3] = [BragiProbe]::CommandSet
    $frame[4] = 0x03
    $frame[6] = $Mode

    $err = $null
    $reply = [BragiProbe]::Exchange($Handle, $frame, 500, [ref]$err)
    if (-not $reply) {
        Write-Host "set mode $Mode`: $err"
        return $false
    }
    $ok = ($reply[4] -eq [BragiProbe]::CommandSet) -and ($reply[5] -eq 0)
    Write-Host ("set mode {0}: answer {1:x2} {2:x2} {3:x2} {4:x2} {5:x2}{6}" -f `
        $Mode, $reply[0], $reply[1], $reply[2], $reply[4], $reply[5], $(if ($ok) { "" } else { " (unexpected)" }))
    return $ok
}

$Properties = @(0x01, 0x02, 0x05, 0x0a, 0x13, 0x41, 0x6e, 0xea)

$handle = Open-VendorInterface
try {
    if ($Hold -gt 0) {
        if (-not (Set-Mode $handle 2)) { Set-Mode $handle 1 | Out-Null; exit 1 }
        Write-Host "software mode on for $Hold s (Ctrl-C to stop early)"
        $stop = [DateTime]::UtcNow.AddSeconds($Hold)
        while ([DateTime]::UtcNow -lt $stop) { Start-Sleep -Milliseconds 100 }
        $restored = Set-Mode $handle 1
        exit ($(if ($restored) { 0 } else { 1 }))
    }

    $failures = 0
    if ($SoftwareMode) {
        if (-not (Set-Mode $handle 2)) { $failures++ }
        Start-Sleep -Milliseconds 50
    }
    foreach ($property in $Properties) {
        if (-not (Read-Property $handle $property)) { $failures++ }
    }
    if ($SoftwareMode) {
        if (-not (Set-Mode $handle 1)) { $failures++ } # always give the hardware mode back
    }
    exit ($(if ($failures -gt 0) { 1 } else { 0 }))
}
finally {
    $handle.Dispose()
}

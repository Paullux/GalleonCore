# Live per-key lighting: Aaryush1's C# solution (Windows, experimental)

This page describes the **C# implementation by Aaryush1**, a contributor who forked GalleonCore. It documents how that solution is built and what it protects against. It is **not** part of GalleonCore, it is **not** a GalleonCore RGB driver, and nothing here is shipped, built or tested by this repository's CI.

- Source: [`lab/` on the `key-lighting` branch of Aaryush1/GalleonCore](https://github.com/Aaryush1/GalleonCore/tree/5d94820ea646838ff1e49c767e298dfc53259c73/lab) (commit `5d94820`, 2026-10-26). That folder is the contributor's own working copy and carries no licence header of its own; no code is copied here.
- Tracking issue: [#56](https://github.com/Paullux/GalleonCore/issues/56).
- The bytes on the wire (sequence, 440-byte frame, worked example, command log) are in [protocol/README.md](protocol/README.md#live-per-key-lighting-windows-2026-10-25). This page does not repeat them.

## Scope and status

| | |
|---|---|
| Hardware | Keyboard part `1b1c:2b0c` of one Galleon 100 SD, firmware **2.3.88** |
| System | Windows 11 only (raw Win32 HID, Raw Input, `SystemEvents`) |
| Runtime | .NET 10 (`net10.0-windows`) |
| Tested by | The contributor, at the desk, watching the keys, then in daily use (see [Daily use](#daily-use)) |
| Not shown | Linux, other firmware (galleon-deck's earlier result was on 3.70.1), other units, iCUE |

Every result below comes from the contributor's own result notes (`lab/G1-RESULTS.md` to `lab/G3-RESULTS.md`). This repository has not reproduced them on its own hardware.

## Components

The solution is a set of small programs over one library.

| Project | Role |
|---|---|
| `GalleonLink` | The library. Talks to the keyboard and holds every safety rule. Only this code writes to the device. |
| `Probe` | Command-line lab tool. Runs the numbered experiments (G1 read-only, G2 first writes, G3 desk checks) and writes logs. |
| `Guard` | Tiny separate process. Releases the keys when the process it watches exits for any reason. |
| `DeskLights` | The long-running helper. Receives a "scene" over a named pipe and animates the keys, the top strip, the Copilot key, the dials and the mouse. |
| `DeskLightsTests` | Offline self-test of the pure parts (scene parsing, rendering). No device needed. |
| `Snap` | Takes a still from a camera aimed at the keyboard, so a test can check what the keys show. |

### Layers inside `GalleonLink`

The layers follow the same split GalleonCore uses: pure decisions apart from I/O.

| Layer | What it does | Pure? |
|---|---|---|
| `Hid` | Enumerates the HID collections of `1b1c:2b0c`, reads the declared report lengths, opens a shared handle on the vendor collection (usage page `0xFF42`, usage 1). Shared access, so the Stream Deck part and Web Hub keep working. | I/O |
| `Bragi` | Frames commands (`[sub, 0x01, session, op]`), splits a frame into `06`/`07` reports, matches replies, **enforces the allowlist**. | I/O, with a static `Check` that is pure |
| `Frames` | Builds the 440-byte frame: `12 00` then 146 RGB triplets, indexed by LED (= USB HID usage). Out-of-range LED indices are rejected, not dropped. | Pure |
| `Policy` | `RateLimiter`, `FreezePolicy`, `YieldPolicy`, reply classification, release-on-start decision. | Pure |
| `Keyboard` | The only entry point the helper should use. Owns the lock, the connection, the background supervisor and the lifecycle (connect, show, release, dispose). | I/O |
| `Input` | Raw Input on a message-only window, for the typing glow, plus the Windows virtual-key to LED map. Key codes are passed to a callback and never logged or stored. | I/O |

## Safety design

The device gives no error when something goes wrong, so every protection is on the host side.

### 1. Transport allowlist

`Bragi.Check` refuses anything not on a short list **before** it reaches the device. It has two modes.

- **Read-only** (G1): get property, ping, open, describe, read, unbind, session start only.
- **Live** (G2 onward): additionally set property 3 and write to buffer 0 bound to file `0x22`.

Rules the check enforces in live mode:

- Only **property 3** may be set, and only to **1** (self-operated) or **2** (host-controlled). Values 3 and 5 are bootloaders, 4 is sleep.
- Writes (`06`/`07`) are accepted only to buffer 0 and only while buffer 0 is bound to file `0x22`.
- A frame length must be between 1 and 440 bytes.
- Factory reset, create or delete file, calibration, pairing and session stop are not on the list, so they can never be sent.
- Session start must be exactly `[1, 4 random bytes, 0]`; ping must carry no payload (a ping with a payload is pairing).

### 2. Release is the host's job

The keyboard stays in host-controlled mode after a crash, with no timeout (measured: still mode 2 more than 20 s after a forced kill). The solution therefore releases from several independent places.

| When | By whom |
|---|---|
| Every start | `Keyboard.Connect` reads property 3 and releases first if it is not a confirmed 1. This covers a leftover from a crash, sleep or kill. |
| Normal exit, Windows suspend, shutdown, sign-out | `Keyboard` (skipped while yielded, see below). |
| The helper process dies for any reason | `Guard`, started by the helper with its own PID. It waits for that process to exit, opens a fresh session, and releases if it finds mode 2. If the helper had already released, it finds mode 1 and does nothing. It retries up to three times. |
| A freeze is suspected | `FreezePolicy` trips and the link releases and stops driving the keys. |

Measured: with the helper deliberately crashed, the keys were back on the onboard profile **0.38 s** later (Guard saw the exit at 0.12 s), with no replug.

**Release sequence:** unbind buffer 0, set property 3 to 1, read property 3 back to confirm. Each step is attempted even if the previous one fails.

### 3. Freeze policy

Two consecutive failures (a timeout or a non-zero status) trip the policy; any success resets the count. Once tripped, the link stops sending and releases, and stays stopped until the keyboard is replugged. This exists because earlier frame layouts froze typing until unplug.

### 4. Rate limit

At most **20 frames per second** on average, as a token bucket with a burst of 2. The cap follows the limit seen on other Bragi keyboards (faster streaming stalled keystrokes there). Frames that arrive too fast are **dropped, not queued**.

The first version slept after each write and reached only 12.8 fps; a strict per-frame gap also lost every other frame when the caller paced at exactly 50 ms, because Windows timers tick about every 15.6 ms. The bucket fixed both. Measured afterwards: 232 full seconds at exactly 20 frames per second, 23 at 19, no failed writes.

### 5. Yield, never fight

The command channel (`MI_01`) is shared. When the Web Hub connects it sets mode 1, and the device keeps answering the helper's frames with status 0, so there is **no error to notice**. Both writing makes the keys flicker. The solution detects another host in two ways.

1. **The mode changed.** While holding the keys it re-reads property 3 about once a second. A value other than 2 means someone else took the keyboard.
2. **A reply nobody asked for.** Every report is classified against the op being waited for: *ours*, *stale* (a late reply to the helper's own timed-out command) or *foreign*. A foreign reply means another host is polling.

On either sign the link **stops sending anything**, not even a ping, so it cannot confuse the other host's replies (the Web Hub matches replies only by the echoed op code, so a reply flood could corrupt a save in progress). It does not release while yielded, because an unbind could break the other host's file transfer. After **60 s with no foreign traffic** it reads the mode; if it is 1 (the other host left) or 2 (its own leftover frame) it releases and resumes.

Measured with the Web Hub opened mid-run: yielded on its first reply, keys showed the onboard profile with no flicker, nothing sent for 89 s, resumed exactly 60 s after the Web Hub's last traffic.

On connect it also **listens for 3 s before sending anything**. If another host is already talking, it starts yielded. `Guard` skips this wait because it must be fast and only acts when the keyboard is still in host mode.

### 6. Reconnect and session expiry

- An I/O failure when the keyboard is no longer enumerated means **unplugged**, not a freeze. The supervisor re-enumerates every second and runs the normal connect path when the keyboard returns (listen, session, release if needed). Measured: back within about 3 s of the device re-enumerating, with no restart of the tool.
- A USB replug resets the keyboard to mode 1 by itself.
- A reply with byte 1 equal to 15 means the session expired. The link starts a new session and takes host mode again; this counts toward the freeze policy.

### 7. Privacy

The typing glow reads key presses through Raw Input, as a copy of each event (it is not in the input path). Key codes are only turned into an LED index and are never logged or stored. The helper's status reports idle seconds and a locked flag (timing only), never keys.

## The `DeskLights` helper

`DeskLights` is the part that draws. A separate process (the contributor's Stream Deck plugin, called `streamdeck-info`) decides what each zone should show and the helper owns every animation.

- **Transport:** Windows named pipe, UTF-8 JSON, one object per line, one client at a time, lines over 64 KiB dropped. Messages: `hello`, `heartbeat`, `scene` (plugin to helper) and `status` (helper to plugin). Full field list: [`lab/DESK-LIGHTS-PROTOCOL.md`](https://github.com/Aaryush1/GalleonCore/blob/5d94820ea646838ff1e49c767e298dfc53259c73/lab/DESK-LIGHTS-PROTOCOL.md).
- **All or nothing:** a scene with any missing field, bad colour, wrong F-key count, unknown effect or unknown top-level field is rejected whole. The previous scene stays on screen and the error is reported in the status. It never shows half a scene.
- **Stale plugin:** no line for 30 s switches to a built-in neutral scene.
- **Zones:** base colour for all keys, F1 to F12 (LEDs `0x3A` to `0x45`), the Copilot key (`0x81`), the top strip (`0x82` to `0x91`), both dials, and the mouse. Effects: steady, pulse (1 s), breathe (4 s), blink (600 ms once per scene number).
- **Ownership split:**
  - Keys, Copilot key and strip go through `GalleonLink` (this page).
  - Dials and mouse go through **Windows Dynamic Lighting** (`LampArray`) and never through the keyboard's own LampArray, which the keys ignore anyway.
- **Rendering** is a pure function of the scene and the recent key presses, so it is testable offline.

The scene protocol, the F-key effects and the mouse rules belong to the contributor's own plugin. They are described here only so the architecture is understandable. GalleonCore does not depend on them.

## Lessons that apply to any implementation

These come out of the contributor's measurements and are worth keeping when GalleonCore eventually writes its own driver.

1. Plan for the keyboard **not** recovering by itself. Release on start, on exit, on suspend, and from a second process.
2. Treat "status 0" as meaningless once another host is present. Watch the mode and unsolicited replies instead.
3. Do not send anything at all while yielded.
4. Pace by deadline (token bucket), not by sleeping after each write.
5. Keep the allowlist in the transport, in pure code with a test, so a mistake in a higher layer cannot reach the device.
6. Never persist: only property 3 values 1 and 2, and only the volatile file `0x22`.

## Daily use

Reported by the contributor on 11 November 2026, from the helper's own logs since 26 October. One person, one keyboard, Windows 11, firmware 2.3.88.

| | |
|---|---|
| Time running | About 10 days in total, over 6 boots |
| Longest single run | 8 days 16 hours (28 October to 5 November), with no failed write, no freeze trip and no reconnect |
| Shutdowns | All 6 released the keys and confirmed mode 1 before the process ended. On 2 of them a second release attempt (the normal stop path racing the shutdown handler) then logged "cannot access a closed file": log noise after a successful release, to be tidied |
| Stream Deck plugin | Restarted a few times. The helper kept the base colour and the typing glow, fell back to its neutral scene after 30 s once, and picked the plugin back up each time |
| Guard | No unplanned exit to catch |
| Typing latency | In the typing test of 25 October, a key lit about 31 ms after the press (typical) and 94 ms at worst, over 455 presses. The Web Hub side was not measured |

One lesson to keep for any watchdog: in the first packaged builds `Guard` itself crashed at start, because it tried to write its log inside the read-only application folder. For two builds there was no crash protection at all, and nothing visibly failed. **Test the watchdog in its deployed form, not only from the development build.**

This is the contributor's own report, not something this repository has reproduced.

## What is not shown

- **Strip map.** `0x81` is the Copilot key and `0x82` is the left end of the strip. From about `0x84` one index lights a whole section, so the strip is not one LED per index. The Web Hub's key table names `0x8B` to `0x91` as seven "Edge" zones. The exact per-index segments are not pinned down.
- **Quiet period.** With the Web Hub left open but idle, the link resumes after 60 s while the Web Hub is still open. It yields again at the Web Hub's next command, but the Web Hub's preview may briefly show the helper's frame. The contributor decided to keep 60 s.
- **Another host that sits in mode 2 with no traffic at all** for 60 s would be overridden. Not tested against iCUE (which does not list this keyboard).
- **Sleep and wake, USB replug, a real crash and the Web Hub in daily use.** The daily-use report below does not cover them: the PC is shut down at night, so no suspend or resume appears in the logs, and the Web Hub has not been opened since the yield test. They rest on the desk tests of 25 October.
- **Dynamic Lighting for the dials** competes with OpenRGB's `Dynamic Light` device and other apps. Ownership between them is not resolved here.

## Path to GalleonCore

This page does not change the roadmap. The decision recorded in [ROADMAP.md](../ROADMAP.md) stands: the C# code stays the contributor's experiment, and a native C++ driver is only a possibility, with no commitment from the contributor. Before anything is integrated, GalleonCore needs its own validation of the points above: the strict 440-byte format, the allowed commands and modes, release on exit, error and suspend, an independent watchdog, and yielding to the Web Hub. Any protocol change also needs a Qt Test case, ideally built from a real capture (see the project rules in [AGENTS.md](../AGENTS.md)).

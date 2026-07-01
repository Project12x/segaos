# Full-Stack Clean-Room Research Plan

Date: 2026-07-01

This note records the clean-room research direction for SegaOS and related
Mega Drive, Sega CD, 32X, Mega EverDrive, and Linux experiments. It is a
planning and provenance document only. It does not authorize copying GPL or
other incompatible source into SegaOS.

## Scope

There are four distinct targets:

| Target | Description | Primary codebase |
|---|---|---|
| SegaOS-CD | Current Mac-like Sega CD desktop OS. Main 68000 owns VDP/input; Sega CD Sub 68000 owns GUI/app rendering. | SegaOS |
| SegaOS-Mars | Bespoke clean-room OS for Mega Drive + Sega CD + 32X, with 32X acceleration and framebuffer display. | SegaOS or a sibling clean-room tree |
| LinuxMD-68K-FullStack | LinuxMD-style no-MMU m68k Linux on the Mega Drive 68000 using Mega EverDrive RAM/storage, with Sega CD and 32X as devices or accelerators. | GPL Linux-side tree |
| Linux32X-SH2-FullStack | No-MMU Linux running on one 32X SH-2, using the other SH-2, Mega Drive 68000, Sega CD Sub 68000, and Mega EverDrive as services. | GPL Linux-side tree |

The targets should share measured facts and behavior specs, not source code.
SegaOS remains clean-room and all-rights-reserved unless a future licensing
decision changes that. Linux experiments can use GPL Linux code inside a GPL
tree, but that source must not be copied or closely ported into SegaOS.

## Current Public LinuxMD Facts

Public source checked on 2026-07-01:

- Top repository: <https://github.com/LinuxMD/linuxmd>
- Public README: <https://raw.githubusercontent.com/LinuxMD/linuxmd/main/README.md>
- Public submodule list: <https://raw.githubusercontent.com/LinuxMD/linuxmd/main/.gitmodules>

Observed public facts:

- LinuxMD describes itself as "Linux for the Sega MegaDrive."
- Required hardware is a Sega Mega Drive plus Mega EverDrive Core or Pro and a
  USB cable.
- The normal emulator path is not expected to work unless the EverDrive SSF2
  mapper, 4 MB RAM, EverDrive file protocol, and EverDrive timer are emulated.
- The repo includes a QEMU fork for an emulated development path.
- The boot flow copies U-Boot, a compressed Linux kernel, and an EROFS rootfs
  to the EverDrive SD card, then starts U-Boot from the EverDrive menu.
- The sample boot log reports 3.8 MiB DRAM, Linux 7.1.0-rc6 on m68k, no-MMU
  flat model support, `console=ttyVDP0 console=ttyED0 root=/dev/edblk`, an
  EverDrive timer clocksource, an EverDrive-backed block device, and an EROFS
  root filesystem.
- The sample boot log reports `Memory: 2532K/4096K available` and notes that
  the architecture does not have kernel memory protection.
- The README states that the system is "insanely slow right now" and that
  interacting with the EverDrive FIFO is slow.
- The README shows a Mega Drive VDP console with scrolling, a heartbeat, and a
  disk activity indicator.

Submodule commits visible from the public GitHub page on 2026-07-01:

| Component | Visible commit |
|---|---|
| buildroot | `bc965eb` |
| linux | `8e5beaf` |
| medtool | `57da327` |
| png2tile | `c79e457` |
| qemu | `23256f7` |
| smolutils | `ccdc0d2` |
| tarwak | `3afdd18` |
| u-boot | `20fa9d1` |

Clean-room consequence:

- LinuxMD proves the general concept of no-MMU Linux on Mega Drive-class
  hardware with Mega EverDrive RAM and services.
- LinuxMD should be treated as a GPL/Linux-side reference for Linux target work.
- For SegaOS, LinuxMD is limited to public behavior facts, hardware questions,
  and black-box measurement ideas. Do not copy LinuxMD kernel, U-Boot, driver,
  QEMU, or utility source into SegaOS.

## Clean-Room Rules

Allowed for SegaOS:

- Public hardware manuals and register descriptions when licensing permits use.
- Measurements from independently written probes.
- Emulator traces, GDB observations, screenshots, VRAM/RAM readback, and timing
  logs produced by SegaOS probes.
- UI behavior observations from historical systems: menu behavior, window
  ordering, redraw semantics, file-manager workflows, app affordances.
- Behavior specs written in our own words.
- Permissive source reuse only after recording repo, commit, license, inspected
  files, and reuse mode.

Forbidden for SegaOS:

- Copying GPL, AGPL, proprietary, or unknown-license source.
- Mechanical translation of GPL/LinuxMD/EmuTOS/FreeMiNT/OpenGEM code.
- Using LinuxMD device-driver structure as a close port for SegaOS.
- Treating Linux kernel source inspection as permission to implement the same
  code in SegaOS.

Allowed for Linux-side targets:

- Linux kernel, U-Boot, LinuxMD, and related GPL source can be studied and
  modified inside a GPL-compatible Linux target tree.
- Any SegaOS-facing interface produced by that work must be described as a
  behavior contract or hardware protocol, not copied source.

Every future implementation note should record:

- Source or probe name.
- URL or local path.
- Commit SHA or artifact hash when applicable.
- License.
- Files inspected.
- Reuse mode: dependency, fork, direct-copy, close-port, pattern-only, or
  clean-room.

## Central Unknowns

The immediate decision is whether 32X should be a future accelerator or the
main foundation for a full-stack system. That depends on facts not yet proven
in this repo:

1. Can either SH-2 directly read and write Mega EverDrive RAM?
2. Can either SH-2 directly access EverDrive FIFO, file, timer, or control
   registers?
3. If SH-2 cannot reach EverDrive services directly, can the Mega Drive 68000
   act as a fast enough I/O server?
4. What is the real 68K to SH-2 command latency?
5. What 32X framebuffer update rate is achievable for GUI workloads?
6. Can Sega CD Word RAM or PRG-RAM participate usefully in a 32X full-stack
   system, or is it better treated as a service processor and storage/audio
   subsystem?
7. What writable storage path is realistic for real documents: Sega CD Backup
   RAM cart, internal BRAM, EverDrive SD-backed services, or a combination?

## Project 0: 32X Visibility And Bandwidth

Project 0 should be a set of independently written probes. It is not a GUI,
not Linux, and not a replacement for SegaOS-CD. It answers whether the full
stack is technically worth pursuing.

### Probe A: 32X Boot And Mailbox

Goal:

- Boot a minimal 32X program.
- Start one SH-2.
- Exchange a small command/status block between Mega Drive 68000 and SH-2.
- Display a visible 32X framebuffer marker.

Evidence:

- Emulator screenshot or capture.
- RAM/register readback proving command and status words.
- Cycle or frame count around the command round trip.

Decision:

- If this is unstable, do not attempt SegaOS-Mars or Linux32X yet.

### Probe B: SH-2 Framebuffer Throughput

Goal:

- Measure clear, rectangle fill, glyph blit, line blit, and small window move
  operations in 32X framebuffer modes useful for a desktop.
- Compare 8bpp indexed mode and 15bpp direct color where practical.

Evidence:

- Per-frame operation counts.
- Tearing or page-flip behavior.
- Screenshot proving expected composition.

Decision:

- If the SH-2 can redraw a Mac-like 320x224/240 desktop at interactive rates,
  SegaOS-Mars is viable as a first-gen Mac-class color desktop.
- If full redraws are too expensive but dirty rects are fast, keep the current
  SegaOS dirty-region model and move only the compositor/blitter to SH-2.

### Probe C: SH-2 Cartridge And EverDrive RAM Visibility

Goal:

- Determine whether SH-2 can see cartridge-space RAM supplied by Mega
  EverDrive.
- Start with read-only detection and safe mirrored-pattern observations.
- Only write to a bounded scratch range after the memory map is understood.

Evidence:

- Address ranges tested.
- Read/write pattern table.
- Any bus error, mirror, byte-lane, cache, or contention behavior.

Decision:

- If SH-2 directly sees EverDrive RAM, Linux32X-SH2 becomes plausible.
- If SH-2 cannot see it, Linux32X-SH2 needs the 68K as a memory or I/O server,
  which may be too slow or too complex.

### Probe D: SH-2 EverDrive Service Visibility

Goal:

- Determine whether SH-2 can access EverDrive FIFO, file protocol, timer, or
  control registers directly.
- Keep this as black-box protocol observation, not a copy of LinuxMD drivers.

Evidence:

- Register read/write visibility table.
- Timeout behavior.
- Whether 68K and SH-2 contend for the same service registers.

Decision:

- Direct SH-2 EverDrive services favor Linux32X-SH2.
- 68K-only EverDrive services favor LinuxMD-68K with 32X acceleration, or
  SegaOS-Mars with a 68K storage server.

### Probe E: 68K, SH-2, And Sega CD Service Bridge

Goal:

- Measure a three-processor command path:
  Mega Drive 68000 <-> 32X SH-2 <-> Sega CD Sub 68000.
- Keep each message coarse: render command, storage command, audio command,
  decompression command.

Evidence:

- Round-trip time for each leg.
- Maximum reliable payload size.
- Failure mode when one side is busy.

Decision:

- If the bridge is fast and deterministic, SegaOS-Mars can use Sega CD as a
  service processor behind SH-2 UI code.
- If it is slow, use Sega CD for storage/audio only and avoid fine-grained
  cross-processor calls.

### Probe F: Writable Storage

Goal:

- Measure practical save/load behavior for internal BRAM, external Backup RAM
  cartridge-class storage, and EverDrive service-backed files where available.

Evidence:

- Capacity.
- Minimum allocation unit.
- Write latency.
- Failure behavior.
- Corruption safety when interrupted.

Decision:

- SegaOS-CD should keep external Backup RAM as the primary document target
  unless EverDrive services are intentionally made part of a separate edition.
- Linux full-stack targets need EverDrive storage to be more than a novelty.

## Target-Specific Research

### SegaOS-CD

Near-term clean-room research:

- Long-running VDP frame policy for dirty-tile updates.
- External Backup RAM cart detection, capacity, and save policy.
- File-manager behavior for tiny documents.
- Mega Mouse and controller-driven UI ergonomics.
- On-screen keyboard and possible external keyboard options.
- Desktop app behavior specs: Notepad, Paint, Calculator, BASIC LIST/edit,
  Preferences.

Do not block SegaOS-CD on 32X. The Sega CD-only desktop is still the most
impressive stock-hardware result because it makes a constrained, real Sega CD
behave like a small GUI computer.

### SegaOS-Mars

Near-term clean-room research:

- 32X framebuffer compositor model.
- Which CPU owns window state: SH-2, Sega CD Sub 68000, or current Sub 68000
  code with SH-2 as a renderer.
- Whether Genesis VDP remains useful for text, menu, cursor, or background
  layers under a 32X overlay.
- Whether Sega CD is a filesystem/audio/decompression service behind a 32X UI.

Likely architecture if probes are positive:

- Master SH-2: desktop compositor, dirty rect rasterization, pointer/window
  feedback.
- Slave SH-2: secondary blits, fills, decompression, image operations, or app
  compute.
- Mega Drive 68000: boot, input, VDP support layer, sound command path, and
  hardware coordination.
- Sega CD Sub 68000: CD, PCM audio, storage, app/resource loading, optional
  app logic.

This target can be meaningfully equivalent to a first-generation Mac in user
experience: small windows, menus, files, text editing, painting, calculator,
preferences, and simple scripting. It should not chase Unix workstation
behavior.

### LinuxMD-68K-FullStack

Near-term GPL-side research:

- Add or prototype 32X framebuffer as a Linux console/framebuffer device.
- Treat Sega CD as block/audio/service hardware if a clean hardware bridge is
  proven.
- Measure EverDrive FIFO and block I/O as the real bottleneck.
- Keep userland tiny: shell, text tools, simple framebuffer UI, not X11/GTK.

This is the fastest credible Linux path because LinuxMD already boots on the
Mega Drive 68000 with Mega EverDrive RAM. Its ceiling is limited by the 68000
being the kernel CPU.

### Linux32X-SH2-FullStack

Near-term GPL-side research:

- Confirm that mainline or LinuxMD-derived SuperH/no-MMU support can be shaped
  into a 32X board target.
- Design a one-SH-2 kernel first. Do not start with SMP.
- Prove timer, IRQ, RAM, console, rootfs, and boot handoff.
- Decide whether the second SH-2 is a Linux-visible helper, a framebuffer
  coprocessor, or unused during first boot.

This has the highest upside but depends almost entirely on Project 0:

- If SH-2 sees EverDrive RAM and services, this is plausible.
- If SH-2 only has 32X RAM and must ask the 68K for all storage, it becomes a
  research project rather than a practical full-stack OS target.

## UI Behavior Research

The useful clean-room UI question is not "can it look like a Mac screenshot?"
It is "can it support a small document workflow?"

Behavior to specify:

- Finder-like desktop:
  - disk/app/document icons
  - open, close, rename, delete, duplicate
  - simple list view before icon-view polish
- Menu bar:
  - Apple/system menu equivalent
  - File/Edit/View/Special style commands
  - disabled item states
- Windows:
  - active/inactive title bars
  - close box
  - drag
  - resize only if affordable
  - content clipping
  - dirty redraw ownership
- Text editor:
  - fixed-width text first
  - save/load
  - line wrapping policy
  - small file limits visible to user
- Paint:
  - small canvas sizes
  - pencil, fill, line, rectangle
  - save only when storage target supports it
- BASIC:
  - LIST/edit/save first
  - evaluator later
  - desktop file I/O last

Historical OSes may guide behavior, terminology, and failure modes. Their
source must not be copied unless a future provenance pass records a compatible
license and an explicit reuse mode.

## Research Artifacts To Keep

Every probe should leave at least one durable artifact:

- A short markdown note under `docs/reference/`.
- The exact build command.
- Emulator and hardware environment.
- Probe result table.
- Screenshot path when visual.
- GDB/readback evidence when correctness matters.
- A "decision impact" sentence tying the result to one or more targets.

For example:

```text
Probe: 32X SH-2 EverDrive RAM visibility
Build: <command>
Environment: hardware or emulator, cartridge, region
Addresses tested: <table>
Result: direct/read-only/direct-rw/not-visible/unsafe
Decision impact: Linux32X-SH2 is green/yellow/red for next boot work
```

## Recommended Next Step

Do Project 0 before committing to either 32X OS direction.

Order:

1. Minimal 32X boot and 68K <-> SH-2 mailbox.
2. 32X framebuffer throughput for desktop primitives.
3. SH-2 visibility into cartridge/EverDrive RAM.
4. SH-2 visibility into EverDrive service registers.
5. 68K <-> SH-2 <-> Sega CD service bridge.
6. Writable storage behavior.

Expected decision after Project 0:

- Keep SegaOS-CD as the mainline if 32X adds only display acceleration.
- Start SegaOS-Mars if SH-2 framebuffer and command queues are strong.
- Keep Linux work on 68K if SH-2 cannot own RAM/storage.
- Start Linux32X-SH2 only if SH-2 can access enough RAM/storage or the 68K
  service path is proven fast enough for a usable no-MMU system.

# SegaOS Loadable App Runtime Goal

SegaOS should aim higher than a desktop-themed tech demo. The target is a
GEOS/GEM/Contiki-class operating environment for stock Sega CD: a small shell
that owns the hardware, exposes OS services, and can launch independent GUI
applications from CD resources.

The Mac-like look remains useful as an interaction reference. It is not the
technical bar. The impressive feat is a stable Sega CD app runtime ABI.

## What Counts As Impressive

A demo is only accepted as an OS-level result when it proves app boundaries:

1. The shell boots first and owns display, input, Word RAM transfer, storage,
   CD resources, and audio commands.
2. At least one app is built as a distinct app artifact or module, not just a
   hardcoded branch in the shell.
3. The shell discovers or selects that app from a CD app catalog.
4. The app requests a window through SegaOS services.
5. The app receives events through SegaOS services.
6. The app draws through SegaOS drawing/text APIs instead of touching VDP or
   Word RAM directly.
7. The app saves or loads a document through SegaOS storage policy instead of
   knowing Backup RAM details.
8. The app exits cleanly and another app can be opened without rebooting.

Until those behaviors exist, built-in tools are useful bring-up code but not the
final OS aspiration.

## Non-Goals

- A launcher that only starts separate games.
- A BASIC interpreter as a standalone novelty.
- A file viewer with no app boundary.
- A CD player replacement.
- A static desktop screenshot with decorative windows.
- A GUI toolkit where applications still bypass the OS for storage, input, or
  display ownership.

## Runtime Model

The first runtime can be cooperative and small. It does not need memory
protection, preemptive multitasking, or dynamic allocation.

The initial ABI should be handle-based and static:

- `AppInit`: app receives an OS service table and returns requested resources.
- `AppEvent`: app handles input, menu, window, timer, and storage-complete
  events.
- `AppDraw`: app draws only inside invalidated content rectangles using
  SegaOS drawing calls.
- `AppCommand`: app receives shell/menu commands such as open, save, close,
  about, and run.
- `AppExit`: app releases handles and returns to the shell.

The shell owns these service families:

- window creation, activation, close, dirty redraw, and clipping;
- fixed-font text, primitive drawing, and image/tile resource presentation;
- mouse/controller/keyboard-style event normalization;
- CD app/resource catalog lookup;
- internal BRAM and external Backup RAM cartridge save policy;
- PCM/CD audio commands when used by apps;
- error dialogs and low-memory/app-load failure reporting.

## First Catalog Contract

`include/app_catalog.h` and `src/sub/app_catalog.c` define the first
host-tested app catalog parser. This is not a module loader yet; it is the
stable shell-side descriptor boundary that future CD/module loading should
consume.

Reference-code-first note:

- Upstream: `drojaazu/megadev@7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`
- License: MIT
- Files inspected: `docs/modules.md`, `lib/main/mmd.macros.s`,
  `lib/sub/cdrom.h`, plus existing SegaOS notes in
  `megadev_modules_and_cdrom.md`
- Reuse mode: pattern-only / clean-room

Relevant lessons applied:

- Keep the resident shell/kernel separate from loaded modules.
- Treat CD file/resource names as ISO9660-style uppercase 8.3 identifiers.
- Avoid running critical interrupt/kernel code from transient Word RAM modules.
- Start with a small descriptor/catalog boundary before loading executable
  code.

Catalog byte layout, version 1:

```text
Header, 16 bytes:
  $00.b[4]  Magic "SAC1"
  $04.w     Version = 1
  $06.w     Entry count, max 32
  $08.w     Entry size = 64
  $0A.w     Reserved, zero
  $0C.l     Reserved, zero

Entry, 64 bytes:
  $00.w     App id, nonzero
  $02.b[12] App name, uppercase ISO-style 8.3 such as TEXT.APP
  $0E.b[24] Display title, printable ASCII
  $26.w     Kind: 1 = built-in rung, 2 = loadable module
  $28.w     Capability flags
  $2A.l     Module LBA, future loader input
  $2E.l     Module byte length; required for loadable modules
  $32.l     Resource LBA
  $36.l     Resource byte length
  $3A.w     Minimum content width
  $3C.w     Minimum content height
  $3E.w     Reserved, zero
```

The parser intentionally reads explicit big-endian bytes rather than casting C
structs. That keeps the format stable across host tests, 68000 target builds,
and later catalog-generation tools.

## First Lifecycle ABI Contract

`include/app_runtime.h` and `src/sub/app_runtime.c` define the first
host-tested lifecycle dispatcher for catalog-backed GUI apps. This is not a CD
module loader yet; it is the OS/app call boundary that a built-in rung,
separately linked app table, or later CD-loaded module should satisfy.

Reference-code-first note:

- Upstream: `drojaazu/megadev@7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`
- License: MIT
- Files inspected: `docs/modules.md`, `lib/main/mmd.macros.s`,
  `lib/sub/cdrom.h`
- Reuse mode: pattern-only / clean-room

The runtime ABI itself is SegaOS clean-room code. Megadev informs the resident
shell/module separation and ISO-style naming discipline. GEOS, GEM/TOS, and
Contiki remain behavior references for app lifecycle ownership, but no
GPL-family or unknown-license OS source is copied or closely ported.

Implemented shape:

- One cooperative active app per `AppRuntime`.
- Versioned `AppRuntimeServices` table owned by SegaOS.
- Fixed `AppRuntimeLimits` for window count, window size, document bytes, and
  scratch bytes.
- OS service callbacks for window request, text drawing, and document save.
- `AppDefinition` entry points: `init`, `event`, `draw`, `command`, `exit`.
- `APP_RT_Start()` validates the service table, catalog entry, app definition,
  app/catalog name match, GUI window capability, minimum window size, and
  service resource limits before calling app init.
- Failed init clears the runtime; failed exit keeps the app marked running so
  the shell can retry or report a trapped app.
- Event, draw, command, and exit callback failures map to explicit runtime
  status codes instead of disappearing into shell control flow.

`tests/test_app_runtime.c` proves the first fake `TEXT.APP` lifecycle: the app
starts from a catalog entry, requests a window through the OS table, receives an
event, draws text through the OS callback, saves through the OS callback, exits,
and rejects invalid service/catalog/app boundaries plus overlapping app starts.

## First Accepted Showpiece

The first GEOS/Contiki-level showpiece should demonstrate:

1. Boot to a readable desktop shell.
2. Open an app catalog backed by CD resources.
3. Launch `TEXT.APP` or an equivalent separately described module.
4. The app requests a document window and receives redraw/input events.
5. Edit or display a small text document.
6. Save the document through SegaOS storage policy.
7. Close the app.
8. Launch a second app, such as `BASIC.APP`.
9. Reopen or import the saved document, or prove a BASIC program persisted.
10. Reboot and load the saved document/program again.

This script proves the operating environment. BASIC, image viewing, and
multimedia become more impressive after this boundary exists.

## Reference Posture

- Megadev remains the permissive Sega CD boot, Gate Array, BRAM, and CD-ROM
  reference.
- SGDK remains a permissive pattern reference for VDP/DMA discipline and the
  current font source.
- GEM/TOS, GEOS, and Contiki are aspiration and behavior references for
  app/runtime discipline. Do not copy GPL-family or unknown-license source into
  SegaOS.
- Any permissive source used for app loading, resource formats, filesystem
  parsing, or scripting needs the usual repo, commit, license, inspected files,
  and reuse-mode note before implementation.

## Current Gate

Do not spend major effort on built-in app polish until the live display path can
show the newest returned frame and the app-runtime boundary is specified enough
to keep future tools from becoming shell code.

The immediate implementation ladder remains:

1. Stable current-frame Word RAM/VDP policy.
2. OS-owned window/event/redraw services outside opt-in probes.
3. A minimal app descriptor/catalog format.
4. A first lifecycle dispatcher with app callbacks and an OS service table.
5. One loadable or separately linked app calling those OS services from the
   desktop shell.
6. Storage service binding for that app.

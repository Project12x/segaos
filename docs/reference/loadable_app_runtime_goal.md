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
4. One loadable or separately linked app calling OS services.
5. Storage service binding for that app.

# How To

## Prerequisites

- **SGDK** installed at `C:\SDKS\SGDK\`
  - Download from [GitHub](https://github.com/Stephane-D/SGDK)
  - Verify: `C:\SDKS\SGDK\bin\gcc.exe --version` should show `m68k-elf-gcc`

## Build

```bash
# Full build (Sub CPU + Main CPU)
C:\SDKS\SGDK\bin\make.exe -f Makefile main

# Sub CPU only
C:\SDKS\SGDK\bin\make.exe -f Makefile sub

# Clean all build artifacts
C:\SDKS\SGDK\bin\make.exe -f Makefile clean

# Show toolchain info
C:\SDKS\SGDK\bin\make.exe -f Makefile info
```

## Project Structure

```
segaos/
  include/          # Shared headers (both CPUs)
    libc/           # Freestanding C headers (stdint, string, etc.)
    blitter.h       # Software renderer API
    common.h        # Inter-CPU communication protocol
    framebuffer.h   # Tile conversion pipeline
    ga_regs.h       # Gate Array register definitions
    mouse.h         # Mega Mouse driver
    vdp.h           # VDP hardware interface
    wm.h            # Window manager
  src/
    main/           # Main CPU sources
      crt0.s        # Startup assembly
      main.c        # Boot sequence + main loop
      main.ld       # Linker script (Work RAM $FF0000)
      mouse.c       # Mega Mouse hardware driver
      framebuffer.c # Linear-to-tile conversion + DMA
    sub/            # Sub CPU sources
      crt0.s        # SP header + startup
      sub.c         # Sub CPU main + command loop
      sub.ld        # Linker script (PRG-RAM $006000)
      blitter.c     # Software framebuffer renderer
      wm.c          # Window manager
      mem.c         # Memory manager
      notepad.c     # Notepad application
      calc.c        # Calculator application
      paint.c       # Paint application
      vkbd.c        # Virtual keyboard
  docs/reference/   # Sega CD hardware reference docs
  build/            # Build output (gitignored)
```

## Adding a New Application

1. Create `src/sub/myapp.c` with app init and event handlers
2. Create `include/myapp.h` with the public API
3. Register the app in `wm.c` (add to app launcher / menu bar)
4. The Makefile auto-discovers `src/sub/*.c`, so no Makefile changes needed

## Common Tasks

### Change Video Mode
In your Sub CPU app, call `BLT_SetMode(BLT_MODE_4BIT)` for 16-color or `BLT_SetMode(BLT_MODE_2BIT)` for 4 grayscale.

### Modify Palette
Edit `FB_PALETTE_WIN31` in `include/framebuffer.h`. MD format: `----BBB-GGG-RRR-` (3 bits per component, shifted left 1).

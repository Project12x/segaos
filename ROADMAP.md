# Roadmap

## Phase 1: Sub CPU Build -- COMPLETE
- [x] crt0.s with SP header and vector table
- [x] Freestanding C headers (stdint, stddef, stdbool, string)
- [x] Linker script (sub.ld) for PRG-RAM
- [x] Makefile with ld.exe direct linking + libgcc.a

## Phase 2: Main CPU VDP Pipeline -- COMPLETE
- [x] crt0.s for Work RAM entry
- [x] Linker script (main.ld) for $FF0000
- [x] Standalone VDP interface (vdp.h)
- [x] Tilemap setup (40x28 sequential tiles)
- [x] Strip-based linear-to-tile conversion
- [x] DMA pipeline (5KB strip buffer)
- [x] Win3.1 palette in MD 9-bit RGB

## Phase 3: Word RAM Bank Sync -- NEXT
- [ ] 1M mode bank swap protocol (DMNA/RET bits)
- [ ] Double-buffer handshake between Main and Sub CPU
- [ ] DMA timing refinement (VBlank vs active display)

## Phase 4: Integration Testing
- [ ] Boot disc image generation (IP/SP packaging)
- [ ] Emulator testing (Ares, BlastEm, or Gens)
- [ ] End-to-end: Sub CPU renders -> display on screen
- [ ] Mouse input -> window interaction

## Phase 5: Polish
- [ ] Clean up linker warnings (RWX permissions)
- [ ] Resolve symbol redefinition warnings
- [ ] Remove unused variables
- [ ] Performance profiling (conversion + DMA budget)

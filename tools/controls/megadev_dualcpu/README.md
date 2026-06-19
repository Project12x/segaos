# Megadev Dual-CPU Control

This fixture is a tiny Sega CD boot control used to separate image/security
problems from Sub CPU user-call startup problems.

Reference-code-first record:

- Upstream: `https://github.com/drojaazu/megadev`
- Commit: `7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`
- License: MIT
- Files inspected: `cfg/ip.ld`, `cfg/sp.ld`, `lib/sub/sp_header.s`,
  `lib/main/gate_arr.def.h`, `lib/sub/gate_arr.def.h`,
  `examples/hello_world/src/ip.s`, `examples/hello_world/src/sp.s`
- Reuse mode: close-port/pattern-only for linker/header shape; clean-room
  control code for the COMSTAT breadcrumbs

Expected pass signal:

- Main control magic: `0x4D43`
- Main phase: `0x0002`
- Sub flag: `0x0003` or `0x0005`
- COMSTAT0/1: `0x5350`, `0x494E` (`SP`, `IN`)

This control passes in BlastEm/GDB with the SegaOS Python ISO builder. It is
the baseline that proves the builder, regional security block, BIOS SP load,
and a Megadev-shaped SP startup are not the active problem. The current SegaOS
`BOOT_PROBE=1` status rung also passes; the remaining strict probe failure is
Word RAM 1M ownership/RET/DMNA handoff.

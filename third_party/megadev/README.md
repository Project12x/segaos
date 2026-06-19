# Megadev Reference Code

This directory records third-party source reuse from Megadev.

- Upstream: https://github.com/drojaazu/megadev
- Commit: `7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`
- Version: MEGADEV 1.2.0
- License: MIT, see `LICENSE`
- Reuse mode: direct-copy for `src/main/security.c`; close-port/pattern-only
  for the SP header/linker contract
- Source files inspected:
  - `lib/security.c`
  - `lib/sub/sp_header.s`
  - `cfg/sp.ld`
  - `lib/cd_boot.s`

`src/main/security.c` is included in the Main CPU IP so the regional Sega CD
security block is first in the IP binary, matching Megadev's `cfg/ip.ld`
placement of `*(.security*)` before normal IP code.

`src/sub/crt0.s` now follows Megadev's SP module-header shape: `"MAIN       "`
module name, flags/version/type fields, resident text length, relative jump
table offset, RAM length, and `sp_init`/`sp_main`/`sp_int2`/`sp_user` relative
jump-table entries. The implementation is adapted to SegaOS' C runtime and
linker symbols rather than copied verbatim.

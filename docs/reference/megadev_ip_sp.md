# The IP and SP

Primary source: `drojaazu/megadev@7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`
(`MEGADEV 1.2.0`, MIT), especially `docs/boot.md`, `cfg/ip.ld`,
`cfg/sp.ld`, `lib/cd_boot.s`, and `lib/sub/sp_header.s`.

The IP (Initial Program) and SP (Sub CPU Program) are small pieces of code that reside within the boot sector of the disc. They are the entry points for your program on the Main and Sub sides, respectively, and are loaded automatically by the Boot ROM on startup. The IP is loaded to 0xFF0000 (the start of the User block) on the main side, and the SP is loaded to 0x6000 (within PRG RAM block 0, at the end of the internal BIOS code) on the Sub side.

Megadev recommends keeping the IP and SP in assembly because they are small,
low-level boot programs that must be predictable. If they become more complex,
Megadev's pattern is to keep them tiny and load extended resident kernels
(`IPX` and `SPX`) from disc.

For SegaOS, the current Main/Sub C programs are closer to IPX/SPX-style kernels
than to minimal boot stubs. The boot-disc plan should either:

- keep the current architecture but pack it with Megadev-compatible IP/SP boot
  fields and a regional security block, or
- split into tiny assembly IP/SP stubs that load larger resident kernels.

## Special notes about the IP
The IP is constrained within the boot sector. Megadev's `ip.ld` default length
is `$E00` bytes, loaded to `$FF0000`, and part of that envelope is occupied by
regional security code.

Megadev's `lib/cd_boot.s` physically includes `ip.bin` at boot-sector offset
`$0200`, but writes `$0800` into the header IP offset field. This is a BIOS
compatibility quirk for the larger US/EU security code. Do not use the header
field as the physical byte-copy offset.

## Special notes about the SP
The SP requires a header at its start, including a jump table defining the location of user code called from BIOS. (See Sections 4 and 5 of the BIOS Manual for more details.) Megadev provides a pre-defined header that is built along with the SP automatically, so you do not need to worry about setting this up yourself. However, you must globally define each of these functions with the following names:

    sp_init
    sp_main
    sp_int2
    sp_user

These correspond to `usercall0` through `usercall3` as defined in the BIOS manual. All four entries must be defined, even if they are not used. (Unused subroutines can contain a simple `rts` or `return` for asm or C, respectively.)

SegaOS's `src/sub/crt0.s` follows this shape today: it emits the Megadev-style
SP module header first, then a relative jump table for `sp_init`, `sp_main`,
`sp_int2`, and `sp_user`.

`src/sub/sub.ld` also uses Megadev-style `SUBALIGN(2)` for the SP text/data/bss
sections. In the current framebuffer probe map, the SP header is at `$6000`,
`sp_init` is at `$602A`, `sp_main` is at `$607E`, and `_TEXT_LENGTH` is `$0412`
(1,042 bytes) for the visible framebuffer probe.

Current Milestone B note: the first C-runtime `BOOT_PROBE=1` experiments proved
SP bytes were loaded but produced no Sub breadcrumbs. A dedicated
Megadev-derived dual-CPU control then proved the SegaOS ISO builder/security
path can boot a Megadev-shaped SP and report from Sub-side code. SegaOS has
since pivoted the probe build to an assembly-only SP path, and
`tools/probe_blastem_boot.ps1 -Probe DualCpuStatus` now proves Sub `sp_init`,
Sub `sp_main`, and Gate Array command/status exchange.

The remaining low-level IP/SP issue is not user-call installation. Word RAM
handoff is now proven for the first bank-0 return: `-Probe DualCpu` writes
`0xa55a/0x5aa5` from Sub at `$0C0000`, clears RET in 1M mode, and Main reads
the same words at `$200000`. `-Probe Framebuffer` then proves a deterministic
4bpp pattern can pass through the same handoff and Main's VDP tile upload path,
and the visible framebuffer variant is confirmed by BlastEm internal
screenshotting. `DESKTOP_REPEAT_PROBE=1` then proves two boot-safe
render/upload cycles through the conservative single-bank path. Keep the
assembly SP probe as the known-good startup reference while designing the
long-running frame and double-buffer policy, then decide whether the production
bootstrap remains a tiny assembly IP/SP stub that enters larger C kernels or a
tightened C runtime with the same invariants.

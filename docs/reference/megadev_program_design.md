# Mega CD Program Architecture Concepts
Source: https://github.com/drojaazu/megadev/blob/master/program_design.md

## The Hardware

Writing a game for retro hardware is, in a way, writing an "operating system" from scratch. The complexity escalates once we bring the Mega CD into the picture. You now have a second CPU, non-persistent memory regions and buffer ownership to manage.

Before starting, you should have a basic understanding of these Mega CD specific pieces:

  - Which components are managed by which CPU (VDP & VRAM -> Main CPU, CD Drive & BRAM -> Sub CPU)
  - Work RAM
  - Word RAM
  - PRG RAM
  - The Gate Array (especially Comm Flags/Command/Status registers)
  - The IP/SP

## Memory Planning

### Work RAM layout (Main CPU)

Default layout, with no user programs or Boot ROM library usage:

    FF0000  +----------------+
            | IP/AP Use      |
            |                |
            | Top of Stack   |
    FFFD00  +----------------+
            | System Use     |
            |                |
    FFFFFF  +----------------+

The System Use area (0xFFFD00) contains the exception/interrupt jump table. The M68000 vector table is defined by the Boot ROM. The vectors point to locations within System Use and cannot be changed (only their pointers).

The stack is allocated to 0xFFFD00 by default. Prefer using global values whenever possible to prevent too much stack usage.

Example layout:

    FF0000  +----------------+
            | "ROM" (~56KB)  |
    FFDF00  +----------------+
            | "RAM" (~7KB)   |
    FFFB00  +----------------+
            | Stack (512B)   |
    FFFD00  +----------------+
            | System Use     |
    FFFFFF  +----------------+

### Memory Layout in Other Regions

**PRG RAM**: 512KB total, only 24KB reserved for BIOS. Sub CPU kernel executes here.

**Word RAM**: Buffer shared between CPUs. Can execute code from here, but never use for kernel code (ownership can change, crash risk).

Two modes:
- **2M mode**: All 256KB owned by one CPU, inaccessible to the other. Main aspect of CPU sync is reconciling ownership.
- **1M mode**: Split into 128KB banks, each owned by one CPU. Ownership can be readily switched. Useful for streaming data (video) loaded from disc via Sub CPU into one bank while Main CPU copies from other bank to VRAM, then switching.

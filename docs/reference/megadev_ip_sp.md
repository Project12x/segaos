# The IP and SP
Source: https://github.com/drojaazu/megadev/blob/master/ip_sp.md

The IP (Initial Program) and SP (Sub CPU Program) are small pieces of code that reside within the boot sector of the disc. They are the entry points for your program on the Main and Sub sides, respectively, and are loaded automatically by the Boot ROM on startup. The IP is loaded to 0xFF0000 (the start of the User block) on the main side, and the SP is loaded to 0x6000 (within PRG RAM block 0, at the end of the internal BIOS code) on the Sub side.

The IP and SP should be written in assembly. This is because two of the integral pieces of Megadev, the MMD loader and the CDROM access API, are written in assembly. As both are necessary for the basic flow of loading and executing files, we want them installed in memory as soon as possible and should thus be included in the IP/SP.

(It *is* theoretically possible to write the IP/SP in C, compile it and the MMD/CDROM code separately as objects, then link the two together, but considering the fact that the IP/SP are meant to be small init programs, we feel this is unnecessarily convoluted and that writing in C may yield code that is too big or not optimal enough.)

If assembly programming is not your strong point or if the IP/SP becomes sufficiently complex as your game's kernel, it may be best to keep the IP/SP tiny, with only the asm MMD loader/CDROM API in each one, and then load an "extended" kernel (what we call the IPX/SPX) to complement or replace the IP/SP.

## Special notes about the IP
The size of the IP is quite constrained within the boot sector. It is allowed only 3.5k of space, part of which is required to be the security code.

## Special notes about the SP
The SP requires a header at its start, including a jump table defining the location of user code called from BIOS. (See Sections 4 and 5 of the BIOS Manual for more details.) Megadev provides a pre-defined header that is built along with the SP automatically, so you do not need to worry about setting this up yourself. However, you must globally define each of these functions with the following names:

    sp_init
    sp_main
    sp_int2
    sp_user

These correspond to `usercall0` through `usercall3` as defined in the BIOS manual. All four entries must be defined, even if they are not used. (Unused subroutines can contain a simple `rts` or `return` for asm or C, respectively.)

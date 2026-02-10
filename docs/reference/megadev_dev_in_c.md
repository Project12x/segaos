# Development using C on Mega CD
Source: https://github.com/drojaazu/megadev/blob/master/dev_in_c.md

## Standard Library
There is no standard library. The Boot ROM library provides some helpers, but no string.h, stdlib.h, stdio.h, etc. No printf, no malloc/free. Everything must be written from scratch.

## Global Variable Initialization
**CRITICAL**: Statically allocated variables (globals) are NOT initialized even if you specify an initial value. E.g.:

    u16 initial_value = 100;  // Will NOT be 100!

The value will be whatever already exists in memory. You must manually assign values before use.

Reason: Retro consoles don't use ELF/PE execution containers. There is no automatic .data initialization. Code execution begins immediately.

It IS possible to copy .data segment as first action in your program, but this must be explicitly implemented.

## Integer Types
Do NOT use `int` type. With gcc-m68k, int is 4 bytes. Use sized types:
- `s8`/`u8` (char)
- `s16`/`u16` (short)  
- `s32`/`u32` (long)

## Fractional Types
M68000 has no FPU. Use fixed-point math instead of float.

## Heap Allocation
No malloc/free. Write custom provisioning for your specific needs. Fixed-size object pools are typical for retro game dev.

## Stack Usage
C pushes values to stack for function calls. Stack fills up quickly.

Strategies:
- Allocate large enough stack (512+ bytes, Bootlib only allocates 256)
- Keep function parameters minimal (use struct pointers)
- Minimize chained function calls (nested calls = nested stack frames)
- Avoid recursive functions
- Prefer globals over locals where possible

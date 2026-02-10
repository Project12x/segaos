/* Freestanding stddef.h for m68k-elf-gcc */
#ifndef _STDDEF_H
#define _STDDEF_H

typedef unsigned long size_t;
typedef signed long ptrdiff_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define offsetof(type, member) __builtin_offsetof(type, member)

#endif /* _STDDEF_H */

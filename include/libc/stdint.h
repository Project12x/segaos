/* Freestanding stdint.h for m68k-elf-gcc (SGDK toolchain)
 * Provides exact-width integer types for Motorola 68000.
 * m68k: char=8, short=16, int=32, long=32, pointer=32
 */
#ifndef _STDINT_H
#define _STDINT_H

/* Exact-width integer types */
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed long int32_t;
typedef unsigned long uint32_t;

/* Minimum-width integer types */
typedef int8_t int_least8_t;
typedef uint8_t uint_least8_t;
typedef int16_t int_least16_t;
typedef uint16_t uint_least16_t;
typedef int32_t int_least32_t;
typedef uint32_t uint_least32_t;

/* Fastest minimum-width integer types (m68k prefers 32-bit ops) */
typedef int32_t int_fast8_t;
typedef uint32_t uint_fast8_t;
typedef int32_t int_fast16_t;
typedef uint32_t uint_fast16_t;
typedef int32_t int_fast32_t;
typedef uint32_t uint_fast32_t;

/* Integer type capable of holding a pointer */
typedef int32_t intptr_t;
typedef uint32_t uintptr_t;

/* Limits */
#define INT8_MIN (-128)
#define INT8_MAX 127
#define UINT8_MAX 255

#define INT16_MIN (-32768)
#define INT16_MAX 32767
#define UINT16_MAX 65535U

#define INT32_MIN (-2147483647L - 1)
#define INT32_MAX 2147483647L
#define UINT32_MAX 4294967295UL

#define INTPTR_MIN INT32_MIN
#define INTPTR_MAX INT32_MAX
#define UINTPTR_MAX UINT32_MAX

#define SIZE_MAX UINT32_MAX

/* NULL */
#ifndef NULL
#define NULL ((void *)0)
#endif

#endif /* _STDINT_H */

#ifndef SEGA_OS_H
#define SEGA_OS_H

#include <stdint.h>

/* Types */
typedef int16_t OSErr;
typedef uint32_t Handle; /* Pointer to Pointer */
typedef uint32_t Ptr;
typedef uint8_t Boolean;
typedef uint32_t Time; /* Ticks (1/60th sec) */

/* Error Codes */
#define noErr 0
#define memFullErr -108
#define nilHandleErr -109

/* Graphics Primitives */
typedef struct {
  int16_t top;
  int16_t left;
  int16_t bottom;
  int16_t right;
} Rect;

typedef struct {
  int16_t x;
  int16_t y;
} Point;

/* Event System */
/* Event Types */
#define nullEvent 0
#define mouseDown 1
#define mouseUp 2
#define keyDown 3
#define keyUp 4
#define autoKey 5
#define updateEvt 6
#define diskEvt 7
#define activateEvt 8

typedef struct {
  uint16_t what;
  uint32_t message;
  uint32_t when;
  Point where;
  uint16_t modifiers;
} EventRecord;

/* Window Manager - see wm.h for full API */
/* Forward declaration only - include wm.h for WindowRecord */

/* Sega Specific Hardware Control */
/* CD BIOS Calls */
extern void CD_PlayTrack(uint8_t track);
extern void CD_Stop(void);

/* VDP Direct Access (Kernel Mode Only) */
typedef struct {
  uint16_t vdp_ctrl_port;
  uint16_t vdp_data_port;
} VDP_Hardware;

#define VDP_BASE ((VDP_Hardware *)0xC00000)

/* Memory Manager */
/* PRG-RAM (Sub CPU) is 512KB total ($000000-$07FFFF)   */
/* First $6000 reserved for BIOS. Usable: 488KB         */
#define SYS_RAM_BASE 0x006000
#define SYS_RAM_SIZE 0x07A000 /* 488KB usable */

/* Main CPU Specific Memory */
#define MAIN_WRAM_BASE 0xFF0000
#define MAIN_WRAM_SIZE 0x010000 /* 64KB */

/* Word RAM (256KB total) - Shared/Swappable */
#define WORD_RAM_BASE 0x200000
#define WORD_RAM_SIZE 0x040000 /* 256KB (2 Mbit) */

/* Backup RAM Cart (Storage) */
#define BRAM_CART_BASE 0x600000
#define BRAM_CART_SIZE 0x020000 /* 128KB standard */

/* Base function prototypes */
void InitGraf(void);
void InitCursor(void);
void InitFonts(void);

Boolean GetNextEvent(uint16_t eventMask, EventRecord *theEvent);

#endif /* SEGA_OS_H */

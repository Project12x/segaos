/*
 * mem.h - Memory Manager for Genesis System 1 (Sub CPU)
 *
 * First-fit free list allocator with coalescing.
 * Operates on a contiguous heap region within PRG-RAM.
 *
 * All allocations are 4-byte aligned (68000 requirement).
 * Each block has an 8-byte header (size + flags).
 * Adjacent free blocks are coalesced on free().
 *
 * PRG-RAM layout (512KB):
 *   0x000000 - 0x00xxxx  Code + rodata + bss (linker-defined)
 *   0x00xxxx - 0x07F7FF  Heap (managed by this allocator)
 *   0x07F800 - 0x07FFFF  Stack (2KB, grows downward)
 *
 * The actual heap bounds are set by MEM_Init() using
 * linker-provided symbols (_heap_start, _heap_end).
 */

#ifndef MEM_H
#define MEM_H

#include <stdint.h>

/* Alignment for all allocations (must be power of 2) */
#define MEM_ALIGN 4

/* Minimum split threshold: don't split a block if the remainder
 * would be smaller than this (header + at least 8 usable bytes) */
#define MEM_MIN_SPLIT 16

/* ============================================================
 * Block Header
 * ============================================================
 * Stored immediately before each allocation.
 * |  size (28 bits) | flags (4 bits) |  -- 4 bytes
 * |  next free pointer               |  -- 4 bytes
 *
 * Total overhead: 8 bytes per block.
 * 'next' is only meaningful when the block is free.
 * ============================================================ */

/* Flag bits stored in the low 4 bits of the size field */
#define MEM_FLAG_USED 0x01

typedef struct BlockHeader {
  uint32_t sizeAndFlags;    /* Upper 28 bits = size, lower 4 = flags  */
  struct BlockHeader *next; /* Next free block (only valid when free) */
} BlockHeader;

/* Helper macros */
#define BLK_SIZE(b) ((b)->sizeAndFlags & ~0x0FU)
#define BLK_IS_USED(b) ((b)->sizeAndFlags & MEM_FLAG_USED)
#define BLK_IS_FREE(b) (!BLK_IS_USED(b))
#define BLK_SET_USED(b) ((b)->sizeAndFlags |= MEM_FLAG_USED)
#define BLK_SET_FREE(b) ((b)->sizeAndFlags &= ~MEM_FLAG_USED)
#define BLK_SET_SIZE(b, s)                                                     \
  ((b)->sizeAndFlags = ((s) & ~0x0FU) | ((b)->sizeAndFlags & 0x0FU))

/* Get pointer to user data from block header */
#define BLK_DATA(b) ((void *)((uint8_t *)(b) + sizeof(BlockHeader)))

/* Get block header from user data pointer */
#define BLK_FROM_DATA(p) ((BlockHeader *)((uint8_t *)(p) - sizeof(BlockHeader)))

/* Get next adjacent block in memory (by adding size) */
#define BLK_NEXT_ADJ(b) ((BlockHeader *)((uint8_t *)(b) + BLK_SIZE(b)))

/* ============================================================
 * Heap Statistics
 * ============================================================ */
typedef struct {
  uint32_t heapSize;    /* Total heap size in bytes      */
  uint32_t usedBytes;   /* Bytes currently allocated      */
  uint32_t freeBytes;   /* Bytes currently free           */
  uint16_t usedBlocks;  /* Number of allocated blocks     */
  uint16_t freeBlocks;  /* Number of free blocks          */
  uint32_t largestFree; /* Largest contiguous free block  */
  uint32_t totalAllocs; /* Lifetime allocation count      */
  uint32_t totalFrees;  /* Lifetime free count            */
} MemStats;

/* ============================================================
 * Public API
 * ============================================================ */

/*
 * Initialize the heap allocator.
 * heapStart: pointer to first usable byte of heap
 * heapEnd:   pointer to one past last usable byte
 * Returns 0 on success, -1 on error (bad params, too small)
 */
int8_t MEM_Init(void *heapStart, void *heapEnd);

/*
 * Allocate 'size' bytes. Returns NULL on failure.
 * Returned pointer is 4-byte aligned.
 */
void *MEM_Alloc(uint32_t size);

/*
 * Allocate and zero 'count * size' bytes. Returns NULL on failure.
 */
void *MEM_AllocZero(uint32_t count, uint32_t size);

/*
 * Free a previously allocated block. ptr may be NULL (no-op).
 * Coalesces with adjacent free blocks.
 */
void MEM_Free(void *ptr);

/*
 * Resize an allocation. Returns NULL on failure (original
 * block is NOT freed on failure). ptr may be NULL (acts as alloc).
 * If newSize is 0, frees ptr and returns NULL.
 */
void *MEM_Realloc(void *ptr, uint32_t newSize);

/*
 * Get heap statistics. Walks the entire heap.
 */
void MEM_GetStats(MemStats *stats);

/*
 * Validate heap integrity. Returns 0 if OK, -1 if corrupt.
 * Walks all blocks checking size chains and free list consistency.
 */
int8_t MEM_Validate(void);

/*
 * Get total free bytes (fast - maintained incrementally).
 */
uint32_t MEM_GetFreeBytes(void);

#endif /* MEM_H */

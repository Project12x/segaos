/*
 * mem.c - First-Fit Free List Memory Allocator
 *
 * Manages a contiguous heap region in PRG-RAM.
 * Adjacent free blocks are coalesced on every free().
 * All allocations are 4-byte aligned.
 */

#include "mem.h"
#include <string.h> /* memset, memcpy */

/* ============================================================
 * Heap State
 * ============================================================ */
static struct {
  BlockHeader *freeList; /* Head of the free list           */
  uint8_t *heapStart;    /* First byte of heap              */
  uint8_t *heapEnd;      /* One past last byte of heap      */
  uint32_t heapSize;     /* Total heap size                 */
  uint32_t freeBytes;    /* Current free bytes (fast query) */
  uint32_t totalAllocs;
  uint32_t totalFrees;
  uint8_t initialized;
} heap;

/* ============================================================
 * Internal: Align size up to MEM_ALIGN boundary
 * ============================================================ */
static uint32_t align_up(uint32_t size) {
  return (size + (MEM_ALIGN - 1)) & ~(MEM_ALIGN - 1);
}

/* ============================================================
 * MEM_Init
 * ============================================================ */
int8_t MEM_Init(void *heapStart, void *heapEnd) {
  uint32_t totalSize;
  BlockHeader *first;

  if (!heapStart || !heapEnd)
    return -1;

  /* Align start up, end down */
  heap.heapStart =
      (uint8_t *)(((uint32_t)heapStart + (MEM_ALIGN - 1)) & ~(MEM_ALIGN - 1));
  heap.heapEnd = (uint8_t *)((uint32_t)heapEnd & ~(MEM_ALIGN - 1));

  if (heap.heapEnd <= heap.heapStart)
    return -1;

  totalSize = (uint32_t)(heap.heapEnd - heap.heapStart);

  /* Need at least one header + MEM_MIN_SPLIT usable bytes */
  if (totalSize < sizeof(BlockHeader) + MEM_MIN_SPLIT)
    return -1;

  heap.heapSize = totalSize;

  /* Create one big free block spanning the entire heap */
  first = (BlockHeader *)heap.heapStart;
  first->sizeAndFlags = totalSize; /* All flag bits clear = free */
  first->next = (BlockHeader *)0;

  heap.freeList = first;
  heap.freeBytes = totalSize - sizeof(BlockHeader);
  heap.totalAllocs = 0;
  heap.totalFrees = 0;
  heap.initialized = 1;

  return 0;
}

/* ============================================================
 * MEM_Alloc
 * ============================================================ */
void *MEM_Alloc(uint32_t size) {
  BlockHeader *curr, *prev;
  uint32_t needed;

  if (!heap.initialized || size == 0)
    return (void *)0;

  /* Align requested size, add header overhead */
  needed = align_up(size) + sizeof(BlockHeader);
  if (needed < sizeof(BlockHeader) + MEM_ALIGN)
    needed = sizeof(BlockHeader) + MEM_ALIGN;

  /* First-fit: walk the free list */
  prev = (BlockHeader *)0;
  curr = heap.freeList;

  while (curr) {
    uint32_t blockSize = BLK_SIZE(curr);

    if (blockSize >= needed) {
      /* Check if we can split this block */
      uint32_t remainder = blockSize - needed;

      if (remainder >= sizeof(BlockHeader) + MEM_MIN_SPLIT) {
        /* Split: create a new free block after the allocation */
        BlockHeader *newFree = (BlockHeader *)((uint8_t *)curr + needed);
        newFree->sizeAndFlags = remainder; /* flags = 0 = free */
        newFree->next = curr->next;

        /* Update current block to allocated size */
        curr->sizeAndFlags = needed;
        BLK_SET_USED(curr);
        curr->next = (BlockHeader *)0;

        /* Replace curr with newFree in free list */
        if (prev)
          prev->next = newFree;
        else
          heap.freeList = newFree;

        heap.freeBytes -= needed;
      } else {
        /* Use entire block (no split) */
        BLK_SET_USED(curr);

        /* Remove from free list */
        if (prev)
          prev->next = curr->next;
        else
          heap.freeList = curr->next;
        curr->next = (BlockHeader *)0;

        heap.freeBytes -= blockSize;
      }

      heap.totalAllocs++;
      return BLK_DATA(curr);
    }

    prev = curr;
    curr = curr->next;
  }

  /* No suitable block found */
  return (void *)0;
}

/* ============================================================
 * MEM_AllocZero
 * ============================================================ */
void *MEM_AllocZero(uint32_t count, uint32_t size) {
  uint32_t total = count * size;
  void *ptr;

  /* Overflow check */
  if (count != 0 && total / count != size)
    return (void *)0;

  ptr = MEM_Alloc(total);
  if (ptr)
    memset(ptr, 0, total);

  return ptr;
}

/* ============================================================
 * MEM_Free
 * ============================================================ */
void MEM_Free(void *ptr) {
  BlockHeader *block, *curr, *prev;
  BlockHeader *adjNext;
  uint8_t *blockEnd;

  if (!ptr || !heap.initialized)
    return;

  block = BLK_FROM_DATA(ptr);

  /* Sanity: is this block within the heap and marked used? */
  if ((uint8_t *)block < heap.heapStart || (uint8_t *)block >= heap.heapEnd ||
      BLK_IS_FREE(block)) {
    return; /* Bad pointer or double-free */
  }

  /* Mark as free */
  BLK_SET_FREE(block);
  heap.freeBytes += BLK_SIZE(block);
  heap.totalFrees++;

  /* Insert into free list in address order (needed for coalescing).
   * Walk the list to find the correct insertion point. */
  prev = (BlockHeader *)0;
  curr = heap.freeList;

  while (curr && curr < block) {
    prev = curr;
    curr = curr->next;
  }

  /* Insert block between prev and curr */
  block->next = curr;
  if (prev)
    prev->next = block;
  else
    heap.freeList = block;

  /* Coalesce with next adjacent block if it's free */
  blockEnd = (uint8_t *)block + BLK_SIZE(block);
  if (blockEnd < heap.heapEnd) {
    adjNext = (BlockHeader *)blockEnd;
    if (BLK_IS_FREE(adjNext) && adjNext == curr) {
      /* Merge block with curr */
      BLK_SET_SIZE(block, BLK_SIZE(block) + BLK_SIZE(adjNext));
      block->next = adjNext->next;
      heap.freeBytes += sizeof(BlockHeader); /* Reclaim header */
    }
  }

  /* Coalesce with previous adjacent block if it's free */
  if (prev) {
    uint8_t *prevEnd = (uint8_t *)prev + BLK_SIZE(prev);
    if (prevEnd == (uint8_t *)block) {
      /* Merge prev with block */
      BLK_SET_SIZE(prev, BLK_SIZE(prev) + BLK_SIZE(block));
      prev->next = block->next;
      heap.freeBytes += sizeof(BlockHeader); /* Reclaim header */
    }
  }
}

/* ============================================================
 * MEM_Realloc
 * ============================================================ */
void *MEM_Realloc(void *ptr, uint32_t newSize) {
  BlockHeader *block;
  uint32_t oldDataSize;
  void *newPtr;
  uint32_t copySize;

  if (!ptr)
    return MEM_Alloc(newSize);

  if (newSize == 0) {
    MEM_Free(ptr);
    return (void *)0;
  }

  block = BLK_FROM_DATA(ptr);
  oldDataSize = BLK_SIZE(block) - sizeof(BlockHeader);

  /* If the block is already big enough, keep it */
  if (oldDataSize >= newSize)
    return ptr;

  /* Try to expand into adjacent free block */
  {
    uint8_t *blockEnd = (uint8_t *)block + BLK_SIZE(block);
    if (blockEnd < heap.heapEnd) {
      BlockHeader *adjNext = (BlockHeader *)blockEnd;
      if (BLK_IS_FREE(adjNext)) {
        uint32_t combined = BLK_SIZE(block) + BLK_SIZE(adjNext);
        uint32_t needed = align_up(newSize) + sizeof(BlockHeader);

        if (combined >= needed) {
          /* Remove adjNext from free list */
          BlockHeader *fprev = (BlockHeader *)0;
          BlockHeader *fcurr = heap.freeList;
          while (fcurr && fcurr != adjNext) {
            fprev = fcurr;
            fcurr = fcurr->next;
          }
          if (fcurr == adjNext) {
            if (fprev)
              fprev->next = adjNext->next;
            else
              heap.freeList = adjNext->next;
          }

          heap.freeBytes -= BLK_SIZE(adjNext);

          /* Check if we should split the combined block */
          uint32_t remainder = combined - needed;
          if (remainder >= sizeof(BlockHeader) + MEM_MIN_SPLIT) {
            BLK_SET_SIZE(block, needed);
            BlockHeader *split = (BlockHeader *)((uint8_t *)block + needed);
            split->sizeAndFlags = remainder;
            split->next = (BlockHeader *)0;

            /* Insert split block into free list */
            BlockHeader *sp = (BlockHeader *)0;
            BlockHeader *sc = heap.freeList;
            while (sc && sc < split) {
              sp = sc;
              sc = sc->next;
            }
            split->next = sc;
            if (sp)
              sp->next = split;
            else
              heap.freeList = split;

            heap.freeBytes += remainder;
          } else {
            BLK_SET_SIZE(block, combined);
          }

          return ptr; /* Same address, block is now bigger */
        }
      }
    }
  }

  /* Fall back: allocate new, copy, free old */
  newPtr = MEM_Alloc(newSize);
  if (!newPtr)
    return (void *)0;

  copySize = oldDataSize < newSize ? oldDataSize : newSize;
  memcpy(newPtr, ptr, copySize);
  MEM_Free(ptr);

  return newPtr;
}

/* ============================================================
 * MEM_GetStats
 * ============================================================ */
void MEM_GetStats(MemStats *stats) {
  BlockHeader *blk;

  if (!stats)
    return;

  memset(stats, 0, sizeof(MemStats));
  stats->heapSize = heap.heapSize;
  stats->totalAllocs = heap.totalAllocs;
  stats->totalFrees = heap.totalFrees;

  if (!heap.initialized)
    return;

  /* Walk all blocks linearly through memory */
  blk = (BlockHeader *)heap.heapStart;
  while ((uint8_t *)blk < heap.heapEnd) {
    uint32_t size = BLK_SIZE(blk);
    if (size == 0)
      break; /* Corrupt - prevent infinite loop */

    if (BLK_IS_USED(blk)) {
      stats->usedBlocks++;
      stats->usedBytes += size;
    } else {
      uint32_t dataSize = size - sizeof(BlockHeader);
      stats->freeBlocks++;
      stats->freeBytes += size;
      if (dataSize > stats->largestFree)
        stats->largestFree = dataSize;
    }

    blk = (BlockHeader *)((uint8_t *)blk + size);
  }
}

/* ============================================================
 * MEM_Validate
 * ============================================================ */
int8_t MEM_Validate(void) {
  BlockHeader *blk;
  uint32_t totalSize = 0;
  uint16_t freeCount = 0;

  if (!heap.initialized)
    return -1;

  /* Walk all blocks checking size chains */
  blk = (BlockHeader *)heap.heapStart;
  while ((uint8_t *)blk < heap.heapEnd) {
    uint32_t size = BLK_SIZE(blk);

    /* Size must be non-zero and aligned */
    if (size == 0 || (size & (MEM_ALIGN - 1)) != 0)
      return -1;

    /* Block must not extend past heap end */
    if ((uint8_t *)blk + size > heap.heapEnd)
      return -1;

    /* Size must include at least the header */
    if (size < sizeof(BlockHeader))
      return -1;

    if (BLK_IS_FREE(blk))
      freeCount++;

    totalSize += size;
    blk = (BlockHeader *)((uint8_t *)blk + size);
  }

  /* Total sizes must equal heap size */
  if (totalSize != heap.heapSize)
    return -1;

  /* Verify free list: each entry must be within heap and marked free */
  blk = heap.freeList;
  while (blk) {
    if ((uint8_t *)blk < heap.heapStart || (uint8_t *)blk >= heap.heapEnd)
      return -1;

    if (BLK_IS_USED(blk))
      return -1;

    freeCount--;
    blk = blk->next;
  }

  /* Free list count must match linear walk count */
  if (freeCount != 0)
    return -1;

  return 0; /* Heap is valid */
}

/* ============================================================
 * MEM_GetFreeBytes
 * ============================================================ */
uint32_t MEM_GetFreeBytes(void) { return heap.freeBytes; }

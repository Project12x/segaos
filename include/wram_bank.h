/*
 * wram_bank.h - Main-side Sega CD Word RAM bank selection helpers.
 *
 * Reference: drojaazu/megadev@7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef
 * (MIT), lib/main/gate_arr.def.h. Reuse mode: pattern-only / clean-room.
 */

#ifndef WRAM_BANK_H
#define WRAM_BANK_H

#include "ga_regs.h"
#include <stdint.h>

/* Word RAM bank addresses as seen by Main CPU in 1M mode. */
#define WRAM_BANK0_MAIN ((const uint8_t *)0x200000)
#define WRAM_BANK1_MAIN ((const uint8_t *)0x220000)

#define WRAM_MAIN_BANK0_INDEX 0U
#define WRAM_MAIN_BANK1_INDEX 1U

static inline uint8_t WRAM_MainBankIndexFromMemMode(uint8_t memModeLow) {
  if ((memModeLow & MEM_MODE_1M) == 0)
    return WRAM_MAIN_BANK0_INDEX;

  return (memModeLow & MEM_MODE_RET) ? WRAM_MAIN_BANK1_INDEX
                                     : WRAM_MAIN_BANK0_INDEX;
}

static inline const uint8_t *WRAM_MainBankFromIndex(uint8_t bankIndex) {
  return bankIndex == WRAM_MAIN_BANK1_INDEX ? WRAM_BANK1_MAIN : WRAM_BANK0_MAIN;
}

static inline const uint8_t *WRAM_MainBankFromMemMode(uint8_t memModeLow) {
  return WRAM_MainBankFromIndex(WRAM_MainBankIndexFromMemMode(memModeLow));
}

#endif /* WRAM_BANK_H */

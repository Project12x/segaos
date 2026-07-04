#include "wram_bank.h"

#include <stdio.h>
#include <stdint.h>

static int failures;

static void expect_u8(uint8_t actual, uint8_t expected, const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected %u got %u\n", name, expected, actual);
    failures++;
  }
}

static void expect_ptr(const uint8_t *actual, const uint8_t *expected,
                       const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected %p got %p\n", name, (const void *)expected,
           (const void *)actual);
    failures++;
  }
}

int main(void) {
  expect_u8(WRAM_MainBankIndexFromMemMode(MEM_MODE_1M),
            WRAM_MAIN_BANK0_INDEX, "1M RET clear selects bank 0");
  expect_u8(WRAM_MainBankIndexFromMemMode(MEM_MODE_1M | MEM_MODE_RET),
            WRAM_MAIN_BANK1_INDEX, "1M RET set selects bank 1");
  expect_u8(WRAM_MainBankIndexFromMemMode(MEM_MODE_1M | MEM_MODE_DMNA),
            WRAM_MAIN_BANK0_INDEX, "1M DMNA does not change bank index");
  expect_u8(WRAM_MainBankIndexFromMemMode(MEM_MODE_1M | MEM_MODE_DMNA |
                                          MEM_MODE_RET),
            WRAM_MAIN_BANK1_INDEX, "1M DMNA with RET set selects bank 1");
  expect_u8(WRAM_MainBankIndexFromMemMode(MEM_MODE_RET),
            WRAM_MAIN_BANK0_INDEX, "2M mode uses bank 0 alias");

  expect_ptr(WRAM_MainBankFromIndex(WRAM_MAIN_BANK0_INDEX), WRAM_BANK0_MAIN,
             "bank 0 pointer");
  expect_ptr(WRAM_MainBankFromIndex(WRAM_MAIN_BANK1_INDEX), WRAM_BANK1_MAIN,
             "bank 1 pointer");
  expect_ptr(WRAM_MainBankFromMemMode(MEM_MODE_1M | MEM_MODE_RET),
             WRAM_BANK1_MAIN, "mem mode to bank 1 pointer");

  if (failures) {
    printf("wram bank tests failed: %d\n", failures);
    return 1;
  }

  printf("wram bank tests passed\n");
  return 0;
}

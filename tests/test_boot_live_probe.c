#include "boot_live_probe.h"

#include <stdio.h>
#include <stdint.h>

static int failures;

static void expect_u8(uint8_t actual, uint8_t expected, const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected %u got %u\n", name, expected, actual);
    failures++;
  }
}

static void expect_u16(uint16_t actual, uint16_t expected, const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected %u got %u\n", name, expected, actual);
    failures++;
  }
}

static void expect_uintptr(uintptr_t actual, uintptr_t expected,
                           const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected 0x%lx got 0x%lx\n", name,
           (unsigned long)expected, (unsigned long)actual);
    failures++;
  }
}

int main(void) {
  expect_u8(BootLiveProbe_ShouldRender(0, 4), 1, "render first frame");
  expect_u8(BootLiveProbe_ShouldRender(3, 4), 1, "render final frame");
  expect_u8(BootLiveProbe_ShouldRender(4, 4), 0, "stop at target");
  expect_u8(BootLiveProbe_ShouldRender(0, 0), 0, "zero target disables");
  expect_u16(BootLiveProbe_NextFrame(0), 1, "first marker");
  expect_u16(BootLiveProbe_NextFrame(3), 4, "fourth marker");
  expect_u16(BootLiveProbe_FrameSentinelWord(4), 0x4444,
             "frame 4 sentinel word");
  expect_uintptr(BootLiveProbe_FrameSentinelOffset(), 34560UL,
                 "frame sentinel offset");
  expect_u8(BootLiveProbe_SelectFrameBank(0x1111, 0x4444, 4), 1,
            "select bank 1 for frame 4");
  expect_u8(BootLiveProbe_SelectFrameBank(0x2222, 0x1111, 2), 0,
            "select bank 0 for frame 2");
  expect_u8(BootLiveProbe_SelectFrameBank(0x1111, 0x2222, 3), 0xff,
            "reject missing frame sentinel");

  if (failures) {
    printf("boot live probe tests failed: %d\n", failures);
    return 1;
  }

  return 0;
}

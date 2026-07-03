#include "blitter.h"
#include "boot_live_probe.h"

#include <stdio.h>
#include <string.h>

static uint8_t framebuffer[BLT_FRAMEBUF_SIZE_4];

static int expect_u8(uint8_t actual, uint8_t expected, const char *name) {
  if (actual != expected) {
    fprintf(stderr, "%s expected 0x%02x got 0x%02x\n", name, expected, actual);
    return 1;
  }
  return 0;
}

int main(void) {
  Rect screen;
  Rect marker;
  uintptr_t offset;
  int failures = 0;

  memset(framebuffer, 0, sizeof(framebuffer));
  BLT_Init(framebuffer);
  BLT_SetMode(BLT_MODE_4BIT);

  screen.left = 0;
  screen.top = 0;
  screen.right = BLT_SCREEN_W;
  screen.bottom = BLT_SCREEN_H;
  BLT_SetClipRect(&screen);

  marker.left = BOOT_LIVE_PROBE_SENTINEL_X;
  marker.top = BOOT_LIVE_PROBE_SENTINEL_Y;
  marker.right = marker.left + BOOT_LIVE_PROBE_SENTINEL_W;
  marker.bottom = marker.top + BOOT_LIVE_PROBE_SENTINEL_H;
  BLT_FillRect(&marker, 4);

  offset = BootLiveProbe_FrameSentinelOffset();
  failures += expect_u8(framebuffer[offset], 0x44, "sentinel byte 0");
  failures += expect_u8(framebuffer[offset + 1U], 0x44, "sentinel byte 1");

  return failures ? 1 : 0;
}

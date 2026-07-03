#ifndef BOOT_LIVE_PROBE_H
#define BOOT_LIVE_PROBE_H

#include <stdint.h>

#define BOOT_LIVE_PROBE_SENTINEL_X 0U
#define BOOT_LIVE_PROBE_SENTINEL_Y 216U
#define BOOT_LIVE_PROBE_SENTINEL_W 16U
#define BOOT_LIVE_PROBE_SENTINEL_H 8U
#define BOOT_LIVE_PROBE_LINEAR_BPR 160U
#define BOOT_LIVE_PROBE_BANK_MISSING 0xffU

static inline uint8_t BootLiveProbe_ShouldRender(uint16_t completedFrames,
                                                 uint16_t targetFrames) {
  return (targetFrames != 0U && completedFrames < targetFrames) ? 1U : 0U;
}

static inline uint16_t BootLiveProbe_NextFrame(uint16_t completedFrames) {
  return (uint16_t)(completedFrames + 1U);
}

static inline uint16_t BootLiveProbe_FrameSentinelWord(uint16_t frame) {
  uint16_t nibble = (uint16_t)(frame & 0x000fU);

  return (uint16_t)((nibble << 12) | (nibble << 8) | (nibble << 4) | nibble);
}

static inline uintptr_t BootLiveProbe_FrameSentinelOffset(void) {
  return (uintptr_t)((BOOT_LIVE_PROBE_SENTINEL_Y *
                      BOOT_LIVE_PROBE_LINEAR_BPR) +
                     (BOOT_LIVE_PROBE_SENTINEL_X / 2U));
}

static inline uint8_t BootLiveProbe_SelectFrameBank(uint16_t bank0Sentinel,
                                                    uint16_t bank1Sentinel,
                                                    uint16_t frame) {
  uint16_t expected = BootLiveProbe_FrameSentinelWord(frame);

  if (bank0Sentinel == expected)
    return 0;
  if (bank1Sentinel == expected)
    return 1;
  return BOOT_LIVE_PROBE_BANK_MISSING;
}

#endif

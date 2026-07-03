#ifndef BOOT_FRAME_MARKER_H
#define BOOT_FRAME_MARKER_H

#include <stdint.h>

#define BOOT_FRAME_MARKER_LEN 8

static inline void BootFrameMarker_Format(uint16_t frame,
                                          char out[BOOT_FRAME_MARKER_LEN]) {
  uint16_t digit = (uint16_t)(frame % 10U);

  out[0] = 'F';
  out[1] = 'r';
  out[2] = 'a';
  out[3] = 'm';
  out[4] = 'e';
  out[5] = ' ';
  out[6] = (char)('0' + digit);
  out[7] = '\0';
}

#endif

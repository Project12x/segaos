#include "boot_frame_marker.h"

#include <stdio.h>
#include <string.h>

static int expect_marker(uint16_t frame, const char *expected) {
  char marker[BOOT_FRAME_MARKER_LEN];

  BootFrameMarker_Format(frame, marker);
  if (strcmp(marker, expected) != 0) {
    fprintf(stderr, "frame %u expected '%s' got '%s'\n", frame, expected,
            marker);
    return 1;
  }

  return 0;
}

int main(void) {
  if (expect_marker(1, "Frame 1") != 0)
    return 1;
  if (expect_marker(4, "Frame 4") != 0)
    return 1;
  if (expect_marker(10, "Frame 0") != 0)
    return 1;

  return 0;
}

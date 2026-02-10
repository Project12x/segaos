/*
 * mouse.c - Sega Mega Mouse Driver for Genesis System 1
 *
 * Runs on the Main CPU. Polls the Mega Mouse via the Genesis
 * controller port using the TH/TR/TL nibble handshake protocol.
 *
 * Call Mouse_Init() once at boot, then Mouse_Poll() each VBlank.
 */

#include "mouse.h"

/* ============================================================
 * State
 * ============================================================ */
static MouseState mouse;
static volatile uint8_t *dataPort; /* Pointer to active data port  */
static volatile uint8_t *ctrlPort; /* Pointer to active ctrl port  */
static int16_t boundsMinX, boundsMinY;
static int16_t boundsMaxX, boundsMaxY;

/* ============================================================
 * Internal: Wait for TL line to reach expected state
 *
 * TL is bit 4 of the data port read.
 * We spin until TL matches, with a timeout to avoid hanging
 * if no mouse is connected.
 * ============================================================ */
static uint8_t wait_tl(uint8_t expected) {
  uint16_t timeout = 4000;
  uint8_t tlBit = expected ? 0x10 : 0x00;

  while (timeout--) {
    if ((*dataPort & 0x10) == tlBit) {
      return 1; /* TL matched */
    }
  }
  return 0; /* Timeout: mouse not responding */
}

/* ============================================================
 * Internal: Read one nibble from the mouse
 *
 * Protocol:
 *   1. Write phase value ($20 or $00) to toggle TR
 *   2. Wait for TL to match expected state
 *   3. Read D3-D0 from data port (lower 4 bits)
 *
 * Phase alternates: $20 (TR=1), $00 (TR=0), $20, $00, ...
 * TL expected:       1 (high),   0 (low),    1,    0, ...
 * ============================================================ */
static uint8_t read_nibble(uint8_t phase) {
  uint8_t tlExpect = (phase & 0x20) ? 1 : 0;

  *dataPort = phase;

  if (!wait_tl(tlExpect)) {
    return 0xFF; /* Timeout sentinel */
  }

  return *dataPort & 0x0F;
}

/* ============================================================
 * Public API
 * ============================================================ */

void Mouse_Init(uint8_t port) {
  /* Clear state */
  mouse.x = 160; /* Center of 320px screen */
  mouse.y = 112; /* Center of 224px screen */
  mouse.dx = 0;
  mouse.dy = 0;
  mouse.buttons = 0;
  mouse.prevButtons = 0;
  mouse.connected = 0;
  mouse.overflow = 0;

  /* Set default bounds to screen */
  boundsMinX = 0;
  boundsMinY = 0;
  boundsMaxX = 319;
  boundsMaxY = 223;

  /* Select controller port */
  if (port == 2) {
    dataPort = &IO_DATA_2;
    ctrlPort = &IO_CTRL_2;
  } else {
    dataPort = &IO_DATA_1;
    ctrlPort = &IO_CTRL_1;
  }

  /* Configure port: TH and TR as outputs (bits 6,5) */
  *ctrlPort = IO_CTRL_MOUSE;

  /* Initial idle state: TH=1, TR=1 */
  *dataPort = 0x60;
}

uint8_t Mouse_Poll(void) {
  uint8_t nibbles[9];
  uint8_t i;
  uint8_t phase;
  int16_t rawX, rawY;
  uint8_t signX, signY;
  uint8_t overflowBits;

  /* Save previous button state for edge detection */
  mouse.prevButtons = mouse.buttons;
  mouse.dx = 0;
  mouse.dy = 0;
  mouse.overflow = 0;

  /* --- Begin mouse data transfer --- */

  /* Step 1: Request data (TH=1, TR=1) */
  *dataPort = 0x60;

  /* Small delay for mouse to prepare */
  for (i = 0; i < 10; i++) {
    __asm__ volatile("nop");
  }

  /* Step 2: Read 9 nibbles, alternating phase */
  phase = 0x20; /* Start with TR=1, TH=0 */
  for (i = 0; i < 9; i++) {
    nibbles[i] = read_nibble(phase);

    /* Check for timeout (mouse not connected) */
    if (nibbles[i] == 0xFF) {
      mouse.connected = 0;
      *dataPort = 0x60; /* Return to idle */
      return 0;
    }

    /* Alternate phase: $20 -> $00 -> $20 -> $00 ... */
    phase ^= 0x20;
  }

  /* Step 3: End transfer (TH=1, TR=1) */
  *dataPort = 0x60;

  /* --- Parse nibbles --- */

  /* Nibble 0: Mouse ID (should be $0 for Mega Mouse) */
  if (nibbles[0] != 0x00) {
    /* Not a Mega Mouse (might be a gamepad) */
    mouse.connected = 0;
    return 0;
  }

  mouse.connected = 1;

  /* Nibble 1: Overflow flags */
  overflowBits = nibbles[1];
  mouse.overflow = (overflowBits & 0x03) ? 1 : 0;
  /* bit 0 = X overflow, bit 1 = Y overflow */

  /* Nibble 2: Sign bits */
  signX = (nibbles[2] & 0x01); /* 1 = negative X */
  signY = (nibbles[2] & 0x02); /* 1 = negative Y (bit 1) */

  /* Nibble 3: Button states (active when bit is 0) */
  /* Hardware reports 1=released, 0=pressed. We invert. */
  mouse.buttons = (~nibbles[3]) & 0x0F;

  /* Nibbles 4-5: X movement (high nibble, low nibble) */
  rawX = (int16_t)((nibbles[4] << 4) | nibbles[5]);

  /* Nibbles 6-7: Y movement (high nibble, low nibble) */
  rawY = (int16_t)((nibbles[6] << 4) | nibbles[7]);

  /* Apply sign */
  if (signX)
    rawX = -rawX;
  if (signY)
    rawY = -rawY;

  /* Clamp overflow */
  if (overflowBits & 0x01)
    rawX = signX ? -255 : 255;
  if (overflowBits & 0x02)
    rawY = signY ? -255 : 255;

  /* Store deltas */
  mouse.dx = rawX;
  mouse.dy = rawY;

  /* Accumulate into absolute position */
  mouse.x += rawX;
  mouse.y += rawY;

  /* Clamp to bounds */
  if (mouse.x < boundsMinX)
    mouse.x = boundsMinX;
  if (mouse.x > boundsMaxX)
    mouse.x = boundsMaxX;
  if (mouse.y < boundsMinY)
    mouse.y = boundsMinY;
  if (mouse.y > boundsMaxY)
    mouse.y = boundsMaxY;

  /* Nibble 8: Checksum (ignored, transfer already complete) */

  return 1;
}

const MouseState *Mouse_GetState(void) { return &mouse; }

uint8_t Mouse_ButtonPressed(uint8_t btn) {
  return (mouse.buttons & btn) && !(mouse.prevButtons & btn);
}

uint8_t Mouse_ButtonReleased(uint8_t btn) {
  return !(mouse.buttons & btn) && (mouse.prevButtons & btn);
}

void Mouse_SetPosition(int16_t x, int16_t y) {
  mouse.x = x;
  mouse.y = y;

  /* Clamp to bounds */
  if (mouse.x < boundsMinX)
    mouse.x = boundsMinX;
  if (mouse.x > boundsMaxX)
    mouse.x = boundsMaxX;
  if (mouse.y < boundsMinY)
    mouse.y = boundsMinY;
  if (mouse.y > boundsMaxY)
    mouse.y = boundsMaxY;
}

void Mouse_SetBounds(int16_t minX, int16_t minY, int16_t maxX, int16_t maxY) {
  boundsMinX = minX;
  boundsMinY = minY;
  boundsMaxX = maxX;
  boundsMaxY = maxY;

  /* Re-clamp current position */
  Mouse_SetPosition(mouse.x, mouse.y);
}

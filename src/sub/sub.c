/*
 * sub.c - Sub CPU Entry Point (Sega CD 68000 @ 12.5MHz)
 *
 * This is the OS kernel entry point. The BIOS loads this into PRG-RAM
 * at $006000 and jumps here after initialization.
 *
 * The Sub CPU owns:
 *   - 512KB PRG-RAM (program + data)
 *   - Word RAM bank (in 1M mode, swappable with Main CPU)
 *   - CD-ROM drive (via BIOS calls)
 *   - ASIC graphics processor
 *   - Internal Backup RAM (8KB)
 */

#include "boot_probe.h"
#include "common.h"
#if defined(DESKTOP_PUMP_PROBE) || defined(BOOT_SAFE_LIVE_PROBE)
#include "boot_frame_marker.h"
#endif
#ifdef BOOT_SAFE_LIVE_PROBE
#include "boot_live_probe.h"
#endif
#ifndef BOOT_PROBE
#ifdef BASIC_BRAM_PROBE
#include "basic_bram_smoke.h"
#include "bram.h"
#endif
#include "blitter.h"
#ifndef BOOT_SAFE_DESKTOP
#include "calc.h"
#endif
#include "input.h"
#include "mem.h"
#ifndef BOOT_SAFE_DESKTOP
#include "menubar.h"
#include "notepad.h"
#include "paint.h"
#endif
#include "sega_os.h"
#include "sysfont.h"
#ifndef BOOT_SAFE_DESKTOP
#include "vkbd.h"
#endif
#include "wm.h"
#endif

#ifndef BOOT_PROBE
/* Cursor state (tracks mouse position for drawing) */
static int16_t cursorX = 160;
static int16_t cursorY = 112;
static int16_t prevCursorX = 160;
static int16_t prevCursorY = 112;

/* Drag state */
static Window *dragWindow = (Window *)0;
static int16_t dragOffsetX = 0;
static int16_t dragOffsetY = 0;

#ifndef BOOT_SAFE_DESKTOP
/* Counter for auto-naming windows */
static uint8_t windowCounter = 0;
#endif

#ifndef BOOT_SAFE_DESKTOP
/* 1-bit cursor bitmap (11x16 pixels, classic Mac arrow) */
static const uint8_t cursorBitmap[] = {
    0xC0, 0x00, /* 11...... ........ */
    0xA0, 0x00, /* 1.1..... ........ */
    0x90, 0x00, /* 1..1.... ........ */
    0x88, 0x00, /* 1...1... ........ */
    0x84, 0x00, /* 1....1.. ........ */
    0x82, 0x00, /* 1.....1. ........ */
    0x81, 0x00, /* 1......1 ........ */
    0x80, 0x80, /* 1....... .1...... */
    0x80, 0x40, /* 1....... ..1..... */
    0x83, 0xC0, /* 1.....11 11...... */
    0x92, 0x00, /* 1..1..1. ........ */
    0xA2, 0x00, /* 1.1...1. ........ */
    0xC1, 0x00, /* 11....1. ........ */
    0x01, 0x00, /* ......1. ........ */
    0x00, 0x80, /* .......  1....... */
    0x00, 0x00, /* ........ ........ */
};
#endif

#ifdef BASIC_BRAM_PROBE
static BramBiosContext basicBramProbeContext;
static BramBiosOps basicBramProbeOps;
static BasicBramSmokeResult basicBramProbeResult;
#endif
#if defined(BOOT_SAFE_DESKTOP) &&                                             \
    (defined(DESKTOP_PUMP_PROBE) || defined(BOOT_SAFE_LIVE_PROBE))
static uint16_t bootFrameMarkerIndex;
#endif
#endif

/* Forward declarations */
#ifndef BOOT_PROBE
static void os_init(void);
#ifdef BOOT_SAFE_DESKTOP
static void render_boot_safe_desktop(void) __attribute__((noinline));
#endif
#endif
static void process_command(uint8_t cmd);

#ifdef BOOT_PROBE
volatile uint16_t segaos_probe_sub_magic;
volatile uint16_t segaos_probe_sub_phase;
volatile uint16_t segaos_probe_sub_cmd;
volatile uint16_t segaos_probe_sub_param0;
volatile uint16_t segaos_probe_sub_param1;
volatile uint16_t segaos_probe_sub_wram_word0;
volatile uint16_t segaos_probe_sub_wram_word1;
volatile uint16_t segaos_probe_sub_had_wram_before;
volatile uint16_t segaos_probe_sub_had_wram_after;
#endif

/*
 * sub_init - Called by crt0.s sp_init during BIOS initialization
 *
 * Performs one-time OS setup. BSS is already cleared by crt0.
 */
void sub_init(void) {
#ifdef BOOT_PROBE
  segaos_probe_sub_magic = PROBE_SUB_MAGIC;
  segaos_probe_sub_phase = PROBE_PHASE_SUB_READY;
  sub_write_result(0, SUB_STATE_READY);
  sub_write_result(1, PROBE_READY_MAGIC);
  GA_SUB_SET_FLAG(STATUS_IDLE);
#else
#ifdef BOOT_SAFE_DESKTOP
  BLT_Init((uint8_t *)0x0C0000);
  BLT_SetMode(BLT_MODE_4BIT);
#endif
  /* Do not publish READY here. Main must wait until sub_main reaches the
   * command loop so command flags cannot race BIOS/usercall startup. */
#endif
}

/*
 * sub_main - Called by crt0.s sp_main as the primary entry point
 *
 * Runs the cooperative multitasking command loop. Does not return.
 */
void sub_main(void) {
#if !defined(BOOT_PROBE) &&                                                 \
    (defined(DESKTOP_INIT_PROBE) || defined(BOOT_SAFE_DESKTOP))
  uint8_t readyPublished = 0;
#endif
#if defined(DESKTOP_INIT_PROBE)
  sub_write_result(7, 0x7201);
#endif
#if !defined(BOOT_PROBE) && !defined(DESKTOP_INIT_PROBE) &&                 \
    !defined(BOOT_SAFE_DESKTOP)
  sub_write_result(0, SUB_STATE_READY);
  GA_SUB_SET_FLAG(STATUS_IDLE);
#endif
  while (1) {
#ifdef BOOT_PROBE
    uint8_t cmd = sub_wait_cmd();
#elif defined(DESKTOP_INIT_PROBE) || defined(BOOT_SAFE_DESKTOP)
    uint8_t cmd;
    uint8_t signal;
    do {
      signal = GA_SUB_READ_MAIN_FLAG();
      if (!readyPublished) {
        sub_write_result(0, SUB_STATE_READY);
        sub_write_result(1, SUB_READY_MAGIC);
        GA_SUB_SET_FLAG(STATUS_IDLE);
        readyPublished = 1;
      }
    } while (signal == COMM_MAIN_IDLE);
    cmd = (uint8_t)GA_SUB_REG16(GA_COMM_CMD0);
#else
    uint8_t cmd = sub_wait_cmd();
#endif
#if defined(DESKTOP_INIT_PROBE)
    sub_write_result(7, 0x7202);
    sub_write_result(6, (uint16_t)cmd);
#endif
    process_command(cmd);
  }
}

#ifndef BOOT_PROBE
#ifdef BOOT_SAFE_DESKTOP
static void boot_draw_menu_labels(void) {
  BLT_DrawString(8, 4, "File", SysFont_Get(), BLT_BLACK);
  BLT_DrawString(48, 4, "Edit", SysFont_Get(), BLT_BLACK);
  BLT_DrawString(88, 4, "View", SysFont_Get(), BLT_BLACK);
}

static void boot_draw_window_body(void) {
  Rect divider;

  divider.left = 52;
  divider.top = 83;
  divider.right = 246;
  divider.bottom = 84;

  BLT_DrawString(56, 68, "Welcome to SegaOS", SysFont_Get(), BLT_BLACK);
  BLT_FillRect(&divider, BLT_BLACK);
  BLT_DrawString(56, 96, "68K desktop shell", SysFont_Get(), BLT_BLACK);
  BLT_DrawString(56, 112, "Word RAM -> VDP", SysFont_Get(), BLT_BLACK);
#if defined(DESKTOP_PUMP_PROBE) || defined(BOOT_SAFE_LIVE_PROBE)
  {
    char marker[BOOT_FRAME_MARKER_LEN];

    BootFrameMarker_Format(bootFrameMarkerIndex, marker);
    BLT_DrawString(56, 128, marker, SysFont_Get(), BLT_BLACK);
  }
#else
  BLT_DrawString(56, 128, "Boot-safe pre-alpha", SysFont_Get(), BLT_BLACK);
#endif
}

#ifdef BOOT_SAFE_LIVE_PROBE
static void boot_draw_live_probe_sentinel(void) {
  Rect marker;
  uint8_t color = (uint8_t)(bootFrameMarkerIndex & 0x000fU);

  marker.left = BOOT_LIVE_PROBE_SENTINEL_X;
  marker.top = BOOT_LIVE_PROBE_SENTINEL_Y;
  marker.right = (int16_t)(marker.left + BOOT_LIVE_PROBE_SENTINEL_W);
  marker.bottom = (int16_t)(marker.top + BOOT_LIVE_PROBE_SENTINEL_H);

  BLT_FillRect(&marker, color);
}
#endif

#ifndef DESKTOP_WM_PROBE
static void boot_draw_boot_window(void) {
  Rect shadow;
  Rect frame;
  Rect titleBar;
  Rect content;
  Rect closeBox;
  uint8_t titleFill;

  shadow.left = 44;
  shadow.top = 40;
  shadow.right = 262;
  shadow.bottom = 157;

  frame.left = 40;
  frame.top = 34;
  frame.right = 258;
  frame.bottom = 153;

  titleBar.left = 41;
  titleBar.top = 35;
  titleBar.right = 257;
  titleBar.bottom = 54;

  content.left = 41;
  content.top = 55;
  content.right = 257;
  content.bottom = 152;

  closeBox.left = 45;
  closeBox.top = 38;
  closeBox.right = 57;
  closeBox.bottom = 50;

  titleFill = (BLT_GetMode() == BLT_MODE_2BIT) ? BLT_2_LIGHT_GRAY
                                               : BLT_4_LIGHT_GRAY;

  BLT_FillRect(&shadow, BLT_4_DARK_GRAY);
  BLT_FillRect(&frame, BLT_GetWhite());
  BLT_DrawRect(&frame, BLT_BLACK);
  BLT_FillRect(&titleBar, titleFill);
  BLT_DrawRect(&titleBar, BLT_BLACK);
  BLT_DrawRect(&closeBox, BLT_BLACK);
  BLT_DrawString(125, 37, "SegaOS", SysFont_Get(), BLT_BLACK);
  BLT_FillRect(&content, BLT_GetWhite());
  boot_draw_window_body();
}
#endif

#ifdef DESKTOP_WM_PROBE
static Window *bootWmProbeWindow;

static void boot_wm_probe_init_window(void) {
  Rect bounds;

  if (bootWmProbeWindow)
    return;

  WM_Init();

  bounds.left = 40;
  bounds.top = 34;
  bounds.right = 258;
  bounds.bottom = 153;

  bootWmProbeWindow =
      WM_NewWindow(&bounds, "SegaOS", WM_STYLE_DOCUMENT, WF_VISIBLE);
}

static void boot_wm_probe_draw_windows(const Rect *dirty) {
  Window *win;

  for (win = WM_GetBottomWindow(); win; win = win->above) {
    DirtyWindowRedraw plan;

    if (!(win->flags & WF_VISIBLE))
      continue;

    DR_PlanWindowRedraw(dirty, &win->frame, &plan);
    if (!plan.hasWindow)
      continue;

    BLT_SetClipRect(&plan.clip);
    BLT_DrawWindowFrame(win, SysFont_Get());
    boot_draw_window_body();
  }
}

static void boot_wm_probe_publish_metrics(void) {
  Window *active = WM_GetActiveWindow();

  sub_write_result(2, WM_GetWindowCount());
  sub_write_result(3, active ? active->flags : 0xffff);
  sub_write_result(5, active ? (uint16_t)(((uint16_t)active->frame.left << 8) |
                                          ((uint16_t)active->frame.top & 0xff))
                             : 0xffff);
}
#endif

static void render_boot_safe_desktop(void) {
#ifdef BOOT_SAFE_TEXT_PROBE
  Rect screen;
  Rect marker;

  screen.left = 0;
  screen.top = 0;
  screen.right = 320;
  screen.bottom = 224;
  marker.left = 12;
  marker.top = 12;
  marker.right = 28;
  marker.bottom = 28;

  BLT_ResetClip();
  BLT_FillRect(&screen, BLT_GetWhite());
  BLT_DrawRect(&screen, BLT_BLACK);
  BLT_FillRect(&marker, BLT_BLACK);
  BLT_DrawStringScaled(64, 80, "SegaOS", SysFont_Get(), BLT_BLACK, 3);
  BLT_DrawStringScaled(64, 116, "TEXT OK", SysFont_Get(), BLT_BLACK, 2);
  BLT_DrawString(64, 152, "SGDK FONT", SysFont_Get(), BLT_BLACK);
#else
  DirtyRect dirtyStorage[4];
  DirtyRectList dirtyList;
  Rect screen;
  uint8_t i;

  screen.left = 0;
  screen.top = 0;
  screen.right = WM_SCREEN_W;
  screen.bottom = WM_SCREEN_H;

#ifdef DESKTOP_WM_PROBE
  boot_wm_probe_init_window();
#endif

  DR_InitList(&dirtyList, dirtyStorage, 4, &screen);
  DR_AddRect(&dirtyList, &screen);

  for (i = 0; i < DR_GetCount(&dirtyList); i++) {
    DirtyRect *dirty = DR_GetRect(&dirtyList, i);
    if (!dirty || !dirty->valid)
      continue;

    BLT_SetClipRect(&dirty->rect);

    WM_DrawDesktopInRect(&dirty->rect);
    boot_draw_menu_labels();
#ifdef DESKTOP_WM_PROBE
    boot_wm_probe_draw_windows(&dirty->rect);
#else
    boot_draw_boot_window();
#endif
#ifdef BOOT_SAFE_LIVE_PROBE
    boot_draw_live_probe_sentinel();
#endif
  }

  BLT_ResetClip();
#ifdef DESKTOP_WM_PROBE
  boot_wm_probe_publish_metrics();
#endif
#endif
}
#endif

static void os_init(void) {
  sub_write_result(7, 0x7301);
  /* Initialize blitter with the verified 1M bank-0 Word RAM base address. */
  BLT_Init((uint8_t *)0x0C0000);
  sub_write_result(7, 0x7302);
  BLT_SetMode(BLT_MODE_4BIT); /* Match Main CPU framebuffer pipeline */
  sub_write_result(7, 0x7303);

#ifdef BOOT_SAFE_DESKTOP
  BLT_Init((uint8_t *)0x0C0000);
  sub_write_result(7, 0x73f1);
  BLT_SetMode(BLT_MODE_4BIT);
  sub_write_result(7, 0x73f2);
  sub_write_result(7, 0x73fe);
#else
  /* Initialize Window Manager */
  WM_Init();
  sub_write_result(7, 0x7304);

  /* Draw initial desktop (gray pattern + menu bar) */
  WM_DrawDesktop();
  {
    Rect desktop;
    desktop.left = 0;
    desktop.top = 0;
    desktop.right = 320;
    desktop.bottom = 224;
    WM_InvalidateRect(&desktop);
  }
  sub_write_result(7, 0x7305);

#ifndef BOOT_SAFE_DESKTOP
  /* Initialize Menu Bar with default menus */
  MenuBar_Init();
  {
    int8_t fileMenu = MenuBar_AddMenu("File");
    if (fileMenu >= 0) {
      MenuBar_AddItem(fileMenu, "New", 0x0101, MIF_NONE);
      MenuBar_AddItem(fileMenu, "Open", 0x0102, MIF_NONE);
      MenuBar_AddItem(fileMenu, "Close", 0x0103, MIF_NONE);
      MenuBar_AddSeparator(fileMenu);
      MenuBar_AddItem(fileMenu, "Quit", 0x0104, MIF_NONE);
    }
    int8_t editMenu = MenuBar_AddMenu("Edit");
    if (editMenu >= 0) {
      MenuBar_AddItem(editMenu, "Undo", 0x0201, MIF_DISABLED);
      MenuBar_AddSeparator(editMenu);
      MenuBar_AddItem(editMenu, "Cut", 0x0202, MIF_NONE);
      MenuBar_AddItem(editMenu, "Copy", 0x0203, MIF_NONE);
      MenuBar_AddItem(editMenu, "Paste", 0x0204, MIF_NONE);
    }
    int8_t appsMenu = MenuBar_AddMenu("Apps");
    if (appsMenu >= 0) {
      MenuBar_AddItem(appsMenu, "Calculator", 0x0301, MIF_NONE);
      MenuBar_AddItem(appsMenu, "Notepad", 0x0302, MIF_NONE);
      MenuBar_AddItem(appsMenu, "Paint", 0x0303, MIF_NONE);
    }
  }
#endif

  /* Initialize memory manager.
   * Heap region is defined by linker script symbols.
   * _heap_start = end of BSS, _heap_end = start of stack area. */
  {
    extern uint8_t _heap_start;
    extern uint8_t _heap_end;
    MEM_Init(&_heap_start, &_heap_end);
  }
  sub_write_result(7, 0x73fe);

  /* TODO: Initialize file system (ISO 9660 reader, BRAM wrappers) */
#endif
}
#endif

static void process_command(uint8_t cmd) {
  sub_ack();

  switch (cmd) {
#ifdef BOOT_PROBE
  case CMD_BOOT_PROBE: {
    volatile uint16_t *wram = (volatile uint16_t *)0x0C0000;

    segaos_probe_sub_cmd = cmd;
    segaos_probe_sub_param0 = sub_read_param(0);
    segaos_probe_sub_param1 = sub_read_param(1);
    segaos_probe_sub_had_wram_before = sub_has_wram();

    wram[0] = segaos_probe_sub_param0;
    wram[1] = segaos_probe_sub_param1;
    segaos_probe_sub_wram_word0 = wram[0];
    segaos_probe_sub_wram_word1 = wram[1];

    sub_return_wram();
    segaos_probe_sub_had_wram_after = sub_has_wram();

    sub_write_result(0, PROBE_DONE_MAGIC);
    sub_write_result(1, segaos_probe_sub_wram_word0);
    sub_write_result(2, segaos_probe_sub_wram_word1);
    sub_write_result(3, segaos_probe_sub_had_wram_before);
    segaos_probe_sub_phase = PROBE_PHASE_DONE;
    sub_done();
    break;
  }

  default:
    sub_error();
    break;
#else
  case CMD_INIT_OS:
    os_init();
    sub_write_result(0, SUB_STATE_READY);
    sub_done();
    break;

  case CMD_RENDER_FRAME: {
#ifdef BOOT_SAFE_DESKTOP
#if defined(DESKTOP_PUMP_PROBE) || defined(BOOT_SAFE_LIVE_PROBE)
    bootFrameMarkerIndex = sub_read_param(0);
#endif
    sub_write_result(0, SUB_STATE_RENDERING);
    sub_write_result(7, 0x7401);

    sub_wait_wram();
    sub_write_result(7, 0x7402);

    render_boot_safe_desktop();
    sub_write_result(7, 0x7403);

    sub_return_wram();
    sub_write_result(7, 0x7404);
#if defined(DESKTOP_PUMP_PROBE) || defined(BOOT_SAFE_LIVE_PROBE)
    sub_write_result(1, bootFrameMarkerIndex);
#endif
    sub_write_result(0, SUB_STATE_READY);
    sub_done();
    break;
#else
    Window *win;
    uint8_t i;

    sub_write_result(0, SUB_STATE_RENDERING);
    sub_wait_wram();

    /* Process dirty rects via Window Manager */
    uint8_t count = WM_BeginUpdate();
    sub_write_result(1, (uint16_t)count);

    /* For each dirty rect, render to Word RAM */
    for (i = 0; i < count; i++) {
      DirtyRect *dr = WM_GetDirtyRect(i);
      if (!dr || !dr->valid)
        continue;

      /* 1. Clip blitter to this dirty rect */
      BLT_SetClipRect(&dr->rect);

      /* 2. Redraw root desktop/menu pixels inside this dirty region */
      WM_DrawDesktopInRect(&dr->rect);

      /* 3. Walk window list back-to-front (painter's algorithm) */
      for (win = WM_GetBottomWindow(); win; win = win->above) {
        if (win->flags & WF_VISIBLE) {
          BLT_DrawWindowFrame(win, SysFont_Get());
          /* Call app's draw callback for content */
          if (win->drawProc) {
            win->drawProc(win);
          }
        }
      }
    }

    /* 4. Draw menu bar (always on top of windows) */
    BLT_ResetClip();
#ifndef BOOT_SAFE_DESKTOP
    MenuBar_Draw();
    if (MenuBar_IsTracking()) {
      MenuBar_DrawDropdown();
    }
#endif

    /* 5. Draw mouse cursor last (on top of everything) */
    BLT_BlitBitmap1(cursorX, cursorY, cursorBitmap, 11, 16, BLT_BLACK);

    WM_EndUpdate();

    /* Give the finished Word RAM framebuffer to Main CPU. */
    sub_return_wram();

    sub_write_result(0, SUB_STATE_READY);
    sub_done();
    break;
#endif
  }

#ifdef BASIC_BRAM_PROBE
  case CMD_BASIC_BRAM_PROBE: {
    uint8_t ok;

    sub_write_result(7, 0x7501);
    if (!BRM_InitInternalBiosOps(&basicBramProbeContext,
                                 &basicBramProbeOps)) {
      BAS_ClearBramSmokeResult(&basicBramProbeResult);
      basicBramProbeResult.status = BAS_BRAM_SMOKE_STORAGE_INIT_FAILED;
      sub_write_result(0, basicBramProbeResult.magic);
      sub_write_result(1, basicBramProbeResult.status);
      sub_write_result(2, basicBramProbeResult.formatStatus);
      sub_write_result(3, 0);
      sub_write_result(4, 0);
      sub_write_result(5, 0);
      sub_write_result(6, 0);
      sub_write_result(7, 0x75e2);
      sub_done();
      break;
    }

    ok = BAS_RunBramSmoke(&basicBramProbeResult, &basicBramProbeOps,
                          BAS_BRAM_SMOKE_FILENAME);
    sub_write_result(0, basicBramProbeResult.magic);
    sub_write_result(1, basicBramProbeResult.status);
    sub_write_result(2, basicBramProbeResult.formatStatus);
    sub_write_result(3, basicBramProbeResult.totalBlocks4K);
    sub_write_result(4, basicBramProbeResult.freeBlocks4K);
    sub_write_result(5, (uint16_t)((basicBramProbeResult.saveOk << 8) |
                                  basicBramProbeResult.loadOk));
    sub_write_result(6, (uint16_t)((basicBramProbeResult.lineCount << 8) |
                                  ((basicBramProbeResult.lastSaveTarget & 0xfU)
                                   << 4) |
                                  (basicBramProbeResult.lastLoadTarget &
                                   0xfU)));
    sub_write_result(7, ok ? 0x75ff
                           : (uint16_t)(0x7500U |
                                        (basicBramProbeResult.status & 0xffU)));
    sub_done();
    break;
  }
#endif

  case CMD_OPEN_WINDOW: {
    /* Params: x, y, w, h from CMD registers */
    uint16_t x = sub_read_param(0);
    uint16_t y = sub_read_param(1);
    uint16_t w = sub_read_param(2);
    uint16_t h = sub_read_param(3);

    Rect bounds;
    bounds.left = (int16_t)x;
    bounds.top = (int16_t)y;
    bounds.right = (int16_t)(x + w);
    bounds.bottom = (int16_t)(y + h);

    Window *win =
        WM_NewWindow(&bounds, "Untitled", WM_STYLE_DOCUMENT, WF_VISIBLE);

    /* Return window ID (or 0xFF on failure) */
    sub_write_result(0, win ? win->id : 0xFF);
    sub_done();
    break;
  }

  case CMD_CLOSE_WINDOW: {
    uint16_t winId = sub_read_param(0);
    Window *win = WM_GetWindowById((uint8_t)winId);
    if (win) {
      WM_DisposeWindow(win);
      sub_write_result(0, 0); /* Success */
    } else {
      sub_write_result(0, 0xFF); /* Not found */
    }
    sub_done();
    break;
  }

  case CMD_CD_PLAY: {
    uint16_t track = sub_read_param(0);
    /* TODO: Issue BIOS call to play CD audio track */
    (void)track;
    sub_done();
    break;
  }

  case CMD_MOUSE_EVENT: {
    InputEvent evt;
    Input_DecodeMouseEvent(&evt);

    /* Update cursor position */
    prevCursorX = cursorX;
    prevCursorY = cursorY;
    cursorX = evt.x;
    cursorY = evt.y;

    /* Mark old and new cursor positions as dirty */
    Rect dirtyOld, dirtyNew;
    dirtyOld.left = prevCursorX;
    dirtyOld.top = prevCursorY;
    dirtyOld.right = prevCursorX + 11;
    dirtyOld.bottom = prevCursorY + 16;
    WM_AddDirtyRect(&dirtyOld);

    dirtyNew.left = cursorX;
    dirtyNew.top = cursorY;
    dirtyNew.right = cursorX + 11;
    dirtyNew.bottom = cursorY + 16;
    WM_AddDirtyRect(&dirtyNew);

    /* Process mouse events through Window Manager */
    if (evt.type == INPUT_EVT_MOUSE_DOWN) {
      Point clickPt;
      clickPt.x = evt.x;
      clickPt.y = evt.y;

      HitTestResult hit = WM_HitTest(clickPt);

      switch (hit.part) {
      case WM_HIT_CONTENT:
        if (hit.window) {
          WM_SelectWindow(hit.window);
          /* Route click to app's clickProc */
          if (hit.window->clickProc) {
            hit.window->clickProc(hit.window, clickPt);
          }
        }
        break;
      case WM_HIT_DRAG:
        /* Start drag: record offset from window origin to click point */
        if (hit.window) {
          WM_SelectWindow(hit.window);
          dragWindow = hit.window;
          dragOffsetX = evt.x - hit.window->frame.left;
          dragOffsetY = evt.y - hit.window->frame.top;
        }
        break;
      case WM_HIT_CLOSE:
        if (hit.window) {
          WM_DisposeWindow(hit.window);
        }
        break;
      case WM_HIT_GROW:
        /* TODO: resize interaction */
        break;
#ifndef BOOT_SAFE_DESKTOP
      case WM_HIT_MENUBAR:
        MenuBar_HandleMouseDown(evt.x, evt.y);
        break;
#endif
      default:
        break;
      }
    } else if (evt.type == INPUT_EVT_MOUSE_MOVE) {
#ifndef BOOT_SAFE_DESKTOP
      if (MenuBar_IsTracking()) {
        MenuBar_HandleMouseMove(evt.x, evt.y);
      }
#endif
    } else if (evt.type == INPUT_EVT_MOUSE_DRAG) {
#ifndef BOOT_SAFE_DESKTOP
      if (MenuBar_IsTracking()) {
        MenuBar_HandleMouseMove(evt.x, evt.y);
      } else if (dragWindow) {
#else
      if (dragWindow) {
#endif
        /* Move window to new position, maintaining grab offset */
        int16_t newX = evt.x - dragOffsetX;
        int16_t newY = evt.y - dragOffsetY;
        /* Clamp to keep title bar on screen */
        if (newY < WM_MENUBAR_H)
          newY = WM_MENUBAR_H;
        WM_MoveWindow(dragWindow, newX, newY);
      } else {
        /* Route drag to active window's dragProc (for Paint etc.) */
        Window *active = WM_GetActiveWindow();
        if (active && active->dragProc) {
          Point dragPt;
          dragPt.x = evt.x;
          dragPt.y = evt.y;
          active->dragProc(active, dragPt);
        }
      }
    } else if (evt.type == INPUT_EVT_MOUSE_UP) {
#ifndef BOOT_SAFE_DESKTOP
      if (MenuBar_IsTracking()) {
        MenuSelection sel = MenuBar_HandleMouseUp(evt.x, evt.y);
        if (sel.commandID != 0) {
          /* Dispatch menu commands */
          switch (sel.commandID) {
          case 0x0101: { /* File > New */
            char title[16];
            Rect bounds;
            windowCounter++;
            title[0] = 'W';
            title[1] = 'i';
            title[2] = 'n';
            title[3] = 'd';
            title[4] = 'o';
            title[5] = 'w';
            title[6] = ' ';
            title[7] = '0' + (windowCounter / 10);
            title[8] = '0' + (windowCounter % 10);
            title[9] = '\0';
            bounds.left = 30 + (windowCounter * 12) % 120;
            bounds.top = 40 + (windowCounter * 10) % 80;
            bounds.right = bounds.left + 180;
            bounds.bottom = bounds.top + 120;
            WM_NewWindow(&bounds, title, WM_STYLE_DOCUMENT,
                         WF_VISIBLE | WF_HAS_GROW);
            break;
          }
          case 0x0103: { /* File > Close */
            Window *active = WM_GetActiveWindow();
            if (active) {
              WM_DisposeWindow(active);
            }
            break;
          }
          case 0x0301: /* Apps > Calculator */
            Calc_Open();
            break;
          case 0x0302: /* Apps > Notepad */
            Notepad_Open();
            break;
          case 0x0303: /* Apps > Paint */
            Paint_Open();
            break;
          default:
            break;
          }
        }
      }
#endif
      /* Release drag */
      dragWindow = (Window *)0;
    }

    sub_done();
    break;
  }

  case CMD_WRAM_SWAP:
    /* Explicit bank swap request from Main CPU */
    sub_return_wram();
    sub_done();
    break;

  default:
    /* Unknown command */
    sub_error();
    break;
#endif
  }
}

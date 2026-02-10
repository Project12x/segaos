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

#define SUB_CPU
#include "blitter.h"
#include "calc.h"
#include "common.h"
#include "input.h"
#include "mem.h"
#include "menubar.h"
#include "notepad.h"
#include "paint.h"
#include "sega_os.h"
#include "sysfont.h"
#include "vkbd.h"
#include "wm.h"

/* Cursor state (tracks mouse position for drawing) */
static int16_t cursorX = 160;
static int16_t cursorY = 112;
static int16_t prevCursorX = 160;
static int16_t prevCursorY = 112;

/* Drag state */
static Window *dragWindow = (Window *)0;
static int16_t dragOffsetX = 0;
static int16_t dragOffsetY = 0;

/* Counter for auto-naming windows */
static uint8_t windowCounter = 0;

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

/* Forward declarations */
static void os_init(void);
static void process_command(uint8_t cmd);

/*
 * sub_init - Called by crt0.s sp_init during BIOS initialization
 *
 * Performs one-time OS setup. BSS is already cleared by crt0.
 */
void sub_init(void) {
  /* Signal we are alive */
  sub_write_result(0, SUB_STATE_BOOTING);
  GA_SUB_SET_FLAG(STATUS_BUSY);

  /* Initialize OS subsystems */
  os_init();

  /* Signal ready */
  sub_write_result(0, SUB_STATE_READY);
  GA_SUB_SET_FLAG(STATUS_IDLE);
}

/*
 * sub_main - Called by crt0.s sp_main as the primary entry point
 *
 * Runs the cooperative multitasking command loop. Does not return.
 */
void sub_main(void) {
  while (1) {
    uint8_t cmd = sub_wait_cmd();
    process_command(cmd);
  }
}

static void os_init(void) {
  /* Initialize blitter with Word RAM Bank 0 base address */
  /* In 1M mode, Sub CPU sees Bank 0 at $0C0000 (128KB) */
  /* Bank 1 is at $0E0000 (used for ASIC pixel-mapped access) */
  BLT_Init((uint8_t *)0x0C0000);
  BLT_SetMode(BLT_MODE_2BIT); /* Default: 4 grayscale shades */

  /* Initialize Window Manager */
  WM_Init();

  /* Draw initial desktop (gray pattern + menu bar) */
  WM_DrawDesktop();

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

  /* Initialize memory manager.
   * Heap region is defined by linker script symbols.
   * _heap_start = end of BSS, _heap_end = start of stack area. */
  {
    extern uint8_t _heap_start;
    extern uint8_t _heap_end;
    MEM_Init(&_heap_start, &_heap_end);
  }

  /* TODO: Initialize file system (ISO 9660 reader, BRAM wrappers) */
}

static void process_command(uint8_t cmd) {
  sub_ack();

  switch (cmd) {
  case CMD_INIT_OS:
    os_init();
    sub_done();
    break;

  case CMD_RENDER_FRAME: {
    Window *win;
    uint8_t i;

    sub_write_result(0, SUB_STATE_RENDERING);

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

      /* 2. Fill desktop pattern in dirty region */
      BLT_FillRectPattern(&dr->rect, &PAT_GRAY_50);

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
    MenuBar_Draw();
    if (MenuBar_IsTracking()) {
      MenuBar_DrawDropdown();
    }

    /* 5. Draw mouse cursor last (on top of everything) */
    BLT_BlitBitmap1(cursorX, cursorY, cursorBitmap, 11, 16, BLT_BLACK);

    WM_EndUpdate();

    /* Swap Word RAM banks: give finished frame to Main CPU.
     * Sub CPU gets the other bank to render the next frame into. */
    sub_return_wram();

    sub_write_result(0, SUB_STATE_READY);
    sub_done();
    break;
  }

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
      case WM_HIT_MENUBAR:
        MenuBar_HandleMouseDown(evt.x, evt.y);
        break;
      default:
        break;
      }
    } else if (evt.type == INPUT_EVT_MOUSE_MOVE) {
      if (MenuBar_IsTracking()) {
        MenuBar_HandleMouseMove(evt.x, evt.y);
      }
    } else if (evt.type == INPUT_EVT_MOUSE_DRAG) {
      if (MenuBar_IsTracking()) {
        MenuBar_HandleMouseMove(evt.x, evt.y);
      } else if (dragWindow) {
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
  }
}

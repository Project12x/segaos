/*
 * menubar.c - Menu Bar Implementation for Genesis System 1
 *
 * Renders the top-of-screen menu bar and handles pull-down menus.
 * All state is statically allocated — no malloc.
 */

#include "menubar.h"
#include "blitter.h"
#include "sega_os.h"
#include "sysfont.h"


#include <string.h>

/* ============================================================
 * Global State
 * ============================================================ */
static MenuBar menuBar;

/* ============================================================
 * Init
 * ============================================================ */
void MenuBar_Init(void) {
  memset(&menuBar, 0, sizeof(MenuBar));
  menuBar.activeMenu = -1;
  menuBar.activeItem = -1;
}

/* ============================================================
 * Menu/Item Management
 * ============================================================ */
int8_t MenuBar_AddMenu(const char *title) {
  if (menuBar.menuCount >= MENU_MAX_MENUS)
    return -1;

  uint8_t idx = menuBar.menuCount++;
  Menu *m = &menuBar.menus[idx];
  m->title = title;
  m->itemCount = 0;

  /* Compute title position */
  if (idx == 0) {
    m->titleX = MENUBAR_FIRST_X;
  } else {
    Menu *prev = &menuBar.menus[idx - 1];
    m->titleX = prev->titleX + prev->titleWidth + MENUBAR_PADDING;
  }
  m->titleWidth = SysFont_StringWidth(title) + MENUBAR_PADDING;

  return (int8_t)idx;
}

int8_t MenuBar_AddItem(uint8_t menuIndex, const char *text, uint16_t commandID,
                       uint8_t flags) {
  if (menuIndex >= menuBar.menuCount)
    return -1;
  Menu *m = &menuBar.menus[menuIndex];
  if (m->itemCount >= MENU_MAX_ITEMS)
    return -1;

  uint8_t idx = m->itemCount++;
  MenuItem *item = &m->items[idx];
  item->text = text;
  item->flags = flags;
  item->shortcutKey = 0;
  item->commandID = commandID;

  return (int8_t)idx;
}

int8_t MenuBar_AddSeparator(uint8_t menuIndex) {
  return MenuBar_AddItem(menuIndex, NULL, 0, MIF_SEPARATOR);
}

void MenuBar_SetItemEnabled(uint8_t menuIndex, uint8_t itemIndex,
                            uint8_t enabled) {
  if (menuIndex >= menuBar.menuCount)
    return;
  Menu *m = &menuBar.menus[menuIndex];
  if (itemIndex >= m->itemCount)
    return;

  if (enabled) {
    m->items[itemIndex].flags &= ~MIF_DISABLED;
  } else {
    m->items[itemIndex].flags |= MIF_DISABLED;
  }
}

void MenuBar_SetItemChecked(uint8_t menuIndex, uint8_t itemIndex,
                            uint8_t checked) {
  if (menuIndex >= menuBar.menuCount)
    return;
  Menu *m = &menuBar.menus[menuIndex];
  if (itemIndex >= m->itemCount)
    return;

  if (checked) {
    m->items[itemIndex].flags |= MIF_CHECKED;
  } else {
    m->items[itemIndex].flags &= ~MIF_CHECKED;
  }
}

/* ============================================================
 * Rendering
 * ============================================================ */

/* Compute dropdown width for a menu (widest item + padding) */
static int16_t compute_dropdown_width(const Menu *m) {
  int16_t maxW = MENU_MIN_WIDTH;
  for (uint8_t i = 0; i < m->itemCount; i++) {
    if (m->items[i].flags & MIF_SEPARATOR)
      continue;
    if (!m->items[i].text)
      continue;
    int16_t w = SysFont_StringWidth(m->items[i].text) + MENU_PADDING_X * 2;
    /* Add space for checkmark if needed */
    if (m->items[i].flags & MIF_CHECKED) {
      w += 10; /* Checkmark width */
    }
    if (w > maxW)
      maxW = w;
  }
  return maxW;
}

/* Compute total dropdown height */
static int16_t compute_dropdown_height(const Menu *m) {
  int16_t h = MENU_PADDING_Y * 2;
  for (uint8_t i = 0; i < m->itemCount; i++) {
    if (m->items[i].flags & MIF_SEPARATOR) {
      h += MENU_SEPARATOR_H;
    } else {
      h += MENU_ITEM_HEIGHT;
    }
  }
  return h;
}

void MenuBar_Draw(void) {
  /* Bar background: white rect with black border at bottom */
  Rect barRect;
  barRect.left = 0;
  barRect.top = 0;
  barRect.right = 320;
  barRect.bottom = MENUBAR_HEIGHT;
  BLT_FillRect(&barRect, 0); /* White background */

  /* Bottom border */
  BLT_DrawHLine(0, 319, MENUBAR_HEIGHT - 1, 1);

  /* Draw menu titles */
  for (uint8_t i = 0; i < menuBar.menuCount; i++) {
    const Menu *m = &menuBar.menus[i];
    if (!m->title)
      continue;

    if (menuBar.isOpen && menuBar.activeMenu == (int8_t)i) {
      /* Active title: inverted (white on black) */
      Rect titleRect;
      titleRect.left = m->titleX - 4;
      titleRect.top = 1;
      titleRect.right = m->titleX + m->titleWidth;
      titleRect.bottom = MENUBAR_HEIGHT - 1;
      BLT_FillRect(&titleRect, 1); /* Black fill */
      SysFont_DrawString(m->titleX, MENUBAR_TEXT_Y, m->title, 0);
    } else {
      /* Normal title: black on white */
      SysFont_DrawString(m->titleX, MENUBAR_TEXT_Y, m->title, 1);
    }
  }
}

void MenuBar_DrawDropdown(void) {
  if (!menuBar.isOpen || menuBar.activeMenu < 0)
    return;

  const Menu *m = &menuBar.menus[menuBar.activeMenu];
  int16_t dropX = m->titleX - 4;
  int16_t dropY = MENUBAR_HEIGHT;
  int16_t dropW = compute_dropdown_width(m);
  int16_t dropH = compute_dropdown_height(m);

  /* Drop shadow (2px offset, drawn first) */
  Rect shadowRect;
  shadowRect.left = dropX + MENU_SHADOW_SIZE;
  shadowRect.top = dropY + MENU_SHADOW_SIZE;
  shadowRect.right = dropX + dropW + MENU_SHADOW_SIZE;
  shadowRect.bottom = dropY + dropH + MENU_SHADOW_SIZE;
  BLT_FillRect(&shadowRect, 1);

  /* Dropdown background: white with black border */
  Rect dropRect;
  dropRect.left = dropX;
  dropRect.top = dropY;
  dropRect.right = dropX + dropW;
  dropRect.bottom = dropY + dropH;
  BLT_FillRect(&dropRect, 0); /* White fill */

  /* Border */
  BLT_DrawHLine(dropX, dropX + dropW - 1, dropY, 1);
  BLT_DrawHLine(dropX, dropX + dropW - 1, dropY + dropH - 1, 1);
  BLT_DrawVLine(dropX, dropY, dropY + dropH - 1, 1);
  BLT_DrawVLine(dropX + dropW - 1, dropY, dropY + dropH - 1, 1);

  /* Draw items */
  int16_t itemY = dropY + MENU_PADDING_Y;
  for (uint8_t i = 0; i < m->itemCount; i++) {
    const MenuItem *item = &m->items[i];

    if (item->flags & MIF_SEPARATOR) {
      /* Dashed separator line */
      int16_t sepY = itemY + MENU_SEPARATOR_H / 2;
      BLT_DrawHLine(dropX + 2, dropX + dropW - 3, sepY, 1);
      itemY += MENU_SEPARATOR_H;
      continue;
    }

    /* Highlight if this is the active item */
    if (menuBar.activeItem == (int8_t)i) {
      Rect hlRect;
      hlRect.left = dropX + 1;
      hlRect.top = itemY;
      hlRect.right = dropX + dropW - 1;
      hlRect.bottom = itemY + MENU_ITEM_HEIGHT;
      BLT_FillRect(&hlRect, 1); /* Black highlight */
    }

    if (item->text) {
      int16_t textX = dropX + MENU_PADDING_X;
      int16_t textY = itemY + 2;

      /* Checkmark */
      if (item->flags & MIF_CHECKED) {
        /* Simple checkmark: draw a small "v" */
        uint8_t color = (menuBar.activeItem == (int8_t)i) ? 0 : 1;
        SysFont_DrawString(textX, textY, "\x1A", color);
        textX += 10;
      }

      if (item->flags & MIF_DISABLED) {
        /* Disabled: draw in gray pattern (skip every other pixel) */
        /* For 1-bit, we use the text still but it's dimmed */
        SysFont_DrawString(textX, textY, item->text, 1);
        /* Overlay white checkerboard for "grayed" effect */
        Rect grayRect;
        grayRect.left = textX;
        grayRect.top = textY;
        grayRect.right = textX + SysFont_StringWidth(item->text);
        grayRect.bottom = textY + 10;
        BLT_FillRectPattern(&grayRect, &PAT_GRAY_50);
      } else {
        uint8_t color = (menuBar.activeItem == (int8_t)i) ? 0 : 1;
        SysFont_DrawString(textX, textY, item->text, color);
      }
    }

    itemY += MENU_ITEM_HEIGHT;
  }
}

/* ============================================================
 * Mouse Interaction
 * ============================================================ */

uint8_t MenuBar_HandleMouseDown(int16_t x, int16_t y) {
  /* Check if click is in menu bar region */
  if (y >= MENUBAR_HEIGHT) {
    if (menuBar.isOpen) {
      MenuBar_Close();
    }
    return 0;
  }

  /* Find which menu title was clicked */
  for (uint8_t i = 0; i < menuBar.menuCount; i++) {
    const Menu *m = &menuBar.menus[i];
    if (x >= m->titleX - 4 && x < m->titleX + m->titleWidth) {
      menuBar.activeMenu = (int8_t)i;
      menuBar.activeItem = -1;
      menuBar.isOpen = 1;
      return 1;
    }
  }

  return 0;
}

void MenuBar_HandleMouseMove(int16_t x, int16_t y) {
  if (!menuBar.isOpen || menuBar.activeMenu < 0)
    return;

  /* Check if mouse moved to a different menu title */
  if (y < MENUBAR_HEIGHT) {
    for (uint8_t i = 0; i < menuBar.menuCount; i++) {
      const Menu *m = &menuBar.menus[i];
      if (x >= m->titleX - 4 && x < m->titleX + m->titleWidth) {
        if (menuBar.activeMenu != (int8_t)i) {
          menuBar.activeMenu = (int8_t)i;
          menuBar.activeItem = -1;
        }
        return;
      }
    }
    menuBar.activeItem = -1;
    return;
  }

  /* Check if mouse is in dropdown area */
  const Menu *m = &menuBar.menus[menuBar.activeMenu];
  int16_t dropX = m->titleX - 4;
  int16_t dropW = compute_dropdown_width(m);
  int16_t dropY = MENUBAR_HEIGHT;

  if (x < dropX || x >= dropX + dropW) {
    menuBar.activeItem = -1;
    return;
  }

  /* Find which item the mouse is over */
  int16_t itemY = dropY + MENU_PADDING_Y;
  for (uint8_t i = 0; i < m->itemCount; i++) {
    const MenuItem *item = &m->items[i];
    int16_t itemH =
        (item->flags & MIF_SEPARATOR) ? MENU_SEPARATOR_H : MENU_ITEM_HEIGHT;

    if (y >= itemY && y < itemY + itemH) {
      if ((item->flags & MIF_SEPARATOR) || (item->flags & MIF_DISABLED)) {
        menuBar.activeItem = -1;
      } else {
        menuBar.activeItem = (int8_t)i;
      }
      return;
    }
    itemY += itemH;
  }

  menuBar.activeItem = -1;
}

MenuSelection MenuBar_HandleMouseUp(int16_t x, int16_t y) {
  MenuSelection sel;
  sel.menuIndex = 0;
  sel.itemIndex = 0;
  sel.commandID = 0;

  if (menuBar.isOpen && menuBar.activeMenu >= 0 && menuBar.activeItem >= 0) {
    const Menu *m = &menuBar.menus[menuBar.activeMenu];
    const MenuItem *item = &m->items[menuBar.activeItem];

    if (!(item->flags & MIF_DISABLED) && !(item->flags & MIF_SEPARATOR)) {
      sel.menuIndex = (uint8_t)menuBar.activeMenu;
      sel.itemIndex = (uint8_t)menuBar.activeItem;
      sel.commandID = item->commandID;
    }
  }

  (void)x;
  (void)y;

  MenuBar_Close();
  return sel;
}

void MenuBar_Close(void) {
  menuBar.activeMenu = -1;
  menuBar.activeItem = -1;
  menuBar.isOpen = 0;
}

uint8_t MenuBar_IsTracking(void) { return menuBar.isOpen; }

MenuBar *MenuBar_Get(void) { return &menuBar; }

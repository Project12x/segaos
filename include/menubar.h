/*
 * menubar.h - Menu Bar System for Genesis System 1
 *
 * Implements the top-of-screen pull-down menu bar (Mac System 1.0 style).
 *
 * Architecture:
 *   - Menu bar occupies Y=0..19 (20px tall, 1px border at bottom)
 *   - Each menu has a title and a list of items
 *   - Items can be: regular, separator, disabled, or checked
 *   - Menu IDs 0-15 (static allocation, no malloc)
 *   - Item IDs are per-menu, 0-15
 *
 * Rendering:
 *   - Menu bar: white background, black text, 1px black border bottom
 *   - Pull-down: white box with 1px border, black text, black highlight
 *   - Active menu title: inverted (white on black)
 *
 * Coordinate system:
 *   - Menu titles start at X=10, separated by their text width + padding
 *   - Pull-down appears below the title, aligned to title left edge
 */

#ifndef MENUBAR_H
#define MENUBAR_H

#include <stdint.h>

/* ============================================================
 * Constants
 * ============================================================ */
#define MENUBAR_HEIGHT 20  /* Pixels tall (including border)  */
#define MENUBAR_TEXT_Y 5   /* Y offset for title text         */
#define MENUBAR_PADDING 8  /* Horizontal padding per title    */
#define MENUBAR_FIRST_X 10 /* X start of first menu title     */

#define MENU_MAX_MENUS 8    /* Max menus in bar                */
#define MENU_MAX_ITEMS 16   /* Max items per menu              */
#define MENU_ITEM_HEIGHT 14 /* Pixels per menu item            */
#define MENU_SEPARATOR_H 8  /* Pixels for separator item       */
#define MENU_PADDING_X 8    /* Horiz padding inside dropdown   */
#define MENU_PADDING_Y 2    /* Vert padding at top/bottom      */
#define MENU_MIN_WIDTH 80   /* Minimum dropdown width          */
#define MENU_SHADOW_SIZE 2  /* Drop shadow offset              */

/* ============================================================
 * Menu Item Flags
 * ============================================================ */
#define MIF_NONE 0x00
#define MIF_SEPARATOR 0x01 /* This item is a separator line   */
#define MIF_DISABLED 0x02  /* Grayed out, not selectable      */
#define MIF_CHECKED 0x04   /* Checkmark prefix                */

/* ============================================================
 * Data Structures
 * ============================================================ */

/* A single menu item */
typedef struct {
  const char *text;    /* Item label (NULL = unused slot)   */
  uint8_t flags;       /* MIF_* flags                       */
  uint8_t shortcutKey; /* Optional keyboard shortcut (0=none) */
  uint16_t commandID;  /* App-defined command ID            */
} MenuItem;

/* A menu (title + dropdown items) */
typedef struct {
  const char *title;  /* Menu title in bar (NULL = unused) */
  int16_t titleX;     /* Computed X position in bar        */
  int16_t titleWidth; /* Computed pixel width of title     */
  uint8_t itemCount;  /* Number of items                   */
  MenuItem items[MENU_MAX_ITEMS];
} Menu;

/* The complete menu bar state */
typedef struct {
  uint8_t menuCount; /* Number of menus                   */
  int8_t activeMenu; /* Currently open menu (-1 = none)   */
  int8_t activeItem; /* Highlighted item (-1 = none)      */
  uint8_t isOpen;    /* Is a dropdown currently showing?  */
  Menu menus[MENU_MAX_MENUS];
} MenuBar;

/* Result of menu interaction */
typedef struct {
  uint8_t menuIndex;  /* Which menu was selected           */
  uint8_t itemIndex;  /* Which item was selected           */
  uint16_t commandID; /* The item's command ID             */
} MenuSelection;

/* ============================================================
 * API
 * ============================================================ */

/* Initialize the menu bar (call once during OS init) */
void MenuBar_Init(void);

/* Add a menu to the bar. Returns menu index or -1 on failure. */
int8_t MenuBar_AddMenu(const char *title);

/* Add an item to a menu. Returns item index or -1 on failure. */
int8_t MenuBar_AddItem(uint8_t menuIndex, const char *text, uint16_t commandID,
                       uint8_t flags);

/* Add a separator to a menu. */
int8_t MenuBar_AddSeparator(uint8_t menuIndex);

/* Enable/disable a menu item */
void MenuBar_SetItemEnabled(uint8_t menuIndex, uint8_t itemIndex,
                            uint8_t enabled);

/* Check/uncheck a menu item */
void MenuBar_SetItemChecked(uint8_t menuIndex, uint8_t itemIndex,
                            uint8_t checked);

/* Render the menu bar (called during frame render) */
void MenuBar_Draw(void);

/* Render the open dropdown (called during frame render, after bar) */
void MenuBar_DrawDropdown(void);

/* Handle mouse down in menu bar region.
 * Returns 1 if the click was consumed by the menu bar. */
uint8_t MenuBar_HandleMouseDown(int16_t x, int16_t y);

/* Handle mouse move while menu is open (tracking).
 * Updates highlight. */
void MenuBar_HandleMouseMove(int16_t x, int16_t y);

/* Handle mouse up while menu is open.
 * Returns valid MenuSelection if an item was chosen,
 * or selection with commandID=0 if cancelled. */
MenuSelection MenuBar_HandleMouseUp(int16_t x, int16_t y);

/* Close any open menu */
void MenuBar_Close(void);

/* Is the menu bar tracking (dropdown open)? */
uint8_t MenuBar_IsTracking(void);

/* Get the global menu bar instance */
MenuBar *MenuBar_Get(void);

#endif /* MENUBAR_H */

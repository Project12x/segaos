#include "dirty_rect.h"
#include <stdio.h>

static int failures;

static Rect rect_make(int16_t top, int16_t left, int16_t bottom,
                      int16_t right) {
  Rect r;
  r.top = top;
  r.left = left;
  r.bottom = bottom;
  r.right = right;
  return r;
}

static void expect_true(Boolean value, const char *name) {
  if (!value) {
    printf("FAIL: %s expected true\n", name);
    failures++;
  }
}

static void expect_false(Boolean value, const char *name) {
  if (value) {
    printf("FAIL: %s expected false\n", name);
    failures++;
  }
}

static void expect_u16(uint16_t actual, uint16_t expected, const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected %u got %u\n", name, expected, actual);
    failures++;
  }
}

static void expect_rect(Rect actual, Rect expected, const char *name) {
  if (actual.top != expected.top || actual.left != expected.left ||
      actual.bottom != expected.bottom || actual.right != expected.right) {
    printf("FAIL: %s expected {%d,%d,%d,%d} got {%d,%d,%d,%d}\n", name,
           expected.top, expected.left, expected.bottom, expected.right,
           actual.top, actual.left, actual.bottom, actual.right);
    failures++;
  }
}

static void expect_dirty_rect(DirtyRectList *list, uint8_t index, Rect expected,
                              const char *name) {
  DirtyRect *dirty = DR_GetRect(list, index);
  if (!dirty) {
    printf("FAIL: %s missing dirty rect %u\n", name, index);
    failures++;
    return;
  }
  expect_rect(dirty->rect, expected, name);
}

static void clips_to_bounds_and_rejects_empty(void) {
  Rect bounds = rect_make(0, 0, 224, 320);
  Rect r = rect_make(-5, -10, 20, 30);
  Rect outside = rect_make(230, 10, 240, 20);

  expect_true(DR_RectClipToBounds(&r, &bounds), "visible clipped rect");
  expect_rect(r, rect_make(0, 0, 20, 30), "clipped visible rect");
  expect_false(DR_RectClipToBounds(&outside, &bounds), "outside rect");
}

static void intersection_uses_half_open_edges(void) {
  Rect a = rect_make(10, 10, 20, 20);
  Rect touching = rect_make(10, 20, 20, 30);
  Rect overlapping = rect_make(15, 19, 25, 30);
  Rect out = rect_make(0, 0, 0, 0);

  expect_false(DR_RectIntersect(&a, &touching, &out),
               "touching edge is not intersection");
  expect_true(DR_RectIntersect(&a, &overlapping, &out), "overlap intersects");
  expect_rect(out, rect_make(15, 19, 20, 20), "intersection rect");
}

static void subtract_splits_cutout_into_strips(void) {
  Rect src = rect_make(10, 10, 30, 40);
  Rect cut = rect_make(15, 20, 25, 30);
  Rect out[4];
  uint8_t count = DR_RectSubtract(&src, &cut, out, 4);

  expect_u16(count, 4, "subtract centered count");
  expect_rect(out[0], rect_make(10, 10, 15, 40), "subtract top strip");
  expect_rect(out[1], rect_make(25, 10, 30, 40), "subtract bottom strip");
  expect_rect(out[2], rect_make(15, 10, 25, 20), "subtract left strip");
  expect_rect(out[3], rect_make(15, 30, 25, 40), "subtract right strip");
}

static void dirty_list_clips_merges_touching_and_keeps_separate(void) {
  Rect bounds = rect_make(0, 0, 224, 320);
  DirtyRect storage[3];
  DirtyRectList list;

  DR_InitList(&list, storage, 3, &bounds);
  expect_true(DR_AddRect(&list, &((Rect){-5, -5, 10, 10})),
              "add clipped rect");
  expect_true(DR_AddRect(&list, &((Rect){10, 0, 20, 10})),
              "add touching rect");
  expect_true(DR_AddRect(&list, &((Rect){100, 100, 110, 110})),
              "add separate rect");

  expect_u16(DR_GetCount(&list), 2, "dirty count after merge");
  expect_dirty_rect(&list, 0, rect_make(0, 0, 20, 10),
                    "merged touching rect");
  expect_dirty_rect(&list, 1, rect_make(100, 100, 110, 110), "separate rect");
}

static void dirty_list_keeps_corner_touching_rects_separate(void) {
  Rect bounds = rect_make(0, 0, 224, 320);
  DirtyRect storage[3];
  DirtyRectList list;

  DR_InitList(&list, storage, 3, &bounds);
  expect_true(DR_AddRect(&list, &((Rect){0, 0, 10, 10})), "corner first");
  expect_true(DR_AddRect(&list, &((Rect){10, 10, 20, 20})), "corner second");

  expect_u16(DR_GetCount(&list), 2, "corner touching count");
  expect_dirty_rect(&list, 0, rect_make(0, 0, 10, 10), "corner first rect");
  expect_dirty_rect(&list, 1, rect_make(10, 10, 20, 20),
                    "corner second rect");
}

static void dirty_list_overflow_collapses_to_single_bounds(void) {
  Rect bounds = rect_make(0, 0, 224, 320);
  DirtyRect storage[2];
  DirtyRectList list;

  DR_InitList(&list, storage, 2, &bounds);
  expect_true(DR_AddRect(&list, &((Rect){0, 0, 10, 10})), "overflow first");
  expect_true(DR_AddRect(&list, &((Rect){100, 100, 110, 110})),
              "overflow second");
  expect_true(DR_AddRect(&list, &((Rect){200, 200, 210, 210})),
              "overflow third");

  expect_u16(DR_GetCount(&list), 1, "overflow collapsed count");
  expect_dirty_rect(&list, 0, rect_make(0, 0, 210, 210),
                    "overflow collapsed rect");
}

static void dirty_rect_maps_to_tile_range(void) {
  DirtyTileRange range;
  Rect r = rect_make(9, 7, 17, 16);

  expect_true(DR_RectToTileRange(&r, 8, 8, &range), "tile range valid");
  expect_u16(range.x0, 0, "tile x0");
  expect_u16(range.y0, 1, "tile y0");
  expect_u16(range.x1, 2, "tile x1");
  expect_u16(range.y1, 3, "tile y1");
}

static void root_redraw_splits_menu_and_desktop(void) {
  DirtyRootRedraw plan;
  Rect dirty = rect_make(10, 5, 30, 50);

  DR_PlanRootRedraw(&dirty, 20, &plan);

  expect_true(plan.hasMenu, "root split has menu");
  expect_rect(plan.menu, rect_make(10, 5, 20, 50), "root split menu");
  expect_true(plan.hasDesktop, "root split has desktop");
  expect_rect(plan.desktop, rect_make(20, 5, 30, 50), "root split desktop");
}

static void root_redraw_keeps_single_owner_regions(void) {
  DirtyRootRedraw plan;
  Rect menuDirty = rect_make(0, 0, 12, 80);
  Rect desktopDirty = rect_make(40, 90, 100, 220);

  DR_PlanRootRedraw(&menuDirty, 20, &plan);
  expect_true(plan.hasMenu, "menu-only root plan has menu");
  expect_false(plan.hasDesktop, "menu-only root plan has no desktop");
  expect_rect(plan.menu, menuDirty, "menu-only root rect");

  DR_PlanRootRedraw(&desktopDirty, 20, &plan);
  expect_false(plan.hasMenu, "desktop-only root plan has no menu");
  expect_true(plan.hasDesktop, "desktop-only root plan has desktop");
  expect_rect(plan.desktop, desktopDirty, "desktop-only root rect");
}

static void root_redraw_rejects_empty_dirty_rects(void) {
  DirtyRootRedraw plan;
  Rect empty = rect_make(10, 10, 10, 20);

  plan.hasMenu = 1;
  plan.hasDesktop = 1;
  DR_PlanRootRedraw(&empty, 20, &plan);

  expect_false(plan.hasMenu, "empty root plan has no menu");
  expect_false(plan.hasDesktop, "empty root plan has no desktop");
}

int main(void) {
  clips_to_bounds_and_rejects_empty();
  intersection_uses_half_open_edges();
  subtract_splits_cutout_into_strips();
  dirty_list_clips_merges_touching_and_keeps_separate();
  dirty_list_keeps_corner_touching_rects_separate();
  dirty_list_overflow_collapses_to_single_bounds();
  dirty_rect_maps_to_tile_range();
  root_redraw_splits_menu_and_desktop();
  root_redraw_keeps_single_owner_regions();
  root_redraw_rejects_empty_dirty_rects();

  if (failures) {
    printf("%d dirty rect test(s) failed\n", failures);
    return 1;
  }

  printf("dirty rect tests passed\n");
  return 0;
}

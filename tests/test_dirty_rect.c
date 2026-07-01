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

static void expect_tile_upload(DirtyTileQueue *queue, uint8_t index,
                               uint16_t firstTile, uint16_t tileCount,
                               uint16_t byteCount, const char *name) {
  DirtyTileUpload *upload = DR_GetTileUpload(queue, index);
  if (!upload) {
    printf("FAIL: %s missing tile upload %u\n", name, index);
    failures++;
    return;
  }
  expect_u16(upload->firstTile, firstTile, name);
  expect_u16(upload->tileCount, tileCount, name);
  expect_u16(upload->byteCount, byteCount, name);
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

static void dirty_tile_budget_counts_partial_row_spans(void) {
  DirtyTileRange range = {2, 2, 4, 4};
  DirtyTransferBudget budget;

  expect_true(DR_TileRangeBudget(&range, 40, 32, 7524, &budget),
              "partial dirty budget valid");
  expect_u16(budget.firstTile, 82, "partial dirty first tile");
  expect_u16(budget.tileCount, 4, "partial dirty tile count");
  expect_u16(budget.byteCount, 128, "partial dirty byte count");
  expect_u16(budget.rowSpanCount, 2, "partial dirty row spans");
  expect_u16(budget.fitsBudget, 1, "partial dirty fits vblank budget");
}

static void dirty_tile_budget_counts_full_width_as_contiguous_span(void) {
  DirtyTileRange range = {0, 8, 40, 9};
  DirtyTransferBudget budget;

  expect_true(DR_TileRangeBudget(&range, 40, 32, 7524, &budget),
              "full-width row budget valid");
  expect_u16(budget.firstTile, 320, "full-width row first tile");
  expect_u16(budget.tileCount, 40, "full-width row tile count");
  expect_u16(budget.byteCount, 1280, "full-width row byte count");
  expect_u16(budget.rowSpanCount, 1, "full-width row contiguous span");
  expect_u16(budget.fitsBudget, 1, "full-width row fits vblank budget");
}

static void dirty_tile_budget_rejects_full_frame_ntsc_vblank_budget(void) {
  DirtyTileRange range = {0, 0, 40, 28};
  DirtyTransferBudget budget;

  expect_true(DR_TileRangeBudget(&range, 40, 32, 7524, &budget),
              "full-frame budget valid");
  expect_u16(budget.firstTile, 0, "full-frame first tile");
  expect_u16(budget.tileCount, 1120, "full-frame tile count");
  expect_u16(budget.byteCount, 35840, "full-frame byte count");
  expect_u16(budget.rowSpanCount, 1, "full-frame contiguous span");
  expect_u16(budget.fitsBudget, 0, "full-frame exceeds vblank budget");
}

static void dirty_tile_budget_rejects_empty_ranges(void) {
  DirtyTileRange range = {4, 4, 4, 5};
  DirtyTransferBudget budget;

  expect_false(DR_TileRangeBudget(&range, 40, 32, 7524, &budget),
               "empty tile budget rejected");
}

static void dirty_tile_queue_splits_partial_width_rows(void) {
  DirtyTileUpload uploads[4];
  DirtyTileQueue queue;
  DirtyTileRange range = {2, 2, 4, 4};

  DR_InitTileQueue(&queue, uploads, 4, 7524);

  expect_true(DR_QueueTileRange(&queue, &range, 40, 32),
              "queue partial-width range");
  expect_u16(queue.count, 2, "partial queue count");
  expect_u16(queue.byteCount, 128, "partial queue bytes");
  expect_false(queue.budgetExceeded, "partial queue budget flag");
  expect_false(queue.overflow, "partial queue overflow flag");
  expect_tile_upload(&queue, 0, 82, 2, 64, "partial queue row 0");
  expect_tile_upload(&queue, 1, 122, 2, 64, "partial queue row 1");
}

static void dirty_tile_queue_slices_full_frame_to_vblank_budget(void) {
  DirtyTileUpload uploads[4];
  DirtyTileQueue queue;
  DirtyTileRange range = {0, 0, 40, 28};

  DR_InitTileQueue(&queue, uploads, 4, 7524);

  expect_false(DR_QueueTileRange(&queue, &range, 40, 32),
               "full-frame queue exceeds budget");
  expect_u16(queue.count, 1, "full-frame queue count");
  expect_u16(queue.byteCount, 7520, "full-frame queued bytes");
  expect_true(queue.budgetExceeded, "full-frame queue budget flag");
  expect_false(queue.overflow, "full-frame queue overflow flag");
  expect_tile_upload(&queue, 0, 0, 235, 7520, "full-frame queue slice");
}

static void dirty_tile_queue_stops_when_row_budget_runs_out(void) {
  DirtyTileUpload uploads[4];
  DirtyTileQueue queue;
  DirtyTileRange range = {2, 2, 4, 5};

  DR_InitTileQueue(&queue, uploads, 4, 128);

  expect_false(DR_QueueTileRange(&queue, &range, 40, 32),
               "partial queue exceeds small budget");
  expect_u16(queue.count, 2, "small-budget queue count");
  expect_u16(queue.byteCount, 128, "small-budget queue bytes");
  expect_true(queue.budgetExceeded, "small-budget queue flag");
  expect_false(queue.overflow, "small-budget overflow flag");
  expect_tile_upload(&queue, 0, 82, 2, 64, "small-budget row 0");
  expect_tile_upload(&queue, 1, 122, 2, 64, "small-budget row 1");
}

static void dirty_tile_queue_reports_storage_overflow(void) {
  DirtyTileUpload upload;
  DirtyTileQueue queue;
  DirtyTileRange range = {2, 2, 4, 4};

  DR_InitTileQueue(&queue, &upload, 1, 7524);

  expect_false(DR_QueueTileRange(&queue, &range, 40, 32),
               "partial queue exceeds storage");
  expect_u16(queue.count, 1, "overflow queue count");
  expect_u16(queue.byteCount, 64, "overflow queue bytes");
  expect_false(queue.budgetExceeded, "overflow budget flag");
  expect_true(queue.overflow, "overflow flag");
  expect_tile_upload(&queue, 0, 82, 2, 64, "overflow kept first row");
}

static void dirty_tile_queue_builds_from_dirty_rect_list(void) {
  Rect bounds = rect_make(0, 0, 224, 320);
  DirtyRect dirtyStorage[2];
  DirtyRectList list;
  DirtyTileUpload uploads[4];
  DirtyTileQueue queue;

  DR_InitList(&list, dirtyStorage, 2, &bounds);
  expect_true(DR_AddRect(&list, &((Rect){16, 16, 32, 32})),
              "add queue source dirty rect");
  DR_InitTileQueue(&queue, uploads, 4, 7524);

  expect_true(DR_BuildTileQueueFromDirtyList(&list, &queue, 8, 8, 40, 32),
              "build queue from dirty list");
  expect_u16(queue.count, 2, "dirty-list queue count");
  expect_u16(queue.byteCount, 128, "dirty-list queue bytes");
  expect_tile_upload(&queue, 0, 82, 2, 64, "dirty-list queue row 0");
  expect_tile_upload(&queue, 1, 122, 2, 64, "dirty-list queue row 1");
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

static void window_redraw_clips_to_dirty_region(void) {
  DirtyWindowRedraw plan;
  Rect dirty = rect_make(40, 50, 80, 120);
  Rect window = rect_make(34, 40, 153, 258);

  DR_PlanWindowRedraw(&dirty, &window, &plan);

  expect_true(plan.hasWindow, "window redraw intersects");
  expect_rect(plan.clip, dirty, "window redraw clip");
}

static void window_redraw_clips_partial_overlap(void) {
  DirtyWindowRedraw plan;
  Rect dirty = rect_make(20, 20, 60, 90);
  Rect window = rect_make(34, 40, 153, 258);

  DR_PlanWindowRedraw(&dirty, &window, &plan);

  expect_true(plan.hasWindow, "window redraw partial intersects");
  expect_rect(plan.clip, rect_make(34, 40, 60, 90),
              "window redraw partial clip");
}

static void window_redraw_rejects_non_overlapping_regions(void) {
  DirtyWindowRedraw plan;
  Rect dirty = rect_make(0, 0, 20, 320);
  Rect window = rect_make(34, 40, 153, 258);

  plan.hasWindow = 1;
  DR_PlanWindowRedraw(&dirty, &window, &plan);

  expect_false(plan.hasWindow, "window redraw no intersection");
}

int main(void) {
  clips_to_bounds_and_rejects_empty();
  intersection_uses_half_open_edges();
  subtract_splits_cutout_into_strips();
  dirty_list_clips_merges_touching_and_keeps_separate();
  dirty_list_keeps_corner_touching_rects_separate();
  dirty_list_overflow_collapses_to_single_bounds();
  dirty_rect_maps_to_tile_range();
  dirty_tile_budget_counts_partial_row_spans();
  dirty_tile_budget_counts_full_width_as_contiguous_span();
  dirty_tile_budget_rejects_full_frame_ntsc_vblank_budget();
  dirty_tile_budget_rejects_empty_ranges();
  dirty_tile_queue_splits_partial_width_rows();
  dirty_tile_queue_slices_full_frame_to_vblank_budget();
  dirty_tile_queue_stops_when_row_budget_runs_out();
  dirty_tile_queue_reports_storage_overflow();
  dirty_tile_queue_builds_from_dirty_rect_list();
  root_redraw_splits_menu_and_desktop();
  root_redraw_keeps_single_owner_regions();
  root_redraw_rejects_empty_dirty_rects();
  window_redraw_clips_to_dirty_region();
  window_redraw_clips_partial_overlap();
  window_redraw_rejects_non_overlapping_regions();

  if (failures) {
    printf("%d dirty rect test(s) failed\n", failures);
    return 1;
  }

  printf("dirty rect tests passed\n");
  return 0;
}

#include "app_desktop_host.h"
#include <stdio.h>

typedef struct DesktopHostFixture {
  uint16_t requestCalls;
  uint16_t drawCalls;
  uint16_t saveCalls;
  uint16_t lastWindowId;
  uint16_t lastDrawX;
  uint16_t lastDrawY;
  uint16_t lastSaveBytes;
  uint8_t failRequest;
  uint8_t failDraw;
  uint8_t failSave;
  const char *lastText;
  const uint8_t *lastSaveData;
} DesktopHostFixture;

static int failures;

static void expect_true(uint8_t value, const char *name) {
  if (!value) {
    printf("FAIL: %s expected true\n", name);
    failures++;
  }
}

static void expect_false(uint8_t value, const char *name) {
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

static void expect_status(AppRuntimeStatus actual, AppRuntimeStatus expected,
                          const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected status %u got %u\n", name, (unsigned)expected,
           (unsigned)actual);
    failures++;
  }
}

static void expect_text(const char *actual, const char *expected,
                        const char *name) {
  uint16_t i = 0;
  if (!actual || !expected) {
    printf("FAIL: %s expected non-null text\n", name);
    failures++;
    return;
  }
  while (actual[i] || expected[i]) {
    if (actual[i] != expected[i]) {
      printf("FAIL: %s expected '%s' got '%s'\n", name, expected, actual);
      failures++;
      return;
    }
    i++;
  }
}

static uint8_t desktop_request_window(void *user,
                                      const AppCatalogEntry *catalog,
                                      uint16_t width, uint16_t height,
                                      uint16_t *outWindowId) {
  DesktopHostFixture *fixture = (DesktopHostFixture *)user;
  fixture->requestCalls++;
  expect_text(catalog->name, TEXT_APP_NAME, "requested catalog app");
  expect_u16(width, TEXT_APP_MIN_WIDTH, "requested desktop width");
  expect_u16(height, TEXT_APP_MIN_HEIGHT, "requested desktop height");
  if (fixture->failRequest) {
    return 0;
  }
  *outWindowId = 21;
  fixture->lastWindowId = *outWindowId;
  return 1;
}

static uint8_t desktop_draw_text(void *user, uint16_t windowId, uint16_t x,
                                 uint16_t y, const char *text) {
  DesktopHostFixture *fixture = (DesktopHostFixture *)user;
  fixture->drawCalls++;
  fixture->lastWindowId = windowId;
  fixture->lastDrawX = x;
  fixture->lastDrawY = y;
  fixture->lastText = text;
  return (uint8_t)!fixture->failDraw;
}

static uint8_t desktop_save_document(void *user, const uint8_t *data,
                                     uint16_t bytes) {
  DesktopHostFixture *fixture = (DesktopHostFixture *)user;
  fixture->saveCalls++;
  fixture->lastSaveData = data;
  fixture->lastSaveBytes = bytes;
  return (uint8_t)!fixture->failSave;
}

static AppDesktopHostOps make_ops(DesktopHostFixture *fixture) {
  AppDesktopHostOps ops;
  ops.maxWindows = 1;
  ops.maxWindowWidth = 320;
  ops.maxWindowHeight = 224;
  ops.maxDocumentBytes = 2048;
  ops.scratchBytes = 512;
  ops.user = fixture;
  ops.requestWindow = desktop_request_window;
  ops.drawText = desktop_draw_text;
  ops.saveDocument = desktop_save_document;
  return ops;
}

static void hosts_text_app_with_desktop_callbacks(void) {
  DesktopHostFixture fixture = {0};
  AppDesktopHost host;
  AppDesktopHostOps ops = make_ops(&fixture);
  AppRuntimeEvent event = {APP_EVENT_POINTER_DOWN, 4, 8, 0};

  ADH_Init(&host, &ops);
  expect_u16(ADH_AppCount(&host), 1, "desktop app count");
  expect_status(ADH_OpenText(&host), APP_RT_OK, "open desktop text app");
  expect_true(ADH_IsRunning(&host), "desktop host running");
  expect_text(ADH_ActiveName(&host), TEXT_APP_NAME, "desktop active name");
  expect_u16(ADH_TextWindowId(&host), 21, "desktop text window id");
  expect_u16(fixture.requestCalls, 1, "desktop window request calls");

  expect_status(ADH_SendEvent(&host, &event), APP_RT_OK,
                "desktop host event");
  expect_u16(host.textState.eventCalls, 1, "desktop text event calls");
  expect_u16(host.textState.lastEventType, APP_EVENT_POINTER_DOWN,
             "desktop text event type");

  expect_status(ADH_Draw(&host, 3, 5, TEXT_APP_MIN_WIDTH, TEXT_APP_MIN_HEIGHT),
                APP_RT_OK, "desktop host draw");
  expect_u16(host.textState.drawCalls, 1, "desktop text draw calls");
  expect_u16(fixture.drawCalls, 3, "desktop draw callbacks");
  expect_u16(fixture.lastDrawX, 3 + TEXT_APP_TEXT_X, "desktop draw x");
  expect_u16(fixture.lastDrawY,
             5 + TEXT_APP_TEXT_Y + (TEXT_APP_LINE_STEP * 2U),
             "desktop draw y");
  expect_text(fixture.lastText, "Event received", "desktop last text");

  expect_status(ADH_SaveActive(&host), APP_RT_OK, "desktop host save");
  expect_u16(host.textState.saveCalls, 1, "desktop text save calls");
  expect_u16(fixture.saveCalls, 1, "desktop save callbacks");
  expect_u16(fixture.lastSaveBytes, TEXT_APP_DOCUMENT_BYTES,
             "desktop save bytes");
  expect_true(fixture.lastSaveData != 0, "desktop save data pointer");

  expect_status(ADH_Close(&host), APP_RT_OK, "desktop host close");
  expect_false(ADH_IsRunning(&host), "desktop host closed");
  expect_u16(host.textState.exitCalls, 1, "desktop text exit calls");
}

static void reports_desktop_callback_failures(void) {
  DesktopHostFixture fixture = {0};
  AppDesktopHost host;
  AppDesktopHostOps ops = make_ops(&fixture);

  fixture.failRequest = 1;
  ADH_Init(&host, &ops);
  expect_status(ADH_OpenText(&host), APP_RT_INIT_FAILED,
                "desktop request failure");
  expect_false(ADH_IsRunning(&host), "request failure not running");
  fixture.failRequest = 0;

  ADH_Init(&host, &ops);
  expect_status(ADH_OpenText(&host), APP_RT_OK, "open for failure checks");
  fixture.failDraw = 1;
  expect_status(ADH_Draw(&host, 0, 0, TEXT_APP_MIN_WIDTH,
                         TEXT_APP_MIN_HEIGHT),
                APP_RT_DRAW_FAILED, "desktop draw failure");
  fixture.failDraw = 0;

  fixture.failSave = 1;
  expect_status(ADH_SaveActive(&host), APP_RT_COMMAND_FAILED,
                "desktop save failure");
  fixture.failSave = 0;
  expect_status(ADH_Close(&host), APP_RT_OK, "close after failure checks");
}

static void redraws_after_event_close_and_reopen(void) {
  DesktopHostFixture fixture = {0};
  AppDesktopHost host;
  AppDesktopHostOps ops = make_ops(&fixture);
  AppRuntimeEvent event = {APP_EVENT_TIMER, 1, 0, 0};

  ADH_Init(&host, &ops);
  expect_status(ADH_OpenText(&host), APP_RT_OK, "boundary first open");
  expect_status(ADH_SendEvent(&host, &event), APP_RT_OK,
                "boundary first event");
  expect_status(ADH_Close(&host), APP_RT_OK, "boundary close");
  expect_false(ADH_IsRunning(&host), "boundary returned to shell");
  expect_status(ADH_OpenText(&host), APP_RT_OK, "boundary reopen");
  expect_true(ADH_IsRunning(&host), "boundary running after reopen");
  expect_status(ADH_Draw(&host, 0, 0, TEXT_APP_MIN_WIDTH,
                         TEXT_APP_MIN_HEIGHT),
                APP_RT_OK, "boundary redraw after reopen");

  expect_u16(host.textState.initCalls, 2, "boundary init calls");
  expect_u16(host.textState.eventCalls, 1, "boundary event calls");
  expect_u16(host.textState.exitCalls, 1, "boundary exit calls");
  expect_u16(host.textState.drawCalls, 1, "boundary draw calls");
  expect_u16(fixture.drawCalls, 3, "boundary draw callbacks");
  expect_text(fixture.lastText, "Event+close OK", "boundary status text");

  expect_status(ADH_Close(&host), APP_RT_OK, "boundary final close");
}

int main(void) {
  hosts_text_app_with_desktop_callbacks();
  reports_desktop_callback_failures();
  redraws_after_event_close_and_reopen();

  if (failures) {
    printf("app desktop host tests failed: %d\n", failures);
    return 1;
  }

  printf("app desktop host tests passed\n");
  return 0;
}

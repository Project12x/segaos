#include "app_shell.h"
#include "text_app.h"
#include <stdio.h>

typedef struct ShellFixture {
  uint16_t windowRequests;
  uint16_t drawTextCalls;
  uint16_t saveCalls;
  uint16_t lastWindowId;
  uint16_t lastDrawX;
  uint16_t lastDrawY;
  uint16_t lastSaveBytes;
  const char *lastText;
  const uint8_t *lastSaveData;
} ShellFixture;

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

static uint8_t shell_request_window(AppRuntimeContext *ctx, uint16_t width,
                                    uint16_t height,
                                    uint16_t *outWindowId) {
  ShellFixture *fixture = (ShellFixture *)ctx->services->user;
  fixture->windowRequests++;
  expect_u16(width, TEXT_APP_MIN_WIDTH, "text app window width");
  expect_u16(height, TEXT_APP_MIN_HEIGHT, "text app window height");
  *outWindowId = 11;
  fixture->lastWindowId = *outWindowId;
  return 1;
}

static uint8_t shell_draw_text(AppRuntimeContext *ctx, uint16_t windowId,
                               uint16_t x, uint16_t y, const char *text) {
  ShellFixture *fixture = (ShellFixture *)ctx->services->user;
  (void)ctx;
  fixture->drawTextCalls++;
  fixture->lastWindowId = windowId;
  fixture->lastDrawX = x;
  fixture->lastDrawY = y;
  fixture->lastText = text;
  return 1;
}

static uint8_t shell_save_document(AppRuntimeContext *ctx,
                                   const uint8_t *data, uint16_t bytes) {
  ShellFixture *fixture = (ShellFixture *)ctx->services->user;
  (void)ctx;
  fixture->saveCalls++;
  fixture->lastSaveData = data;
  fixture->lastSaveBytes = bytes;
  return 1;
}

static AppRuntimeServices make_services(ShellFixture *fixture) {
  AppRuntimeServices services;
  services.version = APP_RUNTIME_SERVICE_VERSION;
  services.sizeBytes = sizeof(AppRuntimeServices);
  services.limits.maxWindows = 2;
  services.limits.maxWindowWidth = 320;
  services.limits.maxWindowHeight = 224;
  services.limits.maxDocumentBytes = 2048;
  services.limits.scratchBytes = 512;
  services.requestWindow = shell_request_window;
  services.drawText = shell_draw_text;
  services.saveDocument = shell_save_document;
  services.user = fixture;
  return services;
}

static void make_text_builtin(TextAppState *state, AppDefinition *definition,
                              AppShellBuiltin *builtin) {
  TEXT_APP_InitState(state);
  *definition = TEXT_APP_MakeDefinition(state);
  TEXT_APP_FillCatalog(&builtin->catalog);
  builtin->definition = definition;
}

static void launches_text_app_through_shell_services(void) {
  ShellFixture fixture = {0};
  TextAppState textState;
  AppDefinition textDefinition;
  AppShellBuiltin builtin;
  AppShell shell;
  AppCatalogEntry found;
  AppRuntimeServices services = make_services(&fixture);
  AppRuntimeEvent event = {APP_EVENT_POINTER_DOWN, 14, 26, 0};
  AppRuntimeDraw draw = {0, 0, TEXT_APP_MIN_WIDTH, TEXT_APP_MIN_HEIGHT};

  make_text_builtin(&textState, &textDefinition, &builtin);
  APP_SHELL_Init(&shell, &services, &builtin, 1);

  expect_true(APP_SHELL_Find(&shell, TEXT_APP_NAME, &found),
              "find text app catalog entry");
  expect_text(found.title, TEXT_APP_TITLE, "text app title");

  expect_status(APP_SHELL_Open(&shell, TEXT_APP_NAME), APP_RT_OK,
                "open text app");
  expect_true(APP_SHELL_IsRunning(&shell), "shell runtime running");
  expect_text(APP_SHELL_ActiveName(&shell), TEXT_APP_NAME,
              "active shell app name");
  expect_u16(textState.initCalls, 1, "text init calls");
  expect_u16(fixture.windowRequests, 1, "window request count");
  expect_u16(textState.windowId, 11, "text app stored window id");

  expect_status(APP_SHELL_SendEvent(&shell, &event), APP_RT_OK,
                "send text app event");
  expect_u16(textState.eventCalls, 1, "text event calls");
  expect_u16(textState.lastEventType, APP_EVENT_POINTER_DOWN,
             "text last event type");

  expect_status(APP_SHELL_Draw(&shell, &draw), APP_RT_OK, "draw text app");
  expect_u16(textState.drawCalls, 1, "text draw calls");
  expect_u16(fixture.drawTextCalls, 2, "draw text service calls");
  expect_u16(fixture.lastWindowId, 11, "draw service window id");
  expect_u16(fixture.lastDrawX, TEXT_APP_TEXT_X, "draw service x");
  expect_u16(fixture.lastDrawY, TEXT_APP_TEXT_Y + TEXT_APP_LINE_STEP,
             "draw service y");
  expect_text(fixture.lastText, "OS-owned drawing", "last drawn text");

  expect_status(APP_SHELL_Command(&shell, APP_CMD_SAVE), APP_RT_OK,
                "save text app");
  expect_u16(textState.commandCalls, 1, "text command calls");
  expect_u16(textState.saveCalls, 1, "text save calls");
  expect_u16(fixture.saveCalls, 1, "save service calls");
  expect_u16(fixture.lastSaveBytes, TEXT_APP_DOCUMENT_BYTES,
             "saved document bytes");
  expect_true(fixture.lastSaveData != 0, "saved document pointer");
  if (fixture.lastSaveData) {
    expect_u16(fixture.lastSaveData[0], 'T', "saved byte 0");
    expect_u16(fixture.lastSaveData[7], 'P', "saved byte 7");
  }

  expect_status(APP_SHELL_Close(&shell), APP_RT_OK, "close text app");
  expect_false(APP_SHELL_IsRunning(&shell), "shell runtime closed");
  expect_u16(textState.exitCalls, 1, "text exit calls");
}

static void rejects_missing_apps_and_reopens_after_close(void) {
  ShellFixture fixture = {0};
  TextAppState textState;
  AppDefinition textDefinition;
  AppShellBuiltin builtin;
  AppShell shell;
  AppRuntimeServices services = make_services(&fixture);

  make_text_builtin(&textState, &textDefinition, &builtin);
  APP_SHELL_Init(&shell, &services, &builtin, 1);

  expect_status(APP_SHELL_Open(&shell, "BASIC.APP"), APP_RT_BAD_APP,
                "missing app open");
  expect_false(APP_SHELL_IsRunning(&shell), "missing app does not run");

  expect_status(APP_SHELL_Open(&shell, TEXT_APP_NAME), APP_RT_OK,
                "first text open");
  expect_status(APP_SHELL_Close(&shell), APP_RT_OK, "first text close");
  expect_status(APP_SHELL_Open(&shell, TEXT_APP_NAME), APP_RT_OK,
                "second text open");
  expect_u16(textState.initCalls, 2, "text app reopened init calls");
  expect_status(APP_SHELL_Close(&shell), APP_RT_OK, "second text close");
  expect_u16(textState.exitCalls, 2, "text app reopened exit calls");
}

int main(void) {
  launches_text_app_through_shell_services();
  rejects_missing_apps_and_reopens_after_close();

  if (failures) {
    printf("app shell tests failed: %d\n", failures);
    return 1;
  }

  printf("app shell tests passed\n");
  return 0;
}

#include "app_runtime.h"
#include <stdio.h>

typedef struct RuntimeFixture {
  uint16_t initCalls;
  uint16_t eventCalls;
  uint16_t drawCalls;
  uint16_t commandCalls;
  uint16_t exitCalls;
  uint16_t windowRequests;
  uint16_t drawTextCalls;
  uint16_t saveCalls;
  uint16_t lastEventType;
  uint16_t lastCommand;
  uint8_t failInit;
  uint8_t failEvent;
  uint8_t failDraw;
  uint8_t failCommand;
  uint8_t failExit;
} RuntimeFixture;

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
  while (actual[i] || expected[i]) {
    if (actual[i] != expected[i]) {
      printf("FAIL: %s expected '%s' got '%s'\n", name, expected, actual);
      failures++;
      return;
    }
    i++;
  }
}

static uint8_t fake_request_window(AppRuntimeContext *ctx, uint16_t width,
                                   uint16_t height, uint16_t *outWindowId) {
  RuntimeFixture *fixture = (RuntimeFixture *)ctx->services->user;
  fixture->windowRequests++;
  expect_u16(width, 160, "requested window width");
  expect_u16(height, 96, "requested window height");
  *outWindowId = 7;
  return 1;
}

static uint8_t fake_draw_text(AppRuntimeContext *ctx, uint16_t windowId,
                              uint16_t x, uint16_t y, const char *text) {
  RuntimeFixture *fixture = (RuntimeFixture *)ctx->services->user;
  fixture->drawTextCalls++;
  expect_u16(windowId, 7, "draw window id");
  expect_u16(x, 4, "draw x");
  expect_u16(y, 8, "draw y");
  expect_text(text, "Hello from TEXT.APP", "draw text");
  return 1;
}

static uint8_t fake_save_document(AppRuntimeContext *ctx, const uint8_t *data,
                                  uint16_t bytes) {
  RuntimeFixture *fixture = (RuntimeFixture *)ctx->services->user;
  fixture->saveCalls++;
  expect_true(data != 0, "save data pointer");
  expect_u16(bytes, 4, "save byte count");
  return 1;
}

static uint8_t text_app_init(AppRuntimeContext *ctx) {
  RuntimeFixture *fixture = (RuntimeFixture *)ctx->appState;
  fixture->initCalls++;
  if (fixture->failInit) {
    return 0;
  }
  return ctx->services->requestWindow(ctx, ctx->catalog->minWidth,
                                      ctx->catalog->minHeight,
                                      &ctx->windowId);
}

static uint8_t text_app_event(AppRuntimeContext *ctx,
                              const AppRuntimeEvent *event) {
  RuntimeFixture *fixture = (RuntimeFixture *)ctx->appState;
  fixture->eventCalls++;
  fixture->lastEventType = event->type;
  return (uint8_t)!fixture->failEvent;
}

static uint8_t text_app_draw(AppRuntimeContext *ctx,
                             const AppRuntimeDraw *draw) {
  RuntimeFixture *fixture = (RuntimeFixture *)ctx->appState;
  fixture->drawCalls++;
  expect_u16(draw->x, 0, "draw rect x");
  expect_u16(draw->width, 160, "draw rect width");
  if (fixture->failDraw) {
    return 0;
  }
  return ctx->services->drawText(ctx, ctx->windowId, 4, 8,
                                 "Hello from TEXT.APP");
}

static uint8_t text_app_command(AppRuntimeContext *ctx, uint16_t command) {
  static const uint8_t doc[4] = {'T', 'E', 'X', 'T'};
  RuntimeFixture *fixture = (RuntimeFixture *)ctx->appState;
  fixture->commandCalls++;
  fixture->lastCommand = command;
  if (fixture->failCommand) {
    return 0;
  }
  if (command == APP_CMD_SAVE) {
    return ctx->services->saveDocument(ctx, doc, sizeof(doc));
  }
  return 1;
}

static uint8_t text_app_exit(AppRuntimeContext *ctx) {
  RuntimeFixture *fixture = (RuntimeFixture *)ctx->appState;
  fixture->exitCalls++;
  return (uint8_t)!fixture->failExit;
}

static AppCatalogEntry make_text_entry(void) {
  AppCatalogEntry entry;
  uint16_t i;
  entry.id = 1;
  for (i = 0; i <= APP_CATALOG_NAME_BYTES; i++) {
    entry.name[i] = 0;
  }
  for (i = 0; i <= APP_CATALOG_TITLE_BYTES; i++) {
    entry.title[i] = 0;
  }
  entry.name[0] = 'T';
  entry.name[1] = 'E';
  entry.name[2] = 'X';
  entry.name[3] = 'T';
  entry.name[4] = '.';
  entry.name[5] = 'A';
  entry.name[6] = 'P';
  entry.name[7] = 'P';
  entry.title[0] = 'T';
  entry.title[1] = 'e';
  entry.title[2] = 'x';
  entry.title[3] = 't';
  entry.kind = APP_KIND_BUILTIN;
  entry.flags = APP_CAP_WINDOW | APP_CAP_DOCUMENT | APP_CAP_STORAGE |
                APP_CAP_TEXT;
  entry.moduleLba = 0;
  entry.moduleBytes = 0;
  entry.resourceLba = 0;
  entry.resourceBytes = 0;
  entry.minWidth = 160;
  entry.minHeight = 96;
  return entry;
}

static AppRuntimeServices make_services(RuntimeFixture *fixture) {
  AppRuntimeServices services;
  services.version = APP_RUNTIME_SERVICE_VERSION;
  services.sizeBytes = sizeof(AppRuntimeServices);
  services.limits.maxWindows = 2;
  services.limits.maxWindowWidth = 320;
  services.limits.maxWindowHeight = 224;
  services.limits.maxDocumentBytes = 4096;
  services.limits.scratchBytes = 512;
  services.requestWindow = fake_request_window;
  services.drawText = fake_draw_text;
  services.saveDocument = fake_save_document;
  services.user = fixture;
  return services;
}

static AppDefinition make_text_definition(RuntimeFixture *fixture) {
  AppDefinition app;
  app.name = "TEXT.APP";
  app.appState = fixture;
  app.init = text_app_init;
  app.event = text_app_event;
  app.draw = text_app_draw;
  app.command = text_app_command;
  app.exit = text_app_exit;
  return app;
}

static void runs_app_lifecycle_through_services(void) {
  RuntimeFixture fixture = {0};
  AppRuntime runtime;
  AppCatalogEntry entry = make_text_entry();
  AppRuntimeServices services = make_services(&fixture);
  AppDefinition app = make_text_definition(&fixture);
  AppRuntimeEvent event = {APP_EVENT_POINTER_DOWN, 3, 4, 0};
  AppRuntimeDraw draw = {0, 0, 160, 96};

  APP_RT_Init(&runtime);
  expect_status(APP_RT_Start(&runtime, &services, &entry, &app), APP_RT_OK,
                "start text app");
  expect_true(APP_RT_IsRunning(&runtime), "runtime running after start");
  expect_text(APP_RT_ActiveName(&runtime), "TEXT.APP", "active app name");
  expect_u16(fixture.initCalls, 1, "init calls");
  expect_u16(fixture.windowRequests, 1, "window requests");

  expect_status(APP_RT_SendEvent(&runtime, &event), APP_RT_OK, "send event");
  expect_u16(fixture.eventCalls, 1, "event calls");
  expect_u16(fixture.lastEventType, APP_EVENT_POINTER_DOWN, "event type");

  expect_status(APP_RT_Draw(&runtime, &draw), APP_RT_OK, "draw app");
  expect_u16(fixture.drawCalls, 1, "draw calls");
  expect_u16(fixture.drawTextCalls, 1, "draw text calls");

  expect_status(APP_RT_Command(&runtime, APP_CMD_SAVE), APP_RT_OK,
                "save command");
  expect_u16(fixture.commandCalls, 1, "command calls");
  expect_u16(fixture.lastCommand, APP_CMD_SAVE, "last command");
  expect_u16(fixture.saveCalls, 1, "save calls");

  expect_status(APP_RT_Stop(&runtime), APP_RT_OK, "stop app");
  expect_false(APP_RT_IsRunning(&runtime), "runtime not running after stop");
  expect_u16(fixture.exitCalls, 1, "exit calls");
}

static void rejects_bad_runtime_boundaries(void) {
  RuntimeFixture fixture = {0};
  AppRuntime runtime;
  AppCatalogEntry entry = make_text_entry();
  AppRuntimeServices services = make_services(&fixture);
  AppDefinition app = make_text_definition(&fixture);

  APP_RT_Init(&runtime);
  expect_status(APP_RT_Start(&runtime, 0, &entry, &app), APP_RT_BAD_ARGUMENT,
                "null services");

  services.version = 99;
  expect_status(APP_RT_Start(&runtime, &services, &entry, &app),
                APP_RT_BAD_SERVICES, "bad service version");
  services = make_services(&fixture);
  services.sizeBytes = 4;
  expect_status(APP_RT_Start(&runtime, &services, &entry, &app),
                APP_RT_BAD_SERVICES, "bad service size");

  services = make_services(&fixture);
  app.name = "BASIC.APP";
  expect_status(APP_RT_Start(&runtime, &services, &entry, &app),
                APP_RT_NAME_MISMATCH, "app name mismatch");

  app = make_text_definition(&fixture);
  entry.flags = APP_CAP_DOCUMENT | APP_CAP_STORAGE | APP_CAP_TEXT;
  expect_status(APP_RT_Start(&runtime, &services, &entry, &app),
                APP_RT_BAD_APP, "missing window capability");

  entry = make_text_entry();
  entry.minWidth = 0;
  expect_status(APP_RT_Start(&runtime, &services, &entry, &app),
                APP_RT_BAD_APP, "missing minimum window width");

  entry = make_text_entry();
  app = make_text_definition(&fixture);
  services.limits.maxWindowWidth = 120;
  expect_status(APP_RT_Start(&runtime, &services, &entry, &app),
                APP_RT_RESOURCE_LIMIT, "window width limit");
}

static void reports_lifecycle_failures(void) {
  RuntimeFixture fixture = {0};
  AppRuntime runtime;
  AppCatalogEntry entry = make_text_entry();
  AppRuntimeServices services = make_services(&fixture);
  AppDefinition app = make_text_definition(&fixture);
  AppRuntimeEvent event = {APP_EVENT_KEY_DOWN, 65, 0, 0};
  AppRuntimeDraw draw = {0, 0, 160, 96};

  APP_RT_Init(&runtime);
  fixture.failInit = 1;
  expect_status(APP_RT_Start(&runtime, &services, &entry, &app),
                APP_RT_INIT_FAILED, "init failure");
  expect_false(APP_RT_IsRunning(&runtime), "not running after failed init");
  fixture.failInit = 0;

  expect_status(APP_RT_Start(&runtime, &services, &entry, &app), APP_RT_OK,
                "start for failure checks");

  fixture.failEvent = 1;
  expect_status(APP_RT_SendEvent(&runtime, &event), APP_RT_EVENT_FAILED,
                "event failure");
  fixture.failEvent = 0;

  fixture.failDraw = 1;
  expect_status(APP_RT_Draw(&runtime, &draw), APP_RT_DRAW_FAILED,
                "draw failure");
  fixture.failDraw = 0;

  fixture.failCommand = 1;
  expect_status(APP_RT_Command(&runtime, APP_CMD_SAVE),
                APP_RT_COMMAND_FAILED, "command failure");
  fixture.failCommand = 0;

  fixture.failExit = 1;
  expect_status(APP_RT_Stop(&runtime), APP_RT_EXIT_FAILED, "exit failure");
  expect_true(APP_RT_IsRunning(&runtime), "still running after failed exit");
  fixture.failExit = 0;
  expect_status(APP_RT_Stop(&runtime), APP_RT_OK, "exit after failure cleared");
}

static void prevents_overlapping_apps(void) {
  RuntimeFixture fixture = {0};
  AppRuntime runtime;
  AppCatalogEntry entry = make_text_entry();
  AppRuntimeServices services = make_services(&fixture);
  AppDefinition app = make_text_definition(&fixture);

  APP_RT_Init(&runtime);
  expect_status(APP_RT_Start(&runtime, &services, &entry, &app), APP_RT_OK,
                "start first app");
  expect_status(APP_RT_Start(&runtime, &services, &entry, &app),
                APP_RT_ALREADY_RUNNING, "reject overlapping app");
  expect_status(APP_RT_Stop(&runtime), APP_RT_OK, "stop first app");
}

int main(void) {
  runs_app_lifecycle_through_services();
  rejects_bad_runtime_boundaries();
  reports_lifecycle_failures();
  prevents_overlapping_apps();

  if (failures) {
    printf("app runtime tests failed: %d\n", failures);
    return 1;
  }

  printf("app runtime tests passed\n");
  return 0;
}

#ifndef SEGAOS_APP_RUNTIME_H
#define SEGAOS_APP_RUNTIME_H

#include "app_catalog.h"
#include <stdint.h>

#define APP_RUNTIME_SERVICE_VERSION 1U

typedef enum AppRuntimeStatus {
  APP_RT_OK = 0,
  APP_RT_BAD_ARGUMENT = 1,
  APP_RT_BAD_SERVICES = 2,
  APP_RT_BAD_APP = 3,
  APP_RT_NAME_MISMATCH = 4,
  APP_RT_RESOURCE_LIMIT = 5,
  APP_RT_ALREADY_RUNNING = 6,
  APP_RT_NO_APP = 7,
  APP_RT_INIT_FAILED = 8,
  APP_RT_EVENT_FAILED = 9,
  APP_RT_DRAW_FAILED = 10,
  APP_RT_COMMAND_FAILED = 11,
  APP_RT_EXIT_FAILED = 12
} AppRuntimeStatus;

typedef enum AppRuntimeEventType {
  APP_EVENT_NONE = 0,
  APP_EVENT_POINTER_DOWN = 1,
  APP_EVENT_POINTER_UP = 2,
  APP_EVENT_KEY_DOWN = 3,
  APP_EVENT_TIMER = 4,
  APP_EVENT_STORAGE_DONE = 5
} AppRuntimeEventType;

typedef enum AppRuntimeCommand {
  APP_CMD_OPEN = 1,
  APP_CMD_SAVE = 2,
  APP_CMD_CLOSE = 3,
  APP_CMD_RUN = 4,
  APP_CMD_ABOUT = 5
} AppRuntimeCommand;

typedef struct AppRuntimeLimits {
  uint16_t maxWindows;
  uint16_t maxWindowWidth;
  uint16_t maxWindowHeight;
  uint16_t maxDocumentBytes;
  uint16_t scratchBytes;
} AppRuntimeLimits;

typedef struct AppRuntimeEvent {
  uint16_t type;
  uint16_t param0;
  uint16_t param1;
  uint16_t param2;
} AppRuntimeEvent;

typedef struct AppRuntimeDraw {
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
} AppRuntimeDraw;

struct AppRuntimeServices;

typedef struct AppRuntimeContext {
  const struct AppRuntimeServices *services;
  const AppCatalogEntry *catalog;
  void *appState;
  uint16_t windowId;
} AppRuntimeContext;

typedef uint8_t (*AppRequestWindowFn)(AppRuntimeContext *ctx, uint16_t width,
                                      uint16_t height,
                                      uint16_t *outWindowId);
typedef uint8_t (*AppDrawTextFn)(AppRuntimeContext *ctx, uint16_t windowId,
                                 uint16_t x, uint16_t y, const char *text);
typedef uint8_t (*AppSaveDocumentFn)(AppRuntimeContext *ctx,
                                     const uint8_t *data, uint16_t bytes);

typedef struct AppRuntimeServices {
  uint16_t version;
  uint16_t sizeBytes;
  AppRuntimeLimits limits;
  AppRequestWindowFn requestWindow;
  AppDrawTextFn drawText;
  AppSaveDocumentFn saveDocument;
  void *user;
} AppRuntimeServices;

typedef uint8_t (*AppInitFn)(AppRuntimeContext *ctx);
typedef uint8_t (*AppEventFn)(AppRuntimeContext *ctx,
                              const AppRuntimeEvent *event);
typedef uint8_t (*AppDrawFn)(AppRuntimeContext *ctx,
                             const AppRuntimeDraw *draw);
typedef uint8_t (*AppCommandFn)(AppRuntimeContext *ctx, uint16_t command);
typedef uint8_t (*AppExitFn)(AppRuntimeContext *ctx);

typedef struct AppDefinition {
  const char *name;
  void *appState;
  AppInitFn init;
  AppEventFn event;
  AppDrawFn draw;
  AppCommandFn command;
  AppExitFn exit;
} AppDefinition;

typedef struct AppRuntime {
  uint8_t running;
  AppCatalogEntry activeCatalog;
  AppRuntimeContext context;
  const AppDefinition *definition;
} AppRuntime;

void APP_RT_Init(AppRuntime *runtime);
uint8_t APP_RT_IsRunning(const AppRuntime *runtime);
const char *APP_RT_ActiveName(const AppRuntime *runtime);
AppRuntimeStatus APP_RT_Start(AppRuntime *runtime,
                              const AppRuntimeServices *services,
                              const AppCatalogEntry *catalog,
                              const AppDefinition *definition);
AppRuntimeStatus APP_RT_SendEvent(AppRuntime *runtime,
                                  const AppRuntimeEvent *event);
AppRuntimeStatus APP_RT_Draw(AppRuntime *runtime,
                             const AppRuntimeDraw *draw);
AppRuntimeStatus APP_RT_Command(AppRuntime *runtime, uint16_t command);
AppRuntimeStatus APP_RT_Stop(AppRuntime *runtime);

#endif

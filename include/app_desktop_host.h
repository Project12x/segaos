#ifndef SEGAOS_APP_DESKTOP_HOST_H
#define SEGAOS_APP_DESKTOP_HOST_H

#include "app_shell.h"
#include "text_app.h"
#include <stdint.h>

typedef uint8_t (*AppDesktopHostRequestWindowFn)(
    void *user, const AppCatalogEntry *catalog, uint16_t width,
    uint16_t height, uint16_t *outWindowId);
typedef uint8_t (*AppDesktopHostDrawTextFn)(void *user, uint16_t windowId,
                                            uint16_t x, uint16_t y,
                                            const char *text);
typedef uint8_t (*AppDesktopHostSaveDocumentFn)(void *user,
                                                const uint8_t *data,
                                                uint16_t bytes);

typedef struct AppDesktopHostOps {
  uint16_t maxWindows;
  uint16_t maxWindowWidth;
  uint16_t maxWindowHeight;
  uint16_t maxDocumentBytes;
  uint16_t scratchBytes;
  void *user;
  AppDesktopHostRequestWindowFn requestWindow;
  AppDesktopHostDrawTextFn drawText;
  AppDesktopHostSaveDocumentFn saveDocument;
} AppDesktopHostOps;

typedef struct AppDesktopHost {
  AppShell shell;
  AppRuntimeServices services;
  TextAppState textState;
  AppDefinition textDefinition;
  AppShellBuiltin textBuiltin;
  AppDesktopHostOps ops;
} AppDesktopHost;

void ADH_Init(AppDesktopHost *host, const AppDesktopHostOps *ops);
uint16_t ADH_AppCount(const AppDesktopHost *host);
AppRuntimeStatus ADH_OpenText(AppDesktopHost *host);
AppRuntimeStatus ADH_SendEvent(AppDesktopHost *host,
                               const AppRuntimeEvent *event);
AppRuntimeStatus ADH_Draw(AppDesktopHost *host, uint16_t x, uint16_t y,
                          uint16_t width, uint16_t height);
AppRuntimeStatus ADH_SaveActive(AppDesktopHost *host);
AppRuntimeStatus ADH_Close(AppDesktopHost *host);
uint8_t ADH_IsRunning(const AppDesktopHost *host);
const char *ADH_ActiveName(const AppDesktopHost *host);
uint16_t ADH_TextWindowId(const AppDesktopHost *host);

#endif

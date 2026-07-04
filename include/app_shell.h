#ifndef SEGAOS_APP_SHELL_H
#define SEGAOS_APP_SHELL_H

#include "app_runtime.h"
#include <stdint.h>

typedef struct AppShellBuiltin {
  AppCatalogEntry catalog;
  const AppDefinition *definition;
} AppShellBuiltin;

typedef struct AppShell {
  AppRuntime runtime;
  const AppRuntimeServices *services;
  const AppShellBuiltin *builtins;
  uint16_t builtinCount;
} AppShell;

void APP_SHELL_Init(AppShell *shell, const AppRuntimeServices *services,
                    const AppShellBuiltin *builtins, uint16_t builtinCount);
uint16_t APP_SHELL_AppCount(const AppShell *shell);
uint8_t APP_SHELL_Find(const AppShell *shell, const char *name,
                       AppCatalogEntry *out);
AppRuntimeStatus APP_SHELL_Open(AppShell *shell, const char *name);
AppRuntimeStatus APP_SHELL_SendEvent(AppShell *shell,
                                     const AppRuntimeEvent *event);
AppRuntimeStatus APP_SHELL_Draw(AppShell *shell, const AppRuntimeDraw *draw);
AppRuntimeStatus APP_SHELL_Command(AppShell *shell, uint16_t command);
AppRuntimeStatus APP_SHELL_Close(AppShell *shell);
uint8_t APP_SHELL_IsRunning(const AppShell *shell);
const char *APP_SHELL_ActiveName(const AppShell *shell);

#endif

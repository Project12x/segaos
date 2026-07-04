#include "app_shell.h"

static uint8_t app_shell_cstr_equal(const char *a, const char *b) {
  uint16_t i = 0;

  if (!a || !b) {
    return 0;
  }

  while (a[i] || b[i]) {
    if (a[i] != b[i]) {
      return 0;
    }
    i++;
  }

  return 1;
}

static const AppShellBuiltin *app_shell_find_builtin(const AppShell *shell,
                                                     const char *name) {
  uint16_t i;

  if (!shell || !name || !APP_IsValidIsoName(name)) {
    return (const AppShellBuiltin *)0;
  }

  for (i = 0; i < shell->builtinCount; i++) {
    const AppShellBuiltin *builtin = &shell->builtins[i];
    if (builtin->definition &&
        app_shell_cstr_equal(builtin->catalog.name, name)) {
      return builtin;
    }
  }

  return (const AppShellBuiltin *)0;
}

void APP_SHELL_Init(AppShell *shell, const AppRuntimeServices *services,
                    const AppShellBuiltin *builtins, uint16_t builtinCount) {
  if (!shell) {
    return;
  }

  APP_RT_Init(&shell->runtime);
  shell->services = services;
  shell->builtins = builtins;
  shell->builtinCount = builtinCount;
}

uint16_t APP_SHELL_AppCount(const AppShell *shell) {
  if (!shell) {
    return 0;
  }
  return shell->builtinCount;
}

uint8_t APP_SHELL_Find(const AppShell *shell, const char *name,
                       AppCatalogEntry *out) {
  const AppShellBuiltin *builtin;

  if (!out) {
    return 0;
  }

  builtin = app_shell_find_builtin(shell, name);
  if (!builtin) {
    return 0;
  }

  *out = builtin->catalog;
  return 1;
}

AppRuntimeStatus APP_SHELL_Open(AppShell *shell, const char *name) {
  const AppShellBuiltin *builtin;

  if (!shell || !name) {
    return APP_RT_BAD_ARGUMENT;
  }

  builtin = app_shell_find_builtin(shell, name);
  if (!builtin) {
    return APP_RT_BAD_APP;
  }

  return APP_RT_Start(&shell->runtime, shell->services, &builtin->catalog,
                      builtin->definition);
}

AppRuntimeStatus APP_SHELL_SendEvent(AppShell *shell,
                                     const AppRuntimeEvent *event) {
  if (!shell) {
    return APP_RT_BAD_ARGUMENT;
  }
  return APP_RT_SendEvent(&shell->runtime, event);
}

AppRuntimeStatus APP_SHELL_Draw(AppShell *shell, const AppRuntimeDraw *draw) {
  if (!shell) {
    return APP_RT_BAD_ARGUMENT;
  }
  return APP_RT_Draw(&shell->runtime, draw);
}

AppRuntimeStatus APP_SHELL_Command(AppShell *shell, uint16_t command) {
  if (!shell) {
    return APP_RT_BAD_ARGUMENT;
  }
  return APP_RT_Command(&shell->runtime, command);
}

AppRuntimeStatus APP_SHELL_Close(AppShell *shell) {
  if (!shell) {
    return APP_RT_BAD_ARGUMENT;
  }
  return APP_RT_Stop(&shell->runtime);
}

uint8_t APP_SHELL_IsRunning(const AppShell *shell) {
  if (!shell) {
    return 0;
  }
  return APP_RT_IsRunning(&shell->runtime);
}

const char *APP_SHELL_ActiveName(const AppShell *shell) {
  if (!shell) {
    return "";
  }
  return APP_RT_ActiveName(&shell->runtime);
}

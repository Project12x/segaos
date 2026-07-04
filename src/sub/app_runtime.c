#include "app_runtime.h"

static uint8_t app_rt_cstr_equal(const char *a, const char *b) {
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

static void app_rt_clear_entry(AppCatalogEntry *entry) {
  uint16_t i;

  if (!entry) {
    return;
  }

  entry->id = 0;
  for (i = 0; i <= APP_CATALOG_NAME_BYTES; i++) {
    entry->name[i] = 0;
  }
  for (i = 0; i <= APP_CATALOG_TITLE_BYTES; i++) {
    entry->title[i] = 0;
  }
  entry->kind = 0;
  entry->flags = 0;
  entry->moduleLba = 0;
  entry->moduleBytes = 0;
  entry->resourceLba = 0;
  entry->resourceBytes = 0;
  entry->minWidth = 0;
  entry->minHeight = 0;
}

void APP_RT_Init(AppRuntime *runtime) {
  if (!runtime) {
    return;
  }

  runtime->running = 0;
  app_rt_clear_entry(&runtime->activeCatalog);
  runtime->context.services = (const AppRuntimeServices *)0;
  runtime->context.catalog = (const AppCatalogEntry *)0;
  runtime->context.appState = (void *)0;
  runtime->context.windowId = 0;
  runtime->definition = (const AppDefinition *)0;
}

uint8_t APP_RT_IsRunning(const AppRuntime *runtime) {
  if (!runtime) {
    return 0;
  }
  return runtime->running;
}

const char *APP_RT_ActiveName(const AppRuntime *runtime) {
  if (!runtime || !runtime->running) {
    return "";
  }
  return runtime->activeCatalog.name;
}

static uint8_t app_rt_services_valid(const AppRuntimeServices *services) {
  if (!services) {
    return 0;
  }

  if (services->version != APP_RUNTIME_SERVICE_VERSION ||
      services->sizeBytes < sizeof(AppRuntimeServices)) {
    return 0;
  }

  if (!services->requestWindow || !services->drawText ||
      !services->saveDocument) {
    return 0;
  }

  if (services->limits.maxWindows == 0 ||
      services->limits.maxWindowWidth == 0 ||
      services->limits.maxWindowHeight == 0) {
    return 0;
  }

  return 1;
}

static uint8_t app_rt_definition_valid(const AppDefinition *definition) {
  if (!definition || !definition->name || !definition->init ||
      !definition->event || !definition->draw || !definition->command ||
      !definition->exit) {
    return 0;
  }
  return APP_IsValidIsoName(definition->name);
}

static uint8_t app_rt_catalog_valid(const AppCatalogEntry *catalog) {
  if (!catalog || catalog->id == 0 || !APP_IsValidIsoName(catalog->name)) {
    return 0;
  }

  if (catalog->kind != APP_KIND_BUILTIN && catalog->kind != APP_KIND_MODULE) {
    return 0;
  }

  if (catalog->kind == APP_KIND_MODULE && catalog->moduleBytes == 0) {
    return 0;
  }

  if (!(catalog->flags & APP_CAP_WINDOW) || catalog->minWidth == 0 ||
      catalog->minHeight == 0) {
    return 0;
  }

  return 1;
}

AppRuntimeStatus APP_RT_Start(AppRuntime *runtime,
                              const AppRuntimeServices *services,
                              const AppCatalogEntry *catalog,
                              const AppDefinition *definition) {
  if (!runtime || !catalog) {
    return APP_RT_BAD_ARGUMENT;
  }

  if (runtime->running) {
    return APP_RT_ALREADY_RUNNING;
  }

  if (!app_rt_services_valid(services)) {
    return services ? APP_RT_BAD_SERVICES : APP_RT_BAD_ARGUMENT;
  }

  if (!app_rt_definition_valid(definition) || !app_rt_catalog_valid(catalog)) {
    return APP_RT_BAD_APP;
  }

  if (!app_rt_cstr_equal(definition->name, catalog->name)) {
    return APP_RT_NAME_MISMATCH;
  }

  if (catalog->minWidth > services->limits.maxWindowWidth ||
      catalog->minHeight > services->limits.maxWindowHeight) {
    return APP_RT_RESOURCE_LIMIT;
  }

  runtime->activeCatalog = *catalog;
  runtime->definition = definition;
  runtime->context.services = services;
  runtime->context.catalog = &runtime->activeCatalog;
  runtime->context.appState = definition->appState;
  runtime->context.windowId = 0;

  if (!definition->init(&runtime->context)) {
    APP_RT_Init(runtime);
    return APP_RT_INIT_FAILED;
  }

  runtime->running = 1;
  return APP_RT_OK;
}

AppRuntimeStatus APP_RT_SendEvent(AppRuntime *runtime,
                                  const AppRuntimeEvent *event) {
  if (!runtime || !event) {
    return APP_RT_BAD_ARGUMENT;
  }
  if (!runtime->running || !runtime->definition) {
    return APP_RT_NO_APP;
  }
  if (!runtime->definition->event(&runtime->context, event)) {
    return APP_RT_EVENT_FAILED;
  }
  return APP_RT_OK;
}

AppRuntimeStatus APP_RT_Draw(AppRuntime *runtime,
                             const AppRuntimeDraw *draw) {
  if (!runtime || !draw) {
    return APP_RT_BAD_ARGUMENT;
  }
  if (!runtime->running || !runtime->definition) {
    return APP_RT_NO_APP;
  }
  if (!runtime->definition->draw(&runtime->context, draw)) {
    return APP_RT_DRAW_FAILED;
  }
  return APP_RT_OK;
}

AppRuntimeStatus APP_RT_Command(AppRuntime *runtime, uint16_t command) {
  if (!runtime) {
    return APP_RT_BAD_ARGUMENT;
  }
  if (!runtime->running || !runtime->definition) {
    return APP_RT_NO_APP;
  }
  if (!runtime->definition->command(&runtime->context, command)) {
    return APP_RT_COMMAND_FAILED;
  }
  return APP_RT_OK;
}

AppRuntimeStatus APP_RT_Stop(AppRuntime *runtime) {
  if (!runtime) {
    return APP_RT_BAD_ARGUMENT;
  }
  if (!runtime->running || !runtime->definition) {
    return APP_RT_NO_APP;
  }
  if (!runtime->definition->exit(&runtime->context)) {
    return APP_RT_EXIT_FAILED;
  }
  APP_RT_Init(runtime);
  return APP_RT_OK;
}

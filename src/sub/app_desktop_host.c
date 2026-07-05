#include "app_desktop_host.h"

static uint16_t adh_limit_or_default(uint16_t value, uint16_t fallback) {
  return value ? value : fallback;
}

static void adh_copy_ops(AppDesktopHostOps *dest,
                         const AppDesktopHostOps *src) {
  dest->maxWindows = src ? src->maxWindows : 0;
  dest->maxWindowWidth = src ? src->maxWindowWidth : 0;
  dest->maxWindowHeight = src ? src->maxWindowHeight : 0;
  dest->maxDocumentBytes = src ? src->maxDocumentBytes : 0;
  dest->scratchBytes = src ? src->scratchBytes : 0;
  dest->user = src ? src->user : (void *)0;
  dest->requestWindow = src ? src->requestWindow
                            : (AppDesktopHostRequestWindowFn)0;
  dest->drawText = src ? src->drawText : (AppDesktopHostDrawTextFn)0;
  dest->saveDocument = src ? src->saveDocument
                           : (AppDesktopHostSaveDocumentFn)0;
}

static AppDesktopHost *adh_from_context(AppRuntimeContext *ctx) {
  if (!ctx || !ctx->services || !ctx->services->user) {
    return (AppDesktopHost *)0;
  }
  return (AppDesktopHost *)ctx->services->user;
}

static uint8_t adh_request_window(AppRuntimeContext *ctx, uint16_t width,
                                  uint16_t height,
                                  uint16_t *outWindowId) {
  AppDesktopHost *host = adh_from_context(ctx);

  if (!host || !outWindowId || !host->ops.requestWindow) {
    return 0;
  }

  return host->ops.requestWindow(host->ops.user, ctx->catalog, width, height,
                                 outWindowId);
}

static uint8_t adh_draw_text(AppRuntimeContext *ctx, uint16_t windowId,
                             uint16_t x, uint16_t y, const char *text) {
  AppDesktopHost *host = adh_from_context(ctx);

  if (!host || !host->ops.drawText) {
    return 0;
  }

  return host->ops.drawText(host->ops.user, windowId, x, y, text);
}

static uint8_t adh_save_document(AppRuntimeContext *ctx, const uint8_t *data,
                                 uint16_t bytes) {
  AppDesktopHost *host = adh_from_context(ctx);

  if (!host || !host->ops.saveDocument) {
    return 0;
  }

  return host->ops.saveDocument(host->ops.user, data, bytes);
}

void ADH_Init(AppDesktopHost *host, const AppDesktopHostOps *ops) {
  if (!host) {
    return;
  }

  adh_copy_ops(&host->ops, ops);
  host->services.version = APP_RUNTIME_SERVICE_VERSION;
  host->services.sizeBytes = sizeof(AppRuntimeServices);
  host->services.limits.maxWindows =
      adh_limit_or_default(host->ops.maxWindows, 1);
  host->services.limits.maxWindowWidth =
      adh_limit_or_default(host->ops.maxWindowWidth, 320);
  host->services.limits.maxWindowHeight =
      adh_limit_or_default(host->ops.maxWindowHeight, 224);
  host->services.limits.maxDocumentBytes =
      adh_limit_or_default(host->ops.maxDocumentBytes,
                           TEXT_APP_DOCUMENT_BYTES);
  host->services.limits.scratchBytes = host->ops.scratchBytes;
  host->services.requestWindow = adh_request_window;
  host->services.drawText = adh_draw_text;
  host->services.saveDocument = adh_save_document;
  host->services.user = host;

  TEXT_APP_InitState(&host->textState);
  host->textDefinition = TEXT_APP_MakeDefinition(&host->textState);
  TEXT_APP_FillCatalog(&host->textBuiltin.catalog);
  host->textBuiltin.definition = &host->textDefinition;

  APP_SHELL_Init(&host->shell, &host->services, &host->textBuiltin, 1);
}

uint16_t ADH_AppCount(const AppDesktopHost *host) {
  if (!host) {
    return 0;
  }
  return APP_SHELL_AppCount(&host->shell);
}

AppRuntimeStatus ADH_OpenText(AppDesktopHost *host) {
  if (!host) {
    return APP_RT_BAD_ARGUMENT;
  }
  return APP_SHELL_Open(&host->shell, TEXT_APP_NAME);
}

AppRuntimeStatus ADH_SendEvent(AppDesktopHost *host,
                               const AppRuntimeEvent *event) {
  if (!host) {
    return APP_RT_BAD_ARGUMENT;
  }
  return APP_SHELL_SendEvent(&host->shell, event);
}

AppRuntimeStatus ADH_Draw(AppDesktopHost *host, uint16_t x, uint16_t y,
                          uint16_t width, uint16_t height) {
  AppRuntimeDraw draw;

  if (!host) {
    return APP_RT_BAD_ARGUMENT;
  }

  draw.x = x;
  draw.y = y;
  draw.width = width;
  draw.height = height;
  return APP_SHELL_Draw(&host->shell, &draw);
}

AppRuntimeStatus ADH_SaveActive(AppDesktopHost *host) {
  if (!host) {
    return APP_RT_BAD_ARGUMENT;
  }
  return APP_SHELL_Command(&host->shell, APP_CMD_SAVE);
}

AppRuntimeStatus ADH_Close(AppDesktopHost *host) {
  if (!host) {
    return APP_RT_BAD_ARGUMENT;
  }
  return APP_SHELL_Close(&host->shell);
}

uint8_t ADH_IsRunning(const AppDesktopHost *host) {
  if (!host) {
    return 0;
  }
  return APP_SHELL_IsRunning(&host->shell);
}

const char *ADH_ActiveName(const AppDesktopHost *host) {
  if (!host) {
    return "";
  }
  return APP_SHELL_ActiveName(&host->shell);
}

uint16_t ADH_TextWindowId(const AppDesktopHost *host) {
  if (!host) {
    return 0;
  }
  return host->textState.windowId;
}

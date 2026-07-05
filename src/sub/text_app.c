#include "text_app.h"

static const uint8_t text_app_document[TEXT_APP_DOCUMENT_BYTES] = {
    'T', 'E', 'X', 'T', '.', 'A', 'P', 'P',
    '\n', 'H', 'e', 'l', 'l', 'o', '\n'};

static void text_app_clear_catalog(AppCatalogEntry *entry) {
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

static void text_app_copy(char *dest, uint16_t maxBytes, const char *src) {
  uint16_t i;

  if (!dest || !src) {
    return;
  }

  for (i = 0; i < maxBytes && src[i]; i++) {
    dest[i] = src[i];
  }
  dest[i] = 0;
}

static TextAppState *text_app_state(AppRuntimeContext *ctx) {
  if (!ctx || !ctx->appState) {
    return (TextAppState *)0;
  }
  return (TextAppState *)ctx->appState;
}

static const char *text_app_status_line(const TextAppState *state) {
  if (state->eventCalls && state->exitCalls) {
    return "Event+close OK";
  }
  if (state->eventCalls) {
    return "Event received";
  }
  return "Awaiting event";
}

static uint8_t text_app_init(AppRuntimeContext *ctx) {
  TextAppState *state = text_app_state(ctx);
  uint16_t windowId = 0;

  if (!state || !ctx->services || !ctx->catalog ||
      !ctx->services->requestWindow) {
    return 0;
  }

  state->initCalls++;
  if (!ctx->services->requestWindow(ctx, ctx->catalog->minWidth,
                                    ctx->catalog->minHeight, &windowId)) {
    return 0;
  }

  state->windowId = windowId;
  ctx->windowId = windowId;
  return 1;
}

static uint8_t text_app_event(AppRuntimeContext *ctx,
                              const AppRuntimeEvent *event) {
  TextAppState *state = text_app_state(ctx);

  if (!state || !event) {
    return 0;
  }

  state->eventCalls++;
  state->lastEventType = event->type;
  return 1;
}

static uint8_t text_app_draw(AppRuntimeContext *ctx,
                             const AppRuntimeDraw *draw) {
  TextAppState *state = text_app_state(ctx);
  uint16_t x;
  uint16_t y;

  if (!state || !draw || !ctx->services || !ctx->services->drawText) {
    return 0;
  }

  x = (uint16_t)(draw->x + TEXT_APP_TEXT_X);
  y = (uint16_t)(draw->y + TEXT_APP_TEXT_Y);
  state->drawCalls++;

  if (!ctx->services->drawText(ctx, ctx->windowId, x, y, TEXT_APP_NAME)) {
    return 0;
  }

  if (!ctx->services->drawText(ctx, ctx->windowId, x,
                               (uint16_t)(y + TEXT_APP_LINE_STEP),
                               "OS-owned drawing")) {
    return 0;
  }

  return ctx->services->drawText(
      ctx, ctx->windowId, x, (uint16_t)(y + (TEXT_APP_LINE_STEP * 2U)),
      text_app_status_line(state));
}

static uint8_t text_app_command(AppRuntimeContext *ctx, uint16_t command) {
  TextAppState *state = text_app_state(ctx);

  if (!state) {
    return 0;
  }

  state->commandCalls++;
  state->lastCommand = command;

  if (command == APP_CMD_SAVE) {
    if (!ctx->services || !ctx->services->saveDocument) {
      return 0;
    }
    if (!ctx->services->saveDocument(ctx, text_app_document,
                                     TEXT_APP_DOCUMENT_BYTES)) {
      return 0;
    }
    state->saveCalls++;
  }

  return 1;
}

static uint8_t text_app_exit(AppRuntimeContext *ctx) {
  TextAppState *state = text_app_state(ctx);

  if (!state) {
    return 0;
  }

  state->exitCalls++;
  return 1;
}

void TEXT_APP_InitState(TextAppState *state) {
  if (!state) {
    return;
  }

  state->initCalls = 0;
  state->eventCalls = 0;
  state->drawCalls = 0;
  state->commandCalls = 0;
  state->exitCalls = 0;
  state->saveCalls = 0;
  state->lastEventType = APP_EVENT_NONE;
  state->lastCommand = 0;
  state->windowId = 0;
}

void TEXT_APP_FillCatalog(AppCatalogEntry *out) {
  if (!out) {
    return;
  }

  text_app_clear_catalog(out);
  out->id = 1;
  text_app_copy(out->name, APP_CATALOG_NAME_BYTES, TEXT_APP_NAME);
  text_app_copy(out->title, APP_CATALOG_TITLE_BYTES, TEXT_APP_TITLE);
  out->kind = APP_KIND_BUILTIN;
  out->flags = APP_CAP_WINDOW | APP_CAP_DOCUMENT | APP_CAP_STORAGE |
               APP_CAP_TEXT;
  out->minWidth = TEXT_APP_MIN_WIDTH;
  out->minHeight = TEXT_APP_MIN_HEIGHT;
}

AppDefinition TEXT_APP_MakeDefinition(TextAppState *state) {
  AppDefinition definition;

  definition.name = TEXT_APP_NAME;
  definition.appState = state;
  definition.init = text_app_init;
  definition.event = text_app_event;
  definition.draw = text_app_draw;
  definition.command = text_app_command;
  definition.exit = text_app_exit;
  return definition;
}

#ifndef SEGAOS_TEXT_APP_H
#define SEGAOS_TEXT_APP_H

#include "app_runtime.h"
#include <stdint.h>

#define TEXT_APP_NAME "TEXT.APP"
#define TEXT_APP_TITLE "Text Editor"
#define TEXT_APP_MIN_WIDTH 196U
#define TEXT_APP_MIN_HEIGHT 116U
#define TEXT_APP_TEXT_X 8U
#define TEXT_APP_TEXT_Y 10U
#define TEXT_APP_LINE_STEP 14U
#define TEXT_APP_DOCUMENT_BYTES 15U

typedef struct TextAppState {
  uint16_t initCalls;
  uint16_t eventCalls;
  uint16_t drawCalls;
  uint16_t commandCalls;
  uint16_t exitCalls;
  uint16_t saveCalls;
  uint16_t lastEventType;
  uint16_t lastCommand;
  uint16_t windowId;
} TextAppState;

void TEXT_APP_InitState(TextAppState *state);
void TEXT_APP_FillCatalog(AppCatalogEntry *out);
AppDefinition TEXT_APP_MakeDefinition(TextAppState *state);

#endif

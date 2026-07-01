#include "basic.h"

static uint8_t bas_is_space(char ch) {
  return (uint8_t)(ch == ' ' || ch == '\t');
}

static char bas_upper(char ch) {
  if (ch >= 'a' && ch <= 'z')
    return (char)(ch - ('a' - 'A'));
  return ch;
}

static uint8_t bas_keyword_matches(const char *text, const char *keyword,
                                   uint8_t len) {
  uint8_t i;

  for (i = 0; i < len; i++) {
    if (bas_upper(text[i]) != keyword[i])
      return 0;
  }
  return 1;
}

static uint16_t bas_strlen_limited(const char *text, uint16_t maxLen) {
  uint16_t len = 0;

  if (!text)
    return 0;
  while (text[len] && len < maxLen) {
    len++;
  }
  return len;
}

static const char *bas_skip_spaces(const char *text) {
  while (text && bas_is_space(*text)) {
    text++;
  }
  return text;
}

static const char *bas_trim_end(const char *start, const char *end) {
  while (end > start && bas_is_space(*(end - 1))) {
    end--;
  }
  return end;
}

static uint8_t bas_keyword_len(const char *keyword) {
  uint8_t len = 0;

  while (keyword[len]) {
    len++;
  }
  return len;
}

typedef struct {
  const char *name;
  BasicToken token;
} BasicKeyword;

static const BasicKeyword basicKeywords[] = {
    {"PRINT", BAS_TOK_PRINT}, {"INPUT", BAS_TOK_INPUT},
    {"LET", BAS_TOK_LET},     {"IF", BAS_TOK_IF},
    {"THEN", BAS_TOK_THEN},   {"GOTO", BAS_TOK_GOTO},
    {"GOSUB", BAS_TOK_GOSUB}, {"RETURN", BAS_TOK_RETURN},
    {"FOR", BAS_TOK_FOR},     {"NEXT", BAS_TOK_NEXT},
    {"END", BAS_TOK_END},     {"REM", BAS_TOK_REM},
};

static BasicLine basScratchLines[BAS_MAX_PROGRAM_LINES];
static uint8_t basScratchStorage[BAS_MAX_PROGRAM_STORAGE];

static BasicToken bas_detect_token(const char **body) {
  uint8_t i;

  for (i = 0; i < (uint8_t)(sizeof(basicKeywords) / sizeof(basicKeywords[0]));
       i++) {
    uint8_t len = bas_keyword_len(basicKeywords[i].name);

    if (bas_keyword_matches(*body, basicKeywords[i].name, len)) {
      char after = (*body)[len];
      if (after == 0 || bas_is_space(after)) {
        *body = bas_skip_spaces(*body + len);
        return basicKeywords[i].token;
      }
    }
  }

  return BAS_TOK_RAW;
}

static uint8_t bas_find_line(const BasicProgram *program, uint16_t number,
                             uint8_t *indexOut) {
  uint8_t i;

  if (indexOut)
    *indexOut = 0;
  if (!program)
    return 0;

  for (i = 0; i < program->lineCount; i++) {
    if (program->lines[i].number == number) {
      if (indexOut)
        *indexOut = i;
      return 1;
    }
    if (program->lines[i].number > number) {
      if (indexOut)
        *indexOut = i;
      return 0;
    }
  }

  if (indexOut)
    *indexOut = program->lineCount;
  return 0;
}

static uint8_t bas_command_equals(const char *input, const char *command) {
  uint8_t i = 0;

  input = bas_skip_spaces(input);
  while (command[i]) {
    if (bas_upper(input[i]) != command[i])
      return 0;
    i++;
  }

  input = bas_skip_spaces(input + i);
  return (uint8_t)(*input == 0);
}

static void bas_set_command_result(BasicCommandResult *result,
                                   BasicCommandKind kind, uint8_t ok,
                                   uint8_t linesEmitted) {
  if (!result)
    return;
  result->kind = kind;
  result->ok = ok;
  result->linesEmitted = linesEmitted;
}

static uint8_t bas_line_payload_length(const BasicParsedLine *parsed) {
  return (uint8_t)(1U + parsed->payloadLength);
}

static uint8_t bas_write_replacement_line(BasicLine *lines, uint8_t *storage,
                                          uint8_t *outCount, uint16_t *used,
                                          uint16_t storageCapacity,
                                          const BasicParsedLine *replacement) {
  uint16_t len = bas_line_payload_length(replacement);

  if ((uint32_t)*used + len > storageCapacity)
    return 0;

  storage[*used] = (uint8_t)replacement->token;
  for (uint16_t j = 0; j < replacement->payloadLength; j++) {
    storage[*used + 1U + j] = (uint8_t)replacement->payload[j];
  }
  lines[*outCount].number = replacement->number;
  lines[*outCount].offset = *used;
  lines[*outCount].length = len;
  *used = (uint16_t)(*used + len);
  *outCount = (uint8_t)(*outCount + 1U);
  return 1;
}

static uint8_t bas_repack_lines(BasicProgram *program,
                                const BasicParsedLine *replacement,
                                uint8_t replaceIndex,
                                uint8_t insertReplacement,
                                uint16_t deleteNumber) {
  uint16_t used = 0;
  uint8_t outCount = 0;
  uint8_t i;
  uint8_t inserted = 0;

  if (!program || program->lineCapacity > BAS_MAX_PROGRAM_LINES ||
      program->storageCapacity > BAS_MAX_PROGRAM_STORAGE)
    return 0;

  for (i = 0; i < program->lineCount; i++) {
    const BasicLine *old = &program->lines[i];
    const uint8_t *oldBytes;
    uint16_t len;

    if (insertReplacement && !inserted && outCount == replaceIndex) {
      if (!bas_write_replacement_line(basScratchLines, basScratchStorage,
                                      &outCount, &used,
                                      program->storageCapacity, replacement)) {
        return 0;
      }
      inserted = 1;
    }

    if (old->number == deleteNumber ||
        (insertReplacement && old->number == replacement->number)) {
      continue;
    }

    oldBytes = BAS_GetLineBytes(program, old);
    len = old->length;
    if (!oldBytes || (uint32_t)used + len > program->storageCapacity)
      return 0;
    for (uint16_t j = 0; j < len; j++) {
      basScratchStorage[used + j] = oldBytes[j];
    }
    basScratchLines[outCount].number = old->number;
    basScratchLines[outCount].offset = used;
    basScratchLines[outCount].length = len;
    used = (uint16_t)(used + len);
    outCount++;
  }

  if (insertReplacement && !inserted) {
    if (!bas_write_replacement_line(basScratchLines, basScratchStorage,
                                    &outCount, &used, program->storageCapacity,
                                    replacement)) {
      return 0;
    }
  }

  for (i = 0; i < used; i++) {
    program->storage[i] = basScratchStorage[i];
  }
  for (i = 0; i < outCount; i++) {
    program->lines[i] = basScratchLines[i];
  }
  program->lineCount = outCount;
  program->storageUsed = used;
  return 1;
}

void BAS_InitProgram(BasicProgram *program, BasicLine *lines,
                     uint8_t lineCapacity, uint8_t *storage,
                     uint16_t storageCapacity) {
  if (!program)
    return;

  program->lines = lines;
  program->lineCapacity = lineCapacity;
  program->lineCount = 0;
  program->storage = storage;
  program->storageCapacity = storageCapacity;
  program->storageUsed = 0;
}

void BAS_ClearProgram(BasicProgram *program) {
  if (!program)
    return;
  program->lineCount = 0;
  program->storageUsed = 0;
}

uint8_t BAS_ParseSourceLine(const char *source, BasicParsedLine *out) {
  uint32_t number = 0;
  const char *p;
  const char *bodyStart;
  const char *bodyEnd;
  uint16_t payloadLength;

  if (!source || !out)
    return 0;

  p = bas_skip_spaces(source);
  if (*p < '0' || *p > '9')
    return 0;

  while (*p >= '0' && *p <= '9') {
    number = (number * 10U) + (uint32_t)(*p - '0');
    if (number > BAS_MAX_LINE_NUMBER)
      return 0;
    p++;
  }
  if (number == 0)
    return 0;

  p = bas_skip_spaces(p);
  out->number = (uint16_t)number;
  out->hasBody = (uint8_t)(*p != 0);
  out->token = BAS_TOK_RAW;
  out->payload[0] = 0;
  out->payloadLength = 0;

  if (!out->hasBody)
    return 1;

  bodyStart = p;
  out->token = bas_detect_token(&bodyStart);
  bodyStart = bas_skip_spaces(bodyStart);
  bodyEnd = bodyStart + bas_strlen_limited(bodyStart, BAS_MAX_PAYLOAD_TEXT + 1U);
  bodyEnd = bas_trim_end(bodyStart, bodyEnd);
  payloadLength = (uint16_t)(bodyEnd - bodyStart);
  if (payloadLength > BAS_MAX_PAYLOAD_TEXT)
    return 0;

  for (uint16_t i = 0; i < payloadLength; i++) {
    out->payload[i] = bodyStart[i];
  }
  out->payload[payloadLength] = 0;
  out->payloadLength = payloadLength;
  return 1;
}

uint8_t BAS_StoreSourceLine(BasicProgram *program, const char *source) {
  BasicParsedLine parsed;
  uint8_t index = 0;
  uint8_t exists;

  if (!program || !program->lines || !program->storage)
    return 0;
  if (!BAS_ParseSourceLine(source, &parsed))
    return 0;

  exists = bas_find_line(program, parsed.number, &index);
  if (!parsed.hasBody) {
    if (!exists)
      return 1;
    return bas_repack_lines(program, (const BasicParsedLine *)0, 0, 0,
                            parsed.number);
  }

  if (!exists && program->lineCount >= program->lineCapacity)
    return 0;
  if (program->lineCapacity > BAS_MAX_PROGRAM_LINES ||
      program->storageCapacity > BAS_MAX_PROGRAM_STORAGE)
    return 0;

  return bas_repack_lines(program, &parsed, index, 1, 0);
}

const BasicLine *BAS_GetLine(const BasicProgram *program, uint8_t index) {
  if (!program || !program->lines || index >= program->lineCount)
    return (const BasicLine *)0;
  return &program->lines[index];
}

const uint8_t *BAS_GetLineBytes(const BasicProgram *program,
                                const BasicLine *line) {
  if (!program || !line || !program->storage)
    return (const uint8_t *)0;
  if ((uint32_t)line->offset + line->length > program->storageUsed)
    return (const uint8_t *)0;
  return &program->storage[line->offset];
}

const char *BAS_TokenName(BasicToken token) {
  switch (token) {
  case BAS_TOK_PRINT:
    return "PRINT";
  case BAS_TOK_INPUT:
    return "INPUT";
  case BAS_TOK_LET:
    return "LET";
  case BAS_TOK_IF:
    return "IF";
  case BAS_TOK_THEN:
    return "THEN";
  case BAS_TOK_GOTO:
    return "GOTO";
  case BAS_TOK_GOSUB:
    return "GOSUB";
  case BAS_TOK_RETURN:
    return "RETURN";
  case BAS_TOK_FOR:
    return "FOR";
  case BAS_TOK_NEXT:
    return "NEXT";
  case BAS_TOK_END:
    return "END";
  case BAS_TOK_REM:
    return "REM";
  case BAS_TOK_RAW:
  default:
    return "";
  }
}

uint8_t BAS_DecodeLine(const BasicProgram *program, const BasicLine *line,
                       char *out, uint16_t outBytes) {
  const uint8_t *bytes;
  BasicToken token;
  const char *name;
  uint16_t pos = 0;
  uint16_t payloadLen;

  if (!out || outBytes == 0)
    return 0;
  out[0] = 0;
  bytes = BAS_GetLineBytes(program, line);
  if (!bytes || line->length == 0)
    return 0;

#define BAS_APPEND_CHAR(ch)                                                    \
  do {                                                                         \
    if (pos + 1U >= outBytes)                                                  \
      return 0;                                                                \
    out[pos++] = (char)(ch);                                                   \
    out[pos] = 0;                                                              \
  } while (0)

  {
    uint16_t divisor = 10000U;
    uint8_t started = 0;
    uint16_t number = line->number;

    while (divisor > 0) {
      uint8_t digit = (uint8_t)(number / divisor);
      if (digit || started || divisor == 1U) {
        BAS_APPEND_CHAR((char)('0' + digit));
        started = 1;
      }
      number = (uint16_t)(number % divisor);
      divisor = (uint16_t)(divisor / 10U);
    }
  }

  token = (BasicToken)bytes[0];
  name = BAS_TokenName(token);
  BAS_APPEND_CHAR(' ');
  if (token != BAS_TOK_RAW && name[0]) {
    for (uint16_t i = 0; name[i]; i++) {
      BAS_APPEND_CHAR(name[i]);
    }
    if (line->length > 1)
      BAS_APPEND_CHAR(' ');
  }

  payloadLen = (uint16_t)(line->length - 1U);
  for (uint16_t i = 0; i < payloadLen; i++) {
    BAS_APPEND_CHAR(bytes[1U + i]);
  }

#undef BAS_APPEND_CHAR
  return 1;
}

uint8_t BAS_ListProgram(const BasicProgram *program, BasicLineSink sink,
                        void *user, char *lineBuffer,
                        uint16_t lineBufferBytes, uint8_t *linesEmitted) {
  uint8_t emitted = 0;

  if (linesEmitted)
    *linesEmitted = 0;
  if (!program || !lineBuffer || lineBufferBytes == 0)
    return 0;

  for (uint8_t i = 0; i < program->lineCount; i++) {
    const BasicLine *line = BAS_GetLine(program, i);

    if (!line || !BAS_DecodeLine(program, line, lineBuffer, lineBufferBytes))
      return 0;
    if (sink && !sink(lineBuffer, user))
      return 0;
    emitted++;
  }

  if (linesEmitted)
    *linesEmitted = emitted;
  return 1;
}

uint8_t BAS_SubmitConsoleLine(BasicProgram *program, const char *input,
                              BasicLineSink sink, void *user,
                              char *lineBuffer, uint16_t lineBufferBytes,
                              BasicCommandResult *result) {
  const char *p;
  uint8_t ok;
  uint8_t emitted = 0;

  bas_set_command_result(result, BAS_CMD_ERROR, 0, 0);
  if (!program || !input)
    return 0;

  p = bas_skip_spaces(input);
  if (*p == 0) {
    bas_set_command_result(result, BAS_CMD_EMPTY, 1, 0);
    return 1;
  }

  if (*p >= '0' && *p <= '9') {
    ok = BAS_StoreSourceLine(program, p);
    bas_set_command_result(result, BAS_CMD_PROGRAM_LINE, ok, 0);
    return ok;
  }

  if (bas_command_equals(p, "LIST")) {
    ok = BAS_ListProgram(program, sink, user, lineBuffer, lineBufferBytes,
                         &emitted);
    bas_set_command_result(result, BAS_CMD_LIST, ok, emitted);
    return ok;
  }

  if (bas_command_equals(p, "NEW")) {
    BAS_ClearProgram(program);
    bas_set_command_result(result, BAS_CMD_NEW, 1, 0);
    return 1;
  }

  bas_set_command_result(result, BAS_CMD_UNSUPPORTED, 0, 0);
  return 0;
}

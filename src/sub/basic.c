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

static void bas_set_run_result(BasicRunResult *result, BasicRunStatus status,
                               uint8_t statementsExecuted,
                               uint8_t linesEmitted, uint16_t errorLine) {
  if (!result)
    return;
  result->status = status;
  result->statementsExecuted = statementsExecuted;
  result->linesEmitted = linesEmitted;
  result->errorLine = errorLine;
}

static void bas_clear_value(BasicValue *value) {
  if (!value)
    return;
  value->kind = BAS_VALUE_NONE;
  value->integer = 0;
  value->string[0] = 0;
  value->stringLength = 0;
}

static uint8_t bas_evaluate_expression_internal(const char *source,
                                                const BasicRuntime *runtime,
                                                BasicValue *out,
                                                uint8_t *undefinedVariable);

static uint8_t bas_variable_index(char name, uint8_t *indexOut) {
  char upper = bas_upper(name);

  if (upper < 'A' || upper > 'Z')
    return 0;
  if (indexOut)
    *indexOut = (uint8_t)(upper - 'A');
  return 1;
}

void BAS_InitRuntime(BasicRuntime *runtime) {
  if (!runtime)
    return;

  for (uint8_t i = 0; i < BAS_VARIABLE_COUNT; i++) {
    runtime->integerDefined[i] = 0;
    runtime->integerValues[i] = 0;
  }
}

uint8_t BAS_RuntimeSetInteger(BasicRuntime *runtime, char name,
                              int16_t value) {
  uint8_t index;

  if (!runtime || !bas_variable_index(name, &index))
    return 0;
  runtime->integerDefined[index] = 1;
  runtime->integerValues[index] = value;
  return 1;
}

uint8_t BAS_RuntimeGetInteger(const BasicRuntime *runtime, char name,
                              int16_t *out) {
  uint8_t index;

  if (!runtime || !out || !bas_variable_index(name, &index))
    return 0;
  if (!runtime->integerDefined[index])
    return 0;
  *out = runtime->integerValues[index];
  return 1;
}

static uint8_t bas_parse_integer_term_with_runtime(
    const char **cursor, const BasicRuntime *runtime, int16_t *out,
    uint8_t *undefinedVariable) {
  const char *p;
  uint8_t negative = 0;
  uint8_t digitSeen = 0;
  uint32_t magnitude = 0;
  uint32_t limit;

  if (!cursor || !*cursor || !out)
    return 0;

  p = bas_skip_spaces(*cursor);
  if (*p == '+' || *p == '-') {
    negative = (uint8_t)(*p == '-');
    p++;
  }

  {
    uint8_t index;

    if (bas_variable_index(*p, &index)) {
      int32_t value;

      if (!runtime || !runtime->integerDefined[index]) {
        if (undefinedVariable)
          *undefinedVariable = 1;
        return 0;
      }

      value = runtime->integerValues[index];
      if (negative)
        value = -value;
      if (value < -32768L || value > 32767L)
        return 0;
      *out = (int16_t)value;
      *cursor = p + 1;
      return 1;
    }
  }

  limit = negative ? 32768UL : 32767UL;
  while (*p >= '0' && *p <= '9') {
    digitSeen = 1;
    magnitude = (magnitude * 10UL) + (uint32_t)(*p - '0');
    if (magnitude > limit)
      return 0;
    p++;
  }

  if (!digitSeen)
    return 0;

  *out = negative ? (int16_t)(-((int32_t)magnitude)) : (int16_t)magnitude;
  *cursor = p;
  return 1;
}

static uint8_t bas_eval_integer_expression_with_runtime(
    const char *source, const BasicRuntime *runtime, BasicValue *out,
    uint8_t *undefinedVariable) {
  const char *p = source;
  int16_t term;
  int32_t total;

  if (undefinedVariable)
    *undefinedVariable = 0;

  if (!bas_parse_integer_term_with_runtime(&p, runtime, &term,
                                           undefinedVariable))
    return 0;
  total = term;

  while (1) {
    char op;

    p = bas_skip_spaces(p);
    if (*p == 0)
      break;
    if (*p != '+' && *p != '-')
      return 0;
    op = *p++;

    if (!bas_parse_integer_term_with_runtime(&p, runtime, &term,
                                             undefinedVariable))
      return 0;
    if (op == '+')
      total += term;
    else
      total -= term;
    if (total < -32768L || total > 32767L)
      return 0;
  }

  out->kind = BAS_VALUE_INTEGER;
  out->integer = (int16_t)total;
  return 1;
}

static uint8_t bas_eval_string_literal(const char *source, BasicValue *out) {
  const char *p = bas_skip_spaces(source);
  uint16_t len = 0;

  if (*p != '"')
    return 0;
  p++;

  while (*p && *p != '"') {
    if (len >= BAS_MAX_STRING_VALUE)
      return 0;
    out->string[len++] = *p++;
  }
  if (*p != '"')
    return 0;
  p++;
  p = bas_skip_spaces(p);
  if (*p != 0)
    return 0;

  out->string[len] = 0;
  out->stringLength = len;
  out->kind = BAS_VALUE_STRING;
  return 1;
}

static uint8_t bas_write_int16(char *out, uint16_t outBytes, int16_t value) {
  uint16_t pos = 0;
  uint16_t divisor = 10000U;
  uint8_t started = 0;
  int32_t wideValue = value;
  uint32_t magnitude;

  if (!out || outBytes == 0)
    return 0;
  out[0] = 0;

#define BAS_WRITE_OUT_CHAR(ch)                                                 \
  do {                                                                         \
    if (pos + 1U >= outBytes)                                                  \
      return 0;                                                                \
    out[pos++] = (char)(ch);                                                   \
    out[pos] = 0;                                                              \
  } while (0)

  if (wideValue < 0) {
    BAS_WRITE_OUT_CHAR('-');
    magnitude = (uint32_t)(-wideValue);
  } else {
    magnitude = (uint32_t)wideValue;
  }

  while (divisor > 0) {
    uint8_t digit = (uint8_t)(magnitude / divisor);
    if (digit || started || divisor == 1U) {
      BAS_WRITE_OUT_CHAR((char)('0' + digit));
      started = 1;
    }
    magnitude = magnitude % divisor;
    divisor = (uint16_t)(divisor / 10U);
  }

#undef BAS_WRITE_OUT_CHAR
  return 1;
}

static uint8_t bas_copy_payload_to_buffer(const uint8_t *bytes,
                                          uint16_t lineLength, char *out,
                                          uint16_t outBytes) {
  uint16_t payloadLength;

  if (!bytes || !out || outBytes == 0 || lineLength == 0)
    return 0;

  payloadLength = (uint16_t)(lineLength - 1U);
  if (payloadLength + 1U > outBytes)
    return 0;
  for (uint16_t i = 0; i < payloadLength; i++) {
    out[i] = (char)bytes[1U + i];
  }
  out[payloadLength] = 0;
  return 1;
}

static uint8_t bas_parse_line_target(const char *source, uint16_t *target) {
  const char *p = bas_skip_spaces(source);
  uint32_t number = 0;
  uint8_t digitSeen = 0;

  if (!source || !target)
    return 0;

  while (*p >= '0' && *p <= '9') {
    digitSeen = 1;
    number = (number * 10UL) + (uint32_t)(*p - '0');
    if (number > BAS_MAX_LINE_NUMBER)
      return 0;
    p++;
  }

  if (!digitSeen || number == 0)
    return 0;
  p = bas_skip_spaces(p);
  if (*p != 0)
    return 0;

  *target = (uint16_t)number;
  return 1;
}

static uint8_t bas_starts_with_keyword(const char *source,
                                       const char *keyword) {
  uint8_t i = 0;

  if (!source || !keyword)
    return 0;

  while (keyword[i]) {
    if (bas_upper(source[i]) != keyword[i])
      return 0;
    i++;
  }
  return (uint8_t)(source[i] == 0 || bas_is_space(source[i]));
}

static char *bas_find_standalone_keyword(char *source, const char *keyword) {
  char *p = source;

  if (!source || !keyword)
    return (char *)0;

  while (*p) {
    uint8_t atBoundary = (uint8_t)(p == source || bas_is_space(*(p - 1)));

    if (atBoundary && bas_starts_with_keyword(p, keyword))
      return p;
    p++;
  }

  return (char *)0;
}

static uint8_t bas_eval_integer_condition_value(
    const char *source, const BasicRuntime *runtime, int16_t *out,
    uint8_t *undefinedVariable) {
  BasicValue value;

  if (!bas_evaluate_expression_internal(source, runtime, &value,
                                        undefinedVariable))
    return 0;
  if (value.kind != BAS_VALUE_INTEGER)
    return 0;

  *out = value.integer;
  return 1;
}

static char *bas_find_condition_operator(char *source, uint8_t *operatorLen) {
  char *p = source;

  if (operatorLen)
    *operatorLen = 0;
  if (!source)
    return (char *)0;

  while (*p) {
    if (*p == '=') {
      if (operatorLen)
        *operatorLen = 1;
      return p;
    }
    if (*p == '<' || *p == '>') {
      if ((p[0] == '<' && p[1] == '>') || p[1] == '=') {
        if (operatorLen)
          *operatorLen = 2;
      } else if (operatorLen) {
        *operatorLen = 1;
      }
      return p;
    }
    p++;
  }

  return (char *)0;
}

static uint8_t bas_compare_condition_values(int16_t left, int16_t right,
                                            const char *op,
                                            uint8_t operatorLen,
                                            uint8_t *truth) {
  if (!op || !truth)
    return 0;

  if (operatorLen == 1 && op[0] == '=') {
    *truth = (uint8_t)(left == right);
    return 1;
  }
  if (operatorLen == 1 && op[0] == '<') {
    *truth = (uint8_t)(left < right);
    return 1;
  }
  if (operatorLen == 1 && op[0] == '>') {
    *truth = (uint8_t)(left > right);
    return 1;
  }
  if (operatorLen == 2 && op[0] == '<' && op[1] == '=') {
    *truth = (uint8_t)(left <= right);
    return 1;
  }
  if (operatorLen == 2 && op[0] == '>' && op[1] == '=') {
    *truth = (uint8_t)(left >= right);
    return 1;
  }
  if (operatorLen == 2 && op[0] == '<' && op[1] == '>') {
    *truth = (uint8_t)(left != right);
    return 1;
  }

  return 0;
}

static uint8_t bas_eval_if_condition(char *source, const BasicRuntime *runtime,
                                     uint8_t *truth,
                                     BasicRunStatus *status) {
  char *conditionEnd;
  char *op;
  uint8_t operatorLen = 0;
  uint8_t undefinedVariable = 0;
  char operatorText[2];
  int16_t left;
  int16_t right;

  if (status)
    *status = BAS_RUN_BAD_EXPRESSION;
  if (!source || !truth)
    return 0;

  source = (char *)bas_skip_spaces(source);
  conditionEnd = (char *)bas_trim_end(source, source + bas_strlen_limited(
                                                    source,
                                                    BAS_MAX_PAYLOAD_TEXT));
  *conditionEnd = 0;
  if (*source == 0)
    return 0;

  op = bas_find_condition_operator(source, &operatorLen);
  if (!op) {
    if (!bas_eval_integer_condition_value(source, runtime, &left,
                                          &undefinedVariable)) {
      if (status && undefinedVariable)
        *status = BAS_RUN_UNDEFINED_VARIABLE;
      return 0;
    }
    *truth = (uint8_t)(left != 0);
    return 1;
  }

  operatorText[0] = op[0];
  operatorText[1] = (operatorLen == 2) ? op[1] : 0;
  *op = 0;
  if (!bas_eval_integer_condition_value(source, runtime, &left,
                                        &undefinedVariable)) {
    if (status && undefinedVariable)
      *status = BAS_RUN_UNDEFINED_VARIABLE;
    return 0;
  }
  if (!bas_eval_integer_condition_value(op + operatorLen, runtime, &right,
                                        &undefinedVariable)) {
    if (status && undefinedVariable)
      *status = BAS_RUN_UNDEFINED_VARIABLE;
    return 0;
  }

  return bas_compare_condition_values(left, right, operatorText, operatorLen,
                                      truth);
}

static uint8_t bas_parse_if_target(const char *source, uint16_t *target) {
  const char *p = bas_skip_spaces(source);

  if (bas_starts_with_keyword(p, "GOTO"))
    p = bas_skip_spaces(p + 4);
  return bas_parse_line_target(p, target);
}

static uint8_t bas_execute_if(char *source, const BasicRuntime *runtime,
                              uint8_t *truth, uint16_t *target,
                              BasicRunStatus *status) {
  char *thenKeyword;
  char *targetText;

  if (status)
    *status = BAS_RUN_BAD_EXPRESSION;
  if (!source || !truth || !target)
    return 0;

  thenKeyword = bas_find_standalone_keyword(source, "THEN");
  if (!thenKeyword)
    return 0;

  *thenKeyword = 0;
  targetText = thenKeyword + 4;
  if (!bas_eval_if_condition(source, runtime, truth, status))
    return 0;

  if (!bas_parse_if_target(targetText, target)) {
    if (status)
      *status = BAS_RUN_BAD_TARGET;
    return 0;
  }

  return 1;
}

static uint8_t bas_value_to_line(const BasicValue *value, char *out,
                                 uint16_t outBytes) {
  if (!value || !out || outBytes == 0)
    return 0;

  if (value->kind == BAS_VALUE_INTEGER)
    return bas_write_int16(out, outBytes, value->integer);

  if (value->kind == BAS_VALUE_STRING) {
    if (value->stringLength + 1U > outBytes)
      return 0;
    for (uint16_t i = 0; i <= value->stringLength; i++) {
      out[i] = value->string[i];
    }
    return 1;
  }

  return 0;
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

  if (bas_command_equals(p, "RUN")) {
    BasicRunResult runResult;

    ok = BAS_RunProgram(program, sink, user, lineBuffer, lineBufferBytes,
                        &runResult);
    bas_set_command_result(result, BAS_CMD_RUN, ok, runResult.linesEmitted);
    return ok;
  }

  bas_set_command_result(result, BAS_CMD_UNSUPPORTED, 0, 0);
  return 0;
}

static uint8_t bas_evaluate_expression_internal(const char *source,
                                                const BasicRuntime *runtime,
                                                BasicValue *out,
                                                uint8_t *undefinedVariable) {
  const char *p;

  if (undefinedVariable)
    *undefinedVariable = 0;
  if (!source || !out)
    return 0;

  bas_clear_value(out);
  p = bas_skip_spaces(source);
  if (*p == 0)
    return 0;

  if (*p == '"')
    return bas_eval_string_literal(p, out);

  return bas_eval_integer_expression_with_runtime(p, runtime, out,
                                                 undefinedVariable);
}

uint8_t BAS_EvaluateExpression(const char *source, BasicValue *out) {
  return bas_evaluate_expression_internal(source, (const BasicRuntime *)0, out,
                                          (uint8_t *)0);
}

uint8_t BAS_EvaluateExpressionWithRuntime(const char *source,
                                          const BasicRuntime *runtime,
                                          BasicValue *out) {
  return bas_evaluate_expression_internal(source, runtime, out, (uint8_t *)0);
}

static uint8_t bas_execute_let(BasicRuntime *runtime, const char *source,
                               BasicRunStatus *status) {
  const char *p = bas_skip_spaces(source);
  uint8_t index;
  BasicValue value;
  uint8_t undefinedVariable = 0;

  if (status)
    *status = BAS_RUN_BAD_ASSIGNMENT;
  if (!runtime || !p || !bas_variable_index(*p, &index))
    return 0;

  p++;
  p = bas_skip_spaces(p);
  if (*p != '=')
    return 0;
  p++;

  if (!bas_evaluate_expression_internal(p, runtime, &value,
                                        &undefinedVariable)) {
    if (status && undefinedVariable)
      *status = BAS_RUN_UNDEFINED_VARIABLE;
    return 0;
  }
  if (value.kind != BAS_VALUE_INTEGER)
    return 0;

  runtime->integerDefined[index] = 1;
  runtime->integerValues[index] = value.integer;
  return 1;
}

static uint8_t bas_parse_input_target(const char *source, uint8_t *indexOut) {
  const char *p = bas_skip_spaces(source);

  if (!p || !bas_variable_index(*p, indexOut))
    return 0;

  p++;
  p = bas_skip_spaces(p);
  return (uint8_t)(*p == 0);
}

static uint8_t bas_execute_input(BasicRuntime *runtime, const char *targetText,
                                 BasicInputSource input, void *user,
                                 char *lineBuffer, uint16_t lineBufferBytes,
                                 BasicRunStatus *status) {
  uint8_t index;
  BasicValue value;

  if (status)
    *status = BAS_RUN_BAD_ASSIGNMENT;
  if (!runtime || !bas_parse_input_target(targetText, &index))
    return 0;

  if (!input) {
    if (status)
      *status = BAS_RUN_INPUT_UNAVAILABLE;
    return 0;
  }
  if (!input(lineBuffer, lineBufferBytes, user)) {
    if (status)
      *status = BAS_RUN_INPUT_UNAVAILABLE;
    return 0;
  }

  if (!BAS_EvaluateExpression(lineBuffer, &value) ||
      value.kind != BAS_VALUE_INTEGER) {
    if (status)
      *status = BAS_RUN_BAD_INPUT;
    return 0;
  }

  runtime->integerDefined[index] = 1;
  runtime->integerValues[index] = value.integer;
  return 1;
}

uint8_t BAS_RunProgram(const BasicProgram *program, BasicLineSink sink,
                       void *user, char *lineBuffer,
                       uint16_t lineBufferBytes, BasicRunResult *result) {
  BasicRuntime runtime;

  BAS_InitRuntime(&runtime);
  return BAS_RunProgramWithRuntime(program, &runtime, sink, user, lineBuffer,
                                   lineBufferBytes, result);
}

uint8_t BAS_RunProgramWithRuntime(const BasicProgram *program,
                                  BasicRuntime *runtime, BasicLineSink sink,
                                  void *user, char *lineBuffer,
                                  uint16_t lineBufferBytes,
                                  BasicRunResult *result) {
  return BAS_RunProgramWithIO(program, runtime, sink, (BasicInputSource)0, user,
                              lineBuffer, lineBufferBytes, result);
}

uint8_t BAS_RunProgramWithIO(const BasicProgram *program, BasicRuntime *runtime,
                             BasicLineSink sink, BasicInputSource input,
                             void *user, char *lineBuffer,
                             uint16_t lineBufferBytes,
                             BasicRunResult *result) {
  uint8_t statementsExecuted = 0;
  uint8_t linesEmitted = 0;
  uint8_t pc = 0;

  bas_set_run_result(result, BAS_RUN_COMPLETE, 0, 0, 0);
  if (!program || !lineBuffer || lineBufferBytes == 0) {
    bas_set_run_result(result, BAS_RUN_BUFFER_TOO_SMALL, 0, 0, 0);
    return 0;
  }
  if (!runtime) {
    bas_set_run_result(result, BAS_RUN_UNDEFINED_VARIABLE, 0, 0, 0);
    return 0;
  }

  while (pc < program->lineCount) {
    const BasicLine *line;
    const uint8_t *bytes;
    BasicToken token;

    if (statementsExecuted >= BAS_RUN_MAX_STEPS) {
      line = BAS_GetLine(program, pc);
      bas_set_run_result(result, BAS_RUN_STEP_LIMIT, statementsExecuted,
                         linesEmitted, line ? line->number : 0);
      return 0;
    }

    line = BAS_GetLine(program, pc);
    bytes = BAS_GetLineBytes(program, line);
    if (!line || !bytes || line->length == 0) {
      bas_set_run_result(result, BAS_RUN_UNSUPPORTED_STATEMENT,
                         statementsExecuted, linesEmitted, 0);
      return 0;
    }

    token = (BasicToken)bytes[0];
    statementsExecuted++;

    if (token == BAS_TOK_END) {
      bas_set_run_result(result, BAS_RUN_HALTED, statementsExecuted,
                         linesEmitted, 0);
      return 1;
    }

    if (token == BAS_TOK_PRINT) {
      BasicValue value;
      uint8_t undefinedVariable = 0;

      if (!bas_copy_payload_to_buffer(bytes, line->length, lineBuffer,
                                      lineBufferBytes)) {
        bas_set_run_result(result, BAS_RUN_BUFFER_TOO_SMALL,
                           statementsExecuted, linesEmitted, line->number);
        return 0;
      }
      if (!bas_evaluate_expression_internal(lineBuffer, runtime, &value,
                                            &undefinedVariable)) {
        bas_set_run_result(result,
                           undefinedVariable ? BAS_RUN_UNDEFINED_VARIABLE
                                             : BAS_RUN_BAD_EXPRESSION,
                           statementsExecuted, linesEmitted, line->number);
        return 0;
      }
      if (!bas_value_to_line(&value, lineBuffer, lineBufferBytes)) {
        bas_set_run_result(result, BAS_RUN_BUFFER_TOO_SMALL,
                           statementsExecuted, linesEmitted, line->number);
        return 0;
      }
      if (sink && !sink(lineBuffer, user)) {
        bas_set_run_result(result, BAS_RUN_OUTPUT_REJECTED,
                           statementsExecuted, linesEmitted, line->number);
        return 0;
      }
      linesEmitted++;
      pc++;
      continue;
    }

    if (token == BAS_TOK_INPUT) {
      BasicRunStatus inputStatus = BAS_RUN_BAD_ASSIGNMENT;

      if (!bas_copy_payload_to_buffer(bytes, line->length, lineBuffer,
                                      lineBufferBytes)) {
        bas_set_run_result(result, BAS_RUN_BUFFER_TOO_SMALL,
                           statementsExecuted, linesEmitted, line->number);
        return 0;
      }
      if (!bas_execute_input(runtime, lineBuffer, input, user, lineBuffer,
                             lineBufferBytes, &inputStatus)) {
        bas_set_run_result(result, inputStatus, statementsExecuted,
                           linesEmitted, line->number);
        return 0;
      }
      pc++;
      continue;
    }

    if (token == BAS_TOK_LET) {
      BasicRunStatus assignmentStatus = BAS_RUN_BAD_ASSIGNMENT;

      if (!bas_copy_payload_to_buffer(bytes, line->length, lineBuffer,
                                      lineBufferBytes)) {
        bas_set_run_result(result, BAS_RUN_BUFFER_TOO_SMALL,
                           statementsExecuted, linesEmitted, line->number);
        return 0;
      }
      if (!bas_execute_let(runtime, lineBuffer, &assignmentStatus)) {
        bas_set_run_result(result, assignmentStatus, statementsExecuted,
                           linesEmitted, line->number);
        return 0;
      }
      pc++;
      continue;
    }

    if (token == BAS_TOK_IF) {
      BasicRunStatus ifStatus = BAS_RUN_BAD_EXPRESSION;
      uint8_t truth = 0;
      uint16_t target;

      if (!bas_copy_payload_to_buffer(bytes, line->length, lineBuffer,
                                      lineBufferBytes)) {
        bas_set_run_result(result, BAS_RUN_BUFFER_TOO_SMALL,
                           statementsExecuted, linesEmitted, line->number);
        return 0;
      }
      if (!bas_execute_if(lineBuffer, runtime, &truth, &target, &ifStatus)) {
        bas_set_run_result(result, ifStatus, statementsExecuted, linesEmitted,
                           line->number);
        return 0;
      }
      if (truth) {
        uint8_t targetIndex = 0;

        if (!bas_find_line(program, target, &targetIndex)) {
          bas_set_run_result(result, BAS_RUN_MISSING_LINE, statementsExecuted,
                             linesEmitted, line->number);
          return 0;
        }
        pc = targetIndex;
      } else {
        pc++;
      }
      continue;
    }

    if (token == BAS_TOK_GOTO) {
      uint16_t target;
      uint8_t targetIndex = 0;

      if (!bas_copy_payload_to_buffer(bytes, line->length, lineBuffer,
                                      lineBufferBytes)) {
        bas_set_run_result(result, BAS_RUN_BUFFER_TOO_SMALL,
                           statementsExecuted, linesEmitted, line->number);
        return 0;
      }
      if (!bas_parse_line_target(lineBuffer, &target)) {
        bas_set_run_result(result, BAS_RUN_BAD_TARGET, statementsExecuted,
                           linesEmitted, line->number);
        return 0;
      }
      if (!bas_find_line(program, target, &targetIndex)) {
        bas_set_run_result(result, BAS_RUN_MISSING_LINE, statementsExecuted,
                           linesEmitted, line->number);
        return 0;
      }
      pc = targetIndex;
      continue;
    }

    bas_set_run_result(result, BAS_RUN_UNSUPPORTED_STATEMENT,
                       statementsExecuted, linesEmitted, line->number);
    return 0;
  }

  bas_set_run_result(result, BAS_RUN_COMPLETE, statementsExecuted,
                     linesEmitted, 0);
  return 1;
}

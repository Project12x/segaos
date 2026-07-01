#include "basic.h"
#include <stdio.h>
#include <string.h>

static int failures;
static char listedLines[4][48];
static uint8_t listedLineCount;

static void expect_true(uint8_t value, const char *name) {
  if (!value) {
    printf("FAIL: %s expected true\n", name);
    failures++;
  }
}

static void expect_false(uint8_t value, const char *name) {
  if (value) {
    printf("FAIL: %s expected false\n", name);
    failures++;
  }
}

static void expect_u8(uint8_t actual, uint8_t expected, const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected %u got %u\n", name, expected, actual);
    failures++;
  }
}

static void expect_u16(uint16_t actual, uint16_t expected, const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected %u got %u\n", name, expected, actual);
    failures++;
  }
}

static void expect_i16(int16_t actual, int16_t expected, const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected %d got %d\n", name, expected, actual);
    failures++;
  }
}

static void expect_str(const char *actual, const char *expected,
                       const char *name) {
  if (strcmp(actual, expected) != 0) {
    printf("FAIL: %s expected \"%s\" got \"%s\"\n", name, expected, actual);
    failures++;
  }
}

static uint8_t capture_list_line(const char *line, void *user) {
  (void)user;
  if (listedLineCount >= 4)
    return 0;
  for (uint8_t i = 0; i < sizeof(listedLines[0]); i++) {
    listedLines[listedLineCount][i] = line[i];
    if (!line[i])
      break;
  }
  listedLines[listedLineCount][sizeof(listedLines[0]) - 1U] = 0;
  listedLineCount++;
  return 1;
}

static void clear_list_capture(void) {
  listedLineCount = 0;
  for (uint8_t y = 0; y < 4; y++) {
    listedLines[y][0] = 0;
  }
}

static void parse_print_line_tokenizes_keyword(void) {
  BasicParsedLine parsed;

  expect_true(BAS_ParseSourceLine("10 PRINT \"HELLO\"", &parsed),
              "parse print source line");
  expect_u16(parsed.number, 10, "print line number");
  expect_u8(parsed.token, BAS_TOK_PRINT, "print token");
  expect_str(parsed.payload, "\"HELLO\"", "print payload");
  expect_u16(parsed.payloadLength, 7, "print payload length");
}

static void program_stores_lines_sorted(void) {
  BasicLine lines[4];
  uint8_t storage[64];
  BasicProgram program;

  BAS_InitProgram(&program, lines, 4, storage, sizeof(storage));
  expect_true(BAS_StoreSourceLine(&program, "20 GOTO 10"), "store line 20");
  expect_true(BAS_StoreSourceLine(&program, "10 PRINT \"HELLO\""),
              "store line 10");

  expect_u8(program.lineCount, 2, "line count after sorted insert");
  expect_u16(BAS_GetLine(&program, 0)->number, 10, "first sorted line");
  expect_u16(BAS_GetLine(&program, 1)->number, 20, "second sorted line");
}

static void keyword_prefix_stays_raw(void) {
  BasicParsedLine parsed;

  expect_true(BAS_ParseSourceLine("10 PRINTX \"NO\"", &parsed),
              "parse raw keyword prefix line");
  expect_u8(parsed.token, BAS_TOK_RAW, "keyword prefix is raw");
  expect_str(parsed.payload, "PRINTX \"NO\"", "keyword prefix payload");
}

static void replacing_line_compacts_storage(void) {
  BasicLine lines[4];
  uint8_t storage[64];
  BasicProgram program;
  char out[40];

  BAS_InitProgram(&program, lines, 4, storage, sizeof(storage));
  expect_true(BAS_StoreSourceLine(&program, "10 PRINT \"HELLO\""),
              "store original line");
  expect_true(BAS_StoreSourceLine(&program, "20 END"), "store end line");
  expect_true(BAS_StoreSourceLine(&program, "10 PRINT \"BYE\""),
              "replace line 10");

  expect_u8(program.lineCount, 2, "line count after replace");
  expect_true(BAS_DecodeLine(&program, BAS_GetLine(&program, 0), out,
                             sizeof(out)),
              "decode replaced line");
  expect_str(out, "10 PRINT \"BYE\"", "decoded replaced line");
  expect_u16(program.storageUsed, 7, "compacted storage size");
}

static void empty_body_deletes_line(void) {
  BasicLine lines[4];
  uint8_t storage[64];
  BasicProgram program;

  BAS_InitProgram(&program, lines, 4, storage, sizeof(storage));
  expect_true(BAS_StoreSourceLine(&program, "10 PRINT \"HELLO\""),
              "store line before delete");
  expect_true(BAS_StoreSourceLine(&program, "10"), "delete line");

  expect_u8(program.lineCount, 0, "line count after delete");
  expect_u16(program.storageUsed, 0, "storage used after delete");
}

static void rejects_invalid_or_oversized_source(void) {
  BasicParsedLine parsed;
  BasicLine lines[1];
  uint8_t storage[4];
  BasicProgram program;

  BAS_InitProgram(&program, lines, 1, storage, sizeof(storage));

  expect_false(BAS_ParseSourceLine("PRINT \"NO LINE\"", &parsed),
               "reject missing line number");
  expect_false(BAS_ParseSourceLine("0 PRINT \"ZERO\"", &parsed),
               "reject zero line number");
  expect_false(BAS_StoreSourceLine(&program, "10 PRINT \"TOO BIG\""),
               "reject storage overflow");
  expect_u8(program.lineCount, 0, "no line after overflow");
}

static void shell_stores_lines_and_lists_program(void) {
  BasicLine lines[4];
  uint8_t storage[96];
  BasicProgram program;
  BasicCommandResult result;
  char lineBuffer[48];

  BAS_InitProgram(&program, lines, 4, storage, sizeof(storage));
  clear_list_capture();

  expect_true(BAS_SubmitConsoleLine(&program, "20 GOTO 10", capture_list_line,
                                    0, lineBuffer, sizeof(lineBuffer),
                                    &result),
              "shell stores line 20");
  expect_u8(result.kind, BAS_CMD_PROGRAM_LINE, "line input command kind");
  expect_true(BAS_SubmitConsoleLine(&program, "10 PRINT \"HELLO\"",
                                    capture_list_line, 0, lineBuffer,
                                    sizeof(lineBuffer), &result),
              "shell stores line 10");

  expect_true(BAS_SubmitConsoleLine(&program, "LIST", capture_list_line, 0,
                                    lineBuffer, sizeof(lineBuffer), &result),
              "shell lists program");
  expect_u8(result.kind, BAS_CMD_LIST, "list command kind");
  expect_u8(result.linesEmitted, 2, "listed line count result");
  expect_u8(listedLineCount, 2, "captured line count");
  expect_str(listedLines[0], "10 PRINT \"HELLO\"", "first listed line");
  expect_str(listedLines[1], "20 GOTO 10", "second listed line");
}

static void shell_new_clears_program(void) {
  BasicLine lines[4];
  uint8_t storage[64];
  BasicProgram program;
  BasicCommandResult result;
  char lineBuffer[32];

  BAS_InitProgram(&program, lines, 4, storage, sizeof(storage));
  expect_true(BAS_StoreSourceLine(&program, "10 PRINT \"HELLO\""),
              "store before NEW");
  expect_true(BAS_SubmitConsoleLine(&program, "NEW", capture_list_line, 0,
                                    lineBuffer, sizeof(lineBuffer), &result),
              "shell NEW command");

  expect_u8(result.kind, BAS_CMD_NEW, "new command kind");
  expect_u8(program.lineCount, 0, "NEW clears line count");
  expect_u16(program.storageUsed, 0, "NEW clears storage");
}

static void shell_rejects_unsupported_commands(void) {
  BasicLine lines[4];
  uint8_t storage[64];
  BasicProgram program;
  BasicCommandResult result;
  char lineBuffer[32];

  BAS_InitProgram(&program, lines, 4, storage, sizeof(storage));
  expect_false(BAS_SubmitConsoleLine(&program, "RUN", capture_list_line, 0,
                                     lineBuffer, sizeof(lineBuffer), &result),
               "shell rejects RUN before evaluator exists");
  expect_u8(result.kind, BAS_CMD_UNSUPPORTED, "unsupported command kind");
  expect_u8(program.lineCount, 0, "unsupported leaves program empty");
}

static void evaluates_integer_expressions(void) {
  BasicValue value;

  expect_true(BAS_EvaluateExpression("12 + 5 - 3", &value),
              "evaluate integer arithmetic");
  expect_u8(value.kind, BAS_VALUE_INTEGER, "integer expression kind");
  expect_i16(value.integer, 14, "integer expression result");

  expect_true(BAS_EvaluateExpression("-4 + +7", &value),
              "evaluate signed integer arithmetic");
  expect_u8(value.kind, BAS_VALUE_INTEGER, "signed integer expression kind");
  expect_i16(value.integer, 3, "signed integer expression result");
}

static void evaluates_string_literals(void) {
  BasicValue value;

  expect_true(BAS_EvaluateExpression("\"HELLO\"", &value),
              "evaluate string literal");
  expect_u8(value.kind, BAS_VALUE_STRING, "string expression kind");
  expect_str(value.string, "HELLO", "string expression value");
  expect_u16(value.stringLength, 5, "string expression length");
}

static void rejects_bad_expressions(void) {
  BasicValue value;

  expect_false(BAS_EvaluateExpression("", &value), "reject empty expression");
  expect_false(BAS_EvaluateExpression("\"NO END", &value),
               "reject unterminated string");
  expect_false(BAS_EvaluateExpression("1 + \"X\"", &value),
               "reject mixed integer and string expression");
  expect_false(BAS_EvaluateExpression("40000", &value),
               "reject integer overflow");
}

int main(void) {
  parse_print_line_tokenizes_keyword();
  program_stores_lines_sorted();
  keyword_prefix_stays_raw();
  replacing_line_compacts_storage();
  empty_body_deletes_line();
  rejects_invalid_or_oversized_source();
  shell_stores_lines_and_lists_program();
  shell_new_clears_program();
  shell_rejects_unsupported_commands();
  evaluates_integer_expressions();
  evaluates_string_literals();
  rejects_bad_expressions();

  if (failures) {
    printf("basic program tests failed: %d\n", failures);
    return 1;
  }

  printf("basic program tests passed\n");
  return 0;
}

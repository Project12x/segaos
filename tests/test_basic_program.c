#include "basic.h"
#include <stdio.h>
#include <string.h>

static int failures;

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

static void expect_str(const char *actual, const char *expected,
                       const char *name) {
  if (strcmp(actual, expected) != 0) {
    printf("FAIL: %s expected \"%s\" got \"%s\"\n", name, expected, actual);
    failures++;
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

int main(void) {
  parse_print_line_tokenizes_keyword();
  program_stores_lines_sorted();
  keyword_prefix_stays_raw();
  replacing_line_compacts_storage();
  empty_body_deletes_line();
  rejects_invalid_or_oversized_source();

  if (failures) {
    printf("basic program tests failed: %d\n", failures);
    return 1;
  }

  printf("basic program tests passed\n");
  return 0;
}

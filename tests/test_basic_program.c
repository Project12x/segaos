#include "basic.h"
#include <stdio.h>
#include <string.h>

static int failures;
static char listedLines[4][48];
static uint8_t listedLineCount;
static char runOutputLines[4][48];
static uint8_t runOutputLineCount;

typedef struct {
  const char *const *lines;
  uint8_t count;
  uint8_t index;
} BasicInputFixture;

typedef struct {
  uint8_t bytes[192];
  uint16_t bytesUsed;
  uint8_t saveCalls;
  uint8_t loadCalls;
  uint8_t failLoad;
} BasicStorageFixture;

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

static uint8_t capture_run_output(const char *line, void *user) {
  (void)user;
  if (runOutputLineCount >= 4)
    return 0;
  for (uint8_t i = 0; i < sizeof(runOutputLines[0]); i++) {
    runOutputLines[runOutputLineCount][i] = line[i];
    if (!line[i])
      break;
  }
  runOutputLines[runOutputLineCount][sizeof(runOutputLines[0]) - 1U] = 0;
  runOutputLineCount++;
  return 1;
}

static void clear_run_capture(void) {
  runOutputLineCount = 0;
  for (uint8_t y = 0; y < 4; y++) {
    runOutputLines[y][0] = 0;
  }
}

static uint8_t feed_input_line(char *out, uint16_t outBytes, void *user) {
  BasicInputFixture *fixture = (BasicInputFixture *)user;
  const char *line;
  uint16_t i = 0;

  if (!fixture || !out || outBytes == 0 || fixture->index >= fixture->count)
    return 0;

  line = fixture->lines[fixture->index];
  while (line[i] && i + 1U < outBytes) {
    out[i] = line[i];
    i++;
  }
  if (line[i])
    return 0;

  out[i] = 0;
  fixture->index++;
  return 1;
}

static uint8_t capture_program_image(const uint8_t *image, uint16_t imageBytes,
                                     void *user) {
  BasicStorageFixture *fixture = (BasicStorageFixture *)user;

  if (!fixture || !image || imageBytes > sizeof(fixture->bytes))
    return 0;

  for (uint16_t i = 0; i < imageBytes; i++) {
    fixture->bytes[i] = image[i];
  }
  fixture->bytesUsed = imageBytes;
  fixture->saveCalls++;
  return 1;
}

static uint8_t feed_program_image(uint8_t *image, uint16_t imageBytes,
                                  uint16_t *bytesRead, void *user) {
  BasicStorageFixture *fixture = (BasicStorageFixture *)user;

  if (bytesRead)
    *bytesRead = 0;
  if (!fixture || !image || !bytesRead || fixture->failLoad ||
      fixture->bytesUsed > imageBytes)
    return 0;

  for (uint16_t i = 0; i < fixture->bytesUsed; i++) {
    image[i] = fixture->bytes[i];
  }
  *bytesRead = fixture->bytesUsed;
  fixture->loadCalls++;
  return 1;
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

static void program_image_round_trips_tokenized_program(void) {
  BasicLine lines[4];
  BasicLine importedLines[4];
  uint8_t storage[128];
  uint8_t importedStorage[128];
  uint8_t image[192];
  BasicProgram program;
  BasicProgram imported;
  uint16_t written = 0;
  char lineBuffer[48];

  BAS_InitProgram(&program, lines, 4, storage, sizeof(storage));
  BAS_InitProgram(&imported, importedLines, 4, importedStorage,
                  sizeof(importedStorage));
  clear_list_capture();

  expect_true(BAS_StoreSourceLine(&program, "20 GOTO 40"),
              "store image line 20");
  expect_true(BAS_StoreSourceLine(&program, "10 PRINT \"A\""),
              "store image line 10");
  expect_true(BAS_StoreSourceLine(&program, "30 LET A = 5"),
              "store image line 30");
  expect_true(BAS_StoreSourceLine(&program, "40 END"),
              "store image line 40");

  expect_u16(BAS_ProgramImageSize(&program),
             (uint16_t)(8U + (program.lineCount * 6U) + program.storageUsed),
             "program image size");
  expect_true(BAS_ExportProgramImage(&program, image, sizeof(image), &written),
              "export program image");
  expect_u16(written, BAS_ProgramImageSize(&program),
             "exported program image bytes");
  expect_true(BAS_ImportProgramImage(&imported, image, written),
              "import program image");

  expect_true(BAS_ListProgram(&imported, capture_list_line, 0, lineBuffer,
                              sizeof(lineBuffer), 0),
              "list imported program");
  expect_u8(listedLineCount, 4, "imported listed line count");
  expect_str(listedLines[0], "10 PRINT \"A\"", "imported first line");
  expect_str(listedLines[1], "20 GOTO 40", "imported second line");
  expect_str(listedLines[2], "30 LET A = 5", "imported third line");
  expect_str(listedLines[3], "40 END", "imported fourth line");
}

static void program_image_reports_required_export_size(void) {
  BasicLine lines[1];
  uint8_t storage[32];
  uint8_t image[4];
  BasicProgram program;
  uint16_t written = 0;

  BAS_InitProgram(&program, lines, 1, storage, sizeof(storage));
  expect_true(BAS_StoreSourceLine(&program, "10 END"),
              "store small image program");

  expect_false(BAS_ExportProgramImage(&program, image, sizeof(image),
                                      &written),
               "reject undersized image export");
  expect_u16(written, BAS_ProgramImageSize(&program),
             "undersized export required bytes");
}

static void program_image_rejects_bad_magic_without_clearing_program(void) {
  BasicLine lines[1];
  BasicLine importedLines[1];
  uint8_t storage[32];
  uint8_t importedStorage[32];
  uint8_t image[64];
  BasicProgram program;
  BasicProgram imported;
  uint16_t written = 0;
  char lineBuffer[48];

  BAS_InitProgram(&program, lines, 1, storage, sizeof(storage));
  BAS_InitProgram(&imported, importedLines, 1, importedStorage,
                  sizeof(importedStorage));
  clear_list_capture();

  expect_true(BAS_StoreSourceLine(&program, "10 END"),
              "store image source before corrupt import");
  expect_true(BAS_StoreSourceLine(&imported, "10 PRINT \"KEEP\""),
              "store image destination before corrupt import");
  expect_true(BAS_ExportProgramImage(&program, image, sizeof(image), &written),
              "export before corrupt import");
  image[0] = 0;

  expect_false(BAS_ImportProgramImage(&imported, image, written),
               "reject corrupt image magic");
  expect_true(BAS_ListProgram(&imported, capture_list_line, 0, lineBuffer,
                              sizeof(lineBuffer), 0),
              "list destination after corrupt import");
  expect_u8(listedLineCount, 1, "corrupt import preserves line count");
  expect_str(listedLines[0], "10 PRINT \"KEEP\"",
             "corrupt import preserves program");
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

static void shell_run_executes_print_program(void) {
  BasicLine lines[4];
  uint8_t storage[64];
  BasicProgram program;
  BasicCommandResult result;
  char lineBuffer[48];

  BAS_InitProgram(&program, lines, 4, storage, sizeof(storage));
  clear_run_capture();

  expect_true(BAS_StoreSourceLine(&program, "10 PRINT \"HELLO\""),
              "store string print before RUN");
  expect_true(BAS_StoreSourceLine(&program, "20 PRINT 12 + 5"),
              "store integer print before RUN");
  expect_true(BAS_StoreSourceLine(&program, "30 END"),
              "store end before RUN");

  expect_true(BAS_SubmitConsoleLine(&program, "RUN", capture_run_output, 0,
                                    lineBuffer, sizeof(lineBuffer), &result),
              "shell RUN command");
  expect_u8(result.kind, BAS_CMD_RUN, "run command kind");
  expect_u8(result.linesEmitted, 2, "run command output count");
  expect_u8(runOutputLineCount, 2, "captured run output count");
  expect_str(runOutputLines[0], "HELLO", "first run output line");
  expect_str(runOutputLines[1], "17", "second run output line");
}

static void shell_save_exports_program_image_to_storage_callback(void) {
  BasicLine lines[4];
  uint8_t storageBytes[96];
  uint8_t imageScratch[192];
  BasicProgram program;
  BasicStorageFixture storage = {0};
  BasicStorageIO storageIo;
  BasicCommandResult result;
  char lineBuffer[48];

  BAS_InitProgram(&program, lines, 4, storageBytes, sizeof(storageBytes));
  expect_true(BAS_StoreSourceLine(&program, "10 PRINT \"SAVE\""),
              "store line before shell SAVE");
  expect_true(BAS_StoreSourceLine(&program, "20 END"),
              "store end before shell SAVE");

  storageIo.save = capture_program_image;
  storageIo.load = feed_program_image;
  storageIo.user = &storage;
  storageIo.imageBuffer = imageScratch;
  storageIo.imageBufferBytes = sizeof(imageScratch);

  expect_true(BAS_SubmitConsoleLineWithStorage(
                  &program, "SAVE", capture_list_line, 0, lineBuffer,
                  sizeof(lineBuffer), &storageIo, &result),
              "shell SAVE command");
  expect_u8(result.kind, BAS_CMD_SAVE, "save command kind");
  expect_u16(result.bytesTransferred, BAS_ProgramImageSize(&program),
             "save image byte count");
  expect_u8(storage.saveCalls, 1, "save callback count");
  expect_u16(storage.bytesUsed, BAS_ProgramImageSize(&program),
             "saved image bytes used");
  expect_u8(storage.bytes[0], 'S', "save image magic S");
  expect_u8(storage.bytes[1], 'B', "save image magic B");
  expect_u8(storage.bytes[2], 'A', "save image magic A");
  expect_u8(storage.bytes[3], 'S', "save image magic final S");
}

static void shell_load_imports_program_image_from_storage_callback(void) {
  BasicLine sourceLines[4];
  BasicLine destLines[4];
  uint8_t sourceStorage[96];
  uint8_t destStorage[96];
  uint8_t imageScratch[192];
  BasicProgram source;
  BasicProgram dest;
  BasicStorageFixture storage = {0};
  BasicStorageIO storageIo;
  BasicCommandResult result;
  uint16_t written = 0;
  char lineBuffer[48];

  BAS_InitProgram(&source, sourceLines, 4, sourceStorage,
                  sizeof(sourceStorage));
  BAS_InitProgram(&dest, destLines, 4, destStorage, sizeof(destStorage));
  clear_list_capture();

  expect_true(BAS_StoreSourceLine(&source, "10 PRINT \"LOAD\""),
              "store source line before LOAD");
  expect_true(BAS_StoreSourceLine(&source, "20 END"),
              "store source end before LOAD");
  expect_true(BAS_ExportProgramImage(&source, storage.bytes,
                                     sizeof(storage.bytes), &written),
              "prepare stored image before LOAD");
  storage.bytesUsed = written;

  expect_true(BAS_StoreSourceLine(&dest, "10 PRINT \"OLD\""),
              "store old line before LOAD");

  storageIo.save = capture_program_image;
  storageIo.load = feed_program_image;
  storageIo.user = &storage;
  storageIo.imageBuffer = imageScratch;
  storageIo.imageBufferBytes = sizeof(imageScratch);

  expect_true(BAS_SubmitConsoleLineWithStorage(
                  &dest, "LOAD", capture_list_line, 0, lineBuffer,
                  sizeof(lineBuffer), &storageIo, &result),
              "shell LOAD command");
  expect_u8(result.kind, BAS_CMD_LOAD, "load command kind");
  expect_u16(result.bytesTransferred, written, "load image byte count");
  expect_u8(storage.loadCalls, 1, "load callback count");

  expect_true(BAS_ListProgram(&dest, capture_list_line, 0, lineBuffer,
                              sizeof(lineBuffer), 0),
              "list program after shell LOAD");
  expect_u8(listedLineCount, 2, "loaded shell line count");
  expect_str(listedLines[0], "10 PRINT \"LOAD\"", "loaded first line");
  expect_str(listedLines[1], "20 END", "loaded second line");
}

static void shell_load_failure_preserves_existing_program(void) {
  BasicLine destLines[4];
  uint8_t destStorage[96];
  uint8_t imageScratch[192];
  BasicProgram dest;
  BasicStorageFixture storage = {0};
  BasicStorageIO storageIo;
  BasicCommandResult result;
  char lineBuffer[48];

  BAS_InitProgram(&dest, destLines, 4, destStorage, sizeof(destStorage));
  clear_list_capture();
  expect_true(BAS_StoreSourceLine(&dest, "10 PRINT \"KEEP\""),
              "store old line before failed LOAD");

  storage.bytes[0] = 0;
  storage.bytes[1] = 'B';
  storage.bytes[2] = 'A';
  storage.bytes[3] = 'S';
  storage.bytesUsed = 8;

  storageIo.save = capture_program_image;
  storageIo.load = feed_program_image;
  storageIo.user = &storage;
  storageIo.imageBuffer = imageScratch;
  storageIo.imageBufferBytes = sizeof(imageScratch);

  expect_false(BAS_SubmitConsoleLineWithStorage(
                   &dest, "LOAD", capture_list_line, 0, lineBuffer,
                   sizeof(lineBuffer), &storageIo, &result),
               "shell rejects corrupt LOAD image");
  expect_u8(result.kind, BAS_CMD_LOAD, "failed load command kind");
  expect_u8(result.ok, 0, "failed load result ok");
  expect_true(BAS_ListProgram(&dest, capture_list_line, 0, lineBuffer,
                              sizeof(lineBuffer), 0),
              "list program after failed shell LOAD");
  expect_u8(listedLineCount, 1, "failed load keeps line count");
  expect_str(listedLines[0], "10 PRINT \"KEEP\"",
             "failed load preserves program");
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

static void evaluates_integer_variables_with_runtime(void) {
  BasicRuntime runtime;
  BasicValue value;

  BAS_InitRuntime(&runtime);
  expect_true(BAS_RuntimeSetInteger(&runtime, 'A', 6), "set runtime variable");
  expect_true(BAS_EvaluateExpressionWithRuntime("A + 4", &runtime, &value),
              "evaluate variable expression");
  expect_u8(value.kind, BAS_VALUE_INTEGER, "variable expression kind");
  expect_i16(value.integer, 10, "variable expression result");
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

static void runner_reports_unsupported_statement_line(void) {
  BasicLine lines[2];
  uint8_t storage[64];
  BasicProgram program;
  BasicRunResult result;
  char lineBuffer[32];

  BAS_InitProgram(&program, lines, 2, storage, sizeof(storage));
  expect_true(BAS_StoreSourceLine(&program, "10 REM NOT YET"),
              "store unsupported runner line");

  expect_false(BAS_RunProgram(&program, capture_run_output, 0, lineBuffer,
                              sizeof(lineBuffer), &result),
               "runner rejects unsupported statement");
  expect_u8(result.status, BAS_RUN_UNSUPPORTED_STATEMENT,
            "unsupported statement status");
  expect_u16(result.errorLine, 10, "unsupported statement line");
}

static void runner_goto_jumps_to_target_line(void) {
  BasicLine lines[5];
  uint8_t storage[128];
  BasicProgram program;
  BasicRunResult result;
  char lineBuffer[48];

  BAS_InitProgram(&program, lines, 5, storage, sizeof(storage));
  clear_run_capture();
  expect_true(BAS_StoreSourceLine(&program, "10 PRINT \"A\""),
              "store goto first print");
  expect_true(BAS_StoreSourceLine(&program, "20 GOTO 40"),
              "store goto jump");
  expect_true(BAS_StoreSourceLine(&program, "30 PRINT \"B\""),
              "store skipped print");
  expect_true(BAS_StoreSourceLine(&program, "40 PRINT \"C\""),
              "store goto target print");
  expect_true(BAS_StoreSourceLine(&program, "50 END"), "store goto end");

  expect_true(BAS_RunProgram(&program, capture_run_output, 0, lineBuffer,
                             sizeof(lineBuffer), &result),
              "runner executes GOTO");
  expect_u8(result.status, BAS_RUN_HALTED, "goto run status");
  expect_u8(result.linesEmitted, 2, "goto output count");
  expect_str(runOutputLines[0], "A", "goto first output");
  expect_str(runOutputLines[1], "C", "goto target output");
}

static void runner_reports_missing_goto_target(void) {
  BasicLine lines[2];
  uint8_t storage[64];
  BasicProgram program;
  BasicRunResult result;
  char lineBuffer[32];

  BAS_InitProgram(&program, lines, 2, storage, sizeof(storage));
  expect_true(BAS_StoreSourceLine(&program, "10 GOTO 999"),
              "store missing goto target");

  expect_false(BAS_RunProgram(&program, capture_run_output, 0, lineBuffer,
                              sizeof(lineBuffer), &result),
               "runner rejects missing GOTO target");
  expect_u8(result.status, BAS_RUN_MISSING_LINE, "missing GOTO status");
  expect_u16(result.errorLine, 10, "missing GOTO line");
}

static void runner_gosub_returns_to_next_line(void) {
  BasicLine lines[6];
  uint8_t storage[160];
  BasicProgram program;
  BasicRunResult result;
  char lineBuffer[48];

  BAS_InitProgram(&program, lines, 6, storage, sizeof(storage));
  clear_run_capture();

  expect_true(BAS_StoreSourceLine(&program, "10 PRINT \"A\""),
              "store gosub first print");
  expect_true(BAS_StoreSourceLine(&program, "20 GOSUB 100"),
              "store gosub jump");
  expect_true(BAS_StoreSourceLine(&program, "30 PRINT \"C\""),
              "store gosub resumed print");
  expect_true(BAS_StoreSourceLine(&program, "40 END"), "store gosub end");
  expect_true(BAS_StoreSourceLine(&program, "100 PRINT \"B\""),
              "store gosub subroutine print");
  expect_true(BAS_StoreSourceLine(&program, "110 RETURN"),
              "store gosub return");

  expect_true(BAS_RunProgram(&program, capture_run_output, 0, lineBuffer,
                             sizeof(lineBuffer), &result),
              "runner executes GOSUB");
  expect_u8(result.status, BAS_RUN_HALTED, "GOSUB run status");
  expect_u8(result.linesEmitted, 3, "GOSUB output count");
  expect_str(runOutputLines[0], "A", "GOSUB first output");
  expect_str(runOutputLines[1], "B", "GOSUB subroutine output");
  expect_str(runOutputLines[2], "C", "GOSUB resumed output");
}

static void runner_reports_missing_gosub_target(void) {
  BasicLine lines[1];
  uint8_t storage[32];
  BasicProgram program;
  BasicRunResult result;
  char lineBuffer[32];

  BAS_InitProgram(&program, lines, 1, storage, sizeof(storage));
  expect_true(BAS_StoreSourceLine(&program, "10 GOSUB 999"),
              "store missing gosub target");

  expect_false(BAS_RunProgram(&program, capture_run_output, 0, lineBuffer,
                              sizeof(lineBuffer), &result),
               "runner rejects missing GOSUB target");
  expect_u8(result.status, BAS_RUN_MISSING_LINE, "missing GOSUB status");
  expect_u16(result.errorLine, 10, "missing GOSUB line");
}

static void runner_reports_return_without_gosub(void) {
  BasicLine lines[1];
  uint8_t storage[32];
  BasicProgram program;
  BasicRunResult result;
  char lineBuffer[32];

  BAS_InitProgram(&program, lines, 1, storage, sizeof(storage));
  expect_true(BAS_StoreSourceLine(&program, "10 RETURN"),
              "store stray RETURN");

  expect_false(BAS_RunProgram(&program, capture_run_output, 0, lineBuffer,
                              sizeof(lineBuffer), &result),
               "runner rejects stray RETURN");
  expect_u8(result.status, BAS_RUN_RETURN_WITHOUT_GOSUB,
            "stray RETURN status");
  expect_u16(result.errorLine, 10, "stray RETURN line");
}

static void runner_reports_gosub_stack_overflow(void) {
  BasicLine lines[1];
  uint8_t storage[32];
  BasicProgram program;
  BasicRunResult result;
  char lineBuffer[32];

  BAS_InitProgram(&program, lines, 1, storage, sizeof(storage));
  expect_true(BAS_StoreSourceLine(&program, "10 GOSUB 10"),
              "store recursive GOSUB");

  expect_false(BAS_RunProgram(&program, capture_run_output, 0, lineBuffer,
                              sizeof(lineBuffer), &result),
               "runner rejects GOSUB overflow");
  expect_u8(result.status, BAS_RUN_GOSUB_STACK_OVERFLOW,
            "GOSUB overflow status");
  expect_u16(result.errorLine, 10, "GOSUB overflow line");
}

static void runner_if_then_jumps_when_comparison_true(void) {
  BasicLine lines[6];
  uint8_t storage[160];
  BasicProgram program;
  BasicRuntime runtime;
  BasicRunResult result;
  char lineBuffer[48];

  BAS_InitProgram(&program, lines, 6, storage, sizeof(storage));
  BAS_InitRuntime(&runtime);
  clear_run_capture();

  expect_true(BAS_StoreSourceLine(&program, "10 LET A = 3"),
              "store IF true setup");
  expect_true(BAS_StoreSourceLine(&program, "20 IF A = 3 THEN 50"),
              "store true IF line");
  expect_true(BAS_StoreSourceLine(&program, "30 PRINT \"BAD\""),
              "store skipped true print");
  expect_true(BAS_StoreSourceLine(&program, "40 END"), "store skipped end");
  expect_true(BAS_StoreSourceLine(&program, "50 PRINT \"OK\""),
              "store true target print");
  expect_true(BAS_StoreSourceLine(&program, "60 END"), "store true final end");

  expect_true(BAS_RunProgramWithRuntime(&program, &runtime, capture_run_output,
                                        0, lineBuffer, sizeof(lineBuffer),
                                        &result),
              "runner executes true IF");
  expect_u8(result.status, BAS_RUN_HALTED, "true IF status");
  expect_u8(result.linesEmitted, 1, "true IF output count");
  expect_str(runOutputLines[0], "OK", "true IF output");
}

static void runner_if_then_falls_through_when_comparison_false(void) {
  BasicLine lines[6];
  uint8_t storage[160];
  BasicProgram program;
  BasicRuntime runtime;
  BasicRunResult result;
  char lineBuffer[48];

  BAS_InitProgram(&program, lines, 6, storage, sizeof(storage));
  BAS_InitRuntime(&runtime);
  clear_run_capture();

  expect_true(BAS_StoreSourceLine(&program, "10 LET A = 2"),
              "store IF false setup");
  expect_true(BAS_StoreSourceLine(&program, "20 IF A = 3 THEN 50"),
              "store false IF line");
  expect_true(BAS_StoreSourceLine(&program, "30 PRINT \"NO\""),
              "store false fallthrough print");
  expect_true(BAS_StoreSourceLine(&program, "40 END"), "store false end");
  expect_true(BAS_StoreSourceLine(&program, "50 PRINT \"BAD\""),
              "store false skipped target");
  expect_true(BAS_StoreSourceLine(&program, "60 END"),
              "store false final end");

  expect_true(BAS_RunProgramWithRuntime(&program, &runtime, capture_run_output,
                                        0, lineBuffer, sizeof(lineBuffer),
                                        &result),
              "runner executes false IF");
  expect_u8(result.status, BAS_RUN_HALTED, "false IF status");
  expect_u8(result.linesEmitted, 1, "false IF output count");
  expect_str(runOutputLines[0], "NO", "false IF output");
}

static void runner_if_then_accepts_goto_target(void) {
  BasicLine lines[6];
  uint8_t storage[160];
  BasicProgram program;
  BasicRuntime runtime;
  BasicRunResult result;
  char lineBuffer[48];

  BAS_InitProgram(&program, lines, 6, storage, sizeof(storage));
  BAS_InitRuntime(&runtime);
  clear_run_capture();

  expect_true(BAS_StoreSourceLine(&program, "10 LET A = 1"),
              "store IF GOTO setup");
  expect_true(BAS_StoreSourceLine(&program, "20 IF A THEN GOTO 50"),
              "store IF GOTO line");
  expect_true(BAS_StoreSourceLine(&program, "30 PRINT \"BAD\""),
              "store IF GOTO skipped print");
  expect_true(BAS_StoreSourceLine(&program, "40 END"), "store IF GOTO end");
  expect_true(BAS_StoreSourceLine(&program, "50 PRINT \"GO\""),
              "store IF GOTO target");
  expect_true(BAS_StoreSourceLine(&program, "60 END"),
              "store IF GOTO final end");

  expect_true(BAS_RunProgramWithRuntime(&program, &runtime, capture_run_output,
                                        0, lineBuffer, sizeof(lineBuffer),
                                        &result),
              "runner executes IF THEN GOTO");
  expect_u8(result.status, BAS_RUN_HALTED, "IF THEN GOTO status");
  expect_u8(result.linesEmitted, 1, "IF THEN GOTO output count");
  expect_str(runOutputLines[0], "GO", "IF THEN GOTO output");
}

static void runner_if_then_handles_relational_operators(void) {
  BasicLine lines[13];
  uint8_t storage[320];
  BasicProgram program;
  BasicRuntime runtime;
  BasicRunResult result;
  char lineBuffer[48];

  BAS_InitProgram(&program, lines, 13, storage, sizeof(storage));
  BAS_InitRuntime(&runtime);
  clear_run_capture();

  expect_true(BAS_StoreSourceLine(&program, "10 LET A = 3"),
              "store IF operator setup");
  expect_true(BAS_StoreSourceLine(&program, "20 IF A <> 4 THEN 40"),
              "store not-equal IF");
  expect_true(BAS_StoreSourceLine(&program, "30 PRINT \"BAD\""),
              "store not-equal skipped print");
  expect_true(BAS_StoreSourceLine(&program, "40 IF A < 4 THEN 60"),
              "store less-than IF");
  expect_true(BAS_StoreSourceLine(&program, "50 PRINT \"BAD\""),
              "store less-than skipped print");
  expect_true(BAS_StoreSourceLine(&program, "60 IF A <= 3 THEN 80"),
              "store less-equal IF");
  expect_true(BAS_StoreSourceLine(&program, "70 PRINT \"BAD\""),
              "store less-equal skipped print");
  expect_true(BAS_StoreSourceLine(&program, "80 IF A > 2 THEN 100"),
              "store greater-than IF");
  expect_true(BAS_StoreSourceLine(&program, "90 PRINT \"BAD\""),
              "store greater-than skipped print");
  expect_true(BAS_StoreSourceLine(&program, "100 IF A >= 3 THEN 120"),
              "store greater-equal IF");
  expect_true(BAS_StoreSourceLine(&program, "110 PRINT \"BAD\""),
              "store greater-equal skipped print");
  expect_true(BAS_StoreSourceLine(&program, "120 PRINT \"OPS\""),
              "store operator target print");
  expect_true(BAS_StoreSourceLine(&program, "130 END"),
              "store operator final end");

  expect_true(BAS_RunProgramWithRuntime(&program, &runtime, capture_run_output,
                                        0, lineBuffer, sizeof(lineBuffer),
                                        &result),
              "runner executes IF relational operators");
  expect_u8(result.status, BAS_RUN_HALTED, "IF relational status");
  expect_u8(result.linesEmitted, 1, "IF relational output count");
  expect_str(runOutputLines[0], "OPS", "IF relational output");
}

static void runner_if_then_reports_missing_target(void) {
  BasicLine lines[1];
  uint8_t storage[48];
  BasicProgram program;
  BasicRunResult result;
  char lineBuffer[48];

  BAS_InitProgram(&program, lines, 1, storage, sizeof(storage));
  expect_true(BAS_StoreSourceLine(&program, "10 IF 1 THEN 999"),
              "store missing IF target");

  expect_false(BAS_RunProgram(&program, capture_run_output, 0, lineBuffer,
                              sizeof(lineBuffer), &result),
               "runner rejects missing IF target");
  expect_u8(result.status, BAS_RUN_MISSING_LINE, "missing IF status");
  expect_u16(result.errorLine, 10, "missing IF line");
}

static void runner_if_then_reports_bad_condition(void) {
  BasicLine lines[2];
  uint8_t storage[64];
  BasicProgram program;
  BasicRunResult result;
  char lineBuffer[48];

  BAS_InitProgram(&program, lines, 2, storage, sizeof(storage));
  expect_true(BAS_StoreSourceLine(&program, "10 IF \"X\" THEN 20"),
              "store bad IF condition");
  expect_true(BAS_StoreSourceLine(&program, "20 END"),
              "store bad IF target");

  expect_false(BAS_RunProgram(&program, capture_run_output, 0, lineBuffer,
                              sizeof(lineBuffer), &result),
               "runner rejects bad IF condition");
  expect_u8(result.status, BAS_RUN_BAD_EXPRESSION, "bad IF status");
  expect_u16(result.errorLine, 10, "bad IF line");
}

static void runner_stops_goto_loops_at_step_limit(void) {
  BasicLine lines[1];
  uint8_t storage[32];
  BasicProgram program;
  BasicRunResult result;
  char lineBuffer[32];

  BAS_InitProgram(&program, lines, 1, storage, sizeof(storage));
  expect_true(BAS_StoreSourceLine(&program, "10 GOTO 10"),
              "store infinite goto loop");

  expect_false(BAS_RunProgram(&program, capture_run_output, 0, lineBuffer,
                              sizeof(lineBuffer), &result),
               "runner stops GOTO loop");
  expect_u8(result.status, BAS_RUN_STEP_LIMIT, "GOTO loop status");
  expect_u8(result.statementsExecuted, BAS_RUN_MAX_STEPS,
            "GOTO loop step count");
  expect_u16(result.errorLine, 10, "GOTO loop line");
}

static void runner_reports_bad_print_expression_line(void) {
  BasicLine lines[2];
  uint8_t storage[64];
  BasicProgram program;
  BasicRunResult result;
  char lineBuffer[32];

  BAS_InitProgram(&program, lines, 2, storage, sizeof(storage));
  expect_true(BAS_StoreSourceLine(&program, "10 PRINT 1 + \"X\""),
              "store bad print expression line");

  expect_false(BAS_RunProgram(&program, capture_run_output, 0, lineBuffer,
                              sizeof(lineBuffer), &result),
               "runner rejects bad print expression");
  expect_u8(result.status, BAS_RUN_BAD_EXPRESSION, "bad expression status");
  expect_u16(result.errorLine, 10, "bad expression line");
}

static void runner_let_sets_integer_variable(void) {
  BasicLine lines[3];
  uint8_t storage[96];
  BasicProgram program;
  BasicRuntime runtime;
  BasicRunResult result;
  char lineBuffer[48];
  int16_t stored = 0;

  BAS_InitProgram(&program, lines, 3, storage, sizeof(storage));
  BAS_InitRuntime(&runtime);
  clear_run_capture();

  expect_true(BAS_StoreSourceLine(&program, "10 LET A = 12 + 5"),
              "store LET assignment");
  expect_true(BAS_StoreSourceLine(&program, "20 PRINT A - 2"),
              "store variable print");
  expect_true(BAS_StoreSourceLine(&program, "30 END"), "store LET end");

  expect_true(BAS_RunProgramWithRuntime(&program, &runtime, capture_run_output,
                                        0, lineBuffer, sizeof(lineBuffer),
                                        &result),
              "runner executes LET");
  expect_u8(result.status, BAS_RUN_HALTED, "LET run status");
  expect_u8(result.linesEmitted, 1, "LET output count");
  expect_str(runOutputLines[0], "15", "LET output line");
  expect_true(BAS_RuntimeGetInteger(&runtime, 'A', &stored),
              "get assigned variable");
  expect_i16(stored, 17, "assigned variable value");
}

static void runner_reports_undefined_variable_line(void) {
  BasicLine lines[1];
  uint8_t storage[32];
  BasicProgram program;
  BasicRunResult result;
  char lineBuffer[32];

  BAS_InitProgram(&program, lines, 1, storage, sizeof(storage));
  expect_true(BAS_StoreSourceLine(&program, "10 PRINT A"),
              "store undefined variable print");

  expect_false(BAS_RunProgram(&program, capture_run_output, 0, lineBuffer,
                              sizeof(lineBuffer), &result),
               "runner rejects undefined variable");
  expect_u8(result.status, BAS_RUN_UNDEFINED_VARIABLE,
            "undefined variable status");
  expect_u16(result.errorLine, 10, "undefined variable line");
}

static void runner_rejects_string_assignment(void) {
  BasicLine lines[1];
  uint8_t storage[32];
  BasicProgram program;
  BasicRuntime runtime;
  BasicRunResult result;
  char lineBuffer[32];

  BAS_InitProgram(&program, lines, 1, storage, sizeof(storage));
  BAS_InitRuntime(&runtime);
  expect_true(BAS_StoreSourceLine(&program, "10 LET A = \"X\""),
              "store string assignment");

  expect_false(BAS_RunProgramWithRuntime(&program, &runtime,
                                         capture_run_output, 0, lineBuffer,
                                         sizeof(lineBuffer), &result),
               "runner rejects string assignment");
  expect_u8(result.status, BAS_RUN_BAD_ASSIGNMENT,
            "string assignment status");
  expect_u16(result.errorLine, 10, "string assignment line");
}

static void runner_input_assigns_integer_variable(void) {
  static const char *const inputLines[] = {"40"};
  BasicInputFixture input = {inputLines, 1, 0};
  BasicLine lines[3];
  uint8_t storage[96];
  BasicProgram program;
  BasicRuntime runtime;
  BasicRunResult result;
  char lineBuffer[48];
  int16_t stored = 0;

  BAS_InitProgram(&program, lines, 3, storage, sizeof(storage));
  BAS_InitRuntime(&runtime);
  clear_run_capture();

  expect_true(BAS_StoreSourceLine(&program, "10 INPUT A"),
              "store input statement");
  expect_true(BAS_StoreSourceLine(&program, "20 PRINT A + 2"),
              "store input print");
  expect_true(BAS_StoreSourceLine(&program, "30 END"), "store input end");

  expect_true(BAS_RunProgramWithIO(&program, &runtime, capture_run_output,
                                   feed_input_line, &input, lineBuffer,
                                   sizeof(lineBuffer), &result),
              "runner executes INPUT");
  expect_u8(result.status, BAS_RUN_HALTED, "INPUT run status");
  expect_u8(result.linesEmitted, 1, "INPUT output count");
  expect_str(runOutputLines[0], "42", "INPUT output line");
  expect_u8(input.index, 1, "INPUT consumed one line");
  expect_true(BAS_RuntimeGetInteger(&runtime, 'A', &stored),
              "get input variable");
  expect_i16(stored, 40, "input variable value");
}

static void runner_input_reports_unavailable_source(void) {
  BasicLine lines[1];
  uint8_t storage[32];
  BasicProgram program;
  BasicRunResult result;
  char lineBuffer[32];

  BAS_InitProgram(&program, lines, 1, storage, sizeof(storage));
  expect_true(BAS_StoreSourceLine(&program, "10 INPUT A"),
              "store unavailable input statement");

  expect_false(BAS_RunProgram(&program, capture_run_output, 0, lineBuffer,
                              sizeof(lineBuffer), &result),
               "runner rejects unavailable input");
  expect_u8(result.status, BAS_RUN_INPUT_UNAVAILABLE,
            "unavailable input status");
  expect_u16(result.errorLine, 10, "unavailable input line");
}

static void runner_input_reports_bad_integer(void) {
  static const char *const inputLines[] = {"\"X\""};
  BasicInputFixture input = {inputLines, 1, 0};
  BasicLine lines[1];
  uint8_t storage[32];
  BasicProgram program;
  BasicRuntime runtime;
  BasicRunResult result;
  char lineBuffer[32];

  BAS_InitProgram(&program, lines, 1, storage, sizeof(storage));
  BAS_InitRuntime(&runtime);
  expect_true(BAS_StoreSourceLine(&program, "10 INPUT A"),
              "store bad input statement");

  expect_false(BAS_RunProgramWithIO(&program, &runtime, capture_run_output,
                                    feed_input_line, &input, lineBuffer,
                                    sizeof(lineBuffer), &result),
               "runner rejects bad integer input");
  expect_u8(result.status, BAS_RUN_BAD_INPUT, "bad input status");
  expect_u16(result.errorLine, 10, "bad input line");
}

int main(void) {
  parse_print_line_tokenizes_keyword();
  program_stores_lines_sorted();
  keyword_prefix_stays_raw();
  replacing_line_compacts_storage();
  empty_body_deletes_line();
  program_image_round_trips_tokenized_program();
  program_image_reports_required_export_size();
  program_image_rejects_bad_magic_without_clearing_program();
  rejects_invalid_or_oversized_source();
  shell_stores_lines_and_lists_program();
  shell_new_clears_program();
  shell_run_executes_print_program();
  shell_save_exports_program_image_to_storage_callback();
  shell_load_imports_program_image_from_storage_callback();
  shell_load_failure_preserves_existing_program();
  evaluates_integer_expressions();
  evaluates_integer_variables_with_runtime();
  evaluates_string_literals();
  rejects_bad_expressions();
  runner_reports_unsupported_statement_line();
  runner_goto_jumps_to_target_line();
  runner_reports_missing_goto_target();
  runner_gosub_returns_to_next_line();
  runner_reports_missing_gosub_target();
  runner_reports_return_without_gosub();
  runner_reports_gosub_stack_overflow();
  runner_if_then_jumps_when_comparison_true();
  runner_if_then_falls_through_when_comparison_false();
  runner_if_then_accepts_goto_target();
  runner_if_then_handles_relational_operators();
  runner_if_then_reports_missing_target();
  runner_if_then_reports_bad_condition();
  runner_stops_goto_loops_at_step_limit();
  runner_reports_bad_print_expression_line();
  runner_let_sets_integer_variable();
  runner_reports_undefined_variable_line();
  runner_rejects_string_assignment();
  runner_input_assigns_integer_variable();
  runner_input_reports_unavailable_source();
  runner_input_reports_bad_integer();

  if (failures) {
    printf("basic program tests failed: %d\n", failures);
    return 1;
  }

  printf("basic program tests passed\n");
  return 0;
}

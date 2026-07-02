/*
 * basic.h - Clean-room SegaOS BASIC program buffer.
 *
 * This is the first interpreter seam: line-number parsing, small keyword
 * tokenization, sorted storage, replace/delete, LIST/NEW shell commands,
 * decode, simple expression values, and a tiny PRINT/END/GOTO/GOSUB/IF/INPUT
 * runner. It does handle fixed A-Z integer LET variables, but not string
 * variables or display/storage hardware yet.
 */

#ifndef BASIC_H
#define BASIC_H

#include <stdint.h>

#define BAS_MAX_LINE_NUMBER 63999U
#define BAS_MAX_PAYLOAD_TEXT 96U
#define BAS_MAX_STRING_VALUE 96U
#define BAS_MAX_PROGRAM_LINES 64U
#define BAS_MAX_PROGRAM_STORAGE 2048U
#define BAS_RUN_MAX_STEPS 128U
#define BAS_VARIABLE_COUNT 26U
#define BAS_GOSUB_STACK_DEPTH 8U

typedef enum {
  BAS_TOK_RAW = 0,
  BAS_TOK_PRINT = 0x81,
  BAS_TOK_INPUT = 0x82,
  BAS_TOK_LET = 0x83,
  BAS_TOK_IF = 0x84,
  BAS_TOK_THEN = 0x85,
  BAS_TOK_GOTO = 0x86,
  BAS_TOK_GOSUB = 0x87,
  BAS_TOK_RETURN = 0x88,
  BAS_TOK_FOR = 0x89,
  BAS_TOK_NEXT = 0x8a,
  BAS_TOK_END = 0x8b,
  BAS_TOK_REM = 0x8c
} BasicToken;

typedef struct {
  uint16_t number;
  BasicToken token;
  char payload[BAS_MAX_PAYLOAD_TEXT + 1];
  uint16_t payloadLength;
  uint8_t hasBody;
} BasicParsedLine;

typedef struct {
  uint16_t number;
  uint16_t offset;
  uint16_t length;
} BasicLine;

typedef struct {
  BasicLine *lines;
  uint8_t lineCapacity;
  uint8_t lineCount;
  uint8_t *storage;
  uint16_t storageCapacity;
  uint16_t storageUsed;
} BasicProgram;

typedef enum {
  BAS_CMD_EMPTY = 0,
  BAS_CMD_PROGRAM_LINE = 1,
  BAS_CMD_LIST = 2,
  BAS_CMD_NEW = 3,
  BAS_CMD_RUN = 4,
  BAS_CMD_UNSUPPORTED = 5,
  BAS_CMD_ERROR = 6
} BasicCommandKind;

typedef uint8_t (*BasicLineSink)(const char *line, void *user);
typedef uint8_t (*BasicInputSource)(char *out, uint16_t outBytes, void *user);

typedef struct {
  BasicCommandKind kind;
  uint8_t ok;
  uint8_t linesEmitted;
} BasicCommandResult;

typedef enum {
  BAS_VALUE_NONE = 0,
  BAS_VALUE_INTEGER = 1,
  BAS_VALUE_STRING = 2
} BasicValueKind;

typedef struct {
  BasicValueKind kind;
  int16_t integer;
  char string[BAS_MAX_STRING_VALUE + 1U];
  uint16_t stringLength;
} BasicValue;

typedef struct {
  uint8_t integerDefined[BAS_VARIABLE_COUNT];
  int16_t integerValues[BAS_VARIABLE_COUNT];
} BasicRuntime;

typedef enum {
  BAS_RUN_COMPLETE = 0,
  BAS_RUN_HALTED = 1,
  BAS_RUN_BAD_EXPRESSION = 2,
  BAS_RUN_UNSUPPORTED_STATEMENT = 3,
  BAS_RUN_OUTPUT_REJECTED = 4,
  BAS_RUN_BUFFER_TOO_SMALL = 5,
  BAS_RUN_BAD_TARGET = 6,
  BAS_RUN_MISSING_LINE = 7,
  BAS_RUN_STEP_LIMIT = 8,
  BAS_RUN_BAD_ASSIGNMENT = 9,
  BAS_RUN_UNDEFINED_VARIABLE = 10,
  BAS_RUN_INPUT_UNAVAILABLE = 11,
  BAS_RUN_BAD_INPUT = 12,
  BAS_RUN_RETURN_WITHOUT_GOSUB = 13,
  BAS_RUN_GOSUB_STACK_OVERFLOW = 14
} BasicRunStatus;

typedef struct {
  BasicRunStatus status;
  uint8_t statementsExecuted;
  uint8_t linesEmitted;
  uint16_t errorLine;
} BasicRunResult;

void BAS_InitProgram(BasicProgram *program, BasicLine *lines,
                     uint8_t lineCapacity, uint8_t *storage,
                     uint16_t storageCapacity);
void BAS_ClearProgram(BasicProgram *program);
uint8_t BAS_ParseSourceLine(const char *source, BasicParsedLine *out);
uint8_t BAS_StoreSourceLine(BasicProgram *program, const char *source);
const BasicLine *BAS_GetLine(const BasicProgram *program, uint8_t index);
const uint8_t *BAS_GetLineBytes(const BasicProgram *program,
                                const BasicLine *line);
const char *BAS_TokenName(BasicToken token);
uint8_t BAS_DecodeLine(const BasicProgram *program, const BasicLine *line,
                       char *out, uint16_t outBytes);
uint8_t BAS_ListProgram(const BasicProgram *program, BasicLineSink sink,
                        void *user, char *lineBuffer,
                        uint16_t lineBufferBytes, uint8_t *linesEmitted);
uint8_t BAS_SubmitConsoleLine(BasicProgram *program, const char *input,
                              BasicLineSink sink, void *user,
                              char *lineBuffer, uint16_t lineBufferBytes,
                              BasicCommandResult *result);
void BAS_InitRuntime(BasicRuntime *runtime);
uint8_t BAS_RuntimeSetInteger(BasicRuntime *runtime, char name, int16_t value);
uint8_t BAS_RuntimeGetInteger(const BasicRuntime *runtime, char name,
                              int16_t *out);
uint8_t BAS_EvaluateExpression(const char *source, BasicValue *out);
uint8_t BAS_EvaluateExpressionWithRuntime(const char *source,
                                          const BasicRuntime *runtime,
                                          BasicValue *out);
uint8_t BAS_RunProgram(const BasicProgram *program, BasicLineSink sink,
                       void *user, char *lineBuffer,
                       uint16_t lineBufferBytes, BasicRunResult *result);
uint8_t BAS_RunProgramWithRuntime(const BasicProgram *program,
                                  BasicRuntime *runtime, BasicLineSink sink,
                                  void *user, char *lineBuffer,
                                  uint16_t lineBufferBytes,
                                  BasicRunResult *result);
uint8_t BAS_RunProgramWithIO(const BasicProgram *program, BasicRuntime *runtime,
                             BasicLineSink sink, BasicInputSource input,
                             void *user, char *lineBuffer,
                             uint16_t lineBufferBytes,
                             BasicRunResult *result);

#endif /* BASIC_H */

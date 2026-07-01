/*
 * basic.h - Clean-room SegaOS BASIC program buffer.
 *
 * This is the first interpreter seam: line-number parsing, small keyword
 * tokenization, sorted storage, replace/delete, and decode. It does not
 * evaluate expressions or touch display/storage hardware yet.
 */

#ifndef BASIC_H
#define BASIC_H

#include <stdint.h>

#define BAS_MAX_LINE_NUMBER 63999U
#define BAS_MAX_PAYLOAD_TEXT 96U
#define BAS_MAX_PROGRAM_LINES 64U
#define BAS_MAX_PROGRAM_STORAGE 2048U

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

void BAS_InitProgram(BasicProgram *program, BasicLine *lines,
                     uint8_t lineCapacity, uint8_t *storage,
                     uint16_t storageCapacity);
uint8_t BAS_ParseSourceLine(const char *source, BasicParsedLine *out);
uint8_t BAS_StoreSourceLine(BasicProgram *program, const char *source);
const BasicLine *BAS_GetLine(const BasicProgram *program, uint8_t index);
const uint8_t *BAS_GetLineBytes(const BasicProgram *program,
                                const BasicLine *line);
const char *BAS_TokenName(BasicToken token);
uint8_t BAS_DecodeLine(const BasicProgram *program, const BasicLine *line,
                       char *out, uint16_t outBytes);

#endif /* BASIC_H */

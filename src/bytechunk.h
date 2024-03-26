#ifndef MEKVM_BYTECHUNK_H
#define MEKVM_BYTECHUNK_H

#include "common.h"
#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NEGATE,
  OP_RETURN,
} OpCode;

typedef struct {
  int count;
  int capacity;
  uint8_t *code;
  int *lines;
  ValueArray constants;
} ByteChunk;

void initByteChunk(ByteChunk *byteChunk);
void writeByteChunk(ByteChunk *byteChunk, uint8_t byte, int line);
int addConstant(ByteChunk *byteChunk, Value value);
void freeByteChunk(ByteChunk *byteChunk);

#endif /* MEKVM_BYTECHUNK_H */

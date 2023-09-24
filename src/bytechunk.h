#ifndef MEKVM_BYTECHUNK_H
#define MEKVM_BYTECHUNK_H

#include "common.h"
#include "memory.h"

typedef enum {
  OP_RETURN,
} OpCode;

typedef struct {
  int count;
  int capacity;
  uint8_t* codes;
} ByteChunk;

void initByteChunk(ByteChunk* chunk);
void freeByteChunk(ByteChunk* chunk);
void writeByte(ByteChunk* chunk, uint8_t byte);

#endif 

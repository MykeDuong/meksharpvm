#ifndef MEKVM_CHUNK_H
#define MEKVM_CHUNK_H

#include "common.h"
#include "memory.h"
#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_RETURN,
} OpCode;

typedef struct {
  int count;
  int capacity;
  uint8_t* code;
  int* lines;
  ValueArray constants;
} ByteChunk;

void initChunk(ByteChunk* chunk);
void writeChunk(ByteChunk* chunk, uint8_t byte, int line);
int addConstant(ByteChunk* chunk, Value value);
void freeChunk(ByteChunk* chunk);

#endif 

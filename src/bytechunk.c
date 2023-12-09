#include <stdint.h>
#include <stdlib.h>

#include "bytechunk.h"
#include "value.h"

void initChunk(ByteChunk *chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->lineSize = 0;
  chunk->lineCapacity = 0;
  chunk->code = NULL;
  chunk->lines = NULL;
  initValueArray(&chunk->constants);
}

void writeChunk(ByteChunk* chunk, uint8_t byte, int line) {
  if (chunk->capacity < chunk->count + 1) {
    // Grow
    int oldCap = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCap);
    chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCap, chunk->capacity);
  }

  // Separate lines with other capacities
  if (chunk->lineCapacity < chunk->lineSize + 2) {
    int oldCap = chunk->lineCapacity;
    chunk->lineCapacity = GROW_CAPACITY(oldCap);
    chunk->lines = GROW_ARRAY(int, chunk->lines, oldCap, chunk->lineCapacity);
  }

  chunk->code[chunk->count] = byte;
  chunk->count++;

  // Line
  if (chunk->lineSize == 0 || line != chunk->lines[chunk->lineSize - 1]) {
    chunk->lines[chunk->lineSize] = 1;
    chunk->lines[chunk->lineSize + 1] = line;
    chunk->lineSize += 2;
  }
  else {
    chunk->lines[chunk->lineSize - 2]++;
  }
}

int addConstant(ByteChunk* chunk, Value value) {
  writeValueArray(&chunk->constants, value);
  return chunk->constants.count - 1;
}

void writeConstant(ByteChunk* chunk, Value value, int line) {
  int idx = addConstant(chunk, value);
  if (idx < 255) {
    writeChunk(chunk, OP_CONSTANT, line);
    writeChunk(chunk, (uint8_t)idx, line);
  } else {
    writeChunk(chunk, OP_CONSTANT_LONG, line);
    writeChunk(chunk, (uint8_t)(idx & 0xff), line);
    writeChunk(chunk, (uint8_t)((idx >> 8) & 0xff), line);
    writeChunk(chunk, (uint8_t)((idx >> 16) & 0xff), line);
  }
}

void freeChunk(ByteChunk* chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
  FREE_ARRAY(int, chunk->lines, chunk->capacity);
  freeValueArray(&chunk->constants);
  initChunk(chunk);
}



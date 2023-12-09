#include <stdlib.h>

#include "bytechunk.h"
#include "value.h"

void initChunk(ByteChunk *chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
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
    chunk->lines = GROW_ARRAY(int, chunk->lines, oldCap, chunk->capacity);
  } 

  chunk->code[chunk->count] = byte;
  chunk->lines[chunk->count] = line;
  chunk->count++;
}

int addConstant(ByteChunk* chunk, Value value) {
  writeValueArray(&chunk->constants, value);
  return chunk->constants.count - 1;
}

void freeChunk(ByteChunk* chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
  FREE_ARRAY(int, chunk->lines, chunk->capacity);
  freeValueArray(&chunk->constants);
  initChunk(chunk);
}



#include <stdio.h>

#include "bytechunk.h"

void initByteChunk(ByteChunk* chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->codes = NULL;
}

void freeByteChunk(ByteChunk* chunk) {
  FREE_ARRAY(uint8_t, chunk->codes, chunk->capacity);
  initByteChunk(chunk);
}

void writeByte(ByteChunk* chunk, uint8_t byte) {
  if (chunk->capacity < chunk->count + 1) {
    int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);
    chunk->codes = GROW_ARRAY(uint8_t, chunk->codes, oldCapacity, chunk->capacity);
  }

  chunk->codes[chunk->count] = byte;
  chunk->count++;
}

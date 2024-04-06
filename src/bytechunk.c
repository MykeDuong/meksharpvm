#include <stdlib.h>

#include "bytechunk.h"
#include "memory.h"
#include "vm.h"

void initByteChunk(ByteChunk *byteChunk) {
  byteChunk->count = 0;
  byteChunk->capacity = 0;
  byteChunk->code = NULL;
  byteChunk->lines = NULL;
  initValueArray(&byteChunk->constants);
}

void writeByteChunk(ByteChunk *byteChunk, uint8_t byte, int line) {
  if (byteChunk->capacity < byteChunk->count + 1) {
    int oldCapacity = byteChunk->capacity;
    byteChunk->capacity = GROW_CAPACITY(oldCapacity);
    byteChunk->code =
        GROW_ARRAY(uint8_t, byteChunk->code, oldCapacity, byteChunk->capacity);
    byteChunk->lines =
        GROW_ARRAY(int, byteChunk->lines, oldCapacity, byteChunk->capacity);
  }

  byteChunk->code[byteChunk->count] = byte;
  byteChunk->lines[byteChunk->count] = line;
  byteChunk->count++;
}

int addConstant(ByteChunk *byteChunk, Value value) {
  push(value);
  writeValueArray(&byteChunk->constants, value);
  pop();
  return byteChunk->constants.count - 1;
}

void freeByteChunk(ByteChunk *byteChunk) {
  FREE_ARRAY(uint8_t, byteChunk->code, byteChunk->capacity);
  FREE_ARRAY(int, byteChunk->lines, byteChunk->capacity);
  freeValueArray(&byteChunk->constants);
  initByteChunk(byteChunk);
}

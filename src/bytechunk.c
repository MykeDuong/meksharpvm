#include <stdint.h>
#include <stdlib.h>

#include "bytechunk.h"
#include "memory.h"
#include "vm.h"
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

void writeChunk(VirtualMachine* vm, ByteChunk* chunk, uint8_t byte, int line) {
  if (chunk->capacity < chunk->count + 1) {
    // Grow
    int oldCap = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCap);
    chunk->code = GROW_ARRAY(vm, uint8_t, chunk->code, oldCap, chunk->capacity);
  }

  // Separate lines with other capacities
  if (chunk->lineCapacity < chunk->lineSize + 2) {
    int oldCap = chunk->lineCapacity;
    chunk->lineCapacity = GROW_CAPACITY(oldCap);
    chunk->lines = GROW_ARRAY(vm, int, chunk->lines, oldCap, chunk->lineCapacity);
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

int addConstant(VirtualMachine* vm, ByteChunk* chunk, Value value) {
  writeValueArray(vm, &chunk->constants, value);
  return chunk->constants.count - 1;
}

void writeConstant(VirtualMachine* vm, ByteChunk* chunk, Value value, int line) {
  int idx = addConstant(vm, chunk, value);
  if (idx < 255) {
    writeChunk(vm, chunk, OP_CONSTANT, line);
    writeChunk(vm, chunk, (uint8_t)idx, line);
  } else {
    writeChunk(vm, chunk, OP_CONSTANT_LONG, line);
    writeChunk(vm, chunk, (uint8_t)(idx & 0xff), line);
    writeChunk(vm, chunk, (uint8_t)((idx >> 8) & 0xff), line);
    writeChunk(vm, chunk, (uint8_t)((idx >> 16) & 0xff), line);
  }
}

void freeChunk(VirtualMachine* vm, ByteChunk* chunk) {
  FREE_ARRAY(vm, uint8_t, chunk->code, chunk->capacity);
  FREE_ARRAY(vm, int, chunk->lines, chunk->capacity);
  freeValueArray(vm, &chunk->constants);
  initChunk(chunk);
}



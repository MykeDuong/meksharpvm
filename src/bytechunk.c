#include <stdint.h>
#include <stdlib.h>

#include "bytechunk.h"
#include "value.h"

void initChunk(ByteChunk *chunk, VirtualMachine* vm, Compiler* compiler) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->lineSize = 0;
  chunk->lineCapacity = 0;
  chunk->code = NULL;
  chunk->lines = NULL;
  initValueArray(&chunk->constants, vm, compiler);
}

void writeChunk(ByteChunk* chunk, uint8_t byte, int line, VirtualMachine* vm, Compiler* compiler) {
  if (chunk->capacity < chunk->count + 1) {
    // Grow
    int oldCap = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCap);
    chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCap, chunk->capacity, vm, compiler);
  }

  // Separate lines with other capacities
  if (chunk->lineCapacity < chunk->lineSize + 2) {
    int oldCap = chunk->lineCapacity;
    chunk->lineCapacity = GROW_CAPACITY(oldCap);
    chunk->lines = GROW_ARRAY(int, chunk->lines, oldCap, chunk->lineCapacity, vm, compiler);
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

int addConstant(ByteChunk* chunk, Value value, VirtualMachine* vm, Compiler* compiler) {
  writeValueArray(&chunk->constants, value, vm, compiler);
  return chunk->constants.count - 1;
}

void writeConstant(ByteChunk* chunk, Value value, int line, VirtualMachine* vm, Compiler* compiler) {
  int idx = addConstant(chunk, value, vm, compiler);
  if (idx < 255) {
    writeChunk(chunk, OP_CONSTANT, line, vm, compiler);
    writeChunk(chunk, (uint8_t)idx, line, vm, compiler);
  } else {
    writeChunk(chunk, OP_CONSTANT_LONG, line, vm, compiler);
    writeChunk(chunk, (uint8_t)(idx & 0xff), line, vm, compiler);
    writeChunk(chunk, (uint8_t)((idx >> 8) & 0xff), line, vm, compiler);
    writeChunk(chunk, (uint8_t)((idx >> 16) & 0xff), line, vm, compiler);
  }
}

void freeChunk(ByteChunk* chunk, VirtualMachine* vm, Compiler* compiler) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity, vm, compiler);
  FREE_ARRAY(int, chunk->lines, chunk->capacity, vm, compiler);
  freeValueArray(&chunk->constants, vm, compiler);
  initChunk(chunk, vm, compiler);
}



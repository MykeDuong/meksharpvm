#include <stdint.h>
#include <stdio.h>

#include "bytechunk.h"
#include "debug.h"

void disassembleByteChunk(ByteChunk *chunk, const char *name) {
  printf("=== %s ===\n", name);

  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

static int simpleInstruction(const char* name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

int disassembleInstruction(ByteChunk *chunk, int offset) {
  printf("%04d ", offset);

  uint8_t instruction = chunk->codes[offset];
  switch (instruction) {
    case OP_RETURN:
      return simpleInstruction("OP_RETURN", offset);
    default:
      printf("Unknown opcode %04d\n", instruction);
      return offset + 1;
  }
}

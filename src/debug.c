#include <stdint.h>
#include <stdio.h>

#include "bytechunk.h"
#include "debug.h"

void disassembleChunk(ByteChunk* chunk, const char* name) {
  printf("======== %s ========\n", name);
  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

static int getLine(ByteChunk* chunk, int offset) {
  int i = 0;
  int cnt = 0;

  while (i < chunk->lineSize) {
    cnt += chunk->lines[i];
    
    if (cnt > offset) {
      return chunk->lines[i + 1];
    }

    i += 2;
  }

  printf("Error with line count\n");
  return i - 1;
}

static int simpleInstruction(const char* name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int constantInstruction(const char* name, ByteChunk* chunk, int offset) {
  uint8_t constant = chunk->code[offset + 1];
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 2;
}

int disassembleInstruction(ByteChunk* chunk, int offset) {
  printf("%04d ", offset);
  int line = getLine(chunk, offset);
  if (offset > 0 && line == getLine(chunk, offset - 1)) {
    printf("   | ");
  } else {
    printf("%4d ", line);
  }

  uint8_t instruction = chunk->code[offset];
  switch (instruction) {
    case OP_CONSTANT:
      return constantInstruction("OP_CONSTANT", chunk, offset);
    case OP_RETURN:
      return simpleInstruction("OP_RETURN", offset);
    default:
      printf("Unknown opcode %d\n", instruction);
      return offset + 1;
  }
}


#include <stdint.h>
#include <stdio.h>

#include "debug.h"
#include "value.h"

void disassembleByteChunk(ByteChunk *byteChunk, const char *name) {
  printf("==== %s ====\n", name);

  for (int offset = 0; offset < byteChunk->count;) {
    offset = disassembleInstruction(byteChunk, offset);
  }
}

static int constantInstruction(const char *name, ByteChunk *byteChunk,
                               int offset) {
  uint8_t constant = byteChunk->code[offset + 1];
  printf("%-16s %4d '", name, constant);
  printValue(byteChunk->constants.values[constant]);
  printf("'\n");
  return offset + 2;
}

static int simpleInstruction(const char *name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

int disassembleInstruction(ByteChunk *byteChunk, int offset) {
  printf("%04d ", offset);

  if (offset > 0 && byteChunk->lines[offset] == byteChunk->lines[offset - 1]) {
    printf("   | ");
  } else {
    printf("%4d ", byteChunk->lines[offset]);
  }

  uint8_t instruction = byteChunk->code[offset];
  switch (instruction) {
    case OP_CONSTANT:
      return constantInstruction("OP_CONSTANT", byteChunk, offset);
    case OP_ADD:
      return simpleInstruction("OP_ADD", offset);
    case OP_SUBTRACT:
      return simpleInstruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY:
      return simpleInstruction("OP_MULTIPLY", offset);
    case OP_DIVIDE:
      return simpleInstruction("OP_DIVIDE", offset);
    case OP_NEGATE:
      return simpleInstruction("OP_NEGATE", offset);
    case OP_RETURN:
      return simpleInstruction("OP_RETURN", offset);
    default:
      printf("Unknown opcode %d\n", instruction);
      return offset + 1;
  }
}

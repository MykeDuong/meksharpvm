#include <stdint.h>
#include <stdio.h>

#include "bytechunk.h"
#include "debug.h"
#include "object.h"
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

static int invokeInstruction(const char *name, ByteChunk *byteChunk,
                             int offset) {
  uint8_t constant = byteChunk->code[offset + 1];
  uint8_t argCount = byteChunk->code[offset + 2];
  printf("%-16s (%d args) %4d", name, argCount, constant);
  printValue(byteChunk->constants.values[constant]);
  printf("\n");
  return offset + 3;
}

static int simpleInstruction(const char *name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int byteInstruction(const char *name, ByteChunk *byteChunk, int offset) {
  uint8_t slot = byteChunk->code[offset + 1];
  printf("%-16s %4d\n", name, slot);
  return offset + 2;
}

static int jumpInstruction(const char *name, int sign, ByteChunk *byteChunk,
                           int offset) {
  uint16_t jump = (uint16_t)(byteChunk->code[offset + 1] << 8);
  jump |= byteChunk->code[offset + 2];
  printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);

  return offset + 3;
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
    case OP_NAH:
      return simpleInstruction("OP_NAH", offset);
    case OP_TRUE:
      return simpleInstruction("OP_TRUE", offset);
    case OP_FALSE:
      return simpleInstruction("OP_FALSE", offset);
    case OP_POP:
      return simpleInstruction("OP_POP", offset);
    case OP_GET_LOCAL:
      return byteInstruction("OP_GET_LOCAL", byteChunk, offset);
    case OP_SET_LOCAL:
      return byteInstruction("OP_GET_LOCAL", byteChunk, offset);
    case OP_GET_GLOBAL:
      return constantInstruction("OP_GET_GLOBAL", byteChunk, offset);
    case OP_SET_GLOBAL:
      return constantInstruction("OP_SET_GLOBAL", byteChunk, offset);
    case OP_DEFINE_GLOBAL:
      return constantInstruction("OP_DEFINE_GLOBAL", byteChunk, offset);
    case OP_GET_UPVALUE:
      return byteInstruction("OP_GET_UPVALUE", byteChunk, offset);
    case OP_SET_UPVALUE:
      return byteInstruction("OP_SET_UPVALUE", byteChunk, offset);
    case OP_GET_PROPERTY:
      return constantInstruction("OP_GET_PROPERTY", byteChunk, offset);
    case OP_SET_PROPERTY:
      return constantInstruction("OP_SET_PROPERTY", byteChunk, offset);
    case OP_GET_SUPER:
      return constantInstruction("OP_GET_SUPER", byteChunk, offset);
    case OP_EQUAL:
      return simpleInstruction("OP_EQUAL", offset);
    case OP_GREATER:
      return simpleInstruction("OP_GREATER", offset);
    case OP_LESS:
      return simpleInstruction("OP_LESS", offset);
    case OP_ADD:
      return simpleInstruction("OP_ADD", offset);
    case OP_SUBTRACT:
      return simpleInstruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY:
      return simpleInstruction("OP_MULTIPLY", offset);
    case OP_DIVIDE:
      return simpleInstruction("OP_DIVIDE", offset);
    case OP_NOT:
      return simpleInstruction("OP_NOT", offset);
    case OP_NEGATE:
      return simpleInstruction("OP_NEGATE", offset);
    case OP_PRINT:
      return simpleInstruction("OP_PRINT", offset);
    case OP_JUMP:
      return jumpInstruction("OP_JUMP", 1, byteChunk, offset);
    case OP_JUMP_IF_FALSE:
      return jumpInstruction("OP_JUMP_IF_FALSE", 1, byteChunk, offset);
    case OP_LOOP:
      return jumpInstruction("OP_LOOP", -1, byteChunk, offset);
    case OP_CALL:
      return byteInstruction("OP_CALL", byteChunk, offset);
    case OP_INVOKE:
      return invokeInstruction("OP_INVOKE", byteChunk, offset);
    case OP_SUPER_INVOKE:
      return invokeInstruction("OP_SUPER_INVOKE", byteChunk, offset);
    case OP_CLOSURE: {
      offset++;
      uint8_t constant = byteChunk->code[offset++];
      printf("%-16s %4d ", "OP_CLOSURE", constant);
      printValue(byteChunk->constants.values[constant]);
      printf("\n");

      ObjectFunction *function =
          AS_FUNCTION(byteChunk->constants.values[constant]);

      for (int j = 0; j < function->upvalueCount; j++) {
        int isLocal = byteChunk->code[offset++];
        int index = byteChunk->code[offset++];
        // Local starts from 1 (offseting the function at the first slot of
        // the stack)
        // Upvalue starts from 0
        printf("%04d    |                     %s %d\n", offset - 2,
               isLocal ? "local" : "upvalue", index);
      }
      return offset;
    }
    case OP_CLOSE_UPVALUE:
      return simpleInstruction("OP_CLOSE_UPVALUE", offset);
    case OP_RETURN:
      return simpleInstruction("OP_RETURN", offset);
    case OP_CLASS:
      return constantInstruction("OP_CLASS", byteChunk, offset);
    case OP_INHERIT:
      return simpleInstruction("OP_INHERIT", offset);
    case OP_METHOD:
      return constantInstruction("OP_METHOD", byteChunk, offset);
    default:
      printf("Unknown opcode %d\n", instruction);
      return offset + 1;
  }
}

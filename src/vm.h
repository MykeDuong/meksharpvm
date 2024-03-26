#ifndef MEKVM_VM_H
#define MEKVM_VM_H

#include "bytechunk.h"
#include "common.h"

#define STACK_MAX 256

typedef struct {
  ByteChunk *byteChunk;
  uint8_t *ip;
  Value stack[STACK_MAX];
  Value *stackTop;
} VirtualMachine;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void initVirtualMachine();
void freeVirtualMachine();

InterpretResult interpret(ByteChunk *byteChunk);
void push(Value value);
Value pop();

#endif /* MEKVM_VM_H */

#ifndef MEKVM_VM_H
#define MEKVM_VM_H

#include "bytechunk.h"
#include "common.h"
#include "table.h"

#define STACK_MAX 256

typedef struct {
  ByteChunk *byteChunk;
  uint8_t *ip;
  Value stack[STACK_MAX];
  Value *stackTop;
  Table globals;
  Table strings;
  Object *objects;
} VirtualMachine;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

extern VirtualMachine vm;

void initVirtualMachine();
void freeVirtualMachine();

InterpretResult interpret(const char *source);
void push(Value value);
Value pop();

#endif /* MEKVM_VM_H */

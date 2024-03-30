#ifndef MEKVM_VM_H
#define MEKVM_VM_H

#include "bytechunk.h"
#include "common.h"
#include "object.h"
#include "table.h"

#define FRAMES_MAX 64
#define STACK_MAX 256

typedef struct {
  ObjectFunction *function;
  uint8_t *ip;
  Value *slots;
} CallFrame;

typedef struct {
  CallFrame frames[FRAMES_MAX];
  int frameCount;

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

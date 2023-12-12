#ifndef MEKVM_VM_H
#define MEKVM_VM_H

#include "bytechunk.h"
#include "value.h"

#define STACK_MAX 256

typedef struct {
  ByteChunk* chunk;
  uint8_t* ip;

  // Stack 
  Value* stack;
  int stackCapacity;
  Value* stackTop;
} VirtualMachine;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;


void initVM(VirtualMachine* vm);
void freeVM(VirtualMachine* vm);
InterpretResult interpret(VirtualMachine* vm, const char* source);
void push(VirtualMachine* vm, Value value);
Value pop(VirtualMachine* vm);


#endif 

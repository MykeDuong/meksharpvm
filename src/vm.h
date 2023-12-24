#ifndef MEKVM_VM_H
#define MEKVM_VM_H

#include "bytechunk.h"
#include "value.h"
#include "table.h"

#define STACK_MAX 256

struct VirtualMachine_Struct{
  ByteChunk* chunk;
  uint8_t* ip;

  // Stack 
  Value* stack;
  int stackCapacity;
  Value* stackTop;
  Table globals;
  Table strings;
  Object* objects;
} ;

typedef struct VirtualMachine_Struct VirtualMachine;

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

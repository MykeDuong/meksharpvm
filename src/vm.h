#ifndef MEKVM_VM_H
#define MEKVM_VM_H

#include "bytechunk.h"
#include "value.h"
#include "table.h"
#include "object.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAME_MAX * UINT8_COUNT)

typedef struct {
  ObjClosure* closure;
  uint8_t* ip;
  int slots;
} CallFrame;

struct VirtualMachine_Struct{
  CallFrame frames[FRAMES_MAX];
  int frameCount;

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

#ifndef MEKVM_COMPILER_H
#define MEKVM_COMPILER_H

#include "scanner.h"
#include "common.h"
#include "vm.h"
#include "object.h"

typedef struct {
  Token name;
  int depth;
  bool isCaptured;
} Local;

typedef struct {
  uint8_t index;
  bool isLocal;
} Upvalue;

typedef enum {
  TYPE_FUNCTION,
  TYPE_SCRIPT,
} FunctionType;

struct Compiler_Struct {
  struct Compiler_Struct* enclosing;
  ObjFunction* function;
  FunctionType type;

  Local locals[UINT8_COUNT];
  int localCount;
  Upvalue upvalues[UINT8_COUNT];
  int scopeDepth;
};

typedef struct Compiler_Struct Compiler;
ObjFunction* compile(VirtualMachine* vm, const char* source);

#endif 
